// Filename:-	efx.h
//
// JKA .efx particle effect system for ModView (minimal, MVP).
// Currently supports only the Particle primitive (the most common one
// in character effects - sparks, glows, muzzle puffs). Other primitives
// (Line, Light, Electricity, Cylinder, Decal, OrientedParticle) are
// accepted by the parser but skipped at spawn time.

#ifndef EFX_H
#define EFX_H

// Fire events on pContainer whose absolute frame matches iCurrentFrame.
// Called from the container's render callback (so parent-chain bolt matrices
// are populated and we can resolve the spawn position). Particles are
// converted to primary-model-local space at spawn time and then live
// independently - they don't follow the animating bolt afterwards. That
// matches the game's non-FX_RELATIVE behaviour where a puff of smoke spawns
// at the bolt's current position and then drifts freely.
void Efx_DispatchFrameEvents(ModelContainer_t *pContainer, int iCurrentFrame);

// Advance + cull physics for live particles. Called once per scene render.
void Efx_TickAll(void);

// Check active saber blades against each other and against the floor plane
// each frame, spawning saber/saber_clash.efx at contact points with a short
// cooldown so sparks don't machine-gun while blades stay in contact. Runs
// regardless of animation events.
void Efx_CheckSaberCollisions(void);

// Draw world-space particles (physics-spawned, non-relative). Call after
// every container has rendered and glPopMatrix'd back to the scene modelview.
void Efx_RenderAll(void);

// Draw relative particles attached to pContainer inside the caller's current
// modelview. Call from each container's render callback so FX_RELATIVE
// particles stay attached to their bolt frame as the saber/weapon animates.
void Efx_RenderForContainer(ModelContainer_t *pContainer);

// Drop cached effect defs and all live particles across all containers.
// Wire to Media_Delete so we don't keep stale GL texture IDs after a
// gamedir change rebuilds the texture cache.
void Efx_Shutdown(void);

#endif	// #ifndef EFX_H
