// Filename:-	efx.cpp
//
// Minimal JKA .efx effect system for ModView. Parses Particle primitives
// and renders live instances as additive-blended billboards attached to
// the bolt that spawned them (so effects move with the model).
//
// File format reference: JoF_EJK/code/client/FxScheduler.cpp / FxUtil.cpp.
// We intentionally support a *subset* of the format - enough to cover the
// majority of character-attached effects (muzzle flashes, force VFX, blood,
// saber ignitions). Non-Particle primitives are tolerated but skipped so
// unknown .efx files load without errors.

#include "stdafx.h"
#include "includes.h"
#include "shader.h"
#include "textures.h"
#include "files.h"
#include "efx.h"

#include <math.h>

// Shader's tokenizer - not exported in shader.h but reusable.
extern char *COM_ParseExt( char **data_p, qboolean allowLineBreaks );

// =============================================================================
// Data: parsed effect template
// =============================================================================

struct EfxParticleDef
{
	int		iLifeMin, iLifeMax;			// ms
	int		iCountMin, iCountMax;		// particles per trigger

	// Spatial (all in bolt-local / model-local space)
	float	vOriginMin[3], vOriginMax[3];	// offset relative to bolt
	float	vVelMin[3],    vVelMax[3];
	float	fGravity;						// units/sec^2, pulls -Z

	// Appearance
	float	fSizeStartMin, fSizeStartMax;
	float	fSizeEndMin,   fSizeEndMax;
	float	fAlphaStart, fAlphaEnd;
	float	vRgbStart[3], vRgbEnd[3];

	char	sShader[128];
	GLuint	uiTexBind;						// resolved at parse time; 0 if missing
};

struct EfxEffectDef
{
	vector<EfxParticleDef> Particles;
};

// =============================================================================
// Data: live particles (spawned instances, in bolt-local / model-local space)
// =============================================================================

struct EfxLiveParticle
{
	DWORD	dwStartMs;
	int		iLifeMs;
	float	vPos[3], vVel[3];		// model-local coords
	float	fGravity;
	float	fSizeStart, fSizeEnd;
	float	fAlphaStart, fAlphaEnd;
	float	vRgbStart[3], vRgbEnd[3];
	GLuint	uiTexBind;
};

static map<string, EfxEffectDef*>						g_EffectCache;
static map<ModelContainer_t*, vector<EfxLiveParticle> >	g_ParticlesByContainer;
static DWORD											g_dwLastUpdateMs = 0;

// =============================================================================
// Small helpers
// =============================================================================

static inline float efx_randFloat(float a, float b)
{
	if (a == b) return a;
	float t = (float)rand() / (float)RAND_MAX;
	return a + (b - a) * t;
}
static inline int efx_randInt(int a, int b)
{
	if (a == b) return a;
	if (b < a) { int t = a; a = b; b = t; }
	return a + (rand() % (b - a + 1));
}
static inline float efx_lerp(float a, float b, float t) { return a + (b - a) * t; }

// Normalize a path to the EFX cache key (lowercase, stripped extension).
static string efx_normalizePath(const char *psPath)
{
	string s = psPath ? psPath : "";
	size_t n = s.size();
	if (n > 4) {
		if (s[n-4] == '.' &&
			tolower((unsigned char)s[n-3]) == 'e' &&
			tolower((unsigned char)s[n-2]) == 'f' &&
			tolower((unsigned char)s[n-1]) == 'x')
			s.resize(n - 4);
	}
	for (size_t i = 0; i < s.size(); i++)
		s[i] = (char)tolower((unsigned char)s[i]);
	return s;
}

static int efx_parseFloats(char **text, float *dst, int count)
{
	int i;
	for (i = 0; i < count; i++) {
		char *tok = COM_ParseExt(text, qfalse);
		if (!tok[0]) break;
		dst[i] = (float)atof(tok);
	}
	return i;
}

// Skip a { ... } block starting at the current read position.
static void efx_skipBracedBlock(char **text)
{
	char *tok = COM_ParseExt(text, qtrue);
	if (tok[0] != '{') return;
	int d = 1;
	while (d > 0) {
		tok = COM_ParseExt(text, qtrue);
		if (!tok[0]) return;
		if (tok[0] == '{') d++;
		else if (tok[0] == '}') d--;
	}
}

// Read a { start N ...; end N ... } block into *outStartMin/Max, *outEndMin/Max.
// Accepts N floats per entry; floats after the first on a line widen the range.
// Returns cursor at the matched '}'.
static void efx_parseInterpolatedBlock(char **text,
									   float *outStartMin, float *outStartMax,
									   float *outEndMin,   float *outEndMax,
									   int count)
{
	char *tok = COM_ParseExt(text, qtrue);
	if (tok[0] != '{') return;

	int d = 1;
	while (d > 0) {
		tok = COM_ParseExt(text, qtrue);
		if (!tok[0]) return;
		if (tok[0] == '{') { d++; continue; }
		if (tok[0] == '}') { d--; if (d == 0) break; continue; }

		if (!Q__stricmp(tok, "start")) {
			float v[8]; int n = efx_parseFloats(text, v, count * 2);
			if (n >= count) for (int i = 0; i < count; i++) outStartMin[i] = v[i];
			if (n >= count) for (int i = 0; i < count; i++) outStartMax[i] = (n >= count*2) ? v[count + i] : v[i];
		}
		else if (!Q__stricmp(tok, "end")) {
			float v[8]; int n = efx_parseFloats(text, v, count * 2);
			if (n >= count) for (int i = 0; i < count; i++) outEndMin[i] = v[i];
			if (n >= count) for (int i = 0; i < count; i++) outEndMax[i] = (n >= count*2) ? v[count + i] : v[i];
		}
		// "flags" and other sub-keywords ignored for MVP
	}
}

// =============================================================================
// .efx file parsing - Particle primitive
// =============================================================================

static bool efx_parseParticle(char **text, EfxParticleDef *out)
{
	char *tok = COM_ParseExt(text, qtrue);
	if (tok[0] != '{') return false;

	// Defaults (picked for sane-looking MVP particles when fields are absent)
	out->iLifeMin = out->iLifeMax = 500;
	out->iCountMin = out->iCountMax = 1;
	out->vOriginMin[0] = out->vOriginMin[1] = out->vOriginMin[2] = 0.0f;
	out->vOriginMax[0] = out->vOriginMax[1] = out->vOriginMax[2] = 0.0f;
	out->vVelMin[0] = out->vVelMin[1] = out->vVelMin[2] = 0.0f;
	out->vVelMax[0] = out->vVelMax[1] = out->vVelMax[2] = 0.0f;
	out->fGravity = 0.0f;
	out->fSizeStartMin = out->fSizeStartMax = 1.0f;
	out->fSizeEndMin   = out->fSizeEndMax   = 1.0f;
	out->fAlphaStart = 1.0f;
	out->fAlphaEnd   = 0.0f;
	out->vRgbStart[0] = out->vRgbStart[1] = out->vRgbStart[2] = 1.0f;
	out->vRgbEnd[0]   = out->vRgbEnd[1]   = out->vRgbEnd[2]   = 1.0f;
	out->sShader[0] = 0;
	out->uiTexBind = 0;

	// Scratch buffers for size/alpha/rgb start+end parsing.
	float sizeStartMin[1] = {1}, sizeStartMax[1] = {1};
	float sizeEndMin[1]   = {1}, sizeEndMax[1]   = {1};
	float alphaStartMin[1]= {1}, alphaStartMax[1]= {1};
	float alphaEndMin[1]  = {0}, alphaEndMax[1]  = {0};
	float rgbStartMin[3]  = {1,1,1}, rgbStartMax[3] = {1,1,1};
	float rgbEndMin[3]    = {1,1,1}, rgbEndMax[3]   = {1,1,1};

	int depth = 1;
	while (depth > 0) {
		tok = COM_ParseExt(text, qtrue);
		if (!tok[0]) return false;
		if (tok[0] == '{') { depth++; continue; }
		if (tok[0] == '}') { depth--; if (depth == 0) break; continue; }

		if (!Q__stricmp(tok, "count")) {
			float v[2]; int n = efx_parseFloats(text, v, 2);
			if (n >= 1) { out->iCountMin = (int)v[0]; out->iCountMax = (n >= 2) ? (int)v[1] : (int)v[0]; }
		}
		else if (!Q__stricmp(tok, "life")) {
			float v[2]; int n = efx_parseFloats(text, v, 2);
			if (n >= 1) { out->iLifeMin = (int)v[0]; out->iLifeMax = (n >= 2) ? (int)v[1] : (int)v[0]; }
		}
		else if (!Q__stricmp(tok, "delay")) {
			// unused in MVP; just eat the values
			float v[2]; efx_parseFloats(text, v, 2);
		}
		else if (!Q__stricmp(tok, "origin")) {
			float v[6]; int n = efx_parseFloats(text, v, 6);
			if (n >= 3) for (int i = 0; i < 3; i++) out->vOriginMin[i] = out->vOriginMax[i] = v[i];
			if (n >= 6) for (int i = 0; i < 3; i++) out->vOriginMax[i] = v[3 + i];
		}
		else if (!Q__stricmp(tok, "velocity")) {
			float v[6]; int n = efx_parseFloats(text, v, 6);
			if (n >= 3) for (int i = 0; i < 3; i++) out->vVelMin[i] = out->vVelMax[i] = v[i];
			if (n >= 6) for (int i = 0; i < 3; i++) out->vVelMax[i] = v[3 + i];
		}
		else if (!Q__stricmp(tok, "gravity")) {
			float v[2]; int n = efx_parseFloats(text, v, 2);
			if (n >= 1) out->fGravity = v[0];
		}
		else if (!Q__stricmp(tok, "size")) {
			efx_parseInterpolatedBlock(text, sizeStartMin, sizeStartMax,
								   sizeEndMin, sizeEndMax, 1);
			out->fSizeStartMin = sizeStartMin[0]; out->fSizeStartMax = sizeStartMax[0];
			out->fSizeEndMin   = sizeEndMin[0];   out->fSizeEndMax   = sizeEndMax[0];
		}
		else if (!Q__stricmp(tok, "alpha")) {
			efx_parseInterpolatedBlock(text, alphaStartMin, alphaStartMax,
								   alphaEndMin, alphaEndMax, 1);
			// single alpha curve, collapse to start/end (ignore min/max range)
			out->fAlphaStart = alphaStartMin[0];
			out->fAlphaEnd   = alphaEndMin[0];
		}
		else if (!Q__stricmp(tok, "rgb")) {
			efx_parseInterpolatedBlock(text, rgbStartMin, rgbStartMax,
								   rgbEndMin, rgbEndMax, 3);
			for (int i = 0; i < 3; i++) { out->vRgbStart[i] = rgbStartMin[i]; out->vRgbEnd[i] = rgbEndMin[i]; }
		}
		else if (!Q__stricmp(tok, "shader") || !Q__stricmp(tok, "shaders")) {
			char *t2 = COM_ParseExt(text, qtrue);
			if (t2[0] == '[') {
				bool gotOne = false;
				while (1) {
					char *t3 = COM_ParseExt(text, qtrue);
					if (!t3[0] || t3[0] == ']') break;
					if (!gotOne) {
						strncpy(out->sShader, t3, sizeof(out->sShader) - 1);
						out->sShader[sizeof(out->sShader) - 1] = 0;
						gotOne = true;
					}
				}
			} else {
				strncpy(out->sShader, t2, sizeof(out->sShader) - 1);
				out->sShader[sizeof(out->sShader) - 1] = 0;
			}
		}
		// Everything else (flags, spawnflags, bounce, cullrange, model, sound,
		// impactFx, deathFx, etc.) is parsed-but-ignored: skip to end of line or
		// next block. The tokenizer already eats plain tokens; braced blocks
		// attached to sub-keywords are rare in Particle primitives.
	}

	// Resolve shader texture immediately and tap into the existing missing-
	// texture reporter so any gaps surface in the standard warning dialog.
	if (out->sShader[0]) {
		int h = Texture_LoadDirect(out->sShader);
		if (h > 0) out->uiTexBind = Texture_GetGLBind(h);
		if (!out->uiTexBind) {
			extern void ReportMissingShaderTexture(const char *name);
			ReportMissingShaderTexture(out->sShader);
		}
	}
	return true;
}

// =============================================================================
// .efx file loader
// =============================================================================

static EfxEffectDef *efx_loadFromDisk(const char *psEffectPathNoExt)
{
	extern char gamedir[];
	char sFullPath[1024];
	_snprintf(sFullPath, sizeof(sFullPath), "%s%s.efx", gamedir, psEffectPathNoExt);
	sFullPath[sizeof(sFullPath) - 1] = 0;

	FILE *f = fopen(sFullPath, "rb");
	if (!f) return NULL;

	fseek(f, 0, SEEK_END);
	long len = ftell(f);
	fseek(f, 0, SEEK_SET);
	if (len <= 0 || len > (1 << 20)) { fclose(f); return NULL; }

	char *buf = (char *)malloc(len + 1);
	if (!buf) { fclose(f); return NULL; }
	size_t readBytes = fread(buf, 1, len, f);
	buf[readBytes] = 0;
	fclose(f);

	EfxEffectDef *effect = new EfxEffectDef();
	char *p = buf;

	while (1) {
		char *tok = COM_ParseExt(&p, qtrue);
		if (!tok[0]) break;

		if (!Q__stricmp(tok, "Particle")) {
			EfxParticleDef part;
			if (efx_parseParticle(&p, &part))
				effect->Particles.push_back(part);
		}
		else if (!Q__stricmp(tok, "Line") ||
				 !Q__stricmp(tok, "OrientedParticle") ||
				 !Q__stricmp(tok, "Light") ||
				 !Q__stricmp(tok, "Cylinder") ||
				 !Q__stricmp(tok, "Electricity") ||
				 !Q__stricmp(tok, "Decal") ||
				 !Q__stricmp(tok, "Emitter") ||
				 !Q__stricmp(tok, "Sound") ||
				 !Q__stricmp(tok, "FxRunner") ||
				 !Q__stricmp(tok, "Trail") ||
				 !Q__stricmp(tok, "Tail") ||
				 !Q__stricmp(tok, "Flash")) {
			efx_skipBracedBlock(&p);
		}
		else if (!Q__stricmp(tok, "repeatDelay")) {
			COM_ParseExt(&p, qfalse);
		}
		else {
			// Unknown top-level - if followed by '{', skip the block
			char *scan = p;
			char *next = COM_ParseExt(&scan, qtrue);
			if (next[0] == '{') {
				p = scan;
				int d = 1;
				while (d > 0) {
					char *t2 = COM_ParseExt(&p, qtrue);
					if (!t2[0]) break;
					if (t2[0] == '{') d++;
					else if (t2[0] == '}') d--;
				}
			}
		}
	}

	free(buf);
	return effect;
}

static EfxEffectDef *efx_getEffect(const char *psEffectPath)
{
	string key = efx_normalizePath(psEffectPath);
	map<string, EfxEffectDef*>::iterator it = g_EffectCache.find(key);
	if (it != g_EffectCache.end()) return it->second;
	EfxEffectDef *fx = efx_loadFromDisk(key.c_str());
	g_EffectCache[key] = fx;		// cache negative too, to skip repeat disk hits
	return fx;
}

// =============================================================================
// Spawn: called when an animevent fires. Spawns one burst of each particle
// primitive in the effect, attached to pContainer's live list in model-local
// coordinates computed from the bolt matrix.
// =============================================================================

static void efx_spawnBurst(ModelContainer_t *pContainer, const char *psEffectPath, const float vBoltOrigin[3])
{
	EfxEffectDef *fx = efx_getEffect(psEffectPath);
	if (!fx || fx->Particles.empty()) return;

	vector<EfxLiveParticle> &list = g_ParticlesByContainer[pContainer];
	DWORD now = GetTickCount();

	for (size_t p = 0; p < fx->Particles.size(); p++) {
		const EfxParticleDef &pd = fx->Particles[p];
		int count = efx_randInt(pd.iCountMin, pd.iCountMax);
		if (count < 1) count = 1;

		for (int i = 0; i < count; i++) {
			EfxLiveParticle lp = {0};
			lp.dwStartMs   = now;
			lp.iLifeMs     = efx_randInt(pd.iLifeMin, pd.iLifeMax);
			if (lp.iLifeMs < 1) lp.iLifeMs = 1;

			for (int k = 0; k < 3; k++)
				lp.vPos[k] = vBoltOrigin[k] + efx_randFloat(pd.vOriginMin[k], pd.vOriginMax[k]);

			for (int k = 0; k < 3; k++)
				lp.vVel[k] = efx_randFloat(pd.vVelMin[k], pd.vVelMax[k]);

			lp.fGravity    = pd.fGravity;
			lp.fSizeStart  = efx_randFloat(pd.fSizeStartMin, pd.fSizeStartMax);
			lp.fSizeEnd    = efx_randFloat(pd.fSizeEndMin,   pd.fSizeEndMax);
			lp.fAlphaStart = pd.fAlphaStart;
			lp.fAlphaEnd   = pd.fAlphaEnd;
			for (int k = 0; k < 3; k++) {
				lp.vRgbStart[k] = pd.vRgbStart[k];
				lp.vRgbEnd[k]   = pd.vRgbEnd[k];
			}
			lp.uiTexBind = pd.uiTexBind;

			list.push_back(lp);
		}
	}
}

// =============================================================================
// Public: Efx_DispatchFrameEvents
// =============================================================================

void Efx_DispatchFrameEvents(ModelContainer_t *pContainer, int iCurrentFrame)
{
	if (!pContainer) return;
	AnimEventsByFrame_t::iterator it = pContainer->AnimEventsByFrame.find(iCurrentFrame);
	if (it == pContainer->AnimEventsByFrame.end()) return;

	for (size_t e = 0; e < it->second.size(); e++) {
		const AnimEvent_t &ev = it->second[e];

		// Chance roll (0 or 100 = always fire)
		if (ev.iChance > 0 && ev.iChance < 100) {
			int roll = rand() % 100;
			if (roll >= ev.iChance) continue;
		}

		// Resolve bolt on this container's model. Try surface-tag first
		// (*r_hand, *flash, etc) then fall back to bone names.
		int iBolt = Model_GetBoltIndex(pContainer->hModel, ev.sBoltName, false, true);
		bool bIsBone = false;
		if (iBolt == -1) {
			iBolt = Model_GetBoltIndex(pContainer->hModel, ev.sBoltName, true, true);
			bIsBone = true;
		}
		if (iBolt == -1) continue;	// unresolvable bolt - silent skip

		// Pull the transformed matrix for this bolt, already in model-local
		// space, populated during the most recent render pass.
		mdxaBone_t *pMat = NULL;
		if (bIsBone) {
			if (iBolt >= 0 && iBolt < MAX_POSSIBLE_BONES && pContainer->XFormedG2BonesValid[iBolt])
				pMat = &pContainer->XFormedG2Bones[iBolt];
		} else {
			if (iBolt >= 0 && iBolt < MAX_G2_SURFACES && pContainer->XFormedG2TagSurfsValid[iBolt])
				pMat = &pContainer->XFormedG2TagSurfs[iBolt];
		}
		if (!pMat) continue;

		float vOrigin[3] = { pMat->matrix[0][3], pMat->matrix[1][3], pMat->matrix[2][3] };
		efx_spawnBurst(pContainer, ev.sEffectPath, vOrigin);
	}
}

// =============================================================================
// Public: Efx_TickAll - physics integration for every container's particles,
// run once per scene render before any container draws.
// =============================================================================

void Efx_TickAll(void)
{
	DWORD now = GetTickCount();
	float dt = 0.0f;
	if (g_dwLastUpdateMs != 0) {
		DWORD elapsed = now - g_dwLastUpdateMs;
		if (elapsed < 500) dt = elapsed * 0.001f;	// clamp pauses/debugger breaks
	}
	g_dwLastUpdateMs = now;
	if (dt <= 0.0f) return;

	for (map<ModelContainer_t*, vector<EfxLiveParticle> >::iterator it = g_ParticlesByContainer.begin();
		 it != g_ParticlesByContainer.end(); ++it) {
		vector<EfxLiveParticle> &list = it->second;
		for (int i = (int)list.size() - 1; i >= 0; i--) {
			EfxLiveParticle &lp = list[i];
			DWORD age = now - lp.dwStartMs;
			if ((int)age >= lp.iLifeMs) {
				list.erase(list.begin() + i);
				continue;
			}
			for (int k = 0; k < 3; k++) lp.vPos[k] += lp.vVel[k] * dt;
			lp.vVel[2] -= lp.fGravity * dt;
		}
	}
}

// =============================================================================
// Public: Efx_RenderForContainer - just draw, no physics (tick is global).
// =============================================================================

void Efx_RenderForContainer(ModelContainer_t *pContainer)
{
	if (!pContainer) return;
	map<ModelContainer_t*, vector<EfxLiveParticle> >::iterator it = g_ParticlesByContainer.find(pContainer);
	if (it == g_ParticlesByContainer.end() || it->second.empty()) return;

	vector<EfxLiveParticle> &list = it->second;
	DWORD now = GetTickCount();

	// Camera-relative right/up from the current modelview, for billboards.
	// Modelview is the model's local space at this point, so right/up are
	// axes of the camera basis inside that space.
	float mv[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, mv);
	float right[3] = { mv[0], mv[4], mv[8] };
	float up[3]    = { mv[1], mv[5], mv[9] };
	// Normalize because HandleAllEvilMatrixCode scales by 0.05
	float rLen = sqrtf(right[0]*right[0]+right[1]*right[1]+right[2]*right[2]);
	float uLen = sqrtf(up[0]*up[0]+up[1]*up[1]+up[2]*up[2]);
	if (rLen > 0.0001f) { right[0]/=rLen; right[1]/=rLen; right[2]/=rLen; }
	if (uLen > 0.0001f) { up[0]/=uLen;    up[1]/=uLen;    up[2]/=uLen; }

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);	// additive - matches how the game renders most particle shaders by default
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_TEXTURE_2D);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	// Sync state tracker so following shader-rendered surfaces re-apply blend state
	extern unsigned int glStateBits;
	glStateBits = 0;

	for (size_t i = 0; i < list.size(); i++) {
		const EfxLiveParticle &lp = list[i];
		DWORD age = now - lp.dwStartMs;
		float t = (float)age / (float)lp.iLifeMs;
		if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

		float size  = efx_lerp(lp.fSizeStart,  lp.fSizeEnd,  t);
		float alpha = efx_lerp(lp.fAlphaStart, lp.fAlphaEnd, t);
		float r = efx_lerp(lp.vRgbStart[0], lp.vRgbEnd[0], t);
		float g = efx_lerp(lp.vRgbStart[1], lp.vRgbEnd[1], t);
		float b = efx_lerp(lp.vRgbStart[2], lp.vRgbEnd[2], t);

		glBindTexture(GL_TEXTURE_2D, lp.uiTexBind);
		glColor4f(r, g, b, alpha);

		float hw = size * 0.5f;
		float rX = right[0] * hw, rY = right[1] * hw, rZ = right[2] * hw;
		float uX = up[0]    * hw, uY = up[1]    * hw, uZ = up[2]    * hw;

		glBegin(GL_QUADS);
			glTexCoord2f(0, 0); glVertex3f(lp.vPos[0] - rX - uX, lp.vPos[1] - rY - uY, lp.vPos[2] - rZ - uZ);
			glTexCoord2f(1, 0); glVertex3f(lp.vPos[0] + rX - uX, lp.vPos[1] + rY - uY, lp.vPos[2] + rZ - uZ);
			glTexCoord2f(1, 1); glVertex3f(lp.vPos[0] + rX + uX, lp.vPos[1] + rY + uY, lp.vPos[2] + rZ + uZ);
			glTexCoord2f(0, 1); glVertex3f(lp.vPos[0] - rX + uX, lp.vPos[1] - rY + uY, lp.vPos[2] - rZ + uZ);
		glEnd();
	}

	glPopAttrib();
	glStateBits = 0;
}

// =============================================================================
// Public: Efx_Shutdown
// =============================================================================

void Efx_Shutdown(void)
{
	for (map<string, EfxEffectDef*>::iterator it = g_EffectCache.begin();
		 it != g_EffectCache.end(); ++it) {
		delete it->second;
	}
	g_EffectCache.clear();
	g_ParticlesByContainer.clear();
	g_dwLastUpdateMs = 0;
}

///////////////// eof /////////////
