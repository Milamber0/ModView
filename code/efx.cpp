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
#include "generic_stuff.h"	// g_bLogDebug
#include "efx.h"

#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Shader's tokenizer - not exported in shader.h but reusable.
extern char *COM_ParseExt( char **data_p, qboolean allowLineBreaks );

// =============================================================================
// Data: parsed effect template
// =============================================================================

enum EfxPrimType { EFXP_PARTICLE = 0, EFXP_TAIL = 1, EFXP_FXRUNNER = 2 };

struct EfxPrimitiveDef
{
	int		eType;						// EFXP_PARTICLE, EFXP_TAIL, or EFXP_FXRUNNER

	// EFXP_FXRUNNER only: paths to chained .efx files this runner plays.
	// When the parent effect spawns, each entry is recursively spawned with
	// the same bolt matrix + relative flag, so chained smoke/glow pieces
	// fire alongside the parent's own primitives.
	vector<string>	sChainedEffects;

	int		iLifeMin, iLifeMax;			// ms
	int		iCountMin, iCountMax;

	// Spatial (all in bolt-local frame when attached)
	float	vOriginMin[3], vOriginMax[3];	// offset relative to bolt
	float	vVelMin[3],    vVelMax[3];
	float	fGravity;						// accel on +Z (JKA convention: positive = upward, so smoke etc. rises)

	// Appearance
	float	fSizeStartMin, fSizeStartMax;
	float	fSizeEndMin,   fSizeEndMax;
	float	fAlphaStart, fAlphaEnd;
	float	vRgbStart[3], vRgbEnd[3];

	// Tail-specific
	float	fLengthStartMin, fLengthStartMax;
	float	fLengthEndMin,   fLengthEndMax;
	float	fSphereRadius;					// for spawnflags orgOnSphere
	bool	bOrgOnSphere;					// random spawn position on sphere of radius fSphereRadius
	bool	bAxisFromSphere;				// velocity direction = outward radial from sphere center

	char	sShader[128];
	GLuint	uiTexBind;						// resolved at parse time; 0 if missing
};

struct EfxEffectDef
{
	vector<EfxPrimitiveDef> Primitives;		// Particle and Tail mixed
};

// =============================================================================
// Data: live particles (spawned instances)
//
// Storage splits by whether the effect was bolted (FX_RELATIVE, stays with
// the bolt) or free-spawned (world space, drifts independently). Bolted-
// anim-event effects are relative; physics-collision sparks are world.
// =============================================================================

struct EfxLiveParticle
{
	int		eType;					// EFXP_PARTICLE or EFXP_TAIL

	DWORD	dwStartMs;
	int		iLifeMs;

	// When relative: vPos/vVel are in the owning container's local frame and
	// render inside its modelview so the particle stays attached to the bolt
	// (matches the game's FX_RELATIVE semantics for bolted effects).
	// When !relative: vPos/vVel are in primary-local and render globally
	// after all containers, so particles drift freely in world-ish space.
	float	vPos[3], vVel[3];
	float	fGravity;
	float	fSizeStart, fSizeEnd;
	float	fLengthStart, fLengthEnd;	// Tail only
	float	fAlphaStart, fAlphaEnd;
	float	vRgbStart[3], vRgbEnd[3];
	GLuint	uiTexBind;
};

static map<string, EfxEffectDef*>						g_EffectCache;
static map<ModelContainer_t*, vector<EfxLiveParticle> >	g_RelativeByContainer;	// FX_RELATIVE, container-local
static vector<EfxLiveParticle>							g_WorldParticles;		// free-drift, primary-local
static DWORD											g_dwLastUpdateMs = 0;

// =============================================================================
// File-based diagnostic log next to the exe (modview_efx.log). Lets us follow
// what the effect system is doing without having to attach a debugger. The log
// is truncated in Efx_Shutdown (gamedir change / scene reset).
// =============================================================================

static CString efx_logPath(void)
{
	char exePath[MAX_PATH] = {0};
	GetModuleFileName(NULL, exePath, MAX_PATH);
	char *lastSlash = strrchr(exePath, '\\');
	if (!lastSlash) lastSlash = strrchr(exePath, '/');
	if (lastSlash) *(lastSlash + 1) = 0;
	return CString(exePath) + "modview_efx.log";
}

static void efx_log(const char *fmt, ...)
{
	// Opt-in via the "-log" command-line flag. End users running ModView
	// normally don't want this scratch file appearing next to the exe.
	if (!g_bLogDebug) return;

	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	_vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	buf[sizeof(buf) - 1] = 0;

	FILE *f = fopen(efx_logPath(), "a");
	if (f) {
		fputs(buf, f);
		fclose(f);
	}
}

// Diagnostics visible without a debugger. Incremented each time an event
// actually dispatches a spawn (bolt resolved, effect loaded). Reset on
// Efx_Shutdown.
int		g_iEfxEventsFired    = 0;		// total events ever triggered this session
int		g_iEfxParticlesLive  = 0;		// refreshed each frame by Efx_TickAll
int		g_iEfxParticlesTotal = 0;		// total particles ever spawned (never resets except on shutdown)
char	g_sEfxLastEffect[256] = {0};	// last effect path that successfully spawned
char	g_sEfxLastBolt[64]    = {0};	// last bolt name that successfully resolved

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
// When `bMirrorIfOneSided` is true and only one of start/end appears in the
// file, the missing side is cloned from the one that was present. Size and
// RGB want this ("constant size 10" from `start 10`). Alpha wants the
// default behaviour, where an unspecified side keeps the caller-set default
// (so `end 0` produces a fade from 1.0 to 0). Returns at the matched '}'.
static void efx_parseInterpolatedBlock(char **text,
									   float *outStartMin, float *outStartMax,
									   float *outEndMin,   float *outEndMax,
									   int count,
									   bool bMirrorIfOneSided)
{
	char *tok = COM_ParseExt(text, qtrue);
	if (tok[0] != '{') return;

	bool bSawStart = false, bSawEnd = false;
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
			bSawStart = true;
		}
		else if (!Q__stricmp(tok, "end")) {
			float v[8]; int n = efx_parseFloats(text, v, count * 2);
			if (n >= count) for (int i = 0; i < count; i++) outEndMin[i] = v[i];
			if (n >= count) for (int i = 0; i < count; i++) outEndMax[i] = (n >= count*2) ? v[count + i] : v[i];
			bSawEnd = true;
		}
		// "flags" and other sub-keywords ignored for MVP
	}

	if (bMirrorIfOneSided) {
		if (bSawStart && !bSawEnd) {
			for (int i = 0; i < count; i++) { outEndMin[i] = outStartMin[i]; outEndMax[i] = outStartMax[i]; }
		} else if (bSawEnd && !bSawStart) {
			for (int i = 0; i < count; i++) { outStartMin[i] = outEndMin[i]; outStartMax[i] = outEndMax[i]; }
		}
	}
}

// =============================================================================
// .efx file parsing - Particle and Tail primitives share most fields
// =============================================================================

static bool efx_parsePrimitiveBlock(char **text, EfxPrimitiveDef *out, int eType)
{
	char *tok = COM_ParseExt(text, qtrue);
	if (tok[0] != '{') return false;

	// Defaults
	out->eType     = eType;
	out->iLifeMin  = out->iLifeMax  = 500;
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
	out->fLengthStartMin = out->fLengthStartMax = 4.0f;
	out->fLengthEndMin   = out->fLengthEndMax   = 4.0f;
	out->fSphereRadius   = 0.0f;
	out->bOrgOnSphere    = false;
	out->bAxisFromSphere = false;
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
		else if (!Q__stricmp(tok, "velocity") || !Q__stricmp(tok, "vel")) {
			// .efx files use both "velocity" and the short "vel" form; saber_block
			// and saber_cut rely on the latter so accept either.
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
								   sizeEndMin, sizeEndMax, 1, true);	// mirror: `start 10` = constant 10
			out->fSizeStartMin = sizeStartMin[0]; out->fSizeStartMax = sizeStartMax[0];
			out->fSizeEndMin   = sizeEndMin[0];   out->fSizeEndMax   = sizeEndMax[0];
		}
		else if (!Q__stricmp(tok, "alpha")) {
			// Defaults preserved: alpha defaults to 1 -> 0 fade, so an
			// unspecified side keeps the default instead of cloning.
			alphaStartMin[0] = 1.0f;
			alphaEndMin[0]   = 0.0f;
			efx_parseInterpolatedBlock(text, alphaStartMin, alphaStartMax,
								   alphaEndMin, alphaEndMax, 1, false);
			out->fAlphaStart = alphaStartMin[0];
			out->fAlphaEnd   = alphaEndMin[0];
		}
		else if (!Q__stricmp(tok, "rgb")) {
			efx_parseInterpolatedBlock(text, rgbStartMin, rgbStartMax,
								   rgbEndMin, rgbEndMax, 3, true);		// mirror: `start R G B` = constant colour
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
		// Tail-specific keywords
		else if (!Q__stricmp(tok, "length")) {
			float lsMin[1] = {4}, lsMax[1] = {4}, leMin[1] = {4}, leMax[1] = {4};
			efx_parseInterpolatedBlock(text, lsMin, lsMax, leMin, leMax, 1, true);
			out->fLengthStartMin = lsMin[0]; out->fLengthStartMax = lsMax[0];
			out->fLengthEndMin   = leMin[0]; out->fLengthEndMax   = leMax[0];
		}
		else if (!Q__stricmp(tok, "radius")) {
			float v[1]; efx_parseFloats(text, v, 1);
			out->fSphereRadius = v[0];
		}
		else if (!Q__stricmp(tok, "spawnflags") || !Q__stricmp(tok, "spawnFlags")) {
			// Consume the rest of the line collecting keywords.
			while (1) {
				char *t2 = COM_ParseExt(text, qfalse);
				if (!t2[0]) break;
				if (!Q__stricmp(t2, "orgOnSphere"))   out->bOrgOnSphere = true;
				else if (!Q__stricmp(t2, "axisFromSphere")) out->bAxisFromSphere = true;
				// other flags (orgOnCylinder, rgbComponentInterpolation, ...) ignored
			}
		}
		// Everything else (flags, bounce, cullrange, model, sound, impactFx,
		// deathFx, etc.) is parsed-but-ignored. Braced blocks attached to unknown
		// sub-keywords could confuse the outer depth counter, but in practice
		// the primitives we support don't use those.
	}

	// Resolve the shader through R_FindShader, not a raw texture load.
	// JKA .efx files typically reference names that are defined in
	// .shader files (e.g. `gfx/effects/forcePush` is a shader whose
	// `map` directive points at `gfx/effects/force_push` on disk).
	// R_FindShader parses that .shader block and returns a fully-bound
	// shader; if the name isn't defined anywhere it falls back to a
	// direct texture load (and fires the missing-texture warning via
	// LoadShaderTexture for us - no need to call ReportMissingShaderTexture
	// ourselves).
	if (out->sShader[0]) {
		shader_t *sh = R_FindShader(out->sShader);
		if (sh && sh->numStages > 0 && sh->stages[0].bundle[0].textures[0]) {
			out->uiTexBind = sh->stages[0].bundle[0].textures[0];
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

	// Effect paths in animevents.cfg are relative to the `effects/` folder,
	// not to gamedir. Only prepend if the caller didn't include it already.
	const char *prefix = "effects/";
	if (_strnicmp(psEffectPathNoExt, "effects/", 8) == 0 ||
		_strnicmp(psEffectPathNoExt, "effects\\", 8) == 0) {
		prefix = "";
	}
	_snprintf(sFullPath, sizeof(sFullPath), "%s%s%s.efx", gamedir, prefix, psEffectPathNoExt);
	sFullPath[sizeof(sFullPath) - 1] = 0;

	FILE *f = fopen(sFullPath, "rb");
	if (!f) {
		efx_log("EFX: could not open '%s'\n", sFullPath);
		return NULL;
	}

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
			EfxPrimitiveDef prim;
			if (efx_parsePrimitiveBlock(&p, &prim, EFXP_PARTICLE))
				effect->Primitives.push_back(prim);
		}
		else if (!Q__stricmp(tok, "Tail")) {
			EfxPrimitiveDef prim;
			if (efx_parsePrimitiveBlock(&p, &prim, EFXP_TAIL))
				effect->Primitives.push_back(prim);
		}
		else if (!Q__stricmp(tok, "FxRunner")) {
			// Parses only the `playfx [ path1 path2 ... ]` list; other
			// FxRunner fields (count, delay, origin) are ignored for MVP.
			// Each listed path is a child effect that plays alongside the
			// parent with the same bolt matrix and relative flag.
			EfxPrimitiveDef prim;
			memset(&prim, 0, sizeof(prim));
			prim.eType     = EFXP_FXRUNNER;
			prim.iCountMin = prim.iCountMax = 1;
			prim.iLifeMin  = prim.iLifeMax  = 1;

			char *tok2 = COM_ParseExt(&p, qtrue);
			if (tok2[0] == '{') {
				int d = 1;
				while (d > 0) {
					tok2 = COM_ParseExt(&p, qtrue);
					if (!tok2[0]) break;
					if (tok2[0] == '{') { d++; continue; }
					if (tok2[0] == '}') { d--; if (d == 0) break; continue; }

					if (!Q__stricmp(tok2, "playfx")) {
						char *t3 = COM_ParseExt(&p, qtrue);
						if (t3[0] == '[') {
							while (1) {
								char *t4 = COM_ParseExt(&p, qtrue);
								if (!t4[0] || t4[0] == ']') break;
								prim.sChainedEffects.push_back(string(t4));
							}
						}
					}
					// count/delay/origin ignored for MVP
				}
			}
			if (!prim.sChainedEffects.empty())
				effect->Primitives.push_back(prim);
		}
		else if (!Q__stricmp(tok, "Line") ||
				 !Q__stricmp(tok, "OrientedParticle") ||
				 !Q__stricmp(tok, "Light") ||
				 !Q__stricmp(tok, "Cylinder") ||
				 !Q__stricmp(tok, "Electricity") ||
				 !Q__stricmp(tok, "Decal") ||
				 !Q__stricmp(tok, "Emitter") ||
				 !Q__stricmp(tok, "Sound") ||
				 !Q__stricmp(tok, "Trail") ||
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

// Transform a bolt-local (X, Y, Z) triple into the owner container's local
// frame using the bolt matrix. Follows JoF_EJK's SFxHelper::GetOriginAxisFromBolt
// axis mapping:
//     axis[0] (FX forward) = bolt matrix column 1
//     axis[1] (FX left)    = bolt matrix column 0
//     axis[2] (FX up)      = bolt matrix column 2
// So .efx X maps to col1, Y maps to col0, Z maps to col2 - columns 0 and 1
// are swapped versus a naive "X->col0" mapping. This is what puts saber_clash's
// `origin 0 -20 0` along the blade (col0 of *blade1 points down the blade, so
// -20 on axis[1]=col0 is +20 toward the tip).
static void efx_rotateByBolt(const mdxaBone_t *pMat, const float v[3], float out[3])
{
	out[0] = pMat->matrix[0][1]*v[0] + pMat->matrix[0][0]*v[1] + pMat->matrix[0][2]*v[2];
	out[1] = pMat->matrix[1][1]*v[0] + pMat->matrix[1][0]*v[1] + pMat->matrix[1][2]*v[2];
	out[2] = pMat->matrix[2][1]*v[0] + pMat->matrix[2][0]*v[1] + pMat->matrix[2][2]*v[2];
}

// Walk up the bolt-parent chain, accumulating each parent bolt's full 3x4
// transform into `inout`. After returning, a point that started in
// pContainer's local space has been moved all the way up into the top-most
// (primary / unbolted) container's local space. For the primary itself, this
// is a no-op.
//
// `bAsPoint` distinguishes point transforms (apply rotation + translation)
// from vector transforms like velocity (apply rotation only).
static void efx_walkParentChainToPrimary(const ModelContainer_t *pContainer, float inout[3], bool bAsPoint)
{
	const ModelContainer_t *p = pContainer;
	while (p) {
		const mdxaBone_t *pBoltMat = NULL;
		const ModelContainer_t *pParent = NULL;

		if (p->pSurfaceBolt_ParentContainer) {
			int iBolt = p->iSurfaceBolt_ParentBoltIndex;
			if (iBolt >= 0 && iBolt < MAX_G2_SURFACES &&
				p->pSurfaceBolt_ParentContainer->XFormedG2TagSurfsValid[iBolt]) {
				pBoltMat = &p->pSurfaceBolt_ParentContainer->XFormedG2TagSurfs[iBolt];
			}
			pParent = p->pSurfaceBolt_ParentContainer;
		}
		else if (p->pBoneBolt_ParentContainer) {
			int iBolt = p->iBoneBolt_ParentBoltIndex;
			if (iBolt >= 0 && iBolt < MAX_POSSIBLE_BONES &&
				p->pBoneBolt_ParentContainer->XFormedG2BonesValid[iBolt]) {
				pBoltMat = &p->pBoneBolt_ParentContainer->XFormedG2Bones[iBolt];
			}
			pParent = p->pBoneBolt_ParentContainer;
		}
		else {
			break;	// reached the primary container
		}

		if (pBoltMat) {
			float prev[3] = { inout[0], inout[1], inout[2] };
			inout[0] = pBoltMat->matrix[0][0]*prev[0] + pBoltMat->matrix[0][1]*prev[1] + pBoltMat->matrix[0][2]*prev[2];
			inout[1] = pBoltMat->matrix[1][0]*prev[0] + pBoltMat->matrix[1][1]*prev[1] + pBoltMat->matrix[1][2]*prev[2];
			inout[2] = pBoltMat->matrix[2][0]*prev[0] + pBoltMat->matrix[2][1]*prev[1] + pBoltMat->matrix[2][2]*prev[2];
			if (bAsPoint) {
				inout[0] += pBoltMat->matrix[0][3];
				inout[1] += pBoltMat->matrix[1][3];
				inout[2] += pBoltMat->matrix[2][3];
			}
		}

		p = pParent;
	}
}

// Spawn a burst. pBoltMat is either a real bolt matrix on pContainer (bRelative
// path) or a fake identity with translation = world hit point (physics path,
// bRelative=false and pContainer=primary).
// Q3's MakeNormalVectors: given a forward direction, build two arbitrary
// perpendicular vectors (right, up) via the classic "shift components and
// Gram-Schmidt" trick. Matches the game's q_math.c exactly so the axis layout
// after FX_PlayEffectID -> PlayEffect(origin, fwd) is identical.
static void efx_makeNormalVectors(const float fwd[3], float right[3], float up[3])
{
	right[1] = -fwd[0];
	right[2] =  fwd[1];
	right[0] =  fwd[2];
	float d = right[0]*fwd[0] + right[1]*fwd[1] + right[2]*fwd[2];
	right[0] -= d * fwd[0];
	right[1] -= d * fwd[1];
	right[2] -= d * fwd[2];
	float rl = sqrtf(right[0]*right[0] + right[1]*right[1] + right[2]*right[2]);
	if (rl > 0.0001f) { right[0]/=rl; right[1]/=rl; right[2]/=rl; }
	// up = right x fwd
	up[0] = right[1]*fwd[2] - right[2]*fwd[1];
	up[1] = right[2]*fwd[0] - right[0]*fwd[2];
	up[2] = right[0]*fwd[1] - right[1]*fwd[0];
}

static void efx_spawnBurst(ModelContainer_t *pContainer, const char *psEffectPath,
						   const mdxaBone_t *pBoltMat, bool bRelative)
{
	EfxEffectDef *fx = efx_getEffect(psEffectPath);
	if (!fx || fx->Primitives.empty()) return;

	DWORD now = GetTickCount();

	// Bolt origin in the owning container's local space.
	float vBoltInContainerLocal[3] = { pBoltMat->matrix[0][3], pBoltMat->matrix[1][3], pBoltMat->matrix[2][3] };

	// Pick the destination list once per spawn.
	vector<EfxLiveParticle> *pList = bRelative
		? &g_RelativeByContainer[pContainer]
		: &g_WorldParticles;

	// "Fix sparks offset on animevents bug" - the stock saber_clash.efx
	// ships with `origin 0 -20 0` which pushes the sparks 20 units down
	// the blade-local Z axis, landing them well off the blade with the
	// animevent-driven axis. When the user opts into the fix we force
	// the per-particle origin offset to zero for saber_clash.efx only,
	// so the effect spawns at the bolt point itself. Applied only at the
	// top-level burst - chained children (e.g. volumetric/smoke.efx via
	// FxRunner) keep their authored offsets.
	bool bForceZeroOrigin = false;
	if (AppVars.bFixSaberClashOffset && psEffectPath) {
		// Match "saber_clash" anywhere in the path so "saber_clash",
		// "saber/saber_clash", or a ".efx"-suffixed variant all hit.
		const char *slash = strrchr(psEffectPath, '/');
		const char *name = slash ? slash + 1 : psEffectPath;
		if (_strnicmp(name, "saber_clash", 11) == 0) bForceZeroOrigin = true;
	}

	bool bLoggedFirst = false;

	for (size_t p = 0; p < fx->Primitives.size(); p++) {
		const EfxPrimitiveDef &pd = fx->Primitives[p];

		// FxRunner: recursively play each chained effect with the same bolt
		// matrix + relative flag. This is how saber_clash.efx chains in a big
		// volumetric/smoke puff on top of its own smaller puff + sparks.
		if (pd.eType == EFXP_FXRUNNER) {
			for (size_t c = 0; c < pd.sChainedEffects.size(); c++) {
				efx_spawnBurst(pContainer, pd.sChainedEffects[c].c_str(), pBoltMat, bRelative);
			}
			continue;
		}

		int count = efx_randInt(pd.iCountMin, pd.iCountMax);
		if (count < 1) count = 1;

		for (int i = 0; i < count; i++) {
			EfxLiveParticle lp = {0};
			lp.eType       = pd.eType;
			lp.dwStartMs   = now;
			lp.iLifeMs     = efx_randInt(pd.iLifeMin, pd.iLifeMax);
			if (lp.iLifeMs < 1) lp.iLifeMs = 1;

			// Origin and velocity are rotated through the caller-provided
			// axis matrix. For anim-event dispatches the axis comes from
			// MakeNormalVectors of the game's hardcoded forward=(0,1,0),
			// not from the bolt's own orientation - see the comment in
			// efx_resolveAndSpawn. For physics collisions the axis is just
			// identity, so origin (which is 0,0,0 for saber_block/saber_cut
			// anyway) doesn't move.
			float vOriginLocal[3], vVelLocal[3];
			for (int k = 0; k < 3; k++) vOriginLocal[k] = efx_randFloat(pd.vOriginMin[k], pd.vOriginMax[k]);
			for (int k = 0; k < 3; k++) vVelLocal[k]    = efx_randFloat(pd.vVelMin[k],    pd.vVelMax[k]);
			if (bForceZeroOrigin) { vOriginLocal[0] = vOriginLocal[1] = vOriginLocal[2] = 0.0f; }

			// Sphere-spawn: pick a random point on a sphere of fSphereRadius
			// around the bolt origin (Tail primitives with spawnflags
			// orgOnSphere). axisFromSphere aligns initial velocity along the
			// outward radial direction, which is how saber_block gets its
			// radiating spark pattern.
			float vSphereDir[3] = {0, 0, 0};
			if (pd.bOrgOnSphere && pd.fSphereRadius > 0.0f) {
				float theta = efx_randFloat(0.0f, 2.0f * (float)M_PI);
				float cosPhi = efx_randFloat(-1.0f, 1.0f);
				float sinPhi = sqrtf(1.0f - cosPhi*cosPhi);
				vSphereDir[0] = sinPhi * cosf(theta);
				vSphereDir[1] = sinPhi * sinf(theta);
				vSphereDir[2] = cosPhi;
				for (int k = 0; k < 3; k++) vOriginLocal[k] += vSphereDir[k] * pd.fSphereRadius;

				if (pd.bAxisFromSphere) {
					// Re-orient velocity along outward radial. Preserve the X
					// magnitude as the "speed" (JKA convention - velocity X is
					// the along-axis component).
					float speed = fabsf(vVelLocal[0]);
					vVelLocal[0] = vSphereDir[0] * speed;
					vVelLocal[1] = vSphereDir[1] * speed;
					vVelLocal[2] = vSphereDir[2] * speed;
				}
			}

			// Rotate both origin and velocity through the caller's axis.
			float vOriginRot[3], vVelRot[3];
			efx_rotateByBolt(pBoltMat, vOriginLocal, vOriginRot);
			efx_rotateByBolt(pBoltMat, vVelLocal,    vVelRot);

			float vPos[3], vVel[3];
			for (int k = 0; k < 3; k++) vPos[k] = vBoltInContainerLocal[k] + vOriginRot[k];
			for (int k = 0; k < 3; k++) vVel[k] = vVelRot[k];

			if (!bLoggedFirst) {
				bLoggedFirst = true;
				const char *sType = (pd.eType == EFXP_TAIL) ? "Tail" : "Particle";
				const char *sMode = bRelative ? "relative" : "world";
				efx_log("    %s (%s): efxOrig=%.1f,%.1f,%.1f rotated=%.1f,%.1f,%.1f pos=%.1f,%.1f,%.1f\n",
						sType, sMode,
						vOriginLocal[0], vOriginLocal[1], vOriginLocal[2],
						vOriginRot[0], vOriginRot[1], vOriginRot[2],
						vPos[0], vPos[1], vPos[2]);
			}

			// For world mode, walk the bolt chain up so particles live in
			// primary-local space where Efx_RenderAll draws them. For relative
			// mode we stop here - particles stay in container-local and will
			// be rendered inside the container's modelview.
			if (!bRelative) {
				efx_walkParentChainToPrimary(pContainer, vPos, /*bAsPoint=*/true);
				efx_walkParentChainToPrimary(pContainer, vVel, /*bAsPoint=*/false);
			}

			for (int k = 0; k < 3; k++) lp.vPos[k] = vPos[k];
			for (int k = 0; k < 3; k++) lp.vVel[k] = vVel[k];

			lp.fGravity    = pd.fGravity;
			lp.fSizeStart  = efx_randFloat(pd.fSizeStartMin, pd.fSizeStartMax);
			lp.fSizeEnd    = efx_randFloat(pd.fSizeEndMin,   pd.fSizeEndMax);
			lp.fLengthStart= efx_randFloat(pd.fLengthStartMin, pd.fLengthStartMax);
			lp.fLengthEnd  = efx_randFloat(pd.fLengthEndMin,   pd.fLengthEndMax);
			lp.fAlphaStart = pd.fAlphaStart;
			lp.fAlphaEnd   = pd.fAlphaEnd;
			for (int k = 0; k < 3; k++) {
				lp.vRgbStart[k] = pd.vRgbStart[k];
				lp.vRgbEnd[k]   = pd.vRgbEnd[k];
			}
			lp.uiTexBind = pd.uiTexBind;

			pList->push_back(lp);
			g_iEfxParticlesTotal++;
		}
	}
}

// =============================================================================
// Public: Efx_DispatchFrameEvents
// =============================================================================

// Try to resolve bolt `sBoltName` on `pContainer` (or any of its bolted
// descendants) and spawn the effect into the container where the bolt lives.
// Returns true on success.
//
// Animevents are attached to the character's animation, but some bolts
// (e.g. *blade1 for saber FX) only exist on child models bolted to the
// character. We walk the bolt tree so the event author doesn't have to
// know which container the bolt ended up on.
static bool efx_resolveAndSpawn(ModelContainer_t *pContainer, const AnimEvent_t &ev)
{
	if (!pContainer || !pContainer->hModel) return false;

	int iBolt = Model_GetBoltIndex(pContainer->hModel, ev.sBoltName, false, true);
	bool bIsBone = false;
	if (iBolt == -1) {
		iBolt = Model_GetBoltIndex(pContainer->hModel, ev.sBoltName, true, true);
		bIsBone = true;
	}

	if (iBolt != -1) {
		mdxaBone_t *pMat = NULL;
		if (bIsBone) {
			if (iBolt >= 0 && iBolt < MAX_POSSIBLE_BONES && pContainer->XFormedG2BonesValid[iBolt])
				pMat = &pContainer->XFormedG2Bones[iBolt];
		} else {
			if (iBolt >= 0 && iBolt < MAX_G2_SURFACES && pContainer->XFormedG2TagSurfsValid[iBolt])
				pMat = &pContainer->XFormedG2TagSurfs[iBolt];
		}
		if (pMat) {
			// Match the game's AEV_EFFECT path exactly (cg_players.c:2826 ->
			// FX_PlayEffectID with a hardcoded forward=(0,1,0), NOT
			// FX_PlayBoltedEffectID). The effect spawns at the bolt's WORLD
			// position with a world-axis frame built from MakeNormalVectors
			// of (0,1,0), and particles live independently after spawn (no
			// FX_RELATIVE tracking). This is why saber_clash's `origin 0 -20 0`
			// resolves to "20 units below the bolt in world Z" rather than
			// "20 units along blade col0".
			//
			// Build a fake 3x4 matrix: rotation = (forward, right, up) from
			// the (0,1,0) convention, translation = bolt's primary-local pos
			// after walking the parent chain. efx_spawnBurst with this matrix
			// and bRelative=false gives us the correct axis mapping.
			float vBoltPrimaryLocal[3] = { pMat->matrix[0][3], pMat->matrix[1][3], pMat->matrix[2][3] };
			efx_walkParentChainToPrimary(pContainer, vBoltPrimaryLocal, /*bAsPoint=*/true);

			float fwd[3] = { 0, 1, 0 };		// game's hardcoded bAngle in cg_players.c
			float right[3], up[3];
			efx_makeNormalVectors(fwd, right, up);

			// Pack into an mdxaBone_t so efx_spawnBurst + efx_rotateByBolt
			// produce: out = X*col1 + Y*col0 + Z*col2 which matches the
			// game's origin calc (X*ax[0] + Y*ax[1] + Z*ax[2]) once we map
			//   col1 <- ax[0] = fwd
			//   col0 <- ax[1] = right
			//   col2 <- ax[2] = up
			mdxaBone_t fakeMat;
			memset(&fakeMat, 0, sizeof(fakeMat));
			fakeMat.matrix[0][1] = fwd[0];   fakeMat.matrix[1][1] = fwd[1];   fakeMat.matrix[2][1] = fwd[2];   // col1 = fwd
			fakeMat.matrix[0][0] = right[0]; fakeMat.matrix[1][0] = right[1]; fakeMat.matrix[2][0] = right[2]; // col0 = right
			fakeMat.matrix[0][2] = up[0];    fakeMat.matrix[1][2] = up[1];    fakeMat.matrix[2][2] = up[2];    // col2 = up
			fakeMat.matrix[0][3] = vBoltPrimaryLocal[0];
			fakeMat.matrix[1][3] = vBoltPrimaryLocal[1];
			fakeMat.matrix[2][3] = vBoltPrimaryLocal[2];

			// Spawn on the primary so the walk-up chain is a no-op - the
			// matrix we just built is already in primary-local coords.
			ModelHandle_t hPrimary = Model_GetPrimaryHandle();
			ModelContainer_t *pPrimary = hPrimary ? ModelContainer_FindFromModelHandle(hPrimary) : pContainer;
			efx_spawnBurst(pPrimary ? pPrimary : pContainer,
						   ev.sEffectPath, &fakeMat, /*bRelative=*/false);

			g_iEfxEventsFired++;
			strncpy(g_sEfxLastEffect, ev.sEffectPath, sizeof(g_sEfxLastEffect) - 1);
			g_sEfxLastEffect[sizeof(g_sEfxLastEffect) - 1] = 0;
			strncpy(g_sEfxLastBolt, ev.sBoltName, sizeof(g_sEfxLastBolt) - 1);
			g_sEfxLastBolt[sizeof(g_sEfxLastBolt) - 1] = 0;

			efx_log("EFX fired: %s on %s (%s) boltLocal=%.1f,%.1f,%.1f world=%.1f,%.1f,%.1f\n",
					ev.sEffectPath, ev.sBoltName,
					pContainer->sLocalPathName[0] ? pContainer->sLocalPathName : "<primary>",
					pMat->matrix[0][3], pMat->matrix[1][3], pMat->matrix[2][3],
					vBoltPrimaryLocal[0], vBoltPrimaryLocal[1], vBoltPrimaryLocal[2]);
			return true;
		}
		// Bolt exists but no matrix yet (first frame / never-rendered child):
		// keep searching. A sibling with the same bolt name might be loaded
		// and already rendered at least once.
	}

	// Recurse into surface-bolted children (sabers, weapons)
	for (size_t b = 0; b < pContainer->tSurfaceBolt_BoltPoints.size(); b++) {
		BoltPoint_t &bp = pContainer->tSurfaceBolt_BoltPoints[b];
		for (size_t c = 0; c < bp.vBoltedContainers.size(); c++) {
			if (efx_resolveAndSpawn(&bp.vBoltedContainers[c], ev)) return true;
		}
	}
	// Recurse into bone-bolted children (e.g. attachments on bones)
	for (size_t b = 0; b < pContainer->tBoneBolt_BoltPoints.size(); b++) {
		BoltPoint_t &bp = pContainer->tBoneBolt_BoltPoints[b];
		for (size_t c = 0; c < bp.vBoltedContainers.size(); c++) {
			if (efx_resolveAndSpawn(&bp.vBoltedContainers[c], ev)) return true;
		}
	}
	return false;
}

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

		if (!efx_resolveAndSpawn(pContainer, ev)) {
			efx_log("EFX: frame %d event '%s' - bolt '%s' not found on model or bolted children\n",
					iCurrentFrame, ev.sEffectPath, ev.sBoltName);
		}
	}
}

// =============================================================================
// Public: Efx_TickAll - physics integration for the global particle list,
// once per scene render, before anything draws.
// =============================================================================

// Common per-particle tick (age + cull + integrate). Shared between world
// and relative lists.
static void efx_tickList(vector<EfxLiveParticle> &list, DWORD now, float dt)
{
	for (int i = (int)list.size() - 1; i >= 0; i--) {
		EfxLiveParticle &lp = list[i];
		DWORD age = now - lp.dwStartMs;
		if ((int)age >= lp.iLifeMs) {
			list.erase(list.begin() + i);
			continue;
		}
		if (dt > 0.0f) {
			for (int k = 0; k < 3; k++) lp.vPos[k] += lp.vVel[k] * dt;
			lp.vVel[2] += lp.fGravity * dt;		// JKA: accel[2] += gravity, so positive gravity = upward (matches FxScheduler.cpp:1371)
		}
	}
}

void Efx_TickAll(void)
{
	DWORD now = GetTickCount();
	float dt = 0.0f;
	if (g_dwLastUpdateMs != 0) {
		DWORD elapsed = now - g_dwLastUpdateMs;
		if (elapsed < 500) dt = elapsed * 0.001f;	// clamp pauses/debugger breaks
	}
	g_dwLastUpdateMs = now;

	int total = 0;
	efx_tickList(g_WorldParticles, now, dt);
	total += (int)g_WorldParticles.size();
	for (map<ModelContainer_t*, vector<EfxLiveParticle> >::iterator it = g_RelativeByContainer.begin();
		 it != g_RelativeByContainer.end(); ++it) {
		efx_tickList(it->second, now, dt);
		total += (int)it->second.size();
	}
	g_iEfxParticlesLive = total;
}

// Setup GL state for particle rendering. Caller is responsible for wrapping
// in glPushAttrib/glPopAttrib.
static void efx_setupGLState(void)
{
	glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
	glDisable(GL_CULL_FACE);
	glEnable(GL_BLEND);
	glBlendFunc(GL_SRC_ALPHA, GL_ONE);	// additive - matches most particle shaders
	glEnable(GL_DEPTH_TEST);
	glDepthMask(GL_FALSE);
	glEnable(GL_TEXTURE_2D);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);
}

// Extract camera right/up/forward from the current modelview matrix. All three
// are in the same coordinate frame the caller is currently rendering in.
static void efx_camAxesFromMV(float right[3], float up[3], float fwd[3])
{
	float mv[16];
	glGetFloatv(GL_MODELVIEW_MATRIX, mv);
	right[0] = mv[0]; right[1] = mv[4]; right[2] = mv[8];
	up[0]    = mv[1]; up[1]    = mv[5]; up[2]    = mv[9];
	fwd[0]   = mv[2]; fwd[1]   = mv[6]; fwd[2]   = mv[10];
	// Normalize because HandleAllEvilMatrixCode bakes in a 0.05 scale
	for (int axis = 0; axis < 3; axis++) {
		float *v = (axis == 0) ? right : (axis == 1) ? up : fwd;
		float L = sqrtf(v[0]*v[0] + v[1]*v[1] + v[2]*v[2]);
		if (L > 0.0001f) { v[0]/=L; v[1]/=L; v[2]/=L; }
	}
}

// Render a Particle primitive as a camera-facing billboard quad.
static void efx_drawParticle(const EfxLiveParticle &lp, float t,
							 const float right[3], const float up[3])
{
	float size  = efx_lerp(lp.fSizeStart,  lp.fSizeEnd,  t);
	float alpha = efx_lerp(lp.fAlphaStart, lp.fAlphaEnd, t);
	float r = efx_lerp(lp.vRgbStart[0], lp.vRgbEnd[0], t);
	float g = efx_lerp(lp.vRgbStart[1], lp.vRgbEnd[1], t);
	float b = efx_lerp(lp.vRgbStart[2], lp.vRgbEnd[2], t);

	glBindTexture(GL_TEXTURE_2D, lp.uiTexBind);
	glColor4f(r, g, b, alpha);

	float hw = size * 0.5f;
	float rX = right[0]*hw, rY = right[1]*hw, rZ = right[2]*hw;
	float uX = up[0]   *hw, uY = up[1]   *hw, uZ = up[2]   *hw;

	glBegin(GL_QUADS);
		glTexCoord2f(0, 0); glVertex3f(lp.vPos[0]-rX-uX, lp.vPos[1]-rY-uY, lp.vPos[2]-rZ-uZ);
		glTexCoord2f(1, 0); glVertex3f(lp.vPos[0]+rX-uX, lp.vPos[1]+rY-uY, lp.vPos[2]+rZ-uZ);
		glTexCoord2f(1, 1); glVertex3f(lp.vPos[0]+rX+uX, lp.vPos[1]+rY+uY, lp.vPos[2]+rZ+uZ);
		glTexCoord2f(0, 1); glVertex3f(lp.vPos[0]-rX+uX, lp.vPos[1]-rY+uY, lp.vPos[2]-rZ+uZ);
	glEnd();
}

// Render a Tail primitive as a thin ribbon pointing along velocity direction,
// width perpendicular to view. Head at current position, tail extends backward
// along -velocity for `length` units.
static void efx_drawTail(const EfxLiveParticle &lp, float t,
						 const float right[3], const float up[3], const float fwd[3])
{
	float width  = efx_lerp(lp.fSizeStart,   lp.fSizeEnd,   t);
	float length = efx_lerp(lp.fLengthStart, lp.fLengthEnd, t);
	float alpha  = efx_lerp(lp.fAlphaStart,  lp.fAlphaEnd,  t);
	float r = efx_lerp(lp.vRgbStart[0], lp.vRgbEnd[0], t);
	float g = efx_lerp(lp.vRgbStart[1], lp.vRgbEnd[1], t);
	float b = efx_lerp(lp.vRgbStart[2], lp.vRgbEnd[2], t);

	// Direction = normalized velocity. Fall back to camera-right if velocity
	// is zero so we still render a visible ribbon.
	float dir[3] = { lp.vVel[0], lp.vVel[1], lp.vVel[2] };
	float dlen = sqrtf(dir[0]*dir[0] + dir[1]*dir[1] + dir[2]*dir[2]);
	if (dlen < 0.0001f) {
		dir[0] = right[0]; dir[1] = right[1]; dir[2] = right[2];
	} else {
		dir[0]/=dlen; dir[1]/=dlen; dir[2]/=dlen;
	}

	// "right" for the ribbon = cross(dir, cam_forward). Perpendicular to both
	// the ribbon axis and the view ray, so the ribbon lies as a screen-facing
	// line. Same trick the saber blade core uses (see r_surface.cpp).
	float wdir[3] = {
		dir[1]*fwd[2] - dir[2]*fwd[1],
		dir[2]*fwd[0] - dir[0]*fwd[2],
		dir[0]*fwd[1] - dir[1]*fwd[0]
	};
	float wlen = sqrtf(wdir[0]*wdir[0] + wdir[1]*wdir[1] + wdir[2]*wdir[2]);
	if (wlen < 0.0001f) {
		wdir[0] = up[0]; wdir[1] = up[1]; wdir[2] = up[2];
	} else {
		wdir[0]/=wlen; wdir[1]/=wlen; wdir[2]/=wlen;
	}

	float hw = width * 0.5f;
	float head[3] = { lp.vPos[0], lp.vPos[1], lp.vPos[2] };
	float tail[3] = {
		head[0] - dir[0]*length,
		head[1] - dir[1]*length,
		head[2] - dir[2]*length
	};

	glBindTexture(GL_TEXTURE_2D, lp.uiTexBind);
	glColor4f(r, g, b, alpha);

	glBegin(GL_QUADS);
		// Head end (U=1)
		glTexCoord2f(1, 0); glVertex3f(head[0]-wdir[0]*hw, head[1]-wdir[1]*hw, head[2]-wdir[2]*hw);
		glTexCoord2f(1, 1); glVertex3f(head[0]+wdir[0]*hw, head[1]+wdir[1]*hw, head[2]+wdir[2]*hw);
		// Tail end (U=0) - matches gfx/misc/spark texture convention (bright at U=0)
		glTexCoord2f(0, 1); glVertex3f(tail[0]+wdir[0]*hw, tail[1]+wdir[1]*hw, tail[2]+wdir[2]*hw);
		glTexCoord2f(0, 0); glVertex3f(tail[0]-wdir[0]*hw, tail[1]-wdir[1]*hw, tail[2]-wdir[2]*hw);
	glEnd();
}

// Render a single list of particles using the current modelview. Caller has
// already set up GL state and camera axes.
static void efx_renderList(const vector<EfxLiveParticle> &list, DWORD now,
						   const float right[3], const float up[3], const float fwd[3])
{
	for (size_t i = 0; i < list.size(); i++) {
		const EfxLiveParticle &lp = list[i];
		DWORD age = now - lp.dwStartMs;
		float t = (float)age / (float)lp.iLifeMs;
		if (t < 0.0f) t = 0.0f; else if (t > 1.0f) t = 1.0f;

		if (lp.eType == EFXP_TAIL) efx_drawTail    (lp, t, right, up, fwd);
		else                       efx_drawParticle(lp, t, right, up);
	}
}

// =============================================================================
// Public: Efx_RenderForContainer - draw this container's relative particles
// inside the caller's current modelview (which should be the container's own
// modelview, set up by HandleAllEvilMatrixCode).
// =============================================================================

void Efx_RenderForContainer(ModelContainer_t *pContainer)
{
	if (!pContainer) return;
	map<ModelContainer_t*, vector<EfxLiveParticle> >::iterator it = g_RelativeByContainer.find(pContainer);
	if (it == g_RelativeByContainer.end() || it->second.empty()) return;

	float right[3], up[3], fwd[3];
	efx_camAxesFromMV(right, up, fwd);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	efx_setupGLState();

	extern unsigned int glStateBits;
	glStateBits = 0;

	efx_renderList(it->second, GetTickCount(), right, up, fwd);

	glPopAttrib();
	glStateBits = 0;
}

// =============================================================================
// Public: Efx_RenderAll - draw every world particle in primary-local space.
// Call from ModelList_Render_Actual after all containers have drawn and
// glPopMatrix'd back to the scene modelview.
// =============================================================================

void Efx_RenderAll(void)
{
	if (g_WorldParticles.empty()) return;

	float right[3], up[3], fwd[3];
	efx_camAxesFromMV(right, up, fwd);

	glPushAttrib(GL_ALL_ATTRIB_BITS);
	efx_setupGLState();

	extern unsigned int glStateBits;
	glStateBits = 0;

	efx_renderList(g_WorldParticles, GetTickCount(), right, up, fwd);

	glPopAttrib();
	glStateBits = 0;
}

// =============================================================================
// Public: Efx_Shutdown
// =============================================================================

// =============================================================================
// Physics-triggered effects: saber-vs-saber and saber-vs-floor spark bursts.
// Runs once per frame independently of animation events.
// =============================================================================

// Find an active blade's line segment in primary-local space. bladeNum is the
// 0-based blade index on the saber (*blade1, *blade2, ...). For the first blade
// we also accept *flash as a fallback to match the render path. Returns false
// when the hand has no blade ignited, the bolt isn't resolvable, or the bolt
// matrix hasn't been populated yet (first frame after load).
static bool efx_getBladeSegment(int hand, int bladeNum, float outStart[3], float outEnd[3])
{
	if (!AppVars.bSaberBlade[hand]) return false;

	ModelHandle_t hPrimary = Model_GetPrimaryHandle();
	if (!hPrimary) return false;

	LPCSTR psHandTag = (hand == 0) ? "*r_hand" : "*l_hand";
	int iHandBolt = Model_GetBoltIndex(hPrimary, psHandTag, false, true);
	if (iHandBolt == -1) return false;

	ModelContainer_t *pPrimary = ModelContainer_FindFromModelHandle(hPrimary);
	if (!pPrimary) return false;
	if (iHandBolt >= (int)pPrimary->tSurfaceBolt_BoltPoints.size()) return false;
	if (pPrimary->tSurfaceBolt_BoltPoints[iHandBolt].vBoltedContainers.empty()) return false;

	ModelContainer_t *pSaber = &pPrimary->tSurfaceBolt_BoltPoints[iHandBolt].vBoltedContainers[0];

	char sBoltName[32];
	sprintf(sBoltName, "*blade%d", bladeNum + 1);
	int iBladeBolt = Model_GetBoltIndex(pSaber->hModel, sBoltName, false, true);
	if (iBladeBolt == -1 && bladeNum == 0) {
		iBladeBolt = Model_GetBoltIndex(pSaber->hModel, "*flash", false, true);
	}
	if (iBladeBolt == -1) return false;
	if (!pSaber->XFormedG2TagSurfsValid[iBladeBolt]) return false;

	mdxaBone_t &bladeMat = pSaber->XFormedG2TagSurfs[iBladeBolt];

	// Origin in saber-local; blade direction is -col0 (saber convention).
	float startSL[3] = { bladeMat.matrix[0][3], bladeMat.matrix[1][3], bladeMat.matrix[2][3] };
	float dirSL[3]   = { -bladeMat.matrix[0][0], -bladeMat.matrix[1][0], -bladeMat.matrix[2][0] };
	float endSL[3]   = {
		startSL[0] + dirSL[0] * AppVars.saberLength,
		startSL[1] + dirSL[1] * AppVars.saberLength,
		startSL[2] + dirSL[2] * AppVars.saberLength
	};

	// Walk the saber's parent chain up to primary-local (one step for a
	// hand-bolted saber: through the hand tag).
	efx_walkParentChainToPrimary(pSaber, startSL, /*bAsPoint=*/true);
	efx_walkParentChainToPrimary(pSaber, endSL,   /*bAsPoint=*/true);

	for (int k = 0; k < 3; k++) { outStart[k] = startSL[k]; outEnd[k] = endSL[k]; }
	return true;
}

#define EFX_MAX_BLADES_PER_HAND 8

// Closest point-to-point distance squared between two line segments. No
// longer used by Efx_CheckSaberCollisions - anim events already play
// choreographed saber_clash for scripted blade-on-blade contact - but kept
// around in case we ever want to re-enable proximity-based clash sparks.
#if 0
static float efx_closestPtsBetweenSegs(const float p1[3], const float p2[3],
									   const float p3[3], const float p4[3],
									   float outA[3], float outB[3])
{
	float d1[3] = { p2[0]-p1[0], p2[1]-p1[1], p2[2]-p1[2] };	// seg A dir
	float d2[3] = { p4[0]-p3[0], p4[1]-p3[1], p4[2]-p3[2] };	// seg B dir
	float r[3]  = { p1[0]-p3[0], p1[1]-p3[1], p1[2]-p3[2] };
	float a = d1[0]*d1[0]+d1[1]*d1[1]+d1[2]*d1[2];
	float e = d2[0]*d2[0]+d2[1]*d2[1]+d2[2]*d2[2];
	float f = d2[0]*r[0] +d2[1]*r[1] +d2[2]*r[2];
	float s, t;

	if (a <= 1e-6f && e <= 1e-6f) { s = t = 0.0f; }
	else if (a <= 1e-6f) { s = 0.0f; t = f / e; if (t<0)t=0; else if (t>1)t=1; }
	else {
		float c = d1[0]*r[0]+d1[1]*r[1]+d1[2]*r[2];
		if (e <= 1e-6f) { t = 0.0f; s = -c/a; if (s<0)s=0; else if (s>1)s=1; }
		else {
			float b = d1[0]*d2[0]+d1[1]*d2[1]+d1[2]*d2[2];
			float denom = a*e - b*b;
			s = (denom != 0.0f) ? (b*f - c*e) / denom : 0.0f;
			if (s<0)s=0; else if (s>1)s=1;
			t = (b*s + f) / e;
			if (t<0) { t=0; s = -c/a; if (s<0)s=0; else if (s>1)s=1; }
			else if (t>1) { t=1; s = (b-c)/a; if (s<0)s=0; else if (s>1)s=1; }
		}
	}
	for (int k = 0; k < 3; k++) {
		outA[k] = p1[k] + d1[k]*s;
		outB[k] = p3[k] + d2[k]*t;
	}
	float dx = outA[0]-outB[0], dy = outA[1]-outB[1], dz = outA[2]-outB[2];
	return dx*dx + dy*dy + dz*dz;
}
#endif	// unused blade-vs-blade helper

// Spawn saber_cut.efx at a given primary-local position with unit-axis
// orientation. Uses the same spawn path as animevents but with a fake
// identity matrix at the hit point.
static void efx_spawnAtPrimaryLocal(const char *psEffectPath, const float vPos[3])
{
	mdxaBone_t fakeMat;
	memset(&fakeMat, 0, sizeof(fakeMat));
	fakeMat.matrix[0][0] = 1; fakeMat.matrix[1][1] = 1; fakeMat.matrix[2][2] = 1;
	fakeMat.matrix[0][3] = vPos[0];
	fakeMat.matrix[1][3] = vPos[1];
	fakeMat.matrix[2][3] = vPos[2];

	// World-mode spawn: particles drift freely in primary-local space
	// (not attached to any bolt), which is the physics-contact semantic.
	ModelHandle_t hPrimary = Model_GetPrimaryHandle();
	ModelContainer_t *pPrimary = hPrimary ? ModelContainer_FindFromModelHandle(hPrimary) : NULL;
	if (!pPrimary) return;
	efx_spawnBurst(pPrimary, psEffectPath, &fakeMat, /*bRelative=*/false);
}

// Cooldown so floor sparks don't spawn every frame while the blade is held
// against the floor. Matches the game's repeatDelay style.
#define EFX_CLASH_COOLDOWN_MS	150
static DWORD g_dwLastFloorClashMs[2][EFX_MAX_BLADES_PER_HAND] = { { 0 } };	// [hand][bladeNum]

// Physics-triggered effects are intentionally scoped to blade-vs-floor only.
// Scripted blade-on-blade clashes come from the animation system (saber_clash
// via animevents.cfg), which the animators choreograph - proximity-based
// blade-blade detection would fire in addition to those and end up duplicating
// or conflicting with the scripted hits.
//
// Iterates all blades per hand so staff/double-bladed sabers spark on either
// end when it hits the floor (game convention: bolt indices *blade1..*blade8).
void Efx_CheckSaberCollisions(void)
{
	if (!AppVars.bSaberCollisionFX) return;		// user-disabled in the weapon dialog
	if (!AppVars.bFloor) return;				// only check floor contacts

	DWORD now = GetTickCount();

	float bladeStart[2][EFX_MAX_BLADES_PER_HAND][3];
	float bladeEnd  [2][EFX_MAX_BLADES_PER_HAND][3];
	bool  haveBlade [2][EFX_MAX_BLADES_PER_HAND] = { { false } };
	int   nBlades   [2] = { 0, 0 };

	for (int hand = 0; hand < 2; hand++) {
		for (int b = 0; b < EFX_MAX_BLADES_PER_HAND; b++) {
			if (!efx_getBladeSegment(hand, b, bladeStart[hand][b], bladeEnd[hand][b])) {
				break;	// blade bolts are sequential; first miss = done
			}
			haveBlade[hand][b] = true;
			nBlades[hand] = b + 1;
		}
	}

	float floorZ = AppVars.fFloorZ;

	for (int hand = 0; hand < 2; hand++) {
		for (int b = 0; b < nBlades[hand]; b++) {
			if (!haveBlade[hand][b]) continue;

			float z1 = bladeStart[hand][b][2];
			float z2 = bladeEnd[hand][b][2];

			// Segment crosses the plane if endpoints are on opposite sides,
			// or both are at/below the plane (blade held against/through floor).
			bool crosses   = (z1 - floorZ) * (z2 - floorZ) < 0.0f;
			bool submerged = (z1 <= floorZ && z2 <= floorZ);
			if (!crosses && !submerged) continue;
			if (now - g_dwLastFloorClashMs[hand][b] < EFX_CLASH_COOLDOWN_MS) continue;

			g_dwLastFloorClashMs[hand][b] = now;
			float hit[3];
			if (crosses) {
				float t = (floorZ - z1) / (z2 - z1);
				hit[0] = bladeStart[hand][b][0] + (bladeEnd[hand][b][0] - bladeStart[hand][b][0]) * t;
				hit[1] = bladeStart[hand][b][1] + (bladeEnd[hand][b][1] - bladeStart[hand][b][1]) * t;
				hit[2] = floorZ;
			} else {
				hit[0] = bladeEnd[hand][b][0];
				hit[1] = bladeEnd[hand][b][1];
				hit[2] = floorZ;
			}
			efx_spawnAtPrimaryLocal("saber/saber_cut", hit);
		}
	}
}

void Efx_Shutdown(void)
{
	for (map<string, EfxEffectDef*>::iterator it = g_EffectCache.begin();
		 it != g_EffectCache.end(); ++it) {
		delete it->second;
	}
	g_EffectCache.clear();
	g_WorldParticles.clear();
	g_RelativeByContainer.clear();
	g_dwLastUpdateMs = 0;
	g_iEfxEventsFired    = 0;
	g_iEfxParticlesLive  = 0;
	g_iEfxParticlesTotal = 0;
	g_sEfxLastEffect[0]  = 0;
	g_sEfxLastBolt[0]    = 0;

	// Truncate the log so the next session starts with a clean file. Same
	// gating as efx_log - don't create the file if debug logging is off.
	if (!g_bLogDebug) return;
	FILE *f = fopen(efx_logPath(), "w");
	if (f) fclose(f);
}

///////////////// eof /////////////
