// Filename:-	R_Surface.cpp
//
// a container module for paste-code to do with surface rendering
//
#include "stdafx.h"
#include "includes.h"
#include "R_Common.h"
#include "text.h"
//
#include "r_surface.h"
#include "textures.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


bool g_bRenderGlowingObjects = false;	// when true, only glow stages are rendered
float g_fAccumulatedShaderTime = 0.0f;	// accumulated shader animation time

// Saved primary model state for the glow pass (survives per-container reset)
drawSurf_t g_glowDrawSurfs[MAX_DRAWSURFS];
int g_glowNumDrawSurfs = 0;
trRefEntity_t g_glowEntities[MAX_MOD_KNOWN];
int g_glowNumEntities = 0;

int giRenderedBoneWeights;
int giOmittedBoneWeights;


void GetWeightColour(int iNumWeights, byte &r, byte &g, byte &b)
{
	switch (iNumWeights)
	{
		case 0:
			
			//assert(0);	// this shouldn't happen, but...
			//
			r=255,g=255,b=255;	// white
			break;

		case 1:

			r=0,g=255,b=0;		// bright green
			break;

		case 2:

			r=0,g=128,b=0;		// dark green
			break;

		case 3:

			r=255,g=255,b=0;	// bright yellow
//			r=128,g=128,b=0;	// dark yellow
			break;

		case 4:

			r=0,g=255,b=255;	// bright cyan
			break;

		default:

			// anything > 4 (shouldn't happen, because carcass will limit it)...
			//
			r=255,g=0,b=0;
			break;
	}
}



void RB_StageIteratorGeneric( void );

void RB_EndSurface( void ) {
	shaderCommands_t *input;

	input = &tess;

	if (input->numIndexes == 0) {
		return;
	}

	if (input->indexes[ACTUAL_SHADER_MAX_INDEXES-1] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - ACTUAL_SHADER_MAX_INDEXES hit");
	}	
	if (input->xyz[ACTUAL_SHADER_MAX_VERTEXES-1][0] != 0) {
		ri.Error (ERR_DROP, "RB_EndSurface() - ACTUAL_SHADER_MAX_VERTEXES hit");
	}
/*MODVIEWREM
	if ( tess.shader == tr.shadowShader ) {
		RB_ShadowTessEnd();
		return;
	}

	// for debugging of sort order issues, stop rendering after a given sort value
	if ( r_debugSort->integer && r_debugSort->integer < tess.shader->sort ) {
		return;
	}

	//
	// update performance counters
	//
	backEnd.pc.c_shaders++;
	backEnd.pc.c_vertexes += tess.numVertexes;
	backEnd.pc.c_indexes += tess.numIndexes;
	backEnd.pc.c_totalIndexes += tess.numIndexes * tess.numPasses;

	//
	// call off to shader specific tess end function
	//
	tess.currentStageIteratorFunc();

	//
	// draw debugging stuff
	//
	if ( r_showtris->integer ) {
		DrawTris (input);
	}
	if ( r_shownormals->integer ) {
		DrawNormals (input);
	}

*/

	RB_StageIteratorGeneric();

	// clear shader so we can tell we don't have any unclosed surfaces
	tess.numIndexes = 0;

//MODVIEWREM	GLimp_LogComment( "----------\n" );
}


/*
==============
We must set some things up before beginning any tesselation,
because a surface may be forced to perform a RB_End due
to overflow.
==============
*/
void RB_BeginSurface( shader_t *shader, int fogNum, GLuint gluiSkinOverrideBind ) {
	tess.numIndexes = 0;
	tess.numVertexes = 0;
	tess.shader = shader;
	tess.gluiTextureBind = gluiSkinOverrideBind; // non-zero means skin overrides first stage texture
	tess.bSurfaceIsG2Tag = false;

	// Shader animation time - pauses/resumes with the shader animation toggle,
	// independent of model animation. Tracks accumulated play time only.
	static DWORD lastTickWhenPlaying = 0;

	DWORD now = GetTickCount();
	if (AppVars.bShaderAnimation) {
		if (lastTickWhenPlaying != 0) {
			g_fAccumulatedShaderTime += (float)(now - lastTickWhenPlaying) / 1000.0f;
		}
		lastTickWhenPlaying = now;
	} else {
		lastTickWhenPlaying = 0;	// reset so next play doesn't jump
	}
	tess.shaderTime = g_fAccumulatedShaderTime;
}



void RB_CheckOverflow( int verts, int indexes ) {
	if (tess.numVertexes + verts < ACTUAL_SHADER_MAX_VERTEXES
		&& tess.numIndexes + indexes < ACTUAL_SHADER_MAX_INDEXES) {
		return;
	}

	RB_EndSurface();

	if ( verts >= ACTUAL_SHADER_MAX_VERTEXES ) {
		ri.Error(ERR_DROP, "RB_CheckOverflow: verts > MAX (%d > %d)", verts, ACTUAL_SHADER_MAX_VERTEXES );
	}
	if ( indexes >= ACTUAL_SHADER_MAX_INDEXES ) {
		ri.Error(ERR_DROP, "RB_CheckOverflow: indices > MAX (%d > %d)", indexes, ACTUAL_SHADER_MAX_INDEXES );
	}

	RB_BeginSurface(tess.shader, 0, tess.gluiTextureBind );
}



///////////////////////////////////////////////////
//
// 2 hack functions for ModView to emulate cgame ent-adding...
//
void R_ModView_BeginEntityAdd()
{
	tr.refdef.num_entities = 0;
	tr.refdef.numDrawSurfs = 0;
}

void R_ModView_AddEntity(ModelHandle_t hModel,			int iFrame_Primary, int iOldFrame_Primary, 
							int iBoneNum_SecondaryStart,int iFrame_Secondary, int iOldFrame_Secondary, 
							int iSurfaceNum_RootOverride,
							float fLerp,
					 		surfaceInfo_t *slist,			// pointer to list of surfaces turned off
							boneInfo_t	*blist,				// pointer to list of bones to be overriden
							mdxaBone_t	*pXFormedG2Bones,		// feedback array for after model has rendered
							bool		*pXFormedG2BonesValid,	// and a validity check because of deactivated bones
							mdxaBone_t	*pXFormedG2TagSurfs,		// feedback array for after model has rendered
							bool		*pXFormedG2TagSurfsValid,	// and a validity check because of deactivated surfs
							//
							int *piRenderedTris,
							int *piRenderedVerts,
							int *piRenderedSurfs,
							int *piXformedG2Bones,
//							int	*piRenderedBoneWeightsThisSurface,
							int *piRenderedBoneWeights,
							int *piOmittedBoneWeights
						 )
{
	trRefEntity_t trHackJob;

	// general params...
	//
	trHackJob.e.hModel				=	hModel;
	trHackJob.e.iFrame_Primary		=	iFrame_Primary;
	trHackJob.e.iOldFrame_Primary	=	iOldFrame_Primary;
	trHackJob.e.iFrame_Secondary	=	iFrame_Secondary;
	trHackJob.e.iOldFrame_Secondary	=	iOldFrame_Secondary;
	trHackJob.e.iBoneNum_SecondaryStart = iBoneNum_SecondaryStart;
	trHackJob.e.iSurfaceNum_RootOverride= iSurfaceNum_RootOverride;

	trHackJob.e.backlerp			=	fLerp;		// 0.0 = current, 1.0 = old


	// Ghoul2 params... (even though model not nec. g2 format)
	//
	//
	trHackJob.e.slist		=	slist;
	trHackJob.e.blist		=	blist;

	// some other crap to make life simpler...
	//
	trHackJob.e.renderfx				= RF_CAP_FRAMES;
	trHackJob.e.piRenderedTris			= piRenderedTris;
	trHackJob.e.piRenderedVerts			= piRenderedVerts;
	trHackJob.e.piRenderedSurfs			= piRenderedSurfs;
	trHackJob.e.piXformedG2Bones		= piXformedG2Bones;
//	trHackJob.e.piRenderedBoneWeightsThisSurface = piRenderedBoneWeightsThisSurface;
	trHackJob.e.piRenderedBoneWeights	= piRenderedBoneWeights;
	trHackJob.e.piOmittedBoneWeights	= piOmittedBoneWeights;

	trHackJob.e.pXFormedG2Bones			= pXFormedG2Bones;
	trHackJob.e.pXFormedG2BonesValid	= pXFormedG2BonesValid;
	trHackJob.e.pXFormedG2TagSurfs		= pXFormedG2TagSurfs;
	trHackJob.e.pXFormedG2TagSurfsValid	= pXFormedG2TagSurfsValid;

	// Entity color for rgbGen entity shaders
	memcpy(trHackJob.e.shaderRGBA, AppVars.entityRGBA, 4);

	// now add it to the list to be processed...
	//
	tr.refdef.entities[ tr.refdef.num_entities++ ] = trHackJob;
}
//
///////////////////////////////////////////////////


void R_AddEntitySurfaces (void)
{
	trRefEntity_t	*ent;
//MODVIEWREM	shader_t		*shader;

	for ( tr.currentEntityNum = 0; 
	      tr.currentEntityNum < tr.refdef.num_entities; 
		  tr.currentEntityNum++ )
	{
		ent = tr.currentEntity = &tr.refdef.entities[tr.currentEntityNum];

//		ent->needDlights = qfalse;

		// we must set up parts of tr.or for model culling
//MODVIEWREM		R_RotateForEntity( ent, &tr.viewParms, &tr.or );

		tr.currentModel = R_GetModelByHandle( ent->e.hModel );
		if (!tr.currentModel)
		{
			assert(0);
//MODVIEWREM			R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
		}
		else
		{
			switch ( tr.currentModel->type )
			{
			case MOD_MESH:
				R_AddMD3Surfaces( ent );
				break;
			case MOD_MD4:
				R_AddAnimSurfaces( ent );				
				break;
			case MOD_MDXM:
				R_AddGhoulSurfaces( ent);
				break;
			case MOD_BRUSH:
				assert(0);
//				R_AddBrushModelSurfaces( ent );
				break;
			case MOD_BAD:		// null model axis
				assert(0);
/*MODVIEWREM
				if ( (ent->e.renderfx & RF_THIRD_PERSON) && !tr.viewParms.isPortal)
				{
					break;
				}
				shader = R_GetShaderByHandle( ent->e.customShader );
				R_AddDrawSurf( &entitySurface, tr.defaultShader, 0, 0 );
*/
				break;
			default:
				ri.Error( ERR_DROP, "R_AddEntitySurfaces: Bad modeltype" );
				break;
			}
		}
	}
}


// entry point from ModView draw function to setup all surfaces ready for actual render call later
void RE_GenerateDrawSurfs( void ) 
{
/*
	R_AddWorldSurfaces ();

	R_AddPolygonSurfaces();

	// set the projection matrix with the minimum zfar
	// now that we have the world bounded
	// this needs to be done before entities are
	// added, because they use the projection
	// matrix for lod calculation
	R_SetupProjection ();
*/
	R_AddEntitySurfaces ();
}



void RB_NULL( surfaceInfo_t *surf ) 
{
	assert(0);
	ri.Error( ERR_DROP, "RB_NULL() reached" );
}

int giSurfaceVertsDrawn;
int giSurfaceTrisDrawn;
int giRenderedBoneWeightDrawn;
void (*rb_surfaceTable[SF_NUM_SURFACE_TYPES])( void *) = {
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceBad,			// SF_BAD, 
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceSkip,			// SF_SKIP, 
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceFace,			// SF_FACE,
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceGrid,			// SF_GRID,
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceTriangles,	// SF_TRIANGLES,
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfacePolychain,	// SF_POLY,
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceMesh,			// SF_MD3,
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceAnim,			// SF_MD4,
	(void(*)(void*))RB_SurfaceGhoul,		// SF_MDX,
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceFlare,		// SF_FLARE,
(void(*)(void*))RB_NULL,//	(void(*)(void*))RB_SurfaceEntity,		// SF_ENTITY
(void(*)(void*))RB_NULL //	(void(*)(void*))RB_SurfaceDisplayList	// SF_DISPLAY_LIST
};


// findme label: finaldrawpos
//
static int		c_vertexes;		// for seeing how long our average strips are
static int		c_begins;
static void R_DrawStripElements( int numIndexes, const glIndex_t *indexes, void ( APIENTRY *element )(GLint) ) 
{
	int i;
	int last[3] = { -1, -1, -1 };
	qboolean even;

	glBegin( GL_TRIANGLE_STRIP );
	c_begins++;

	if ( numIndexes <= 0 ) {
		return;
	}

	// prime the strip
	element( indexes[0] );
	element( indexes[1] );
	element( indexes[2] );
	c_vertexes += 3;

	last[0] = indexes[0];
	last[1] = indexes[1];
	last[2] = indexes[2];

	even = qfalse;

	for ( i = 3; i < numIndexes; i += 3 )
	{
		// odd numbered triangle in potential strip
		if ( !even )
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( indexes[i+0] == last[2] ) && ( indexes[i+1] == last[1] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;
				assert( indexes[i+2] < tess.numVertexes );
				even = qtrue;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				glEnd();

				glBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );

				c_vertexes += 3;

				even = qfalse;
			}
		}
		else
		{
			// check previous triangle to see if we're continuing a strip
			if ( ( last[2] == indexes[i+1] ) && ( last[0] == indexes[i+0] ) )
			{
				element( indexes[i+2] );
				c_vertexes++;

				even = qfalse;
			}
			// otherwise we're done with this strip so finish it and start
			// a new one
			else
			{
				glEnd();

				glBegin( GL_TRIANGLE_STRIP );
				c_begins++;

				element( indexes[i+0] );
				element( indexes[i+1] );
				element( indexes[i+2] );
				c_vertexes += 3;

				even = qfalse;
			}
		}

		// cache the last three vertices
		last[0] = indexes[i+0];
		last[1] = indexes[i+1];
		last[2] = indexes[i+2];
	}

	glEnd();
}



static void R_DrawElements( int numIndexes, const glIndex_t *indexes ) {
	int		primitives;

	primitives = 1;//MODVIEWREM: r_primitives->integer;
/*MODVIEWREM
	// default is to use triangles if compiled vertex arrays are present
	if ( primitives == 0 ) {
		if ( qglLockArraysEXT ) {
			primitives = 2;
		} else {
			primitives = 1;
		}
	}


	if ( primitives == 2 ) {
		qglDrawElements( GL_TRIANGLES, 
						numIndexes,
						GL_INDEX_TYPE,
						indexes );
		return;
	}
*/
	if ( primitives == 1 ) {
		R_DrawStripElements( numIndexes,  indexes, glArrayElement );
		return;
	}

/*	MODVIEWREM
	if ( primitives == 3 ) {
		R_DrawStripElements( numIndexes,  indexes, R_ArrayElementDiscrete );
		return;
	}
*/
	// anything else will cause no drawing
}


static inline void CrossProduct( const vec3_t v1, const vec3_t v2, vec3_t cross )
{
	cross[0] = (v1[1] * v2[2]) - (v1[2] * v2[1]);
	cross[1] = (v1[2] * v2[0]) - (v1[0] * v2[2]);
	cross[2] = (v1[0] * v2[1]) - (v1[1] * v2[0]);
}


static vec_t VectorNormalize2( const vec3_t v, vec3_t out) {
	float	length, ilength;

	length = v[0]*v[0] + v[1]*v[1] + v[2]*v[2];
	length = sqrt (length);

	if (length)
	{
		ilength = 1/length;
		out[0] = v[0]*ilength;
		out[1] = v[1]*ilength;
		out[2] = v[2]*ilength;
	} else {
		VectorClear( out );
	}
		
	return length;

}

/*
// Fuck this maths shit, it doesn't work
#define real_nclip(x0,y0,x1,y1,x2,y2)   (  (y1-y0)*(x2-x1) - (x1-x0)*(y2-y1) )

static bool MouseOverTri(float x0, float x1, float x2, float y0, float y1, float y2, float mX, float mY)
{
	// Check that winding of all 3 lines of x/y(n) mX,mY is in same direction
	// Don't know winding direction, so use first winding check to pick further winding direction checks
	//
	float a = real_nclip(x0, y0, y1, y1, mX,mY);
	if (a == 0) return true; // p0 exactly on edge
	if (a > 0)
	{
		// all further winding checks should be greater of equal to zero for the point to lie inside the polygon
		//
		if (real_nclip(y1,y1,y2,y2,mX,mY)<0) return false;
		if (real_nclip(y2,y2,y0,y0,mX,mY)<0) return false;
	}
	else
	{
		// all further winding checks should be less than zero for the point to lie inside the polygon
		//
		if (real_nclip(y1,y1,y2,y2,mX,mY)>0) return false;
		if (real_nclip(y2,y2,y0,y0,mX,mY)>0) return false;		
	}

	return true;
}
*/


/*
** RB_IterateStagesGeneric
*/
static void RB_IterateStagesGeneric( shaderCommands_t *input )
{
	shader_t *shader = input->shader;

	bool bUseMultiStage = shader && shader->numStages > 0 && !AppVars.bForceWhite && !AppVars.bWireFrame && AppVars.bShaderRendering
						&& !input->bSurfaceIsG2Tag;  // tag surfaces use simple path (draw controlled by bShowTagSurfaces)

	// --- Multi-stage shader rendering (textured, non-wireframe) ---
	if ( bUseMultiStage )
	{
		// Sync GL state tracking with actual GL state
		extern unsigned int glStateBits;
		glEnable( GL_DEPTH_TEST );
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LEQUAL );
		glDisable( GL_ALPHA_TEST );
		glDisable( GL_BLEND );
		glPolygonMode( GL_FRONT_AND_BACK, GL_FILL );
		glStateBits = GLS_DEPTHMASK_TRUE;

		// Apply shader cull mode
		if ( shader->cullType == CT_TWO_SIDED ) {
			glDisable( GL_CULL_FACE );
		} else {
			glEnable( GL_CULL_FACE );
			glCullFace( shader->cullType == CT_BACK_SIDED ? GL_BACK : GL_FRONT );
		}

		// Compute eye position in model space for per-vertex environment mapping.
		// ModView transform: glTranslate(pos) * glScale(0.05) * glRotateX * glRotateY * glRotateZ
		// Eye at origin in eye space -> model space = Rz^-1 * Ry^-1 * Rx^-1 * (1/scale) * (-translation)
		float eyePos[3];
		{
			// Compute eye position in model space for environment mapping.
			// The modelview matrix M = Translate * Scale(s) * Rotation.
			// True eye in model space = R^T * (-translation / s), distance ~= translation/s.
			// But this puts the eye very far away (1/s = 20x) making env maps too uniform.
			// The game camera is typically ~100 units from the model.
			// To match the game look, we place the eye at the rotation-corrected direction
			// but at a game-like distance from the model center.
			float mv[16];
			glGetFloatv( GL_MODELVIEW_MATRIX, mv );

			// Get direction from model to eye (inverse rotation of translation)
			float sx = sqrtf(mv[0]*mv[0] + mv[1]*mv[1] + mv[2]*mv[2]);
			if (sx < 0.0001f) sx = 0.0001f;
			float invS2 = 1.0f / (sx * sx);
			eyePos[0] = -(mv[0]*mv[12] + mv[1]*mv[13] + mv[2]*mv[14]) * invS2;
			eyePos[1] = -(mv[4]*mv[12] + mv[5]*mv[13] + mv[6]*mv[14]) * invS2;
			eyePos[2] = -(mv[8]*mv[12] + mv[9]*mv[13] + mv[10]*mv[14]) * invS2;

			// Clamp distance to game-like range for env mapping (~128 units)
			float dist = sqrtf(eyePos[0]*eyePos[0] + eyePos[1]*eyePos[1] + eyePos[2]*eyePos[2]);
			if (dist > 128.0f) {
				float rescale = 128.0f / dist;
				eyePos[0] *= rescale;
				eyePos[1] *= rescale;
				eyePos[2] *= rescale;
			}
		}

		// Apply vertex deformations before drawing stages
		if ( shader->numDeforms > 0 ) {
			for ( int d = 0; d < shader->numDeforms; d++ ) {
				deformStage_t *ds = &shader->deforms[d];
				if ( ds->deformation == DEFORM_WAVE ) {
					for ( int v = 0; v < input->numVertexes; v++ ) {
						float off = (input->xyz[v][0] + input->xyz[v][1] + input->xyz[v][2]) * ds->deformationSpread;
						float scale = EvalWaveForm( &ds->deformationWave, input->shaderTime + off / ds->deformationWave.frequency );

						// Simpler: use the WAVEVALUE formula directly
						int index = (int)(( ds->deformationWave.phase + off + input->shaderTime * ds->deformationWave.frequency ) * FUNCTABLE_SIZE) & FUNCTABLE_MASK;
						float *table = NULL;
						switch (ds->deformationWave.func) {
							case GF_SIN:				table = sv_sinTable; break;
							case GF_SQUARE:				table = sv_squareTable; break;
							case GF_TRIANGLE:			table = sv_triangleTable; break;
							case GF_SAWTOOTH:			table = sv_sawToothTable; break;
							case GF_INVERSE_SAWTOOTH:	table = sv_inverseSawToothTable; break;
							default:					table = sv_sinTable; break;
						}
						scale = ds->deformationWave.base + table[index] * ds->deformationWave.amplitude;

						input->xyz[v][0] += input->normal[v][0] * scale;
						input->xyz[v][1] += input->normal[v][1] * scale;
						input->xyz[v][2] += input->normal[v][2] * scale;
					}
				}
				else if ( ds->deformation == DEFORM_MOVE ) {
					int index = (int)(( ds->deformationWave.phase + input->shaderTime * ds->deformationWave.frequency ) * FUNCTABLE_SIZE) & FUNCTABLE_MASK;
					float scale = ds->deformationWave.base + sv_sinTable[index] * ds->deformationWave.amplitude;
					for ( int v = 0; v < input->numVertexes; v++ ) {
						input->xyz[v][0] += ds->moveVector[0] * scale;
						input->xyz[v][1] += ds->moveVector[1] * scale;
						input->xyz[v][2] += ds->moveVector[2] * scale;
					}
				}
			}
			// Re-submit modified vertex data
			glVertexPointer( 3, GL_FLOAT, 16, input->xyz );
		}

		for ( int stage = 0; stage < shader->numStages; stage++ )
		{
			shaderStage_t *pStage = &shader->stages[stage];
			if ( !pStage->active ) continue;
			if ( pStage->bundle[0].isLightmap ) continue;

			// Glow pass: only render glow stages. Normal pass: render everything.
			if ( g_bRenderGlowingObjects && !pStage->glow ) continue;

			// Specular stages need per-vertex alpha computation
			bool bPerVertexColor = false;

			// Set per-stage GL state (blending, depth, alpha test)
			GL_State( pStage->stateBits );

			// Compute base texture coordinates based on tcGen
			bool tcModified = false;
			if ( pStage->bundle[0].tcGen == TCGEN_ENVIRONMENT_MAPPED ) {
				// Per-vertex environment mapping: reflect per-vertex viewer direction off normal
				// (matches game's RB_CalcEnvironmentTexCoords)
				for ( int v = 0; v < input->numVertexes; v++ ) {
					float viewer[3];
					viewer[0] = eyePos[0] - input->xyz[v][0];
					viewer[1] = eyePos[1] - input->xyz[v][1];
					viewer[2] = eyePos[2] - input->xyz[v][2];
					// normalize
					float len = viewer[0]*viewer[0] + viewer[1]*viewer[1] + viewer[2]*viewer[2];
					if (len > 0.00001f) {
						float il = 1.0f / sqrtf(len);
						viewer[0] *= il; viewer[1] *= il; viewer[2] *= il;
					}
					float *n = input->normal[v];
					float d = 2.0f * (n[0]*viewer[0] + n[1]*viewer[1] + n[2]*viewer[2]);
					float reflected[3];
					reflected[0] = n[0] * d - viewer[0];
					reflected[1] = n[1] * d - viewer[1];
					reflected[2] = n[2] * d - viewer[2];
					// Q3 convention: X=forward, Y=left, Z=up
					// env map projects onto YZ plane: S=Y, T=Z
					input->svars.texcoords[0][v][0] = 0.5f + reflected[0] * 0.5f;
					input->svars.texcoords[0][v][1] = 0.5f - reflected[2] * 0.5f;
				}
				// Apply tcMod on top of env-mapped coords
				if ( pStage->bundle[0].numTexMods > 0 ) {
					RB_CalcTexMods( &pStage->bundle[0], input->shaderTime,
						input->svars.texcoords[0], input->svars.texcoords[0], input->numVertexes );
				}
				glTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
				tcModified = true;
			}
			else if ( pStage->bundle[0].numTexMods > 0 ) {
				// Copy base UVs from interleaved array (stride 16: [base,lightmap] per vertex)
				// into packed svars array (stride 8) before applying tcMod
				for ( int v = 0; v < input->numVertexes; v++ ) {
					input->svars.texcoords[0][v][0] = input->texCoords[v][0][0];
					input->svars.texcoords[0][v][1] = input->texCoords[v][0][1];
				}
				RB_CalcTexMods( &pStage->bundle[0], input->shaderTime,
					input->svars.texcoords[0], input->svars.texcoords[0], input->numVertexes );
				glTexCoordPointer( 2, GL_FLOAT, 0, input->svars.texcoords[0] );
				tcModified = true;
			}

			// Bind texture - use shader's own texture, not skin override
			// Skin override only applies when shader is a default (no .shader definition)
			if ( stage == 0 && input->gluiTextureBind && shader->defaultShader ) {
				glBindTexture( GL_TEXTURE_2D, input->gluiTextureBind );
			} else {
				R_BindAnimatedImage( &pStage->bundle[0], input->shaderTime );
			}

			// Compute and apply per-stage vertex color based on rgbGen/alphaGen
			{
				float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;

				byte *entityRGBA = input->pRefEnt ? input->pRefEnt->shaderRGBA : NULL;

				switch ( pStage->rgbGen ) {
					case CGEN_IDENTITY:
					case CGEN_IDENTITY_LIGHTING:
					case CGEN_LIGHTING_DIFFUSE:
						r = g = b = 1.0f;
						break;
					case CGEN_ENTITY:
					case CGEN_LIGHTING_DIFFUSE_ENTITY:	// fullbright: diffuse(1) * entity = entity
						if (entityRGBA) { r = entityRGBA[0]/255.0f; g = entityRGBA[1]/255.0f; b = entityRGBA[2]/255.0f; }
						break;
					case CGEN_ONE_MINUS_ENTITY:
						if (entityRGBA) { r = 1.0f-entityRGBA[0]/255.0f; g = 1.0f-entityRGBA[1]/255.0f; b = 1.0f-entityRGBA[2]/255.0f; }
						break;
					case CGEN_CONST:
						r = pStage->constantColor[0] / 255.0f;
						g = pStage->constantColor[1] / 255.0f;
						b = pStage->constantColor[2] / 255.0f;
						break;
					case CGEN_WAVEFORM: {
						float v = EvalWaveForm( &pStage->rgbWave, input->shaderTime );
						if (v < 0) v = 0; if (v > 1) v = 1;
						r = g = b = v;
						break;
					}
					default:
						break;
				}

				switch ( pStage->alphaGen ) {
					case AGEN_IDENTITY:
						a = 1.0f;
						break;
					case AGEN_ENTITY:
						if (entityRGBA) a = entityRGBA[3] / 255.0f;
						break;
					case AGEN_ONE_MINUS_ENTITY:
						if (entityRGBA) a = 1.0f - entityRGBA[3] / 255.0f;
						break;
					case AGEN_CONST:
						a = pStage->constantColor[3] / 255.0f;
						break;
					case AGEN_WAVEFORM: {
						float v = EvalWaveForm( &pStage->alphaWave, input->shaderTime );
						if (v < 0) v = 0; if (v > 1) v = 1;
						a = v;
						break;
					}
					case AGEN_LIGHTING_SPECULAR: {
						// Per-vertex specular: reflect fixed light off normal, dot with viewer, ^4
						// Light from above-forward in model space (Q3: X=fwd, Y=left, Z=up)
						float lightDir[3] = { 0.29f, 0.29f, 0.91f }; // ~above-forward-left
						byte rb = (byte)(r * 255), gb = (byte)(g * 255), bb = (byte)(b * 255);
						for ( int sv = 0; sv < input->numVertexes; sv++ ) {
							float *n = input->normal[sv];

							// Reflect light off normal
							float d = 2.0f * (n[0]*lightDir[0] + n[1]*lightDir[1] + n[2]*lightDir[2]);
							float ref[3];
							ref[0] = n[0]*d - lightDir[0];
							ref[1] = n[1]*d - lightDir[1];
							ref[2] = n[2]*d - lightDir[2];

							// Viewer direction (from vertex toward eye)
							float vw[3];
							vw[0] = eyePos[0] - input->xyz[sv][0];
							vw[1] = eyePos[1] - input->xyz[sv][1];
							vw[2] = eyePos[2] - input->xyz[sv][2];
							float il = 1.0f / (sqrtf(vw[0]*vw[0] + vw[1]*vw[1] + vw[2]*vw[2]) + 0.0001f);

							// Specular = dot(reflected, viewer)^4
							float l = (ref[0]*vw[0] + ref[1]*vw[1] + ref[2]*vw[2]) * il;
							int spec;
							if (l <= 0) { spec = 0; }
							else { l = l*l; l = l*l; spec = (int)(l * 255); if (spec > 255) spec = 255; }

							input->svars.colors[sv][0] = rb;
							input->svars.colors[sv][1] = gb;
							input->svars.colors[sv][2] = bb;
							input->svars.colors[sv][3] = (byte)spec;
						}
						bPerVertexColor = true;
						break;
					}
					default:
						break;
				}

				if (bPerVertexColor) {
					glEnableClientState( GL_COLOR_ARRAY );
					glColorPointer( 4, GL_UNSIGNED_BYTE, 0, input->svars.colors );
				} else {
					glDisableClientState( GL_COLOR_ARRAY );
					glColor4f( r, g, b, a );
				}
			}

			// Draw this stage
			R_DrawElements( input->numIndexes, input->indexes );

			// Restore texcoord pointer if we modified it
			if ( tcModified ) {
				glTexCoordPointer( 2, GL_FLOAT, 16, input->texCoords );
			}
		}

		// Reset state after multi-stage rendering
		GL_State( GLS_DEFAULT );
		glDisableClientState( GL_COLOR_ARRAY );
		glColor4f( 1, 1, 1, 1 );

		// Restore cull state to ModView defaults
		if (AppVars.bShowPolysAsDoubleSided && !AppVars.bForceWhite)
			glDisable( GL_CULL_FACE );
		else {
			glEnable( GL_CULL_FACE );
			glCullFace( GL_FRONT );
		}

		// Drain any GL errors to prevent driver stalls
		while (glGetError() != GL_NO_ERROR) {}
	}
	else
	{
		// --- Simple single-texture path (force-white, wireframe, or no shader) ---
		if ( AppVars.bForceWhite )
		{
			glBindTexture( GL_TEXTURE_2D, 0 );	// no texture = solid white
		}
		else if ( input->gluiTextureBind )
			glBindTexture( GL_TEXTURE_2D, input->gluiTextureBind );
		else if ( shader && shader->numStages > 0 && shader->stages[0].bundle[0].textures[0] )
			glBindTexture( GL_TEXTURE_2D, shader->stages[0].bundle[0].textures[0] );
	}

	{// note additional loop I put here for overriding polys to be wireframe - Ste.

		glPushAttrib(GL_ENABLE_BIT | GL_POLYGON_BIT);	// preserves GL_CULL_FACE, GL_CULL_FACE_MODE, GL_POLYGON_MODE
		{
			if (!input->bSurfaceIsG2Tag			// don't draw G2 surface tags
				|| AppVars.bShowTagSurfaces		// ... unless you really want to
				)
			{
				bool bSurfaceIsUnshadowable =	AppVars.bShowUnshadowableSurfaces && (input->numVertexes > (SHADER_MAX_VERTEXES/2));
				bool bSurfaceIsHighlighted =	AppVars.bSurfaceHighlight &&
												input->hModel == AppVars.hModelToHighLight &&
												(
													(AppVars.iSurfaceNumToHighlight == iITEMHIGHLIGHT_ALL)
													||
													(AppVars.iSurfaceNumToHighlight == iITEMHIGHLIGHT_ALL_TAGSURFACES && input->bSurfaceIsG2Tag)
													||
													(AppVars.iSurfaceNumToHighlight == input->iSurfaceNum)
												);				
				bool bSpecialCaseHighlightSoNoYellowNumberClash = (AppVars.bVertIndexes && AppVars.bVertWeighting && AppVars.iSurfaceNumToHighlight < 0);

				if (bSurfaceIsHighlighted && !bSpecialCaseHighlightSoNoYellowNumberClash)
					glLineWidth(2);
				else
					glLineWidth(1);
				bool b2PassForWire = AppVars.bWireFrame || (AppVars.bWireFrame && bSurfaceIsHighlighted);

				if (b2PassForWire)
				{
					if (AppVars.bShowPolysAsDoubleSided && !AppVars.bForceWhite)
					{
						glEnable(GL_CULL_FACE);	
					}
				}
		
		//		for (int iPass=0; iPass<(AppVars.bWireFrame?2:1); iPass++)
				for (int iPass=0; iPass<(b2PassForWire?2:1); iPass++)
				{
					if (b2PassForWire)
					{
						if (!iPass)
						{
							glCullFace(GL_BACK);

							if (bSurfaceIsHighlighted && !bSpecialCaseHighlightSoNoYellowNumberClash)
								glColor3f(0.5,0.5,0.0);	// dim yellow
							else
								glColor3f(0.5,0.5,0.5);	// dim white
						}
						else
						{
							glCullFace(GL_FRONT);
							if (bSurfaceIsHighlighted && !bSpecialCaseHighlightSoNoYellowNumberClash)
								glColor3f( 1,1,0);		// yellow
							else
								glColor3f( 1,1,1);		// white
						}
					}
					
					if (!bUseMultiStage) // multi-stage already drew via its own loop
						R_DrawElements( input->numIndexes, input->indexes );	// the standard surface draw code
				}

				if (b2PassForWire)
				{						
					if (AppVars.bShowPolysAsDoubleSided && !AppVars.bForceWhite)
					{
						glDisable(GL_CULL_FACE);
					}
				}


				glLineWidth(1);

				// draw surface-highlights?...  (2 types, so do 2 passes to keep code down)
				//
				if (!AppVars.bWireFrame && bSurfaceIsHighlighted && !bSpecialCaseHighlightSoNoYellowNumberClash)
				{
					// do these 3 in case we're not already in wireframe...
					//
					glDisable(GL_TEXTURE_2D);
					glDisable(GL_BLEND);
					glDisable(GL_LIGHTING);					

					glLineWidth(2);
					glColor3f(1,1,0);	// yellow
					
					if (AppVars.iSurfaceNumToHighlight > 0)
					{					
						SurfaceOnOff_t eOnOff = Model_GLMSurface_GetStatus( input->hModel, input->iSurfaceNum );
						if (eOnOff != SURF_ON)
						{
							// then we must be ON only because of highlighting an OFF surface in the treeview,
							//	so show it dimmer (particualrly if they've just turned it off and wonder why they
							//	can still see it...
							//
							glColor3f(0.5,0.5,0);	// dim yellow
						}
					}

					for (int iVert = 0; iVert<input->numIndexes; iVert+=3)
					{
						glBegin(GL_LINE_LOOP);
						{
							glVertex3fv( input->xyz[input->indexes[iVert+0]] );
							glVertex3fv( input->xyz[input->indexes[iVert+1]] );
							glVertex3fv( input->xyz[input->indexes[iVert+2]] );
						}
						glEnd();
					}
					
					glLineWidth(1);
				}

				// draw unshadowable surfaces...
				//
				if (bSurfaceIsUnshadowable)
				{
					// do these 3 in case we're not already in wireframe...
					//
					glDisable(GL_TEXTURE_2D);
					glDisable(GL_BLEND);
					glDisable(GL_LIGHTING);					

					glLineStipple( 8, 0xAAAA);
					glEnable(GL_LINE_STIPPLE);
					glColor3f(1,0,0);	// red

					if (bSurfaceIsHighlighted)
					{
						glLineWidth(4);	// ... or it won't stand out much over the existing yellow highlights
					}

					for (int iVert = 0; iVert<input->numIndexes; iVert+=3)
					{
						glBegin(GL_LINE_LOOP);
						{
							glVertex3fv( input->xyz[input->indexes[iVert+0]] );
							glVertex3fv( input->xyz[input->indexes[iVert+1]] );
							glVertex3fv( input->xyz[input->indexes[iVert+2]] );
						}
						glEnd();
					}
					
					glDisable(GL_LINE_STIPPLE);

					if (bSurfaceIsHighlighted)
					{
						glLineWidth(1);
					}
				}

				if (AppVars.bCrackHighlight && bSurfaceIsHighlighted)
				{
					extern ModelContainer_t* gpContainerBeingRendered;
					if (gpContainerBeingRendered)	// arrrghhh!!!!
					{
						int iCappedLOD = Model_EnsureGenerated_VertEdgeInfo(gpContainerBeingRendered, AppVars.iLOD);

						SurfaceEdgeVertBools_t &SurfaceEdgeVertBools = gpContainerBeingRendered->SurfaceEdgeInfoPerLOD[iCappedLOD];
						SurfaceEdgeVertBools_t::iterator it = SurfaceEdgeVertBools.find(input->iSurfaceNum);
						if (it != SurfaceEdgeVertBools.end())
						{
							VertIsEdge_t &vrVertIsEdge = (*it).second;

							// highlight the edge verts...
							//
							for (int iIndex=0; iIndex<input->numIndexes; iIndex++)
							{
								int iVert = input->indexes[iIndex];
								if (vrVertIsEdge[iVert])
								{										
									Text_Display("*",input->xyz[iVert],0,255,0);					
								}
							}
						}
					}
				}

/*
				if (1)
				{
					extern int g_iScreenWidth;
					extern int g_iScreenHeight;
					extern int g_iViewAreaMouseX;
					extern int g_iViewAreaMouseY;

					// Header: Declared in Glu.h.
					// Library: Use Glu32.lib.

					GLdouble	modelMatrix[16];
					GLdouble	projMatrix[16];
					GLint		viewPort[4];
					int			iOpenGLMouseX = g_iViewAreaMouseX;
					int			iOpenGLMouseY = (g_iScreenHeight - g_iViewAreaMouseY)-1;

					glGetDoublev	( GL_MODELVIEW_MATRIX,  modelMatrix);
					glGetDoublev	( GL_PROJECTION_MATRIX, projMatrix);
					glGetIntegerv	( GL_VIEWPORT,			viewPort);

					for (int iVert = 0; iVert<input->numIndexes; iVert+=3)
					{						
						GLdouble dX[3],dY[3],dZ[3];

						int iSuccess = 0;
						for (int i=0; i<3; i++)
						{
							iSuccess += gluProject(	input->xyz[input->indexes[iVert+i]][0],	// GLdouble objx,
													input->xyz[input->indexes[iVert+i]][1],	// GLdouble objy,
													input->xyz[input->indexes[iVert+i]][2],	// GLdouble objz,
													modelMatrix,							// const GLdouble modelMatrix[16],
													projMatrix,								// const GLdouble projMatrix[16],
													viewPort,								// const GLint viewport[4],
													&dX[i],&dY[i],&dZ[i]
													);
						}

						if (iSuccess == i)
						{
							// got the 3 vert coords as screen coords, now see if the mouse is within this poly
							//
							if (MouseOverTri(dX[0],dX[1],dX[2],dY[0],dY[1],dY[2], iOpenGLMouseX, iOpenGLMouseY))
							{
								AppVars.iSurfaceNumToHighlight = input->iSurfaceNum;
								OutputDebugString(va("Over surface %d\n",input->iSurfaceNum));
								break;
							}
						}
					}
				}
*/
				// draw normals?...
				//
				if (AppVars.bVertexNormals)
				{
					// do these 3 in case we're doing normals but not wireframe...
					//
					glDisable(GL_TEXTURE_2D);
					glDisable(GL_BLEND);
					glDisable(GL_LIGHTING);

					for (int iNormal = 0; iNormal<input->numVertexes/*numIndexes*/; iNormal++)
					{
						glColor3f(1,0.5,1);	// purple
						glBegin(GL_LINES);
						{
							glVertex3fv(	input->xyz[iNormal] );
							glVertex3f (	input->xyz[iNormal][0] + input->normal[iNormal][0],
											input->xyz[iNormal][1] + input->normal[iNormal][1],
											input->xyz[iNormal][2] + input->normal[iNormal][2]
										);
						}				
						glEnd();				
					}
				}

				// show vertex indexes?...
				//
				if (AppVars.bVertIndexes && bSurfaceIsHighlighted && 
						(
						(AppVars.iSurfaceNumToHighlight != iITEMHIGHLIGHT_ALL || AppVars.bVertWeighting)	// or it drops the framerate through the floor!
						&&
						AppVars.iSurfaceNumToHighlight != iITEMHIGHLIGHT_ALL_TAGSURFACES
						)
					)
				{						
					for (int iVert = 0; iVert<input->numIndexes; iVert++)
					{
						byte r=255,g=0,b=0;	// red

						int iNumWeights = 0;

						if (AppVars.bVertWeighting)
						{
							iNumWeights = input->WeightsUsed[input->indexes[iVert]];						

//							if (gpContainerBeingRendered)
//								gpContainerBeingRendered->iRenderedBoneWeightsThisSurface += iNumWeights;

							GetWeightColour(iNumWeights,r,g,b);

							AppVars.bAtleast1VertWeightDisplayed = true;
						}

						if (AppVars.iSurfaceNumToHighlight != iITEMHIGHLIGHT_ALL
							|| iNumWeights>=3
							)
						{
							Text_Display(va(" %d",input->indexes[iVert]),input->xyz[input->indexes[iVert]],r,g,b);					
						}
					}
				}

				// show triangle indexes?...
				//
				if (AppVars.bTriIndexes && bSurfaceIsHighlighted && 
						(
						(AppVars.iSurfaceNumToHighlight != iITEMHIGHLIGHT_ALL)	// or it drops the framerate through the floor!
						&&
						AppVars.iSurfaceNumToHighlight != iITEMHIGHLIGHT_ALL_TAGSURFACES
						)
					)
				{
					for (int iTri = 0; iTri<input->numIndexes; iTri+=3)	// iTri is effectively like stepval 3 for vert parsing
					{
						byte r=0,g=255,b=255;	// magenta

						vec3_t v3TriCentre;

						v3TriCentre[0] =	(
											input->xyz[input->indexes[iTri+0]][0] +
											input->xyz[input->indexes[iTri+1]][0] +
											input->xyz[input->indexes[iTri+2]][0]
											)/3;

						v3TriCentre[1] =	(
											input->xyz[input->indexes[iTri+0]][1] +
											input->xyz[input->indexes[iTri+1]][1] +
											input->xyz[input->indexes[iTri+2]][1]
											)/3;

						v3TriCentre[2] =	(
											input->xyz[input->indexes[iTri+0]][2] +
											input->xyz[input->indexes[iTri+1]][2] +
											input->xyz[input->indexes[iTri+2]][2]
											)/3;

						Text_Display(va("T:%d",iTri/3), v3TriCentre ,r,g,b);					
					}
				}	

				// show vertexes with omitted bone-weights (threshholding)?...
				//
				if (AppVars.bBoneWeightThreshholdingActive && AppVars.bWireFrame)
				{
//					glDisable(GL_TEXTURE_2D);
//					glDisable(GL_BLEND);
//					glDisable(GL_LIGHTING);

//					glLineWidth(9);
					{
//						glColor3f(0,1,0);	// green

//						glBegin(GL_POINTS);
						{
							for (int iVert=0; iVert<input->numIndexes; iVert++)
							{
								if (input->WeightsOmitted[input->indexes[iVert]])
								{										
									Text_Display("*",input->xyz[input->indexes[iVert]],0,255,0);					
								}
							}
						}
//						glEnd();
					}
//					glLineWidth(1);
				}
			}

			// if this is a G2 tag surface, then work out a matrix from it and store for later use...
			//
			if (input->bSurfaceIsG2Tag)
			{
				// not a clever place to do this, but WTF...
				//
				// Anyway, this is some of Jake's mysterious code to turn a one-triangle tag-surface into a matrix...
				//
				vec3_t			axes[3], sides[3];
				float			pTri[3][3], d;

				memcpy(pTri[0],input->xyz[0],sizeof(vec3_t));
				memcpy(pTri[1],input->xyz[1],sizeof(vec3_t));
				memcpy(pTri[2],input->xyz[2],sizeof(vec3_t));

 				// clear out used arrays
 				memset( axes, 0, sizeof( axes ) );
 				memset( sides, 0, sizeof( sides ) );

 				// work out actual sides of the tag triangle
 				for ( int j = 0; j < 3; j++ )
 				{
 					sides[j][0] = pTri[(j+1)%3][0] - pTri[j][0];
 					sides[j][1] = pTri[(j+1)%3][1] - pTri[j][1];
 					sides[j][2] = pTri[(j+1)%3][2] - pTri[j][2];
 				}

 				// do math trig to work out what the matrix will be from this triangle's translated position
 				VectorNormalize2( sides[iG2_TRISIDE_LONGEST], axes[0] );
 				VectorNormalize2( sides[iG2_TRISIDE_SHORTEST], axes[1] );

 				// project shortest side so that it is exactly 90 degrees to the longer side
 				d = DotProduct( axes[0], axes[1] );
 				VectorMA( axes[0], -d, axes[1], axes[0] );
 				VectorNormalize2( axes[0], axes[0] );

 				CrossProduct( sides[iG2_TRISIDE_LONGEST], sides[iG2_TRISIDE_SHORTEST], axes[2] );
 				VectorNormalize2( axes[2], axes[2] );

				//float Jmatrix[3][4];
				mdxaBone_t Jmatrix;

				#define MDX_TAG_ORIGIN 2

 				// set up location in world space of the origin point in out going matrix
 				Jmatrix.matrix[0][3] = pTri[MDX_TAG_ORIGIN][0];
 				Jmatrix.matrix[1][3] = pTri[MDX_TAG_ORIGIN][1];
 				Jmatrix.matrix[2][3] = pTri[MDX_TAG_ORIGIN][2];

 				// copy axis to matrix - do some magic to orient minus Y to positive X and so on so bolt on stuff is oriented correctly
				Jmatrix.matrix[0][0] = axes[1][0];
				Jmatrix.matrix[0][1] = axes[0][0];
				Jmatrix.matrix[0][2] = -axes[2][0];

				Jmatrix.matrix[1][0] = axes[1][1];
				Jmatrix.matrix[1][1] = axes[0][1];
				Jmatrix.matrix[1][2] = -axes[2][1];

				Jmatrix.matrix[2][0] = axes[1][2];
				Jmatrix.matrix[2][1] = axes[0][2];
				Jmatrix.matrix[2][2] = -axes[2][2];				

				input->pRefEnt->pXFormedG2TagSurfs		[input->iSurfaceNum] = Jmatrix;
				input->pRefEnt->pXFormedG2TagSurfsValid	[input->iSurfaceNum] = true;

				{
					static int logCount = 0;
					if (logCount < 50) {
						FILE *f = fopen("C:\\Projects\\ModView\\shader_log.txt", logCount == 0 ? "w" : "a");
						if (f) {
							LPCSTR name = Model_GetSurfaceName(input->hModel, input->iSurfaceNum);
							fprintf(f, "Tag surf %d '%s' origin=(%.1f,%.1f,%.1f)\n",
								input->iSurfaceNum, name ? name : "?",
								Jmatrix.matrix[0][3], Jmatrix.matrix[1][3], Jmatrix.matrix[2][3]);
							fclose(f);
						}
						logCount++;
					}
				}
			}
		}
		glPopAttrib();
		glColor3f( 1,1,1);		
	}
}




void RB_StageIteratorGeneric( void )
{
	shaderCommands_t *input;

	input = &tess;
/* MODVIEWREM
	RB_DeformTessGeometry();

	//
	// log this call
	//
	if ( r_logFile->integer ) 
	{
		// don't just call LogComment, or we will get
		// a call to va() every frame!
		GLimp_LogComment( va("--- RB_StageIteratorGeneric( %s ) ---\n", tess.shader->name) );
	}

	//
	// set face culling appropriately
	//
	GL_Cull( input->shader->cullType );

	// set polygon offset if necessary
	if ( input->shader->polygonOffset )
	{
		qglEnable( GL_POLYGON_OFFSET_FILL );
		qglPolygonOffset( r_offsetFactor->value, r_offsetUnits->value );
	}
*/

/*	MODVIEWREM
	//
	// if there is only a single pass then we can enable color
	// and texture arrays before we compile, otherwise we need
	// to avoid compiling those arrays since they will change
	// during multipass rendering
	//
	if ( tess.numPasses > 1 || input->shader->multitextureEnv )
	{
		setArraysOnce = qfalse;
		qglDisableClientState (GL_COLOR_ARRAY);
		qglDisableClientState (GL_TEXTURE_COORD_ARRAY);
	}
	else
*/
	{
//		setArraysOnce = qtrue;

//MODVIEWREM		glEnableClientState( GL_COLOR_ARRAY);
//		glColorPointer( 4, GL_UNSIGNED_BYTE, 0, tess.svars.colors );

		if (!AppVars.bWireFrame)
		{
			glEnableClientState( GL_TEXTURE_COORD_ARRAY);
			glTexCoordPointer( 2, GL_FLOAT, 16, input->texCoords	);//tess.svars.texcoords[0] );
		}
	}

	//
	// lock XYZ
	//
	glVertexPointer (3, GL_FLOAT, 16, input->xyz);	// padded for SIMD
/*MODVIEWREM
	if (qglLockArraysEXT)
	{
		qglLockArraysEXT(0, input->numVertexes);
		GLimp_LogComment( "glLockArraysEXT\n" );
	}
*/

/*	MODVUEWREM
	//
	// enable color and texcoord arrays after the lock if necessary
	//
	if ( !setArraysOnce )
	{
		glEnableClientState( GL_TEXTURE_COORD_ARRAY );
		glEnableClientState( GL_COLOR_ARRAY );
	}
*/
	//
	// call shader function
	//
	glEnableClientState( GL_VERTEX_ARRAY );
	RB_IterateStagesGeneric( input );

/*	MODVIEWREM
	// 
	// now do any dynamic lighting needed
	//
	if ( tess.dlightBits && tess.shader->sort <= SS_OPAQUE
		&& !(tess.shader->surfaceFlags & (SURF_NODLIGHT | SURF_SKY) ) ) {
		ProjectDlightTexture();
	}

	//
	// now do fog
	//
	if ( tess.fogNum && tess.shader->fogPass ) {
		RB_FogPass();
	}

	// 
	// unlock arrays
	//
	if (qglUnlockArraysEXT) 
	{
		qglUnlockArraysEXT();
		GLimp_LogComment( "glUnlockArraysEXT\n" );
	}

	//
	// reset polygon offset
	//
	if ( input->shader->polygonOffset )
	{
		qglDisable( GL_POLYGON_OFFSET_FILL );
	}
*/
}


/*
=================
Generates an orientation for an entity and viewParms
Does NOT produce any GL calls
Called by both the front end and the back end
=================
*/
/*
void R_RotateForEntity( const trRefEntity_t *ent, const viewParms_t *viewParms,
					   orientationr_t *or ) {
	float	glMatrix[16];
	vec3_t	delta;
	float	axisLength;

	if ( ent->e.reType != RT_MODEL ) {
		*or = viewParms->world;
		return;
	}

	VectorCopy( ent->e.origin, or->origin );

	VectorCopy( ent->e.axis[0], or->axis[0] );
	VectorCopy( ent->e.axis[1], or->axis[1] );
	VectorCopy( ent->e.axis[2], or->axis[2] );

	glMatrix[0] = or->axis[0][0];
	glMatrix[4] = or->axis[1][0];
	glMatrix[8] = or->axis[2][0];
	glMatrix[12] = or->origin[0];

	glMatrix[1] = or->axis[0][1];
	glMatrix[5] = or->axis[1][1];
	glMatrix[9] = or->axis[2][1];
	glMatrix[13] = or->origin[1];

	glMatrix[2] = or->axis[0][2];
	glMatrix[6] = or->axis[1][2];
	glMatrix[10] = or->axis[2][2];
	glMatrix[14] = or->origin[2];

	glMatrix[3] = 0;
	glMatrix[7] = 0;
	glMatrix[11] = 0;
	glMatrix[15] = 1;

	myGlMultMatrix( glMatrix, viewParms->world.modelMatrix, or->modelMatrix );

	// calculate the viewer origin in the model's space
	// needed for fog, specular, and environment mapping
	VectorSubtract( viewParms->or.origin, or->origin, delta );

	// compensate for scale in the axes if necessary
	if ( ent->e.nonNormalizedAxes ) {
		axisLength = VectorLength( ent->e.axis[0] );
		if ( !axisLength ) {
			axisLength = 0;
		} else {
			axisLength = 1.0 / axisLength;
		}
	} else {
		axisLength = 1.0;
	}

	or->viewOrigin[0] = DotProduct( delta, or->axis[0] ) * axisLength;
	or->viewOrigin[1] = DotProduct( delta, or->axis[1] ) * axisLength;
	or->viewOrigin[2] = DotProduct( delta, or->axis[2] ) * axisLength;
}
*/




void RB_RenderDrawSurfList( drawSurf_t *drawSurfs, int numDrawSurfs ) 
{
	shader_t		/**shader,*/ *oldShader;
	int				/*fogNum,*/ oldFogNum;
	int				entityNum, oldEntityNum;
	int				/*dlighted,*/ oldDlighted;
	qboolean		depthRange, oldDepthRange;
	int				i;
	drawSurf_t		*drawSurf;
	int				oldSort;
//	float			originalTime;

/*MODVIEWREM
	// save original time for entity shader offsets
	originalTime = backEnd.refdef.floatTime;

	// clear the z buffer, set the modelview, etc
	RB_BeginDrawingView ();
*/
	// draw everything
	oldEntityNum = -1;
//MODVIEWREM	backEnd.currentEntity = &tr.worldEntity;
	oldShader = NULL;
	oldFogNum = -1;
	oldDepthRange = qfalse;
	oldDlighted = qfalse;
	oldSort = -1;
	depthRange = qfalse;

//MODVIEWREM	backEnd.pc.c_surfaces += numDrawSurfs;

	for (i = 0, drawSurf = drawSurfs ; i < numDrawSurfs ; i++, drawSurf++) 
	{
/*MODVIEWREM
		if ( drawSurf->sort == oldSort ) {
			// fast path, same as previous sort
			rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );
			continue;
		}
		oldSort = drawSurf->sort;
*/
		int shaderIndex = 0;
		R_DecomposeSort( drawSurf->sort, &entityNum, &shaderIndex );

		shader_t *sh = R_GetShaderByIndex( shaderIndex );

		// During glow pass, skip non-glow surfaces entirely (no callbacks, no state changes)
		if (g_bRenderGlowingObjects && (!sh || !sh->hasGlow))
			continue;

				RB_BeginSurface( sh, 0, drawSurf->skinOverrideBind );

				tess.hModel = tr.refdef.entities[entityNum].e.hModel;
				tess.pRefEnt=&tr.refdef.entities[entityNum].e;

		// add the triangles for this surface
		rb_surfaceTable[ *drawSurf->surface ]( drawSurf->surface );


				RB_EndSurface();

				// stats...
				//
				if (!tess.bSurfaceIsG2Tag || AppVars.bShowTagSurfaces)
				{	
					*tr.refdef.entities[entityNum].e.piRenderedTris += giSurfaceTrisDrawn;
					*tr.refdef.entities[entityNum].e.piRenderedVerts+= giSurfaceVertsDrawn;
					*tr.refdef.entities[entityNum].e.piRenderedSurfs+= 1;		// NOT ++!
					*tr.refdef.entities[entityNum].e.piRenderedBoneWeights += giRenderedBoneWeights;
					*tr.refdef.entities[entityNum].e.piOmittedBoneWeights  += giOmittedBoneWeights;
				}
	}
/*MODVIEWREM
	// draw the contents of the last shader batch
	if (oldShader != NULL) {
		RB_EndSurface();
	}

	// go back to the world modelview matrix
	qglLoadMatrixf( backEnd.viewParms.world.modelMatrix );
	if ( depthRange ) {
		qglDepthRange (0, 1);
	}
*/
}



// entry point from ModView draw function to setup all surfaces ready for actual render call later...
//
void RE_RenderDrawSurfs( void )
{
	RB_RenderDrawSurfList( tr.refdef.drawSurfs, tr.refdef.numDrawSurfs );
}


// =============================================================================
// Lightsaber Blade Rendering
// =============================================================================

void RE_DrawSaberBlades( bool bGlowOnly )
{
	// Check if any blades are enabled
	if (!AppVars.bSaberBlade[0] && !AppVars.bSaberBlade[1]) return;

	// For each hand that has a blade on, find the bolted saber model's *blade1 tag
	// and draw the blade from there
	ModelHandle_t hPrimary = Model_GetPrimaryHandle();
	if (!hPrimary) return;

	for (int hand = 0; hand < 2; hand++)
	{
		if (!AppVars.bSaberBlade[hand]) continue;

		// Find the tag surface for this hand on the primary model
		LPCSTR psHandTag = (hand == 0) ? "*r_hand" : "*l_hand";
		int iHandBolt = Model_GetBoltIndex(hPrimary, psHandTag, false);
		if (iHandBolt == -1) continue;

		// Check if something is bolted there
		ModelContainer_t *pContainer = ModelContainer_FindFromModelHandle(hPrimary);
		if (!pContainer) continue;
		if (iHandBolt >= (int)pContainer->tSurfaceBolt_BoltPoints.size()) continue;
		if (pContainer->tSurfaceBolt_BoltPoints[iHandBolt].vBoltedContainers.empty()) continue;

		// Get the bolted saber model
		ModelContainer_t *pSaber = &pContainer->tSurfaceBolt_BoltPoints[iHandBolt].vBoltedContainers[0];

		mdxaBone_t &handMat = pContainer->XFormedG2TagSurfs[iHandBolt];

		// Iterate all blade bolts on the saber (*blade1, *blade2, ... or *flash as fallback)
		// Game: "Assume bladeNum is equal to the bolt index because bolts should be added in order"
		for (int bladeNum = 0; bladeNum < 8; bladeNum++)
		{
		char bladeBoltName[32];
		sprintf(bladeBoltName, "*blade%d", bladeNum + 1);
		int iBladeBolt = Model_GetBoltIndex(pSaber->hModel, bladeBoltName, false, true);
		if (iBladeBolt == -1) {
			if (bladeNum == 0) {
				iBladeBolt = Model_GetBoltIndex(pSaber->hModel, "*flash", false, true);
			}
			if (iBladeBolt == -1) break;  // no more blades
		}

		if (!pSaber->XFormedG2TagSurfsValid[iBladeBolt]) continue;

		mdxaBone_t &bladeMat = pSaber->XFormedG2TagSurfs[iBladeBolt];

		// The blade matrix is in saber-local space. Transform origin and direction
		// to scene space using the parent's hand bolt matrix.
		float bladeLocalOrigin[3], bladeLocalDir[3];
		bladeLocalOrigin[0] = bladeMat.matrix[0][3];
		bladeLocalOrigin[1] = bladeMat.matrix[1][3];
		bladeLocalOrigin[2] = bladeMat.matrix[2][3];
		// Direction from the blade bolt matrix: NEGATIVE column 0
		// col0 points down (-Z in saber space), so -col0 points up along the blade
		bladeLocalDir[0] = -bladeMat.matrix[0][0];
		bladeLocalDir[1] = -bladeMat.matrix[1][0];
		bladeLocalDir[2] = -bladeMat.matrix[2][0];

		// Transform origin: scene_origin = handMat * bladeLocalOrigin (rotate + translate)
		float origin[3];
		origin[0] = handMat.matrix[0][0]*bladeLocalOrigin[0] + handMat.matrix[0][1]*bladeLocalOrigin[1] + handMat.matrix[0][2]*bladeLocalOrigin[2] + handMat.matrix[0][3];
		origin[1] = handMat.matrix[1][0]*bladeLocalOrigin[0] + handMat.matrix[1][1]*bladeLocalOrigin[1] + handMat.matrix[1][2]*bladeLocalOrigin[2] + handMat.matrix[1][3];
		origin[2] = handMat.matrix[2][0]*bladeLocalOrigin[0] + handMat.matrix[2][1]*bladeLocalOrigin[1] + handMat.matrix[2][2]*bladeLocalOrigin[2] + handMat.matrix[2][3];

		// Transform direction: scene_dir = handMat * bladeLocalDir (rotate only, no translate)
		float dir[3];
		dir[0] = handMat.matrix[0][0]*bladeLocalDir[0] + handMat.matrix[0][1]*bladeLocalDir[1] + handMat.matrix[0][2]*bladeLocalDir[2];
		dir[1] = handMat.matrix[1][0]*bladeLocalDir[0] + handMat.matrix[1][1]*bladeLocalDir[1] + handMat.matrix[1][2]*bladeLocalDir[2];
		dir[2] = handMat.matrix[2][0]*bladeLocalDir[0] + handMat.matrix[2][1]*bladeLocalDir[1] + handMat.matrix[2][2]*bladeLocalDir[2];

		float bladeLen = AppVars.saberLength;
		float bladeRadius = 3.0f;  // world units, matches game default

		// Compute end point
		float end[3];
		end[0] = origin[0] + dir[0] * bladeLen;
		end[1] = origin[1] + dir[1] * bladeLen;
		end[2] = origin[2] + dir[2] * bladeLen;

		// Blade flicker - game uses Q_flrand per frame for glow radius jitter
		extern float g_fAccumulatedShaderTime;
		float radiusRange = bladeRadius * 0.075f;
		float radiusStart = bladeRadius - radiusRange;
		// Per-frame random jitter (pseudo-random from time, different per hand)
		float jitter = 0.0f;
		if (AppVars.bShaderAnimation) {
			unsigned int seed = (unsigned int)(g_fAccumulatedShaderTime * 1000.0f) + hand * 7919 + bladeNum * 3571;
			seed = (seed * 1103515245 + 12345);  // simple LCG
			jitter = ((seed >> 16) & 0x7FFF) / (float)0x7FFF * 2.0f - 1.0f;  // -1 to 1
		}
		float glowRadiusMult = 1.0f;
		float saberGlowRadius = (radiusStart + jitter * radiusRange) * glowRadiusMult;

		// Core pulse: game uses abs(sin(time/400)) * 0.1 + 0.8
		float corePulse = 1.0f;
		if (AppVars.bShaderAnimation) {
			corePulse = fabsf(sinf(g_fAccumulatedShaderTime * (float)M_PI / 0.4f)) * 0.1f + 0.8f;
		}

		// Get camera axes in model space for billboarding
		// Must normalize since the modelview includes the 0.05 display scale
		float mv[16];
		glGetFloatv(GL_MODELVIEW_MATRIX, mv);
		float camRight[3] = { mv[0], mv[4], mv[8] };
		float camUp[3]    = { mv[1], mv[5], mv[9] };
		float rNorm = sqrtf(camRight[0]*camRight[0]+camRight[1]*camRight[1]+camRight[2]*camRight[2]);
		float uNorm = sqrtf(camUp[0]*camUp[0]+camUp[1]*camUp[1]+camUp[2]*camUp[2]);
		if (rNorm > 0.001f) { camRight[0]/=rNorm; camRight[1]/=rNorm; camRight[2]/=rNorm; }
		if (uNorm > 0.001f) { camUp[0]/=uNorm; camUp[1]/=uNorm; camUp[2]/=uNorm; }

		// Compute "right" for the core quad (perpendicular to blade and view)
		float v1[3] = { origin[0] - camRight[0]*100, origin[1] - camRight[1]*100, origin[2] - camRight[2]*100 };
		float v2[3] = { end[0] - camRight[0]*100, end[1] - camRight[1]*100, end[2] - camRight[2]*100 };
		// cross(origin_to_view, end_to_view) like the game does
		float viewToStart[3] = { -mv[2]-origin[0]*mv[0]-origin[1]*mv[4]-origin[2]*mv[8],
		                          -mv[6]-origin[0]*mv[1]-origin[1]*mv[5]-origin[2]*mv[9],
		                          -mv[10]-origin[0]*mv[2]-origin[1]*mv[6]-origin[2]*mv[10] };
		float viewToEnd[3] = { -mv[2]-end[0]*mv[0]-end[1]*mv[4]-end[2]*mv[8],
		                        -mv[6]-end[0]*mv[1]-end[1]*mv[5]-end[2]*mv[9],
		                        -mv[10]-end[0]*mv[2]-end[1]*mv[6]-end[2]*mv[10] };
		// Simpler: just cross blade direction with camera forward
		float camFwd[3] = { mv[2], mv[6], mv[10] };
		float coreRight[3];
		coreRight[0] = dir[1]*camFwd[2] - dir[2]*camFwd[1];
		coreRight[1] = dir[2]*camFwd[0] - dir[0]*camFwd[2];
		coreRight[2] = dir[0]*camFwd[1] - dir[1]*camFwd[0];
		float crLen = sqrtf(coreRight[0]*coreRight[0]+coreRight[1]*coreRight[1]+coreRight[2]*coreRight[2]);
		if (crLen > 0.001f) { coreRight[0]/=crLen; coreRight[1]/=crLen; coreRight[2]/=crLen; }

		// Determine saber color
		int colorIdx = AppVars.saberColorIndex[hand];
		if (colorIdx < 0 || colorIdx > 6) colorIdx = 0;

		// For preset colors, the texture is pre-colored so tint white.
		// For custom (index 6), the blend texture is neutral so tint with custom RGB.
		byte colR, colG, colB;
		if (colorIdx == 6) {
			colR = AppVars.saberCustomColor[hand][0];
			colG = AppVars.saberCustomColor[hand][1];
			colB = AppVars.saberCustomColor[hand][2];
		} else {
			colR = colG = colB = 255;
		}

		// Load saber textures per color (once)
		// 0=blue,1=green,2=yellow,3=orange,4=red,5=purple,6=custom(blend)
		struct SaberTexSet { GLuint glow; GLuint core; };
		static SaberTexSet saberTexSets[7] = {{0}};
		static bool saberTexLoaded = false;
		if (!saberTexLoaded) {
			static const char *colorNames[] = { "blue","green","yellow","orange","red","purple","blend" };
			for (int c = 0; c < 7; c++) {
				int h = Texture_LoadDirect(va("gfx/effects/sabers/%s_glow2", colorNames[c]));
				if (h > 0) saberTexSets[c].glow = Texture_GetGLBind(h);
				h = Texture_LoadDirect(va("gfx/effects/sabers/%s_line", colorNames[c]));
				if (h > 0) saberTexSets[c].core = Texture_GetGLBind(h);
			}
			saberTexLoaded = true;
		}

		GLuint glowTex = saberTexSets[colorIdx].glow;
		GLuint coreTex = saberTexSets[colorIdx].core;

		// Save GL state
		glPushAttrib(GL_ALL_ATTRIB_BITS);
		glEnable(GL_TEXTURE_2D);
		glDisable(GL_CULL_FACE);
		glEnable(GL_BLEND);
		glEnable(GL_DEPTH_TEST);
		glDepthMask(GL_FALSE);
		glBlendFunc(GL_ONE, GL_ONE);

		// ---- Glow layer: textured overlapping sprites along the blade ----
		if (glowTex) glBindTexture(GL_TEXTURE_2D, glowTex);
		glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

		float glowRadius = saberGlowRadius;
		float glowStep = glowRadius * 0.65f;
		if (glowStep < 0.5f) glowStep = 0.5f;
		glColor4f(colR/255.0f, colG/255.0f, colB/255.0f, 1.0f);

		for (float dist = bladeLen; dist > 0; dist -= glowStep) {
			float pos[3];
			pos[0] = origin[0] + dir[0] * dist;
			pos[1] = origin[1] + dir[1] * dist;
			pos[2] = origin[2] + dir[2] * dist;

			float r = glowRadius;
			float lx = camRight[0]*r, ly = camRight[1]*r, lz = camRight[2]*r;
			float ux = camUp[0]*r, uy = camUp[1]*r, uz = camUp[2]*r;

			glBegin(GL_QUADS);
				glTexCoord2f(0,0); glVertex3f(pos[0]-lx-ux, pos[1]-ly-uy, pos[2]-lz-uz);
				glTexCoord2f(1,0); glVertex3f(pos[0]+lx-ux, pos[1]+ly-uy, pos[2]+lz-uz);
				glTexCoord2f(1,1); glVertex3f(pos[0]+lx+ux, pos[1]+ly+uy, pos[2]+lz+uz);
				glTexCoord2f(0,1); glVertex3f(pos[0]-lx+ux, pos[1]-ly+uy, pos[2]-lz+uz);
			glEnd();

			glowRadius += 0.017f;
		}

		// Hilt glow sprite
		{
			float r = 5.5f + jitter * 0.25f;  // game: 5.5f + Q_flrand(0,1)*0.25f
			float lx = camRight[0]*r, ly = camRight[1]*r, lz = camRight[2]*r;
			float ux = camUp[0]*r, uy = camUp[1]*r, uz = camUp[2]*r;
			glBegin(GL_QUADS);
				glTexCoord2f(0,0); glVertex3f(origin[0]-lx-ux, origin[1]-ly-uy, origin[2]-lz-uz);
				glTexCoord2f(1,0); glVertex3f(origin[0]+lx-ux, origin[1]+ly-uy, origin[2]+lz-uz);
				glTexCoord2f(1,1); glVertex3f(origin[0]+lx+ux, origin[1]+ly+uy, origin[2]+lz+uz);
				glTexCoord2f(0,1); glVertex3f(origin[0]-lx+ux, origin[1]-ly+uy, origin[2]-lz+uz);
			glEnd();
		}

		// ---- Core layer: textured billboard quad from hilt to tip ----
		// Game's core shader (*_line) has no 'glow' keyword, so skip in glow-only mode
		if (!bGlowOnly) {
			if (coreTex) glBindTexture(GL_TEXTURE_2D, coreTex);
			float coreRadius = (bladeRadius / 3.0f + jitter * radiusRange) * corePulse;
			glColor4f(0.8f + colR/1275.0f, 0.8f + colG/1275.0f, 0.8f + colB/1275.0f, 1.0f);
			float hw = coreRadius;
			glBegin(GL_QUADS);
				glTexCoord2f(0,1); glVertex3f(origin[0]-coreRight[0]*hw, origin[1]-coreRight[1]*hw, origin[2]-coreRight[2]*hw);
				glTexCoord2f(1,1); glVertex3f(origin[0]+coreRight[0]*hw, origin[1]+coreRight[1]*hw, origin[2]+coreRight[2]*hw);
				glTexCoord2f(1,0); glVertex3f(end[0]+coreRight[0]*hw, end[1]+coreRight[1]*hw, end[2]+coreRight[2]*hw);
				glTexCoord2f(0,0); glVertex3f(end[0]-coreRight[0]*hw, end[1]-coreRight[1]*hw, end[2]-coreRight[2]*hw);
			glEnd();
		}

		// Restore GL state
		glPopAttrib();

		} // end bladeNum loop
	}

	// Reset glStateBits so it matches the actual GL state after popAttrib
	extern unsigned int glStateBits;
	glStateBits = 0;
}


// =============================================================================
// Dynamic Glow Post-Process
//
// Pipeline: copy scene → render glow-only → copy glow → blur → composite
// Uses glCopyTexSubImage2D (no FBO/shaders required)
// =============================================================================

static GLuint g_glowSceneTexture = 0;
static GLuint g_glowBlurTexture = 0;
static int g_glowTexWidth = 0;
static int g_glowTexHeight = 0;

static int NextPowerOf2(int v) {
	int p = 1;
	while (p < v) p <<= 1;
	return p;
}

static void Glow_EnsureTextures(int screenW, int screenH)
{
	int texW = NextPowerOf2(screenW);
	int texH = NextPowerOf2(screenH);

	if (g_glowSceneTexture && g_glowTexWidth == texW && g_glowTexHeight == texH)
		return; // already set up at right size

	g_glowTexWidth = texW;
	g_glowTexHeight = texH;

	if (!g_glowSceneTexture) glGenTextures(1, &g_glowSceneTexture);
	if (!g_glowBlurTexture) glGenTextures(1, &g_glowBlurTexture);

	// Scene texture: NEAREST for pixel-perfect copy-back, 16-bit for precision
	glBindTexture(GL_TEXTURE_2D, g_glowSceneTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	// Blur texture: LINEAR for smooth blur sampling, 16-bit for precision, clamp to black border
	glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, texW, texH, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
	float borderColor[4] = {0, 0, 0, 0};
	glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
}

// Draw a fullscreen quad textured with the given texture
static void Glow_DrawFullscreenQuad(GLuint texture, int screenW, int screenH)
{
	float maxS = (float)screenW / (float)g_glowTexWidth;
	float maxT = (float)screenH / (float)g_glowTexHeight;

	glBindTexture(GL_TEXTURE_2D, texture);
	glBegin(GL_QUADS);
		glTexCoord2f(0, 0);		glVertex2f(0, 0);
		glTexCoord2f(maxS, 0);		glVertex2f((float)screenW, 0);
		glTexCoord2f(maxS, maxT);	glVertex2f((float)screenW, (float)screenH);
		glTexCoord2f(0, maxT);		glVertex2f(0, (float)screenH);
	glEnd();
}

void RE_RenderGlowPass( void )
{
	if (!AppVars.bDynamicGlow || !AppVars.bShaderRendering)
		return;

	// Check if anything needs the glow pass
	bool bHasGlowShaders = false;
	for (int i = 0; i < R_GetNumShaders() && !bHasGlowShaders; i++) {
		shader_t *sh = R_GetShaderByIndex(i);
		if (sh && sh->hasGlow) bHasGlowShaders = true;
	}
	bool bHasSaberBlades = (AppVars.bSaberBlade[0] || AppVars.bSaberBlade[1]);
	if (!bHasGlowShaders && !bHasSaberBlades) return;

	GLint viewport[4];
	glGetIntegerv(GL_VIEWPORT, viewport);
	int screenW = viewport[2];
	int screenH = viewport[3];
	if (screenW <= 0 || screenH <= 0) return;

	Glow_EnsureTextures(screenW, screenH);

	float maxS = (float)screenW / (float)g_glowTexWidth;
	float maxT = (float)screenH / (float)g_glowTexHeight;

	// Step 1: Copy scene to texture, render glow-only, copy glow to texture
	glBindTexture(GL_TEXTURE_2D, g_glowSceneTexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], screenW, screenH);

	glClearColor(0, 0, 0, 0);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

	// Re-traverse the bolt tree rendering only glow stages.
	// Each container renders with its correct GL matrix (bolt transforms).
	RE_RenderAllGlowStages();

	// Draw saber glow sprites into the glow buffer
	if (AppVars.bSaberBlade[0] || AppVars.bSaberBlade[1]) {
		RE_DrawSaberBlades(true);
	}


	glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], screenW, screenH);

	// Debug mode: just show the raw glow buffer, skip blur and compositing
	if (AppVars.bDynamicGlowDebug) {
		glMatrixMode(GL_PROJECTION); glPushMatrix(); glLoadIdentity();
		glMatrixMode(GL_MODELVIEW); glPushMatrix(); glLoadIdentity();
		glPopMatrix(); glMatrixMode(GL_PROJECTION); glPopMatrix();
		glEnableClientState(GL_VERTEX_ARRAY);
		return;
	}

	// Step 2: Switch to 2D ortho with half-pixel offset for pixel-perfect mapping
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, screenW, 0, screenH, -1, 1);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();

	glDisable(GL_DEPTH_TEST);
	glDisable(GL_CULL_FACE);
	glEnable(GL_TEXTURE_2D);
	glDisableClientState(GL_VERTEX_ARRAY);
	glDisableClientState(GL_TEXTURE_COORD_ARRAY);
	glDisableClientState(GL_COLOR_ARRAY);

	// Step 3: Restore scene from texture (pixel-perfect with NEAREST + half-pixel ortho)
	glDisable(GL_BLEND);
	glColor4f(1, 1, 1, 1);
	Glow_DrawFullscreenQuad(g_glowSceneTexture, screenW, screenH);

	// Step 4: Multi-pass blur matching game defaults:
	// r_DynamicGlowPasses=5, r_DynamicGlowDelta=0.8, r_DynamicGlowIntensity=1.13
	// Game blurs at 25% resolution, so at full res we need 4x offsets and reduced intensity
	// Match game defaults exactly: r_DynamicGlowPasses=5, Delta=0.8, Intensity=1.13, Scale=0.25
	// Using GL_RGBA16 textures like the game for precision across 5 blur passes
	int   glowPasses    = 5;
	float glowDelta     = 0.8f;
	float glowStartOff  = 0.1f;
	float glowIntensity = 1.13f;
	float tapWeight     = glowIntensity * 0.25f;

	// Quarter resolution like the game (r_DynamicGlowScale = 0.25)
	int blurW = screenW / 4;
	int blurH = screenH / 4;
	if (blurW < 1) blurW = 1;
	if (blurH < 1) blurH = 1;

	// Clear the entire blur texture to black first (prevents edge bleed from stale data)
	glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);
	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16, g_glowTexWidth, g_glowTexHeight, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);

	// Render downsample into the blur viewport area
	glViewport(viewport[0], viewport[1], blurW, blurH);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glOrtho(0, blurW, 0, blurH, -1, 1);
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();

	glScissor(viewport[0], viewport[1], blurW, blurH);
	glEnable(GL_SCISSOR_TEST);

	// Downsample: draw full-res glow into half-res viewport
	glDisable(GL_BLEND);
	glColor4f(1, 1, 1, 1);
	glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);
	// The blur texture still has full-res glow from the copy. Draw it downsampled.
	glBegin(GL_QUADS);
		glTexCoord2f(0, 0);		glVertex2f(0, 0);
		glTexCoord2f(maxS, 0);		glVertex2f((float)blurW, 0);
		glTexCoord2f(maxS, maxT);	glVertex2f((float)blurW, (float)blurH);
		glTexCoord2f(0, maxT);		glVertex2f(0, (float)blurH);
	glEnd();
	// Copy downsampled result back - now texture has half-res data in [0,blurMaxS]x[0,blurMaxT]
	glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);
	glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], blurW, blurH);

	float blurMaxS = (float)blurW / (float)g_glowTexWidth;
	float blurMaxT = (float)blurH / (float)g_glowTexHeight;

	// Multi-pass blur at quarter resolution
	// Game: offset starts at 0.1 texels, adds 0.8 per pass (in blur-buffer pixels)
	// Convert to normalized texcoords: 1 blur texel = 1/texWidth in texcoord space
	float fTexelOffset = glowStartOff;
	for (int pass = 0; pass < glowPasses; pass++) {
		// Offset in normalized texcoords (1 blur pixel = 1/g_glowTexWidth)
		float offS = fTexelOffset / (float)g_glowTexWidth;
		float offT = fTexelOffset / (float)g_glowTexHeight;

		glClearColor(0, 0, 0, 0);
		glClear(GL_COLOR_BUFFER_BIT);
		glEnable(GL_BLEND);
		glBlendFunc(GL_ONE, GL_ONE);
		glColor4f(tapWeight, tapWeight, tapWeight, 1.0f);
		glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);

		float offs[4][2] = {{-offS,-offT},{-offS,offT},{offS,-offT},{offS,offT}};
		for (int t = 0; t < 4; t++) {
			glBegin(GL_QUADS);
				glTexCoord2f(offs[t][0], offs[t][1]);						glVertex2f(0, 0);
				glTexCoord2f(blurMaxS+offs[t][0], offs[t][1]);				glVertex2f((float)blurW, 0);
				glTexCoord2f(blurMaxS+offs[t][0], blurMaxT+offs[t][1]);		glVertex2f((float)blurW, (float)blurH);
				glTexCoord2f(offs[t][0], blurMaxT+offs[t][1]);				glVertex2f(0, (float)blurH);
			glEnd();
		}

		glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);
		glCopyTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, viewport[0], viewport[1], blurW, blurH);

		fTexelOffset += glowDelta;
	}

	glDisable(GL_SCISSOR_TEST);

	// Step 5: Restore full viewport, draw scene, overlay blurred glow
	glViewport(viewport[0], viewport[1], screenW, screenH);
	glMatrixMode(GL_PROJECTION); glLoadIdentity();
	glOrtho(0, screenW, 0, screenH, -1, 1);
	glMatrixMode(GL_MODELVIEW); glLoadIdentity();

	glDisable(GL_BLEND);
	glColor4f(1, 1, 1, 1);
	Glow_DrawFullscreenQuad(g_glowSceneTexture, screenW, screenH);

	// Glow overlay: stretch quarter-res blur to full screen
	// Inset texcoords slightly to prevent edge sampling past valid data
	float halfTexelS = 0.5f / (float)g_glowTexWidth;
	float halfTexelT = 0.5f / (float)g_glowTexHeight;
	glEnable(GL_BLEND);
	if (AppVars.bDynamicGlowSoft)
		glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_COLOR);
	else
		glBlendFunc(GL_ONE, GL_ONE);
	// Fullbright compensation: dim the overlay since fullbright leaves no headroom for glow
	float glowOverlay = 1.0f;
	if (AppVars.bDynamicGlowFullbrightComp)
		glowOverlay = AppVars.bDynamicGlowSoft ? 0.65f : 0.45f;
	glColor4f(glowOverlay, glowOverlay, glowOverlay, 1);
	glBindTexture(GL_TEXTURE_2D, g_glowBlurTexture);
	glBegin(GL_QUADS);
		glTexCoord2f(halfTexelS, halfTexelT);								glVertex2f(0, 0);
		glTexCoord2f(blurMaxS - halfTexelS, halfTexelT);					glVertex2f((float)screenW, 0);
		glTexCoord2f(blurMaxS - halfTexelS, blurMaxT - halfTexelT);		glVertex2f((float)screenW, (float)screenH);
		glTexCoord2f(halfTexelS, blurMaxT - halfTexelT);					glVertex2f(0, (float)screenH);
	glEnd();

	// Step 6: Restore GL state
	glDisable(GL_BLEND);
	glEnable(GL_DEPTH_TEST);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glEnableClientState(GL_VERTEX_ARRAY);
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();

	glEnableClientState(GL_VERTEX_ARRAY);
}


////////////// eof //////////

