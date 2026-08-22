# EmulationStation-X (ES-X)

<p align="left">
  <img alt="License: MIT" src="https://img.shields.io/badge/License-MIT-green.svg" />
  <img alt="Platform: ARM" src="https://img.shields.io/badge/Platform-ARM-informational.svg" />
  <img alt="RetroPie Compatible" src="https://img.shields.io/badge/RetroPie-Compatible-red.svg" />
</p>

<img width="1280" height="720" alt="LOGO" src="https://github.com/user-attachments/assets/e8560813-41dd-4ad8-bf01-ecc6e03e922c" />

[YouTube demo](https://www.youtube.com/watch?v=E0kRlrkbLo0)

## 🎁 X-TRAS — Free Games Starter Catalog

X-TRAS gives a fresh RetroPie installation something useful to launch immediately.

Instead of bundling commercial ROMs, ES-X includes a curated catalog of free homebrew games whose redistribution rights are explicitly documented. Games can be downloaded directly from the ES-X interface and are placed automatically in the correct RetroPie ROM folder.

> **X-TRAS is not an abandonware or commercial-ROM catalog.** Active entries must have clear redistribution rights and a verifiable upstream source.

X-TRAS is designed around a strict catalog policy:

- Free and legally redistributable homebrew only
- No abandonware
- No commercial ROMs
- No freeware-only titles when redistribution permission is unclear
- No fan games or assets with unclear rights
- Author, license and upstream source are documented for every catalog entry
- Direct downloads are used only when a stable, verified source is available
- Already installed games are detected automatically

The catalog is external to the executable:

- Repository catalog: `esx/xtras-free-games.ini`
- Installed catalog: `~/.emulationstation/esx/xtras-free-games.ini`
- Catalog entries can be updated without recompiling ES-X

The initial starter catalog already covers multiple classic systems, including **Game Boy, Game Boy Color, Game Boy Advance, NES, Master System, Game Gear and Mega Drive / Genesis**.

The goal is deliberately simple: **a user starting with zero games can install a small legal starter library and immediately test several systems from inside ES-X.**

The catalog is intentionally small and curated rather than trying to become a massive game store. It is also structured so other projects can reuse or extend the catalog format if they wish.

*A screenshot of the X-TRAS browser will be added here.*

## Example Theme Gallery
<img width="1366" height="768" alt="Captura de pantalla_2026-01-09_19-20-24" src="https://github.com/user-attachments/assets/07912879-ca8d-4122-a640-d32cd15c6fd9" />

## Multilingual scraper
<img width="1366" height="768" alt="Captura de pantalla_2025-12-01_12-24-53" src="https://github.com/user-attachments/assets/83dedb20-1c32-432b-8495-24d2f6fca3a7" />

### ☕ Native Java ME (J2ME) Scraping

ES-X includes dedicated scraping support for Java ME games through the
internal ScreenScraper integration.

Unlike traditional filename-based scraping, ES-X can inspect the JAR package
and read `MIDlet-Name` directly from `META-INF/MANIFEST.MF` to identify the
actual game title.

This is especially useful for J2ME collections, where downloaded games often
contain device names, screen resolutions, database IDs, language tags, or
repacker information in their filenames.

Features include:

- Native J2ME platform mapping to ScreenScraper
- Automatic `MIDlet-Name` detection from JAR manifests
- Cleaned filename fallback for renamed or repacked games
- Artwork fallback when traditional box art is unavailable
- Protection against malformed release dates
- No requirement to manually rename the original JAR files

Because Java ME preservation databases are incomplete, not every title can be
matched automatically, but ES-X significantly improves identification of
real-world J2ME collections.
<img width="1366" height="768" alt="image" src="https://github.com/user-attachments/assets/f3bb5625-2bb8-4299-b333-b0c97c9b7537" />

## Example Theme Options
<img width="1366" height="768" alt="Captura de pantalla_2025-12-05_07-07-19" src="https://github.com/user-attachments/assets/06f4504c-ea88-4088-953c-7b61d3726ca7" />

<img width="1366" height="768" alt="Captura de pantalla_2025-12-31_12-58-30" src="https://github.com/user-attachments/assets/6980ece3-29b9-42ac-ac95-dfda9f15c095" />

<img width="1366" height="768" alt="Captura de pantalla_2025-12-15_13-20-32" src="https://github.com/user-attachments/assets/1f12408e-4004-4334-be2f-2bb138da43b7" />

## Example Grid view
![Captura de pantalla_2026-03-02_13-12-38](https://github.com/user-attachments/assets/59498641-bc9f-4923-903d-c0f1f5121655)
![photo_2026-04-04_09-59-24](https://github.com/user-attachments/assets/44ad1a7b-16ac-482f-afdc-014bb9e81dde)


## Example Gamelist Carousel

ES-X adds carousel support directly to `TextListComponent`, allowing themes to build modern game list layouts without creating a separate view.

The gamelist can now be presented as a horizontal or vertical carousel using scraped artwork, with theme-controlled scaling, spacing, opacity, image source, image fit, and selected item emphasis.

This makes it possible to create PS5-style horizontal game carousels, classic screenshot-based vertical layouts, and other console-inspired interfaces while still using the classic EmulationStation theme structure.

<img width="1280" height="719" alt="photo_2026-05-08_20-49-32" src="https://github.com/user-attachments/assets/7f3e2664-f2e3-4bbc-b72f-802453a25e00" />
<img width="1280" height="719" alt="photo_2026-05-08_20-49-48" src="https://github.com/user-attachments/assets/61321cb0-693c-4f7a-8ca5-def7a5ccebbe" />



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
- Theme-controlled image source and fallback handling
- Safe metadata fallback handling
- Expanded carousel support for system and game views
- Horizontal and vertical gamelist carousel modes using `TextListComponent`

ES-X extends the classic theme engine while preserving compatibility with existing EmulationStation themes.

The goal is to make advanced layouts possible through theme XML instead of requiring hardcoded engine hacks.
### 🎮 Enhanced Grid Layout Control

ES-X expands Grid view flexibility, enabling themes to:

Create single-row horizontal layouts with carousel-like behavior

Scale and highlight the selected item with clean visual separation

Control spacing, composition, and visual emphasis directly via theme XML

Define custom gridtile frames using backgroundImage, allowing themed borders and overlays per state (default / selected)

Build modern console-style interfaces without modifying the engine core

Gridtile rendering has also been stabilized to prevent unintended image alpha modulation, ensuring selected items display with full opacity and accurate color reproduction.

All enhancements preserve performance, compatibility, and the lightweight architecture of classic EmulationStation.

### 🔊 Background Music & Audio System

- SDL_mixer-based background music manager
- Stable shuffle model
- Automatic pause on game launch
- Resume with next track
- Safe handling of corrupted audio files
- Ducking when video is playing
- Navigation sounds (scroll / select / back)

Built for long-term stability on ARM devices.

### 🎬 Refined Transition System
ES-X separates navigation transitions from game launch transitions.

This keeps the system carousel visually stable while allowing smoother
animations within game lists and during game launch.

Inspired by modern console UI design, this produces a more balanced
navigation experience without excessive motion.

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
| Network indicator |  ✅ Stable |
| Native J2ME scraping | ✅ Available |
| MIDlet manifest detection | ✅ Available |
| Theme Browser | ✅ Stable |
| System descriptions / Info viewer | ✅ Stable |
| Quick System Select | ✅ Stable |
| Integrated Skyscraper interface | ✅ Available |
| X-TRAS free games catalog | ✅ Available |
| Gamelist carousel | ✅ Stable |
| Enhanced Grid layouts | ✅ Stable |
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
