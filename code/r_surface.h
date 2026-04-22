// Filename:-	R_Surface.h
//
//	some generic past code for surfaces
//

#ifndef R_SURFACE_H
#define	R_SURFACE_H


void GetWeightColour(int iNumWeights, byte &r, byte &g, byte &b);


void RB_CheckOverflow( int verts, int indexes );
void RE_GenerateDrawSurfs( void );
void RE_RenderDrawSurfs( void );
void RE_RenderGlowPass( void );	// re-renders glow stages only, blurs, composites
void RE_RenderAllGlowStages( void );	// traverses bolt tree rendering only glow stages
void RE_DrawSaberBlades( bool bGlowOnly = false );	// draw lightsaber blades on bolted sabers

// True when the preset saber blade textures (gfx/effects/sabers/<color>_glow2
// etc.) exist on disk under the current gamedir. Custom/RGB sabers always
// work (their textures are bundled in the exe) but preset colors need the
// user to have extracted the base JKA assets. Used by the weapon dialog
// and the blade renderer to gracefully degrade to custom-only.
bool RE_PresetSaberAssetsAvailable(void);

void R_ModView_BeginEntityAdd(void);
void R_ModView_AddEntity(ModelHandle_t hModel,			int iFrame_Primary, int iOldFrame_Primary, 
							int iBoneNum_SecondaryStart,int iFrame_Secondary, int iOldFrame_Secondary, 
							int iSurfaceNum_RootOverride,
							float fLerp,
 					 		surfaceInfo_t *slist,			// pointer to list of surfaces turned off
							boneInfo_t	*blist,				// pointer to list of bones to be overriden
							mdxaBone_t	*pXFormedG2Bones,
							bool		*pXFormedG2BonesValid,
							mdxaBone_t	*pXFormedG2TagSurfs,
							bool		*pXFormedG2TagSurfsValid,
							//
							int *piRenderedTris,
							int *piRenderedVerts,	
							int *piRenderedSurfs,
							int *piXformedG2Bones,
//							int	*piRenderedBoneWeightsThisSurface,
							int *piRenderedBoneWeights,
							int *piOmittedBoneWeights
						 );

extern int giSurfaceVertsDrawn;
extern int giSurfaceTrisDrawn;
extern int giRenderedBoneWeights;
extern int giOmittedBoneWeights;



#endif	// #ifndef R_SURFACE_H

/////////////////// eof //////////////////

