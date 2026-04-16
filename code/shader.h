// Filename:-	shader.h
//
// Shader data structures and rendering state for Q3/JKA-style .shader support


#ifndef SHADER_H
#define SHADER_H

#include "R_Common.h"

// =============================================================================
// Shader scanning / loading
// =============================================================================

void ScanAndLoadShaderFiles( void );
void KillAllShaderFiles(void);

// =============================================================================
// GL state bits (used in shaderStage_t::stateBits)
// =============================================================================

#define GLS_SRCBLEND_ZERO						0x00000001
#define GLS_SRCBLEND_ONE						0x00000002
#define GLS_SRCBLEND_DST_COLOR					0x00000003
#define GLS_SRCBLEND_ONE_MINUS_DST_COLOR		0x00000004
#define GLS_SRCBLEND_SRC_ALPHA					0x00000005
#define GLS_SRCBLEND_ONE_MINUS_SRC_ALPHA		0x00000006
#define GLS_SRCBLEND_DST_ALPHA					0x00000007
#define GLS_SRCBLEND_ONE_MINUS_DST_ALPHA		0x00000008
#define GLS_SRCBLEND_ALPHA_SATURATE				0x00000009
#define		GLS_SRCBLEND_BITS					0x0000000f

#define GLS_DSTBLEND_ZERO						0x00000010
#define GLS_DSTBLEND_ONE						0x00000020
#define GLS_DSTBLEND_SRC_COLOR					0x00000030
#define GLS_DSTBLEND_ONE_MINUS_SRC_COLOR		0x00000040
#define GLS_DSTBLEND_SRC_ALPHA					0x00000050
#define GLS_DSTBLEND_ONE_MINUS_SRC_ALPHA		0x00000060
#define GLS_DSTBLEND_DST_ALPHA					0x00000070
#define GLS_DSTBLEND_ONE_MINUS_DST_ALPHA		0x00000080
#define		GLS_DSTBLEND_BITS					0x000000f0

#define GLS_DEPTHMASK_TRUE						0x00000100

#define GLS_POLYMODE_LINE						0x00001000

#define GLS_DEPTHTEST_DISABLE					0x00010000
#define GLS_DEPTHFUNC_EQUAL						0x00020000

#define GLS_ATEST_GT_0							0x10000000
#define GLS_ATEST_LT_80						0x20000000
#define GLS_ATEST_GE_80						0x40000000
#define		GLS_ATEST_BITS						0x70000000

#define GLS_DEFAULT			GLS_DEPTHMASK_TRUE

// =============================================================================
// Enumerations
// =============================================================================

#define MAX_SHADER_STAGES		8
#define TR_MAX_TEXMODS			4
#define MAX_IMAGE_ANIMATIONS	8
#define MAX_SHADER_DEFORMS		3

typedef enum {
	GF_NONE,
	GF_SIN,
	GF_SQUARE,
	GF_TRIANGLE,
	GF_SAWTOOTH,
	GF_INVERSE_SAWTOOTH,
	GF_NOISE,
	GF_RAND
} genFunc_t;

typedef enum {
	CGEN_BAD,
	CGEN_IDENTITY_LIGHTING,
	CGEN_IDENTITY,
	CGEN_ENTITY,
	CGEN_ONE_MINUS_ENTITY,
	CGEN_EXACT_VERTEX,
	CGEN_VERTEX,
	CGEN_ONE_MINUS_VERTEX,
	CGEN_WAVEFORM,
	CGEN_LIGHTING_DIFFUSE,
	CGEN_LIGHTING_DIFFUSE_ENTITY,
	CGEN_FOG,
	CGEN_CONST
} colorGen_t;

typedef enum {
	AGEN_IDENTITY,
	AGEN_SKIP,
	AGEN_ENTITY,
	AGEN_ONE_MINUS_ENTITY,
	AGEN_VERTEX,
	AGEN_ONE_MINUS_VERTEX,
	AGEN_LIGHTING_SPECULAR,
	AGEN_WAVEFORM,
	AGEN_PORTAL,
	AGEN_CONST
} alphaGen_t;

typedef enum {
	TCGEN_BAD,
	TCGEN_IDENTITY,
	TCGEN_LIGHTMAP,
	TCGEN_TEXTURE,
	TCGEN_ENVIRONMENT_MAPPED,
	TCGEN_VECTOR
} texCoordGen_t;

typedef enum {
	TMOD_NONE,
	TMOD_TRANSFORM,
	TMOD_TURBULENT,
	TMOD_SCROLL,
	TMOD_SCALE,
	TMOD_STRETCH,
	TMOD_ROTATE,
	TMOD_ENTITY_TRANSLATE
} texMod_t;

typedef enum {
	CT_FRONT_SIDED,
	CT_BACK_SIDED,
	CT_TWO_SIDED
} cullType_t;

// =============================================================================
// Shader sub-structures
// =============================================================================

typedef struct waveForm_s {
	genFunc_t	func;
	float		base;
	float		amplitude;
	float		phase;
	float		frequency;
} waveForm_t;

typedef enum {
	DEFORM_NONE,
	DEFORM_WAVE,
	DEFORM_BULGE,
	DEFORM_MOVE,
	DEFORM_NORMALS
} deform_t;

typedef struct deformStage_s {
	deform_t		deformation;
	waveForm_t		deformationWave;
	float			deformationSpread;
	float			bulgeWidth;
	float			bulgeHeight;
	float			bulgeSpeed;
	vec3_t			moveVector;
} deformStage_t;

typedef struct texModInfo_s {
	texMod_t	type;
	waveForm_t	wave;			// TMOD_TURBULENT, TMOD_STRETCH
	float		matrix[2][2];	// TMOD_TRANSFORM
	float		translate[2];	// TMOD_TRANSFORM, TMOD_SCROLL, TMOD_SCALE, TMOD_ROTATE (packed)
} texModInfo_t;

typedef struct image_s {
	char		imgName[MAX_QPATH];
	int			width, height;
	int			uploadWidth, uploadHeight;
	GLuint		texnum;
	struct image_s*	next;
} image_t;

typedef struct textureBundle_s {
	GLuint			textures[MAX_IMAGE_ANIMATIONS];	// GL bind IDs
	int				numImageAnimations;				// 0 or 1 = static, >1 = animated
	float			imageAnimationSpeed;			// frames per second for animMap
	bool			oneShotAnimMap;

	texCoordGen_t	tcGen;
	vec3_t			tcGenVectors[2];				// for TCGEN_VECTOR

	texModInfo_t	texMods[TR_MAX_TEXMODS];
	int				numTexMods;

	bool			isLightmap;
	bool			isClampMap;
} textureBundle_t;

#define NUM_TEXTURE_BUNDLES 2

typedef struct shaderStage_s {
	bool			active;
	bool			isDetail;

	textureBundle_t	bundle[NUM_TEXTURE_BUNDLES];

	colorGen_t		rgbGen;
	waveForm_t		rgbWave;

	alphaGen_t		alphaGen;
	waveForm_t		alphaWave;

	byte			constantColor[4];	// for CGEN_CONST and AGEN_CONST

	unsigned int	stateBits;			// GLS_xxxx mask

	bool			glow;
} shaderStage_t;

// =============================================================================
// shader_t - the main shader definition
// =============================================================================

typedef struct shader_s {
	char		name[MAX_QPATH];		// game path, no extension

	int			index;					// this shader == tr.shaders[index]

	bool		defaultShader;			// true if no .shader definition found
	bool		explicitlyDefined;		// true if found in a .shader file

	cullType_t	cullType;
	bool		polygonOffset;
	bool		noMipMaps;
	bool		noPicMip;

	float		sort;					// for transparency ordering
	bool		hasGlow;				// true if any stage has the glow flag

	int			numDeforms;
	deformStage_t	deforms[MAX_SHADER_DEFORMS];

	int			numStages;
	shaderStage_t	stages[MAX_SHADER_STAGES];

	struct	shader_s	*next;			// hash chain
} shader_t;


// =============================================================================
// Tessellation / draw command buffer
// =============================================================================

typedef byte color4ub_t[4];
typedef unsigned int glIndex_t;

// per-stage computed vars
typedef struct stageVars_s {
	color4ub_t	colors[ACTUAL_SHADER_MAX_VERTEXES];
	vec2_t		texcoords[NUM_TEXTURE_BUNDLES][ACTUAL_SHADER_MAX_VERTEXES];
} stageVars_t;

typedef struct shaderCommands_s
{
	glIndex_t	indexes[ACTUAL_SHADER_MAX_INDEXES];
	vec4_t		xyz[ACTUAL_SHADER_MAX_VERTEXES];
	vec4_t		normal[ACTUAL_SHADER_MAX_VERTEXES];
	vec2_t		texCoords[ACTUAL_SHADER_MAX_VERTEXES][2];
	int			WeightsUsed[ACTUAL_SHADER_MAX_VERTEXES];	// ModView: vert weighting-count
	int			WeightsOmitted[ACTUAL_SHADER_MAX_VERTEXES];	// ModView

	color4ub_t	vertexColors[ACTUAL_SHADER_MAX_VERTEXES];
	stageVars_t	svars;

	shader_t	*shader;
	GLuint		gluiTextureBind;	// legacy: first-stage texture for backward compat

	float		shaderTime;			// time for shader animations (seconds)

	int			numIndexes;
	int			numVertexes;

	// ModView-specific fields
	ModelHandle_t hModel;
	int			iSurfaceNum;
	bool		bSurfaceIsG2Tag;
	refEntity_t *pRefEnt;

} shaderCommands_t;

extern	shaderCommands_t	tess;

// =============================================================================
// Shader API
// =============================================================================

#define FUNCTABLE_SIZE		1024
#define FUNCTABLE_MASK		(FUNCTABLE_SIZE - 1)

// Shader lookup tables (for waveform evaluation)
extern float	sv_sinTable[FUNCTABLE_SIZE];
extern float	sv_triangleTable[FUNCTABLE_SIZE];
extern float	sv_squareTable[FUNCTABLE_SIZE];
extern float	sv_sawToothTable[FUNCTABLE_SIZE];
extern float	sv_inverseSawToothTable[FUNCTABLE_SIZE];

void		Shader_Init( void );			// init tables, called once
shader_t	*R_FindShader( const char *name );			// find or create shader by name
const char	*R_FindShaderTextureName( const char *name );	// backward compat: shader name -> texture path
shader_t	*R_GetShaderByIndex( int index );
int			R_GetNumShaders( void );
void		R_ShutdownShaders( void );

// GL state management
void GL_State( unsigned int stateBits );

// Texture animation binding
void R_BindAnimatedImage( textureBundle_t *bundle, float shaderTime );

// Waveform evaluation
float EvalWaveForm( const waveForm_t *wf, float shaderTime );

// Texture coordinate modification
void RB_CalcTexMods( const textureBundle_t *bundle, float shaderTime,
					 const vec2_t *srcTexCoords, vec2_t *dstTexCoords, int numVertexes );


#endif	// #ifndef SHADER_H

///////////////// eof //////////////
