// Filename:-	shader.cpp
//
// Q3/JKA .shader file parsing and shader registry for ModView


#include "stdafx.h"
#include "includes.h"
#include "R_Common.h"
#include "R_Image.h"
#include "files.h"
#include "textures.h"
//
#include "shader.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

shaderCommands_t tess;

// stuff from lower down, may externalise.
//
char *COM_ParseExt( char **data_p, qboolean allowLineBreaks );


// =============================================================================
// Waveform lookup tables
// =============================================================================

float	sv_sinTable[FUNCTABLE_SIZE];
float	sv_triangleTable[FUNCTABLE_SIZE];
float	sv_squareTable[FUNCTABLE_SIZE];
float	sv_sawToothTable[FUNCTABLE_SIZE];
float	sv_inverseSawToothTable[FUNCTABLE_SIZE];

void Shader_Init( void )
{	for ( int i = 0; i < FUNCTABLE_SIZE; i++ ) {
		float f = (float)i / (float)FUNCTABLE_SIZE;

		sv_sinTable[i] = sin( f * 2.0 * M_PI );

		if ( f < 0.25 )
			sv_triangleTable[i] = 4.0f * f;
		else if ( f < 0.75 )
			sv_triangleTable[i] = 2.0f - 4.0f * f;
		else
			sv_triangleTable[i] = (f - 0.75f) * 4.0f - 1.0f;

		sv_squareTable[i] = (f < 0.5f) ? 1.0f : -1.0f;
		sv_sawToothTable[i] = f;
		sv_inverseSawToothTable[i] = 1.0f - f;
	}
}


// =============================================================================
// Shader registry
// =============================================================================

#define SHADER_FILE_HASH_SIZE	1024
static shader_t	*shaderHashTable[SHADER_FILE_HASH_SIZE];
#define MAX_SHADER_TABLE 1024
static shader_t		shaderTable[MAX_SHADER_TABLE];
static int			numShaders = 0;

shader_t *R_GetShaderByIndex( int index ) {
	if ( index < 0 || index >= numShaders ) return &shaderTable[0]; // default
	return &shaderTable[index];
}

int R_GetNumShaders( void ) {
	return numShaders;
}

void R_ShutdownShaders( void )
{
	memset( shaderHashTable, 0, sizeof(shaderHashTable) );
	memset( shaderTable, 0, sizeof(shaderTable) );
	numShaders = 0;
}


// =============================================================================
// Token parsing (kept from original ModView)
// =============================================================================

static char *s_shaderText = NULL;

static	char	com_token[MAX_TOKEN_CHARS];
static	char	com_parsename[MAX_TOKEN_CHARS];
static	int		com_lines;

void COM_BeginParseSession( const char *name )
{
	com_lines = 0;
	Com_sprintf(com_parsename, sizeof(com_parsename), "%s", name);
}

int COM_GetCurrentParseLine( void )
{
	return com_lines;
}

char *COM_Parse( char **data_p )
{
	return COM_ParseExt( data_p, qtrue );
}

void COM_ParseError( char *format, ... )
{
	va_list argptr;
	static char string[4096];
	va_start (argptr, format);
	vsprintf (string, format, argptr);
	va_end (argptr);
	Com_Printf("ERROR: %s, line %d: %s\n", com_parsename, com_lines, string);
}

void COM_ParseWarning( char *format, ... )
{
	va_list argptr;
	static char string[4096];
	va_start (argptr, format);
	vsprintf (string, format, argptr);
	va_end (argptr);
	Com_Printf("WARNING: %s, line %d: %s\n", com_parsename, com_lines, string);
}

static char *SkipWhitespace( char *data, qboolean *hasNewLines ) {
	int c;
	while( (c = *data) <= ' ') {
		if( !c ) return NULL;
		if( c == '\n' ) {
			com_lines++;
			*hasNewLines = qtrue;
		}
		data++;
	}
	return data;
}

void SkipBracedSection (char **program) {
	char	*token;
	int		depth = 0;
	do {
		token = COM_ParseExt( program, qtrue );
		if( token[1] == 0 ) {
			if( token[0] == '{' ) depth++;
			else if( token[0] == '}' ) depth--;
		}
	} while( depth && *program );
}

void SkipRestOfLine ( char **data ) {
	char *p = *data;
	int c;
	while ( (c = *p++) != 0 ) {
		if ( c == '\n' ) { com_lines++; break; }
	}
	*data = p;
}

char *COM_ParseExt( char **data_p, qboolean allowLineBreaks )
{
	int c = 0, len;
	qboolean hasNewLines = qfalse;
	char *data;

	data = *data_p;
	len = 0;
	com_token[0] = 0;

	if ( !data ) { *data_p = NULL; return com_token; }

	while ( 1 ) {
		data = SkipWhitespace( data, &hasNewLines );
		if ( !data ) { *data_p = NULL; return com_token; }
		if ( hasNewLines && !allowLineBreaks ) { *data_p = data; return com_token; }

		c = *data;

		if ( c == '/' && data[1] == '/' ) {
			data += 2;
			while (*data && *data != '\n') data++;
		} else if ( c=='/' && data[1] == '*' ) {
			data += 2;
			while ( *data && ( *data != '*' || data[1] != '/' ) ) data++;
			if ( *data ) data += 2;
		} else {
			break;
		}
	}

	// handle quoted strings
	if (c == '\"') {
		data++;
		while (1) {
			c = *data++;
			if (c=='\"' || !c) { com_token[len] = 0; *data_p = data; return com_token; }
			if (len < MAX_TOKEN_CHARS) com_token[len++] = c;
		}
	}

	// parse a regular word
	do {
		if (len < MAX_TOKEN_CHARS) com_token[len++] = c;
		data++;
		c = *data;
		if ( c == '\n' ) com_lines++;
	} while (c>32);

	if (len == MAX_TOKEN_CHARS) len = 0;
	com_token[len] = 0;
	*data_p = data;
	return com_token;
}


// =============================================================================
// FindShaderInShaderText (from original ModView)
// =============================================================================

static char *FindShaderInShaderText( const char *shadername ) {
	char *p = s_shaderText;
	char *token;

	if ( !p ) return NULL;

	while ( 1 ) {
		token = COM_ParseExt( &p, qtrue );
		if ( token[0] == 0 ) break;
		if ( !Q__stricmp( token, shadername ) ) return p;
		else SkipBracedSection( &p );
	}
	return NULL;
}


// =============================================================================
// ScanAndLoadShaderFiles (from original ModView, unchanged)
// =============================================================================

#define	MAX_SHADER_FILES	1024

typedef map<string,string>	ShadersFoundAndFilesPicked_t;
							ShadersFoundAndFilesPicked_t ShadersFoundAndFilesPicked;

// Missing-texture tracking. Shader stages that reference textures not on
// disk used to fail silently, leaving models looking like the wrong skin.
// We now collect the misses (deduped) and pop a single WarningBox at the
// end of the render frame - matches the existing "missing shader" report
// pattern in oldskins.cpp.
static set<string>	g_reportedMissingTextures;
static bool			g_bMissingTextureListDirty = false;

void ReportMissingShaderTexture(const char *name)
{
	if (!name || !name[0]) return;

	string key = name;
	if (g_reportedMissingTextures.count(key)) return;
	g_reportedMissingTextures.insert(key);
	g_bMissingTextureListDirty = true;

	char buf[1024];
	_snprintf(buf, sizeof(buf), "ModView shader: missing texture '%s'\n", name);
	buf[sizeof(buf) - 1] = 0;
	OutputDebugString(buf);
}

// Called from ModelList_Render after gbInRenderer goes false, so it's safe
// to pop a modal dialog (never inside a paint/render callback).
void ShowMissingShaderTextureWarningIfAny(void)
{
	if (!g_bMissingTextureListDirty) return;
	g_bMissingTextureListDirty = false;

	string sList;
	for (set<string>::iterator it = g_reportedMissingTextures.begin();
		 it != g_reportedMissingTextures.end(); ++it) {
		sList += *it;
		sList += "\n";
	}
	int lineCount = (int)g_reportedMissingTextures.size();

	#define MAX_BOX_LINES_HERE 50
	if (lineCount > MAX_BOX_LINES_HERE) {
		if (GetYesNo(va("Some shader textures couldn't be loaded (%d missing). "
						"List has > %d entries, send to Notepad?",
						lineCount, MAX_BOX_LINES_HERE))) {
			SendStringToNotepad(va("Missing shader textures:\n\n%s", sList.c_str()),
								"missing_textures.txt");
		}
	} else {
		WarningBox(va("The following shader textures could not be loaded:\n\n%s",
					  sList.c_str()));
	}
}

void KillAllShaderFiles(void)
{
	SAFEFREE(s_shaderText);
	ShadersFoundAndFilesPicked.clear();
	R_ShutdownShaders();

	// Reset miss tracking. KillAllShaderFiles fires on gamedir change and on
	// Ctrl+R refresh, so after a refresh the next render will re-report any
	// still-missing textures from scratch.
	g_reportedMissingTextures.clear();
	g_bMissingTextureListDirty = false;
}


void ScanAndLoadShaderFiles( void )
{
	if (s_shaderText == NULL && strlen(gamedir))
	{
		char **shaderFiles;
		char *buffers[MAX_SHADER_FILES];
		int numShaderFiles;
		int i;
		long sum = 0;

		#define sSHADER_DIR va("%sshaders",gamedir)

		shaderFiles = Sys_ListFiles( sSHADER_DIR, ".shader", NULL, &numShaderFiles, qfalse );

		if ( !shaderFiles || !numShaderFiles )
		{
			if (!bXMenPathHack)
			{
				ri.Printf( PRINT_WARNING, "WARNING: no shader files found in '%s'\n",sSHADER_DIR );
			}
			s_shaderText = CopyString("// blank shader file to avoid re-scanning shaders\n");
			return;
		}

		if ( numShaderFiles > MAX_SHADER_FILES ) numShaderFiles = MAX_SHADER_FILES;

		for ( i = 0; i < numShaderFiles; i++ )
		{
			char filename[MAX_QPATH];
			Com_sprintf( filename, sizeof( filename ), "shaders/%s", shaderFiles[i] );
			StatusMessage( va("Loading shader %d/%d: \"%s\"...",i+1,numShaderFiles,filename));
			sum += ri.FS_ReadFile( filename, (void **)&buffers[i] );
			if ( !buffers[i] ) ri.Error( ERR_DROP, "Couldn't load %s", filename );
		}
		StatusMessage(NULL);

		s_shaderText = (char *)ri.Hunk_Alloc( sum + numShaderFiles*2 );

		for ( i = numShaderFiles - 1; i >= 0 ; i-- ) {
			strcat( s_shaderText, "\n" );
			strcat( s_shaderText, buffers[i] );
			ri.FS_FreeFile( buffers[i] );
		}
		Sys_FreeFileList( shaderFiles );
	}
}


// =============================================================================
// Name-to-enum helpers (ported from JoF_EJK tr_shader.cpp)
// =============================================================================

static int NameToSrcBlendMode( const char *name )
{
	if ( !Q__stricmp( name, "GL_ONE" ) )					return GLS_SRCBLEND_ONE;
	if ( !Q__stricmp( name, "GL_ZERO" ) )					return GLS_SRCBLEND_ZERO;
	if ( !Q__stricmp( name, "GL_DST_COLOR" ) )				return GLS_SRCBLEND_DST_COLOR;
	if ( !Q__stricmp( name, "GL_ONE_MINUS_DST_COLOR" ) )	return GLS_SRCBLEND_ONE_MINUS_DST_COLOR;
	if ( !Q__stricmp( name, "GL_SRC_ALPHA" ) )				return GLS_SRCBLEND_SRC_ALPHA;
	if ( !Q__stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )	return GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA;
	if ( !Q__stricmp( name, "GL_DST_ALPHA" ) )				return GLS_SRCBLEND_DST_ALPHA;
	if ( !Q__stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )	return GLS_SRCBLEND_ONE_MINUS_DST_ALPHA;
	if ( !Q__stricmp( name, "GL_SRC_ALPHA_SATURATE" ) )	return GLS_SRCBLEND_ALPHA_SATURATE;
	return GLS_SRCBLEND_ONE;
}

static int NameToDstBlendMode( const char *name )
{
	if ( !Q__stricmp( name, "GL_ONE" ) )					return GLS_DSTBLEND_ONE;
	if ( !Q__stricmp( name, "GL_ZERO" ) )					return GLS_DSTBLEND_ZERO;
	if ( !Q__stricmp( name, "GL_SRC_ALPHA" ) )				return GLS_DSTBLEND_SRC_ALPHA;
	if ( !Q__stricmp( name, "GL_ONE_MINUS_SRC_ALPHA" ) )	return GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
	if ( !Q__stricmp( name, "GL_DST_ALPHA" ) )				return GLS_DSTBLEND_DST_ALPHA;
	if ( !Q__stricmp( name, "GL_ONE_MINUS_DST_ALPHA" ) )	return GLS_DSTBLEND_ONE_MINUS_DST_ALPHA;
	if ( !Q__stricmp( name, "GL_SRC_COLOR" ) )				return GLS_DSTBLEND_SRC_COLOR;
	if ( !Q__stricmp( name, "GL_ONE_MINUS_SRC_COLOR" ) )	return GLS_DSTBLEND_ONE_MINUS_SRC_COLOR;
	return GLS_DSTBLEND_ONE;
}

static genFunc_t NameToGenFunc( const char *funcname )
{
	if ( !Q__stricmp( funcname, "sin" ) )				return GF_SIN;
	if ( !Q__stricmp( funcname, "square" ) )			return GF_SQUARE;
	if ( !Q__stricmp( funcname, "triangle" ) )			return GF_TRIANGLE;
	if ( !Q__stricmp( funcname, "sawtooth" ) )			return GF_SAWTOOTH;
	if ( !Q__stricmp( funcname, "inversesawtooth" ) )	return GF_INVERSE_SAWTOOTH;
	if ( !Q__stricmp( funcname, "noise" ) )				return GF_NOISE;
	if ( !Q__stricmp( funcname, "random" ) )			return GF_RAND;
	return GF_SIN;
}


// =============================================================================
// Shader stage parsing (ported from JoF_EJK tr_shader.cpp)
// =============================================================================

// current shader being parsed (temporary)
static shader_t		shader_parse;
static shaderStage_t stages_parse[MAX_SHADER_STAGES];

static void ParseWaveForm( char **text, waveForm_t *wave )
{
	char *token;

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) return;
	wave->func = NameToGenFunc( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) return;
	wave->base = (float)atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) return;
	wave->amplitude = (float)atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) return;
	wave->phase = (float)atof( token );

	token = COM_ParseExt( text, qfalse );
	if ( token[0] == 0 ) return;
	wave->frequency = (float)atof( token );
}


static void ParseTexMod( char *text, shaderStage_t *stage )
{
	char *token;
	char **ptext = &text;
	textureBundle_t *bundle = &stage->bundle[0];

	if ( bundle->numTexMods >= TR_MAX_TEXMODS ) return;
	texModInfo_t *tmi = &bundle->texMods[bundle->numTexMods];
	bundle->numTexMods++;

	token = COM_ParseExt( ptext, qfalse );

	if ( !Q__stricmp( token, "turb" ) ) {
		tmi->type = TMOD_TURBULENT;
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->wave.base = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->wave.amplitude = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->wave.phase = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->wave.frequency = (float)atof( token );
	}
	else if ( !Q__stricmp( token, "scale" ) ) {
		tmi->type = TMOD_SCALE;
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->translate[0] = (float)atof( token );	// scale stored in translate
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->translate[1] = (float)atof( token );
	}
	else if ( !Q__stricmp( token, "scroll" ) ) {
		tmi->type = TMOD_SCROLL;
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->translate[0] = (float)atof( token );	// scroll speed stored in translate
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->translate[1] = (float)atof( token );
	}
	else if ( !Q__stricmp( token, "stretch" ) ) {
		tmi->type = TMOD_STRETCH;
		ParseWaveForm( ptext, &tmi->wave );
	}
	else if ( !Q__stricmp( token, "transform" ) ) {
		tmi->type = TMOD_TRANSFORM;
		token = COM_ParseExt( ptext, qfalse ); tmi->matrix[0][0] = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); tmi->matrix[0][1] = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); tmi->matrix[1][0] = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); tmi->matrix[1][1] = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); tmi->translate[0] = (float)atof( token );
		token = COM_ParseExt( ptext, qfalse ); tmi->translate[1] = (float)atof( token );
	}
	else if ( !Q__stricmp( token, "rotate" ) ) {
		tmi->type = TMOD_ROTATE;
		token = COM_ParseExt( ptext, qfalse ); if ( token[0] == 0 ) return;
		tmi->translate[0] = (float)atof( token );	// degsPerSecond in translate[0]
	}
	else if ( !Q__stricmp( token, "entityTranslate" ) ) {
		tmi->type = TMOD_ENTITY_TRANSLATE;
	}
	else {
		// unknown tcmod, skip rest of line
		bundle->numTexMods--;
	}
}


// Load a texture by path and return its GL bind. Returns 0 on failure.
// Uses Texture_LoadDirect to avoid re-entering the shader text parser.
//
// Note: Texture_LoadDirect always returns a valid handle (it caches misses
// so it won't re-attempt the load). The real "did it load" signal is whether
// gluiBind ended up non-zero, so check that instead of the handle.
static GLuint LoadShaderTexture( const char *name )
{
	if ( !name || !name[0] ) return 0;
	if ( name[0] == '$' ) return 0;		// special textures like $lightmap, $whiteimage
	if ( name[0] == '*' ) return 0;		// built-in shader refs like *white, *off, *default

	// [NoMaterial] is Ghoul2's marker for a surface with no material assigned
	// (tag surfaces, caps). It's never a real file - don't warn about it.
	if ( !Q__stricmp(name, "[NoMaterial]") ) return 0;

	int handle = Texture_LoadDirect( name );
	GLuint bind = (handle > 0) ? Texture_GetGLBind( handle ) : 0;
	if (bind != 0) return bind;

	ReportMissingShaderTexture( name );
	return 0;
}


static bool ParseStage( char **text, shaderStage_t *stage )
{
	char *token;
	bool hasMap = false;

	memset( stage, 0, sizeof(*stage) );
	stage->rgbGen = CGEN_IDENTITY;
	stage->alphaGen = AGEN_IDENTITY;
	stage->bundle[0].tcGen = TCGEN_TEXTURE;
	stage->stateBits = GLS_DEFAULT;

	while ( 1 ) {
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] ) return false;
		if ( token[0] == '}' ) break;

		// map <texturename>
		if ( !Q__stricmp( token, "map" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) continue;

			if ( !Q__stricmp( token, "$lightmap" ) ) {
				stage->bundle[0].isLightmap = true;
			} else if ( !Q__stricmp( token, "$whiteimage" ) || !Q__stricmp( token, "*white" ) ) {
				// white image - skip, will render as white
			} else {
				stage->bundle[0].textures[0] = LoadShaderTexture( token );
				stage->bundle[0].numImageAnimations = 1;
			}
			hasMap = true;
		}
		// clampmap <texturename>
		else if ( !Q__stricmp( token, "clampMap" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) continue;
			stage->bundle[0].textures[0] = LoadShaderTexture( token );
			stage->bundle[0].numImageAnimations = 1;
			stage->bundle[0].isClampMap = true;
			hasMap = true;
		}
		// animMap <frequency> <tex1> <tex2> ...
		else if ( !Q__stricmp( token, "animMap" ) || !Q__stricmp( token, "clampanimMap" ) || !Q__stricmp( token, "oneshotanimMap" ) ) {
			bool clamp = !Q__stricmp( token, "clampanimMap" );
			bool oneshot = !Q__stricmp( token, "oneshotanimMap" );

			token = COM_ParseExt( text, qfalse );
			if ( !token[0] ) continue;
			stage->bundle[0].imageAnimationSpeed = (float)atof( token );
			stage->bundle[0].isClampMap = clamp;
			stage->bundle[0].oneShotAnimMap = oneshot;

			int numAnims = 0;
			while ( 1 ) {
				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) break;
				if ( numAnims < MAX_IMAGE_ANIMATIONS ) {
					stage->bundle[0].textures[numAnims] = LoadShaderTexture( token );
					numAnims++;
				}
			}
			stage->bundle[0].numImageAnimations = numAnims;
			hasMap = true;
		}
		// blendFunc <src> <dst>  or  blendFunc <shorthand>
		else if ( !Q__stricmp( token, "blendFunc" ) ) {
			token = COM_ParseExt( text, qfalse );
			int src = 0, dst = 0;
			if ( !Q__stricmp( token, "add" ) ) {
				src = GLS_SRCBLEND_ONE; dst = GLS_DSTBLEND_ONE;
			} else if ( !Q__stricmp( token, "filter" ) ) {
				src = GLS_SRCBLEND_DST_COLOR; dst = GLS_DSTBLEND_ZERO;
			} else if ( !Q__stricmp( token, "blend" ) ) {
				src = GLS_SRCBLEND_SRC_ALPHA; dst = GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA;
			} else {
				src = NameToSrcBlendMode( token );
				token = COM_ParseExt( text, qfalse );
				dst = NameToDstBlendMode( token );
			}
			// Clear old blend bits before setting new ones (second blendFunc overrides first)
			stage->stateBits &= ~(GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS);
			stage->stateBits |= src | dst;
			// GL_ONE GL_ZERO is opaque replacement, keep depth write
			// Everything else is actual blending, disable depth write
			bool isOpaque = (src == GLS_SRCBLEND_ONE && dst == GLS_DSTBLEND_ZERO);
			if ( !isOpaque ) {
				stage->stateBits &= ~GLS_DEPTHMASK_TRUE;
			}
		}
		// rgbGen <mode>
		else if ( !Q__stricmp( token, "rgbGen" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q__stricmp( token, "identity" ) )			stage->rgbGen = CGEN_IDENTITY;
			else if ( !Q__stricmp( token, "identityLighting" ) ) stage->rgbGen = CGEN_IDENTITY_LIGHTING;
			else if ( !Q__stricmp( token, "entity" ) )			stage->rgbGen = CGEN_ENTITY;
			else if ( !Q__stricmp( token, "oneMinusEntity" ) )	stage->rgbGen = CGEN_ONE_MINUS_ENTITY;
			else if ( !Q__stricmp( token, "vertex" ) )			stage->rgbGen = CGEN_VERTEX;
			else if ( !Q__stricmp( token, "exactVertex" ) )	stage->rgbGen = CGEN_EXACT_VERTEX;
			else if ( !Q__stricmp( token, "oneMinusVertex" ) )	stage->rgbGen = CGEN_ONE_MINUS_VERTEX;
			else if ( !Q__stricmp( token, "lightingDiffuse" ) )	stage->rgbGen = CGEN_LIGHTING_DIFFUSE;
			else if ( !Q__stricmp( token, "lightingDiffuseEntity" ) ) stage->rgbGen = CGEN_LIGHTING_DIFFUSE_ENTITY;
			else if ( !Q__stricmp( token, "const" ) || !Q__stricmp( token, "constant" ) ) {
				stage->rgbGen = CGEN_CONST;
				// parse ( r g b )
				token = COM_ParseExt( text, qfalse ); // '('
				token = COM_ParseExt( text, qfalse ); stage->constantColor[0] = (byte)(atof(token) * 255);
				token = COM_ParseExt( text, qfalse ); stage->constantColor[1] = (byte)(atof(token) * 255);
				token = COM_ParseExt( text, qfalse ); stage->constantColor[2] = (byte)(atof(token) * 255);
				token = COM_ParseExt( text, qfalse ); // ')'
			}
			else if ( !Q__stricmp( token, "wave" ) ) {
				stage->rgbGen = CGEN_WAVEFORM;
				ParseWaveForm( text, &stage->rgbWave );
			}
		}
		// alphaGen <mode>
		else if ( !Q__stricmp( token, "alphaGen" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q__stricmp( token, "identity" ) )			stage->alphaGen = AGEN_IDENTITY;
			else if ( !Q__stricmp( token, "entity" ) )			stage->alphaGen = AGEN_ENTITY;
			else if ( !Q__stricmp( token, "oneMinusEntity" ) )	stage->alphaGen = AGEN_ONE_MINUS_ENTITY;
			else if ( !Q__stricmp( token, "vertex" ) )			stage->alphaGen = AGEN_VERTEX;
			else if ( !Q__stricmp( token, "oneMinusVertex" ) )	stage->alphaGen = AGEN_ONE_MINUS_VERTEX;
			else if ( !Q__stricmp( token, "lightingSpecular" ) ) stage->alphaGen = AGEN_LIGHTING_SPECULAR;
			else if ( !Q__stricmp( token, "const" ) || !Q__stricmp( token, "constant" ) ) {
				stage->alphaGen = AGEN_CONST;
				token = COM_ParseExt( text, qfalse );
				stage->constantColor[3] = (byte)(atof(token) * 255);
			}
			else if ( !Q__stricmp( token, "wave" ) ) {
				stage->alphaGen = AGEN_WAVEFORM;
				ParseWaveForm( text, &stage->alphaWave );
			}
			else if ( !Q__stricmp( token, "portal" ) ) {
				stage->alphaGen = AGEN_PORTAL;
				token = COM_ParseExt( text, qfalse ); // range
			}
		}
		// tcGen <mode>
		else if ( !Q__stricmp( token, "tcGen" ) || !Q__stricmp( token, "texGen" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q__stricmp( token, "environment" ) || !Q__stricmp( token, "environmentmodel" ) ) {
				stage->bundle[0].tcGen = TCGEN_ENVIRONMENT_MAPPED;
			} else if ( !Q__stricmp( token, "lightmap" ) ) {
				stage->bundle[0].tcGen = TCGEN_LIGHTMAP;
			} else if ( !Q__stricmp( token, "texture" ) || !Q__stricmp( token, "base" ) ) {
				stage->bundle[0].tcGen = TCGEN_TEXTURE;
			} else if ( !Q__stricmp( token, "vector" ) ) {
				stage->bundle[0].tcGen = TCGEN_VECTOR;
				// parse two vectors
				for (int i = 0; i < 2; i++) {
					token = COM_ParseExt( text, qfalse ); // '('
					for (int j = 0; j < 3; j++) {
						token = COM_ParseExt( text, qfalse );
						stage->bundle[0].tcGenVectors[i][j] = (float)atof(token);
					}
					token = COM_ParseExt( text, qfalse ); // ')'
				}
			}
		}
		// tcMod <type> <params>
		else if ( !Q__stricmp( token, "tcMod" ) ) {
			char buf[1024] = {0};
			// read the rest of the line into buf
			while ( 1 ) {
				token = COM_ParseExt( text, qfalse );
				if ( !token[0] ) break;
				if ( strlen(buf) ) strcat(buf, " ");
				strcat( buf, token );
			}
			ParseTexMod( buf, stage );
		}
		// depthwrite
		else if ( !Q__stricmp( token, "depthWrite" ) ) {
			stage->stateBits |= GLS_DEPTHMASK_TRUE;
		}
		// alphaFunc
		else if ( !Q__stricmp( token, "alphaFunc" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q__stricmp( token, "GT0" ) )		stage->stateBits |= GLS_ATEST_GT_0;
			else if ( !Q__stricmp( token, "LT128" ) )	stage->stateBits |= GLS_ATEST_LT_80;
			else if ( !Q__stricmp( token, "GE128" ) )	stage->stateBits |= GLS_ATEST_GE_80;
		}
		// glow
		else if ( !Q__stricmp( token, "glow" ) ) {
			stage->glow = true;
		}
		// detail
		else if ( !Q__stricmp( token, "detail" ) ) {
			stage->isDetail = true;
		}
		// depthFunc
		else if ( !Q__stricmp( token, "depthFunc" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q__stricmp( token, "equal" ) )
				stage->stateBits |= GLS_DEPTHFUNC_EQUAL;
			else if ( !Q__stricmp( token, "disable" ) )
				stage->stateBits |= GLS_DEPTHTEST_DISABLE;
		}
		// unrecognized: skip rest of line
		else {
			SkipRestOfLine( text );
		}
	}

	if ( !hasMap ) return false;
	stage->active = true;
	return true;
}


static bool ParseShader( char **text, shader_t *dest )
{
	char *token;
	int stageIndex = 0;

	memset( dest, 0, sizeof(*dest) );
	dest->cullType = CT_FRONT_SIDED;
	dest->sort = 3.0f;	// SS_OPAQUE
	dest->explicitlyDefined = true;

	memset( stages_parse, 0, sizeof(stages_parse) );

	token = COM_ParseExt( text, qtrue );
	if ( token[0] != '{' ) return false;

	while ( 1 ) {
		token = COM_ParseExt( text, qtrue );
		if ( !token[0] ) return false;
		if ( token[0] == '}' ) break;

		// stage block
		if ( token[0] == '{' ) {
			if ( stageIndex < MAX_SHADER_STAGES ) {
				if ( ParseStage( text, &stages_parse[stageIndex] ) ) {
					stageIndex++;
				}
			} else {
				SkipBracedSection( text );
			}
			continue;
		}

		// shader-level keywords
		if ( !Q__stricmp( token, "cull" ) ) {
			token = COM_ParseExt( text, qfalse );
			if ( !Q__stricmp( token, "none" ) || !Q__stricmp( token, "twosided" ) || !Q__stricmp( token, "disable" ) )
				dest->cullType = CT_TWO_SIDED;
			else if ( !Q__stricmp( token, "back" ) || !Q__stricmp( token, "backside" ) || !Q__stricmp( token, "backsided" ) )
				dest->cullType = CT_BACK_SIDED;
			else
				dest->cullType = CT_FRONT_SIDED;
		}
		else if ( !Q__stricmp( token, "nomipmaps" ) || !Q__stricmp( token, "nomipmap" ) ) {
			dest->noMipMaps = true;
			dest->noPicMip = true;
		}
		else if ( !Q__stricmp( token, "nopicmip" ) ) {
			dest->noPicMip = true;
		}
		else if ( !Q__stricmp( token, "polygonOffset" ) ) {
			dest->polygonOffset = true;
		}
		else if ( !Q__stricmp( token, "sort" ) ) {
			token = COM_ParseExt( text, qfalse );
			if      ( !Q__stricmp( token, "portal" ) )		dest->sort = 1.0f;
			else if ( !Q__stricmp( token, "sky" ) )			dest->sort = 2.0f;
			else if ( !Q__stricmp( token, "opaque" ) )		dest->sort = 3.0f;
			else if ( !Q__stricmp( token, "banner" ) )		dest->sort = 6.0f;
			else if ( !Q__stricmp( token, "additive" ) )	dest->sort = 9.0f;
			else if ( !Q__stricmp( token, "nearest" ) )		dest->sort = 16.0f;
			else dest->sort = (float)atof( token );
		}
		// deformVertexes
		else if ( !Q__stricmp( token, "deformVertexes" ) ) {
			if ( dest->numDeforms < MAX_SHADER_DEFORMS ) {
				deformStage_t *ds = &dest->deforms[dest->numDeforms];
				memset( ds, 0, sizeof(*ds) );

				token = COM_ParseExt( text, qfalse );

				if ( !Q__stricmp( token, "wave" ) ) {
					// deformVertexes wave <spread> <func> <base> <amp> <phase> <freq>
					token = COM_ParseExt( text, qfalse );
					float spread = (float)atof( token );
					if ( spread != 0.0f )
						ds->deformationSpread = 1.0f / spread;
					else
						ds->deformationSpread = 100.0f;

					ParseWaveForm( text, &ds->deformationWave );
					ds->deformation = DEFORM_WAVE;
					dest->numDeforms++;
				}
				else if ( !Q__stricmp( token, "bulge" ) ) {
					token = COM_ParseExt( text, qfalse ); ds->bulgeWidth = (float)atof(token);
					token = COM_ParseExt( text, qfalse ); ds->bulgeHeight = (float)atof(token);
					token = COM_ParseExt( text, qfalse ); ds->bulgeSpeed = (float)atof(token);
					ds->deformation = DEFORM_BULGE;
					dest->numDeforms++;
				}
				else if ( !Q__stricmp( token, "normal" ) ) {
					token = COM_ParseExt( text, qfalse ); ds->deformationWave.amplitude = (float)atof(token);
					token = COM_ParseExt( text, qfalse ); ds->deformationWave.frequency = (float)atof(token);
					ds->deformation = DEFORM_NORMALS;
					dest->numDeforms++;
				}
				else if ( !Q__stricmp( token, "move" ) ) {
					token = COM_ParseExt( text, qfalse ); ds->moveVector[0] = (float)atof(token);
					token = COM_ParseExt( text, qfalse ); ds->moveVector[1] = (float)atof(token);
					token = COM_ParseExt( text, qfalse ); ds->moveVector[2] = (float)atof(token);
					ParseWaveForm( text, &ds->deformationWave );
					ds->deformation = DEFORM_MOVE;
					dest->numDeforms++;
				}
				else {
					SkipRestOfLine( text );
				}
			} else {
				SkipRestOfLine( text );
			}
		}
		// surfaceparm, qer_editorimage, q3map_* etc - skip
		else if ( !Q__stricmp( token, "surfaceparm" ) || !Q__stricmp( token, "qer_editorimage" ) ||
				  !Q__stricmp( token, "qer_nocarve" ) || !Q__stricmp( token, "qer_trans" ) ||
				  !Q__stricmp( token, "notc" ) || !Q__stricmp( token, "noTC" ) ||
				  !Q__stricmp( token, "entityMergable" ) ||
				  !Q__stricmp( token, "fogParms" ) ) {
			SkipRestOfLine( text );
		}
		else if ( token[0] == 'q' && token[1] == '3' ) {
			SkipRestOfLine( text ); // q3map_* directives
		}
		else {
			SkipRestOfLine( text );
		}
	}

	// Copy parsed stages into destination
	dest->numStages = 0;
	dest->hasGlow = false;
	for ( int i = 0; i < stageIndex; i++ ) {
		// Skip lightmap-only stages for ModView (fullbright rendering)
		if ( stages_parse[i].bundle[0].isLightmap ) continue;

		if ( stages_parse[i].glow ) dest->hasGlow = true;
		dest->stages[dest->numStages] = stages_parse[i];
		dest->numStages++;
	}

	// If blending stages exist but no opaque first stage, adjust sort
	if ( dest->numStages > 0 && (dest->stages[0].stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS)) ) {
		if ( dest->sort == 3.0f ) dest->sort = 9.0f; // transparent
	}

	return dest->numStages > 0;
}


// =============================================================================
// R_FindShader - the main shader lookup/creation function
// =============================================================================

static long ShaderHashValue( const char *fname )
{
	long hash = 0;
	int i = 0;
	while (fname[i] != '\0') {
		char letter = tolower(fname[i]);
		if (letter == '.') break;
		if (letter == '\\') letter = '/';
		hash += (long)(letter) * (i + 119);
		i++;
	}
	return hash & (SHADER_FILE_HASH_SIZE - 1);
}


shader_t *R_FindShader( const char *name )
{
	if ( !name || !name[0] ) {
		return &shaderTable[0];  // return default
	}

	char strippedName[MAX_QPATH];
	COM_StripExtension( name, strippedName );
	_strlwr( strippedName );

	// Check hash table for existing shader
	long hash = ShaderHashValue( strippedName );
	for ( shader_t *sh = shaderHashTable[hash]; sh; sh = sh->next ) {
		if ( !Q__stricmp( sh->name, strippedName ) ) {
			return sh;
		}
	}

	// Not found - need to create one
	if ( numShaders >= MAX_SHADER_TABLE ) {
		Com_Printf("WARNING: R_FindShader - MAX_SHADER_TABLE hit\n");
		return &shaderTable[0];
	}

	shader_t *newShader = &shaderTable[numShaders];
	memset( newShader, 0, sizeof(*newShader) );
	Q_strncpyz( newShader->name, strippedName, sizeof(newShader->name) );
	newShader->index = numShaders;

	// Make sure shader files are loaded
	ScanAndLoadShaderFiles();

	// Try to find in .shader text
	char *shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText ) {
		if ( ParseShader( &shaderText, newShader ) ) {
			Q_strncpyz( newShader->name, strippedName, sizeof(newShader->name) );
			newShader->index = numShaders;
		} else {
			newShader->defaultShader = true;
		}
	} else {
		newShader->defaultShader = true;
	}


	// For default shaders (no .shader file), create a simple single-stage shader
	if ( newShader->defaultShader || newShader->numStages == 0 ) {
		newShader->defaultShader = true;
		newShader->cullType = CT_FRONT_SIDED;
		newShader->sort = 3.0f;
		newShader->numStages = 1;

		shaderStage_t *s = &newShader->stages[0];
		memset( s, 0, sizeof(*s) );
		s->active = true;
		s->rgbGen = CGEN_IDENTITY;
		s->alphaGen = AGEN_IDENTITY;
		s->stateBits = GLS_DEFAULT;
		s->bundle[0].tcGen = TCGEN_TEXTURE;
		s->bundle[0].numImageAnimations = 1;
		s->bundle[0].textures[0] = LoadShaderTexture( name );

		// If the texture with original name didn't load, try stripped name
		if ( s->bundle[0].textures[0] == 0 ) {
			s->bundle[0].textures[0] = LoadShaderTexture( strippedName );
		}
	}

	// Store the first-stage texture as the "primary" GL bind for legacy code
	if ( newShader->numStages > 0 && newShader->stages[0].bundle[0].numImageAnimations > 0 ) {
		// legacy bind is the first texture of the first stage
	}

	// Add to hash table
	newShader->next = shaderHashTable[hash];
	shaderHashTable[hash] = newShader;
	numShaders++;

	return newShader;
}


// =============================================================================
// GL_State - set OpenGL state from GLS_* bits
// =============================================================================

unsigned int glStateBits = 0;

void GL_State( unsigned int stateBits )
{
	unsigned int diff = stateBits ^ glStateBits;
	if ( !diff ) return;

	// blending
	if ( diff & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS) ) {
		GLenum srcFactor, dstFactor;

		switch ( stateBits & GLS_SRCBLEND_BITS ) {
			case GLS_SRCBLEND_ZERO:					srcFactor = GL_ZERO; break;
			case GLS_SRCBLEND_ONE:					srcFactor = GL_ONE; break;
			case GLS_SRCBLEND_DST_COLOR:			srcFactor = GL_DST_COLOR; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_COLOR:	srcFactor = GL_ONE_MINUS_DST_COLOR; break;
			case GLS_SRCBLEND_SRC_ALPHA:			srcFactor = GL_SRC_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA:	srcFactor = GL_ONE_MINUS_SRC_ALPHA; break;
			case GLS_SRCBLEND_DST_ALPHA:			srcFactor = GL_DST_ALPHA; break;
			case GLS_SRCBLEND_ONE_MINUS_DST_ALPHA:	srcFactor = GL_ONE_MINUS_DST_ALPHA; break;
			case GLS_SRCBLEND_ALPHA_SATURATE:		srcFactor = GL_SRC_ALPHA_SATURATE; break;
			default:								srcFactor = GL_ONE; break;
		}

		switch ( stateBits & GLS_DSTBLEND_BITS ) {
			case GLS_DSTBLEND_ZERO:					dstFactor = GL_ZERO; break;
			case GLS_DSTBLEND_ONE:					dstFactor = GL_ONE; break;
			case GLS_DSTBLEND_SRC_COLOR:			dstFactor = GL_SRC_COLOR; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_COLOR:	dstFactor = GL_ONE_MINUS_SRC_COLOR; break;
			case GLS_DSTBLEND_SRC_ALPHA:			dstFactor = GL_SRC_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA:	dstFactor = GL_ONE_MINUS_SRC_ALPHA; break;
			case GLS_DSTBLEND_DST_ALPHA:			dstFactor = GL_DST_ALPHA; break;
			case GLS_DSTBLEND_ONE_MINUS_DST_ALPHA:	dstFactor = GL_ONE_MINUS_DST_ALPHA; break;
			default:								dstFactor = GL_ONE; break;
		}

		if ( stateBits & (GLS_SRCBLEND_BITS | GLS_DSTBLEND_BITS) ) {
			glEnable( GL_BLEND );
			glBlendFunc( srcFactor, dstFactor );
		} else {
			glDisable( GL_BLEND );
		}
	}

	// depth mask
	if ( diff & GLS_DEPTHMASK_TRUE ) {
		if ( stateBits & GLS_DEPTHMASK_TRUE )
			glDepthMask( GL_TRUE );
		else
			glDepthMask( GL_FALSE );
	}

	// depth test
	if ( diff & GLS_DEPTHTEST_DISABLE ) {
		if ( stateBits & GLS_DEPTHTEST_DISABLE )
			glDisable( GL_DEPTH_TEST );
		else
			glEnable( GL_DEPTH_TEST );
	}

	// depth func
	if ( diff & GLS_DEPTHFUNC_EQUAL ) {
		if ( stateBits & GLS_DEPTHFUNC_EQUAL )
			glDepthFunc( GL_EQUAL );
		else
			glDepthFunc( GL_LEQUAL );
	}

	// alpha test
	if ( diff & GLS_ATEST_BITS ) {
		switch ( stateBits & GLS_ATEST_BITS ) {
			case 0:
				glDisable( GL_ALPHA_TEST );
				break;
			case GLS_ATEST_GT_0:
				glEnable( GL_ALPHA_TEST );
				glAlphaFunc( GL_GREATER, 0.0f );
				break;
			case GLS_ATEST_LT_80:
				glEnable( GL_ALPHA_TEST );
				glAlphaFunc( GL_LESS, 0.5f );
				break;
			case GLS_ATEST_GE_80:
				glEnable( GL_ALPHA_TEST );
				glAlphaFunc( GL_GEQUAL, 0.5f );
				break;
		}
	}

	// polygon mode
	if ( diff & GLS_POLYMODE_LINE ) {
		if ( stateBits & GLS_POLYMODE_LINE )
			glPolygonMode( GL_FRONT_AND_BACK, GL_LINE );
		else
			glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
	}

	glStateBits = stateBits;
}


// =============================================================================
// Waveform evaluation
// =============================================================================

float EvalWaveForm( const waveForm_t *wf, float shaderTime )
{
	float *table;

	switch ( wf->func ) {
		case GF_SIN:				table = sv_sinTable; break;
		case GF_TRIANGLE:			table = sv_triangleTable; break;
		case GF_SQUARE:				table = sv_squareTable; break;
		case GF_SAWTOOTH:			table = sv_sawToothTable; break;
		case GF_INVERSE_SAWTOOTH:	table = sv_inverseSawToothTable; break;
		case GF_NOISE:
		case GF_RAND:
			// simplified: just use sin for noise/rand
			table = sv_sinTable;
			break;
		default:
			return 1.0f;
	}

	int index = (int)( ( wf->phase + shaderTime * wf->frequency ) * FUNCTABLE_SIZE ) & FUNCTABLE_MASK;
	return wf->base + table[index] * wf->amplitude;
}


// =============================================================================
// R_BindAnimatedImage - bind the correct texture frame for a bundle
// =============================================================================

void R_BindAnimatedImage( textureBundle_t *bundle, float shaderTime )
{
	if ( bundle->numImageAnimations <= 1 ) {
		if ( bundle->numImageAnimations == 1 && bundle->textures[0] )
			glBindTexture( GL_TEXTURE_2D, bundle->textures[0] );
		return;
	}

	// animated texture
	int index = (int)( shaderTime * bundle->imageAnimationSpeed );

	if ( bundle->oneShotAnimMap ) {
		if ( index >= bundle->numImageAnimations )
			index = bundle->numImageAnimations - 1;
	} else {
		index %= bundle->numImageAnimations;
	}

	if ( index < 0 ) index = 0;

	if ( bundle->textures[index] )
		glBindTexture( GL_TEXTURE_2D, bundle->textures[index] );
}


// =============================================================================
// Texture coordinate modification
// =============================================================================

void RB_CalcTexMods( const textureBundle_t *bundle, float shaderTime,
					 const vec2_t *srcTexCoords, vec2_t *dstTexCoords, int numVertexes )
{
	// Start by copying source to dest
	memcpy( dstTexCoords, srcTexCoords, sizeof(vec2_t) * numVertexes );

	for ( int m = 0; m < bundle->numTexMods; m++ ) {
		const texModInfo_t *tmi = &bundle->texMods[m];

		switch ( tmi->type ) {

		case TMOD_SCROLL: {
			float adjustedScrollS = tmi->translate[0] * shaderTime;
			float adjustedScrollT = tmi->translate[1] * shaderTime;
			// clamp so we don't get huge float precision issues
			adjustedScrollS -= floor( adjustedScrollS );
			adjustedScrollT -= floor( adjustedScrollT );
			for ( int i = 0; i < numVertexes; i++ ) {
				dstTexCoords[i][0] += adjustedScrollS;
				dstTexCoords[i][1] += adjustedScrollT;
			}
			break;
		}

		case TMOD_SCALE: {
			float scaleS = tmi->translate[0];
			float scaleT = tmi->translate[1];
			for ( int i = 0; i < numVertexes; i++ ) {
				dstTexCoords[i][0] *= scaleS;
				dstTexCoords[i][1] *= scaleT;
			}
			break;
		}

		case TMOD_ROTATE: {
			float degsPerSecond = tmi->translate[0];
			float degs = -degsPerSecond * shaderTime;
			float angle = degs * (float)(M_PI / 180.0);
			float sinValue = sin(angle);
			float cosValue = cos(angle);
			// rotation around (0.5, 0.5)
			for ( int i = 0; i < numVertexes; i++ ) {
				float s = dstTexCoords[i][0] - 0.5f;
				float t = dstTexCoords[i][1] - 0.5f;
				dstTexCoords[i][0] = s * cosValue + t * (-sinValue) + 0.5f;
				dstTexCoords[i][1] = s * sinValue + t * cosValue + 0.5f;
			}
			break;
		}

		case TMOD_STRETCH: {
			float p = 1.0f / EvalWaveForm( &tmi->wave, shaderTime );
			// scale around (0.5, 0.5)
			for ( int i = 0; i < numVertexes; i++ ) {
				dstTexCoords[i][0] = (dstTexCoords[i][0] - 0.5f) * p + 0.5f;
				dstTexCoords[i][1] = (dstTexCoords[i][1] - 0.5f) * p + 0.5f;
			}
			break;
		}

		case TMOD_TURBULENT: {
			float now = ( tmi->wave.phase + shaderTime * tmi->wave.frequency );
			for ( int i = 0; i < numVertexes; i++ ) {
				// Use vertex position to vary the turbulence per-vertex
				// For simplicity, use the index as a stand-in since we don't easily have xyz here
				float sOff = sv_sinTable[ ((int)(( (float)i * 0.125f + now ) * FUNCTABLE_SIZE)) & FUNCTABLE_MASK ] * tmi->wave.amplitude;
				float tOff = sv_sinTable[ ((int)(( (float)i * 0.217f + now ) * FUNCTABLE_SIZE)) & FUNCTABLE_MASK ] * tmi->wave.amplitude;
				dstTexCoords[i][0] += sOff;
				dstTexCoords[i][1] += tOff;
			}
			break;
		}

		case TMOD_TRANSFORM: {
			for ( int i = 0; i < numVertexes; i++ ) {
				float s = dstTexCoords[i][0];
				float t = dstTexCoords[i][1];
				dstTexCoords[i][0] = s * tmi->matrix[0][0] + t * tmi->matrix[1][0] + tmi->translate[0];
				dstTexCoords[i][1] = s * tmi->matrix[0][1] + t * tmi->matrix[1][1] + tmi->translate[1];
			}
			break;
		}

		default:
			break;
		}
	}
}


// =============================================================================
// Backward-compatible shader-to-texture-name lookup
// Used by skin system and GLM code that need a texture path string
// =============================================================================

static bool CheckForFilenameArg(LPCSTR &psSearchPos, LPCSTR psKeyword)
{
	LPCSTR psSearchResult = strstr(psSearchPos,psKeyword);
	if (psSearchResult && isspace(psSearchResult[strlen(psKeyword)]))
	{
		psSearchResult += strlen(psKeyword);
		while (*psSearchResult && isspace(*psSearchResult)) psSearchResult++;
		if (strlen(psSearchResult) && *psSearchResult != '$' && ((psSearchResult>psSearchPos)?isspace(psSearchResult[-1]):1) )
		{
			psSearchPos = psSearchResult;
			return true;
		}
	}
	if (psSearchResult) {
		while (*psSearchResult++ != '\n'){}
		psSearchResult++;
		psSearchPos = psSearchResult;
	} else {
		psSearchPos = NULL;
	}
	return false;
}

static LPCSTR Shader_ExtractSuitableFilename(LPCSTR psShaderText)
{
	char *psShaderTextEnd = (char*)psShaderText;
	SkipBracedSection (&psShaderTextEnd);
	int iShaderChars = psShaderTextEnd - psShaderText;

	char *psThisShader = (char *) malloc(iShaderChars+1);
	char *psShaderSearch = psThisShader;
	strncpy(psThisShader,psShaderText,iShaderChars);
	psThisShader[iShaderChars]='\0';

	LPCSTR psAnswer = NULL;
	char *psShaderSearchTry = psShaderSearch;
	if (CheckForFilenameArg((LPCSTR&)psShaderSearchTry, "qer_editorimage"))
		psAnswer = psShaderSearchTry;

	psShaderSearchTry = psShaderSearch;
	while (psShaderSearchTry && !psAnswer) {
		if (CheckForFilenameArg((LPCSTR&)psShaderSearchTry, "map"))
			psAnswer = psShaderSearchTry;
	}
	if (!psAnswer) {
		psShaderSearchTry = psShaderSearch;
		while (psShaderSearchTry && !psAnswer) {
			if (CheckForFilenameArg((LPCSTR&)psShaderSearchTry, "clampmap"))
				psAnswer = psShaderSearchTry;
		}
	}

	static char sReturnName[MAX_QPATH];
	if (psAnswer) {
		strncpy(sReturnName,psAnswer,sizeof(sReturnName));
		sReturnName[sizeof(sReturnName)-1]='\0';
		for (int i=0; i<sizeof(sReturnName)-1; i++) {
			if (sReturnName[i]=='\0') break;
			if (isspace(sReturnName[i])) { sReturnName[i] = '\0'; break; }
		}
	}
	free(psThisShader);
	return psAnswer?sReturnName:NULL;
}


const char *R_FindShaderTextureName( const char *psLocalMaterialName )
{
	static char sReturnString[MAX_QPATH];
	char strippedName[MAX_QPATH];
	char *shaderText;

	COM_StripExtension( psLocalMaterialName, strippedName );
	_strlwr(strippedName);

	ShadersFoundAndFilesPicked_t::iterator it = ShadersFoundAndFilesPicked.find(strippedName);
	if (it != ShadersFoundAndFilesPicked.end())
		return (*it).second.c_str();

	ScanAndLoadShaderFiles();

	shaderText = FindShaderInShaderText( strippedName );
	if ( shaderText )
	{
		LPCSTR psReturnName = Shader_ExtractSuitableFilename(shaderText);
		if (psReturnName)
			psLocalMaterialName = psReturnName;
	}

	ShadersFoundAndFilesPicked[strippedName] = psLocalMaterialName;
	return psLocalMaterialName;
}


////////////////// eof ////////////////
