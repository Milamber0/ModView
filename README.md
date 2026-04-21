# ModView 3

Raven Software's ModView model viewer for Jedi Outcast and Jedi Academy (`.glm`/`.gla`), heavily modified since the 2.1 source release. **This is not the original source code** — it's a community fork with a modernised build system, a much deeper renderer, animation-event and particle support, and a lot of quality-of-life fixes.

Credit: original tool written by Ste Cork at Raven Software (with MFC help from Mike Crowns). Ongoing work by Milamber.

---

## Quick start

1. Grab the latest release's `ModView.exe` from the [Releases page](../../releases).
2. Drop it somewhere — it's self-contained, nothing else to install.
3. Open any `.glm` (character, weapon, prop) from your Jedi Academy / Outcast `base` folder. The tool finds the rest of the gamedir automatically from the path.

Everything below describes what's been added on top of Raven's 2.1 baseline.

---

## What's new in Modview 3

### Rendering

- **Full Q3 / JKA shader system.** `.shader` files are parsed end-to-end: multi-stage blending, `tcMod`, `rgbGen`/`alphaGen`, `depthFunc`, detail stages, lightmaps, `deformVertexes`, environment mapping, and per-vertex specular highlights. Toggle with `Ctrl+Q`.
- **Dynamic glow post-process.** Scene → glow render → multi-pass blur → additive composite, matching the game's `r_DynamicGlow` pipeline (Passes=5, Delta=0.8, Intensity=1.13, quarter-res blur). Bolted models participate correctly. Ctrl+Y shows the raw glow buffer for debugging.
- **Lightsaber blades.** Full blade rendering matching the game's two-layer (glow ring + inner core) approach. Multi-blade saber support (staff, trident, up to 8 blades per hilt). Six preset colors (blue/green/yellow/orange/red/purple) plus a custom RGB picker.
- **Live saber color preview.** The "Custom…" color picker updates the blade in the viewport in real time as you drag — no more picking blind and hitting OK.
- **Bundled RGB blade textures.** The `blend_*` textures that drive the custom-color path are embedded in the exe as resources, so RGB sabers work regardless of what's in your gamedir.
- **Animated shaders.** Shader stage animation is driven by a continuous 30 FPS redraw timer so `tcMod scroll` / `tcMod turb` / `animMap` stages actually move. Toggle with `Ctrl+M`.
- **Shader-driven surface filtering.** Surface on/off state integrates with the skin system properly; `*off` in `.skin` now hides surfaces as intended.

### Effects

- **`animevents.cfg` parser.** Parses the game's animation-event config next to the `.gla` and fires AEV_EFFECT entries on the matching frames.
- **`.efx` particle system.** A subset of the game's Fx format:
  - `Particle` — additive billboards with size/alpha curves, gravity, and bolt-local spawn
  - `Tail` — stretched-quad trails with `orgOnSphere` / `axisFromSphere` spawnflags (e.g. `saber_block` radiating sparks)
  - `FxRunner` — chains child `.efx` files so composite effects like `saber_clash` stack correctly
- **Saber-vs-floor collision sparks.** When a blade crosses the floor plane during an animation, `saber/saber_cut.efx` fires at the contact point. Iterates every blade on every hand so staff-ends spark independently. Toggle in the Weapons dialog.

### Scene / UI additions

- **Weapons & Lightsabers dialog.** Scans `models/weapons2/` and lets you bolt weapons to hands / hips / back / thoracic. Full saber blade configuration (per-hand on/off, color, length, collision-fx toggle) lives here too.
- **Scene state persistence.** Camera position, rotation, animation sequence, active skin, part-skin overrides (`head_*`, `torso_*`, `lower_*`), saber settings, and bolted weapons are saved per-model to `modview_scenes.json` and restored on reload.
- **Find Sequence / Search Animations.** `Ctrl+F` opens a filter over the animation tree — type any substring and hit Enter to jump. Also available in the tree-view right-click menu.
- **Entity color picker.** Per-model entity tint (the `shaderRGBA` pathway) via the toolbar.
- **Transparent PNG screenshots.** Alpha-preserving export for compositing.
- **Parent-folder display in the MRU.** Recent files show as `milamber/model.glm` rather than the ambiguous `model.glm`.
- **Shader toolbar.** Six buttons — animation search, shader rendering toggle, shader animation toggle, entity color, dynamic glow, weapons dialog — with hover tooltips and status-bar hints.

### Build system

- CMake-based build (VS 2022 or any MSVC generator). Statically-linked MFC and CRT.
- 32-bit (`x86` / `Win32`) target — the `_M_IX86` legacy branches in `r_glm.cpp` have never been kept alive, so a 64-bit port is a deferred item.
- **GitHub Actions release workflow** (`.github/workflows/build.yml`). Manual trigger only from the Actions tab; produces a versioned `ModView-3.X-windows-x86.zip`, tags the repo, and publishes a GitHub Release. Version auto-increments from the highest existing `3.X` tag.

### Configuration keys / hotkeys reference

| Shortcut | Action |
|---|---|
| `Ctrl+O` | Open model |
| `Ctrl+Q` | Toggle shader rendering |
| `Ctrl+M` | Toggle shader animation |
| `Ctrl+Y` | Toggle glow debug view |
| `Ctrl+W` | Wireframe mode |
| `Ctrl+L` | Toggle floor |
| `Ctrl+F` | Search animations |
| `Ctrl+H` | Force-white texture mode |
| `Ctrl+R` | Ruler |
| `Ctrl+D` | Cycle FOV |
| `Ctrl+N` | Toggle interpolation |
| `Ctrl+←` | Slow animation |

---

## Building from source

```bash
cmake -S . -B build -A Win32
cmake --build build --config Release
```

The resulting exe lands at `Build/ModView.exe`. To build with an explicit release version number:

```bash
cmake -S . -B build -A Win32 -DMODVIEW_VERSION_STRING=3.5
```

Without the flag, local builds use `3.0-dev` so you can tell them apart from real CI releases.

---

## Contributing

Bug reports and PRs welcome on the [Issues page](../../issues). Because this fork has drifted significantly from Raven's 2.1 baseline, upstream patches rarely apply cleanly — treat this as its own project rather than a downstream.
