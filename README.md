# EmulationStation-X (ES-X)

<p align="left">
  <img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-green.svg" />
  <img alt="Platform: ARM" src="https://img.shields.io/badge/Platform-ARM-informational.svg" />
  <img alt="RetroPie Compatible" src="https://img.shields.io/badge/RetroPie-Compatible-red.svg" />
</p>

<img width="1280" height="720" alt="LOGO" src="https://github.com/user-attachments/assets/e8560813-41dd-4ad8-bf01-ecc6e03e922c" />

[YouTube demo](https://www.youtube.com/watch?v=E0kRlrkbLo0)


## Example Theme Gallery

<table>
  <tr>
    <td><img src="esx/theme-previews/pistationx.png" alt="Pi-Station-X preview" width="100%" /></td>
    <td><img src="esx/theme-previews/mini.png" alt="Mini preview" width="100%" /></td>
  </tr>
  <tr>
    <td><img src="esx/theme-previews/ruckage_snes.png" alt="Ruckage SNES Mini preview" width="100%" /></td>
    <td><img src="esx/theme-previews/artbook.png" alt="ArtBook Next preview" width="100%" /></td>
  </tr>
  <tr>
    <td><img src="esx/theme-previews/alekfull.png" alt="Alekfull NX preview" width="100%" /></td>
    <td><img src="resources/splash.png" alt="ES-X splash" width="100%" /></td>
  </tr>
</table>

## What is ES-X?

EmulationStation-X (ES-X) is a stable, creator-focused evolution of the original EmulationStation engine, built specifically for RetroPie systems.

It started from a simple need: create better themes without leaving RetroPie.

ES-X does not replace EmulationStation. It extends it.

It preserves RetroPie compatibility, ARM performance, and familiar structure, while expanding what themes and UI systems can achieve.

## Philosophy

ES-X follows a clear direction:

- Modernize without fragmenting the ecosystem
- Empower theme creators
- Keep performance ARM-friendly
- Expand carefully, not aggressively

Inspired by projects such as ES-DE, Batocera, and Recalbox, ES-X adapts selected ideas to fit RetroPie’s architecture — not to imitate them, but to expand creative possibilities within a different ecosystem.

## Why ES-X Exists

Classic EmulationStation is stable and lightweight. But for theme creators, achieving modern layouts often requires duplication, structural compromises, or engine-level workarounds.

ES-X reduces that friction.

It treats themes not as static skins, but as configurable systems.

- **Users** benefit from improved visuals and runtime flexibility.
- **Creators** benefit from a cleaner and more modular foundation.

## Who ES-X Is For / Not For

### ES-X is for:

- Theme creators who want advanced control without engine hacks
- RetroPie users who want modern visuals while keeping compatibility
- Contributors interested in careful, production-oriented evolution

### ES-X is not for:

- Replacing RetroPie’s ecosystem with a fully different frontend
- Breaking compatibility for quick experimentation
- Feature bloat without long-term ARM stability

## Core Systems (Production-Ready)

### 🌍 Runtime Language System (.ini)

- Full UI translation without recompilation
- Dynamic loading at runtime
- Community-friendly structure
- Located in: `~/.emulationstation/lang/`

Stable and actively used.

### 🎨 Theme Options System

The heart of ES-X:

- `theme.xml` → draws the UI
- `theme.ini` → defines options
- ES-X → binds both at runtime

This enables:

- Multiple layouts from a single theme
- Performance-friendly variants
- Console-style customization
- Zero duplication

Themes become configurable systems, not fixed presets.

See: `THEME_OPTIONS.md`

### 🧩 Extended Theme Engine

- True `zIndex` layering
- Advanced scale, spacing, and opacity controls
- Text borders & shadows (engine-level)
- Improved carousel flexibility
- Safe metadata fallback handling

Designed to remain compatible with classic themes.

### 🔊 Background Music & Audio System

- SDL_mixer-based background music manager
- Stable shuffle model
- Automatic pause on game launch
- Resume with next track
- Safe handling of corrupted audio files
- Ducking when video is playing
- Navigation sounds (scroll / select / back)

Built for long-term stability on ARM devices.

## Designed for Theme Creators

ES-X prioritizes clarity of mental model.

Compared to traditional EmulationStation workflows:

- Layering is predictable
- Layout variants are modular
- Localization is externalized
- Visual logic is separated from configuration

The goal is not maximum complexity. The goal is accessible expressiveness.

Beautiful themes should not require engine hacks.

## Current Feature Status

| Feature | Status |
|---|---|
| Runtime language system | ✅ Stable |
| Theme variables | ✅ Available |
| Theme Options system | ✅ Stable |
| Text borders & shadows | ✅ Stable |
| Dark menu mode | ✅ Stable |
| Background music | ✅ Stable |
| Navigation sounds | ✅ Stable |
| Audio ducking | ✅ Stable |
| Controller overlay | 🔄 Planned |
| Network indicator | 🔄 Planned |

Core systems are stable for daily use. New capabilities are introduced carefully to preserve compatibility and performance.

## Installation

### Recommended (RetroPie Module)

https://github.com/Renetrox/EmulationStation-X-Module-for-retropie

- ✔ Handles dependencies
- ✔ Correct build flags
- ✔ Clean RetroPie integration

### Manual Build (Advanced)

```bash
git clone --recursive https://github.com/Renetrox/EmulationStation-X.git
cd EmulationStation-X
mkdir build && cd build
cmake ..
make -j4
```

If cloned without submodules:

```bash
git submodule update --init --recursive
```

## Roadmap Focus

- Full language coverage
- Expanded theme-driven layout control
- Controller & network overlays
- Creator-oriented documentation
- Continued refinement without breaking RetroPie stability

## Author

**Dino René Caballero Márquez (Renetrox)**  
Educator • RetroPie Enthusiast • Theme Developer  
Paraguay

## License

MIT (same as original EmulationStation).

---

Accessibility is not just using it — it is understanding it, hearing it, and shaping it your way.
