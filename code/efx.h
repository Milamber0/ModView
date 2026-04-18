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
// Called from the container's render callback (so the GL modelview is
// set to the model's local space - bolt matrices sit in that frame).
// Events whose bolt can't be resolved on this model are silently skipped.
void Efx_DispatchFrameEvents(ModelContainer_t *pContainer, int iCurrentFrame);

// Advance + cull physics for every container's particles. Called once
// per scene render (before containers draw), so each container renders
// only; otherwise the per-container dt would reset after the first
// container and the rest would visibly stall.
void Efx_TickAll(void);

// Draw live particles bolted to pContainer. Called inside the container's
// render callback so the model's modelview is active and particles render
// in model-local space (matching where they spawned from bolt matrices).
void Efx_RenderForContainer(ModelContainer_t *pContainer);

// Drop cached effect defs and all live particles across all containers.
// Wire to Media_Delete so we don't keep stale GL texture IDs after a
// gamedir change rebuilds the texture cache.
void Efx_Shutdown(void);

#endif	// #ifndef EFX_H
