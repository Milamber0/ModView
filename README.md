<p align="center">
  <img src="https://i.imgur.com/odgNk0t.png" alt="ModView 3">
</p>

### Foreword/Disclaimer

This tool was mostly updated through the use of Claude Code, this is an update I've wanted since the day I used ModView for the first time with a skin/model that had a very visual shader, but it's felt like too much work to spend a lot of time to make it. Without Claude Code I would not have bothered taking the effort to finally make it, or be willing to maintain it.

--- 

\
\
Raven Software's ModView model viewer for Jedi Outcast and Jedi Academy (`.glm`/`.gla`), heavily modified since the 2.1 source release. **This is not the original source code** — it's a community fork with a modernised build system, a much deeper renderer, animation-event and particle support, and some quality-of-life fixes.


See the [Gallery](#gallery) for video clips of the features in action.

---

## Quick start

1. Grab the latest release's `ModView.exe` from the [Releases page](../../releases).
2. Drop it somewhere — it's self-contained, nothing else to install.
3. Open any `.glm` (character, weapon, prop) from your Jedi Academy / Outcast `base` folder. The tool finds the rest of the gamedir automatically from the path.

> **You need to extract the base `.pk3` assets.** ModView reads textures, shaders, animevents, and saber blade textures straight off disk — it does not read from `.pk3` archives. Extract `assets0.pk3` / `assets1.pk3` / `assets2.pk3` / `assets3.pk3` into a `base/` folder next to a `ModView.exe` (or anywhere with the right directory layout) and point the tool at a model inside that tree. Without the extracted assets:
>
> - Models load but render missing-texture white.
> - Shader stages, glow, animation events, and `.efx` particles have nothing to resolve against.
> - Preset saber blade colors (blue / green / yellow / orange / red / purple) won't work, and the Weapons & Lightsabers dialog will only offer the bundled **Custom (RGB)** color — ModView ships its own `blend_*` blade textures inside the exe specifically so the custom-color picker works without any assets extracted.

### Heads-up: update check on startup

ModView checks its GitHub releases page on startup to see if a newer version is out. The update prompt has a **"Don't remind me about this version again"** checkbox if you'd rather stay on the current build, and the check is skipped entirely for local `-dev` builds. The endpoint is `api.github.com/repos/Milamber0/ModView/releases/latest` — nothing else leaves the machine.

### Troubleshooting: ModView freezes on startup unless launched as admin

Almost always caused by an **audio-overlay hook** (MSI / Realtek Nahimic's `NahimicOSD.dll`, or occasionally SteelSeries GG, Razer Synapse, or Asus Sonic Studio). These tools inject themselves into every graphical process to draw their own on-screen display, and that hook can deadlock ModView's OpenGL `SwapBuffers` path on some driver versions. Running as administrator sidesteps the hook because of integrity-level rules — hence the symptom.

Fixes, in order of how invasive:

1. **Add `ModView.exe` to the audio software's exclusion list.** In Nahimic / MSI Center open the audio tab, find "Game Visualizer" or "Application List", and add ModView with the hook disabled. Same idea for other overlay tools.
2. **Turn off the on-screen-display globally.** Most of these tools have a single toggle for their overlay; flipping it off stops the DLL injection.
3. **Uninstall the audio software entirely** via Programs & Features if you don't use its features.

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

| New Shortcuts | Action |
|---|---|
| `Ctrl+F` | Search animations |
| `Ctrl+Q` | Toggle shader rendering |
| `Ctrl+M` | Toggle shader animation |
| `Ctrl+Y` | Toggle glow debug view |
| `Shift+C` | Transparent screenshot |

---

## Building from source

```bash
cmake -S . -B build -A Win32
cmake --build build --config Release
```

The resulting exe lands at `Build/ModView.exe`.

---

## Gallery

<table align="center">
  <tr>
    <td align="center" valign="top" width="33%">
      <video src="https://github.com/user-attachments/assets/6b659b9e-8928-4404-9733-7bd42b047a64" width="100%" autoplay loop muted playsinline></video>
      <br><sub>Shaders, animation finder, weapon/saber menu, ingame saber blades, efx</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <video src="https://github.com/user-attachments/assets/5424c4b7-adcd-400e-9429-edcc84828d73" width="100%" autoplay loop muted playsinline></video>
      <br><sub>Advanced shaders</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <video src="https://github.com/user-attachments/assets/4c505a79-b815-4a80-9101-38d62feb1241" width="100%" autoplay loop muted playsinline></video>
      <br><sub>Dynamic Glow</sub>
    </td>
  </tr>
  <tr>
    <td align="center" valign="top" width="33%">
      <video src="https://github.com/user-attachments/assets/c6d056b9-b032-46fd-ad29-e466c53419c5" width="100%" autoplay loop muted playsinline></video>
      <br><sub>Vertex deformation</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <a href="https://i.imgur.com/qp1hnZH.png" title="Click for full-resolution transparent PNG">
        <img src="https://i.imgur.com/qp1hnZHl.png" alt="Character pose, transparent PNG">
      </a>
      <br><sub>Transparent PNG export #1</sub>
    </td>
    <td align="center" valign="top" width="33%">
      <a href="https://i.imgur.com/NaAkjdg.png" title="Click for full-resolution transparent PNG">
        <img src="https://i.imgur.com/NaAkjdgl.png" alt="Wide scene with bolted saber, transparent PNG">
      </a>
      <br><sub>Transparent PNG export #2</sub>
    </td>
  </tr>
</table>

---

## Credits

### Maintainers
- Milamber
- Claude Code

### Contributors
- original tool written by Ste Cork at Raven Software (with MFC help from Mike Crowns). 
- DT85 for the original fork.

### feedback/testers
- Ashura
- Atlas
- Sha

## Contributing

Bug reports and PRs welcome on the [Issues page](../../issues). Because this fork has drifted significantly from Raven's 2.1 baseline or DT85's repo, consider it a separate project.
