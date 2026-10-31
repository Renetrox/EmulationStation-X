# ES-X Theme System

EmulationStation-X (ES-X) loads a theme for each system.

A theme is simply composed of:

- **Views**
- **Elements**
- **Properties**

Most themes are defined in:

```text
theme.xml
```

This file defines the visual structure of the interface.

ES-X is designed so themes remain simple, modular, and scalable even as they grow in complexity.

---

## Theme Lookup Order

For each system, ES-X searches for `theme.xml` using the following priority:

1. **System path (per-system theme)**

   ```text
   [SYSTEM_PATH]/theme.xml
   ```

2. **User theme set**

   ```text
   [HOME]/.emulationstation/themes/[CURRENT_THEME_SET]/[SYSTEM_THEME]/theme.xml
   ```

3. **System theme set**

   ```text
   /etc/emulationstation/themes/[CURRENT_THEME_SET]/[SYSTEM_THEME]/theme.xml
   ```

If multiple themes exist, the first match wins.

**Priority order:**

```text
System path → Home theme set → /etc theme set
```

This architecture allows:

- Per-system overrides
- User customization
- Clean separation between global and local themes

---

## Theme Identifiers

### `SYSTEM_THEME`

This value comes from the `<theme>` tag in `es_systems.cfg`.

Example:

```xml
<theme>snes</theme>
```

If `<theme>` is not defined, ES-X uses the system `<name>`.

### `CURRENT_THEME_SET`

Selected in:

```text
UI Settings → Theme Set
```

If not defined, ES-X loads the first available theme set.

---

## Theme Set Folder Structure

A theme set is simply a structured folder:

```text
themes/
  my_theme_set/
    snes/
      theme.xml

    nes/
      theme.xml

    _inc/
      backgrounds/
      logos/
      audio/

    theme.xml
```

The root `theme.xml` acts as a fallback if a system folder is missing.

This keeps themes:

- modular
- reusable
- easy to maintain

---

# Core Concepts

## Views

A view represents a screen type.

Example:

```xml
<view name="detailed">
  ...
</view>
```

Multiple views can share the same layout:

```xml
<view name="system, basic, detailed, grid">
  ...
</view>
```

## Elements

Elements are the UI components used inside views.

Common elements include:

- `image`
- `text`
- `video`
- `carousel`
- `textlist`
- `imagegrid`
- `rating`
- `datetime`
- `helpsystem`

You can either modify existing elements or create new ones.

### Modify an existing element

```xml
<text name="md_description">
  <color>FFFFFF</color>
</text>
```

### Create a new element

```xml
<image name="e_overlay" extra="true">
  <path>./overlay.png</path>
</image>
```

Extra elements render in order of declaration.

Background elements should generally be defined first.

Themes only need to define what they want to change.

## Properties

Properties control element behavior.

Example:

```xml
<pos>0.1 0.2</pos>
<color>FFFFFFFF</color>
```

You do not need to define every property. Only override the ones that matter.

---

# Required & Advanced Tags

## `formatVersion`

Required in every theme.

```xml
<formatVersion>4</formatVersion>
```

## `resolution`

Optional pixel-based positioning.

```xml
<resolution>1920 1080</resolution>
```

ES-X automatically normalizes coordinates.

> **Note:** Parenting is not supported when `resolution ≠ 1 1`.

## `include`

Allows modular theme files.

```xml
<include>./shared.xml</include>
```

Useful for:

- shared layouts
- reusable components
- modular themes

---

# zIndex Rendering Order

`zIndex` controls rendering layers.

Lower values render first. Higher values render on top.

### Typical System View

| Element | zIndex |
|---|---:|
| Extra elements | 10 |
| `systemcarousel` | 40 |
| `systemInfo` | 50 |

### Typical Gamelist View

| Element | zIndex |
|---|---:|
| background | 0 |
| Extra | 10 |
| list / grid | 20 |
| media | 30 |
| metadata | 40 |
| logo | 50 |

---

# Variables

Variables simplify reuse and scaling.

## System Variables

Automatically available:

```text
${system.name}
${system.fullName}
${system.theme}
${system.gameCount}
${system.mostPlayedImage}
```

Example:

```xml
<path>./_inc/consoles/${system.theme}.png</path>
```

## Theme Variables

Defined inside the theme:

```xml
<variables>
  <themeColor>8b0000</themeColor>
</variables>
```

Usage:

```xml
<color>${themeColor}c0</color>
```

---

# ES-X Extensions

## Carousel (System View)

```xml
<carousel name="systemcarousel">
  <logoScale>1.32</logoScale>
  <minLogoOpacity>0.94</minLogoOpacity>
  <scaledLogoSpacing>1</scaledLogoSpacing>
</carousel>
```

Important properties:

- `logoScale`
- `minLogoOpacity`
- `scaledLogoSpacing`
- `logoPos` / `carouselLogoPos`: absolute selected-logo anchor inside the SystemView carousel without changing the carousel `pos`, `origin`, `size`, or clip.
- `logoOffset`, `logoOffsetX`, `logoOffsetY`: optional fine adjustment after the anchor is resolved.

Depth and spacing can be fully adjusted without modifying engine code.

### Optional curved system icons (TEST)

To mimic the rounded card icons in ES-DE's PS5 menu, add this to
`<view name="system">`. The setting applies to system images, not to text.

```xml
<carousel name="systemcarousel">
  <imageCornerRadius>0.010</imageCornerRadius>
</carousel>
```

A radius of `0` (or an omitted tag) retains the original rectangular draw
path. To use the same style for the gamelist's `<textlist>` carousel,
see [Rounded carousel images](#rounded-carousel-images-test-gl-21-and-gles-10).

### Positioning and spacing a horizontal system carousel (XMB example)

The following parameters are supported by ES-X's `systemcarousel` and can be tuned in the theme XML without changing `SystemView.cpp`:

```xml
<carousel name="systemcarousel">
  <type>horizontal</type>
  <origin>0 0</origin>
  <pos>0 0.126</pos>
  <size>1 0.231</size>
  <logoOffsetX>-0.275</logoOffsetX>
  <logoSpacingX>0.15</logoSpacingX>
  <logoSize>0.108 0.193</logoSize>
  <logoScale>1.20</logoScale>
  <maxLogoCount>11</maxLogoCount>
  <logoAlignment>center</logoAlignment>
  <minLogoOpacity>0.90</minLogoOpacity>
  <scaledLogoSpacing>0.65</scaledLogoSpacing>
  <color>00000000</color>
  <zIndex>80</zIndex>
</carousel>
```

- `pos` and `origin` position the *carousel container*; `size` also defines its clipping rectangle and affects automatic spacing.
- `logoOffsetX` shifts the entire row of logos, including the selected item; it does **not** move the selected item independently.
- `logoSpacingX` sets the horizontal distance between successive logo positions (normalized to screen width), overriding automatic spacing.
- `maxLogoCount` limits how many logos are rendered around the current selection. It does not automatically fill empty screen space.
- `scaledLogoSpacing` adjusts the gap adjacent to the scaled/selected logo; it does not redistribute the full row.
- `selectedLogoOffsetX` changes only the selected logo's horizontal position; `logoPos` / `carouselLogoPos` define an explicit local anchor.

**Practical tip:** to extend the XMB row without moving its chosen anchor, keep `logoOffsetX` and change `logoSpacingX`/`maxLogoCount` first. If the selected logo is off-center, the row remains symmetrically enumerated and some items may be clipped. The sample values reproduce the October 2026 XMB layout tuning; verify the final appearance at your target resolution.



---

## Random game video in System View (experimental)

ES-X can optionally preview **one random scraped game video** when a system is
selected. The existing `mostPlayedVideo` behavior remains unchanged unless
the theme explicitly opts in.

```xml
<view name="system">
  <video name="randomGameVideo">
    <pos>0.65 0.32</pos>
    <maxSize>0.30 0.26</maxSize>
    <delay>1</delay>
    <zIndex>30</zIndex>
  </video>
</view>
```

- Uses each game's `<video>` entry from the gamelist; only existing files are considered.
- Selects another clip when changing systems **or returning to System View**.
- Caches valid paths by system and avoids repeating the previous clip if alternatives exist.
- Uses the same single video player as `mostPlayedVideo`; no background decoding.
- If both `randomGameVideo` and `mostPlayedVideo` are present, random mode takes priority; use only one in the theme.
- With no videos in the system, the preview is hidden. No periodic auto-advance while remaining on one system.
- The option is currently experimental; test on your actual OpenGL ES device before widespread theme use.

---

## Rounded video previews (TEST)

Video elements support optional curved corners without GPU stencil masks,
shader changes or VLC decode changes. Both properties are fractions of the
**screen width**, matching the rounded image feature in ES-X.

```xml
<view name="video">
  <video name="md_video">
    <origin>0.5 0.5</origin>
    <pos>0.83 0.55</pos>
    <maxSize>0.20 0.30</maxSize>
    <videoCornerRadius>0.010</videoCornerRadius>
    <imageCornerRadius>0.010</imageCornerRadius>
    <showSnapshotDelay>true</showSnapshotDelay>
    <showSnapshotNoVideo>true</showSnapshotNoVideo>
    <iterationCount>1</iterationCount>
    <onIterationsDone>image</onIterationsDone>
  </video>
</view>
```

- `videoCornerRadius`: clips the moving frames from VLC to the rounded
  geometry. The same uploaded frame texture is used, with fixed-function GL
  triangle strips and geometry cached per video size and radius.
- `imageCornerRadius`: clips the static snapshot shown during delay, on
  missing video, or after `onIterationsDone=image`. This is separate from
  the moving frame setting.
- Both are independently optional and default to `0`, leaving all existing
  rectangular video rendering unchanged.
- Also applies to `mostPlayedVideo` and `randomGameVideo` inside the
  `system` view, because they share `VideoVlcComponent`.
- The rounded frame geometry is drawn only when needed. Its dimensions
  follow the final displayed video size (`size` / `maxSize`).
- **TEST (2026-10-28):** XMB `md_video` visual test was reported successful by
  the author. Verify all four corners, the image snapshot after playback, and
  the `randomGameVideo`/`mostPlayedVideo` cases separately. The Orange Pi 4A
  **video** render path and performance are still unverified. This test feature
  is available in `main` by explicit maintainer request; the existing rounded
  **image** support is separately confirmed on desktop and Orange Pi.

---

## Navigation Sounds

Themes can define navigation audio.

```xml
<feature supported="navigationsounds">
  <view name="all">
    <sound name="scroll">
      <path>./audio/scroll.wav</path>
    </sound>
  </view>
</feature>
```

Only `.wav` files are supported.

---

## Gamelist Carousel Mode (ES-X)

ES-X extends `TextListComponent` with an optional carousel mode for game lists.

This allows themes to create modern carousel-based game browsing layouts while still using the classic EmulationStation `textlist` structure.

The carousel can be horizontal or vertical.

### ES-DE-inspired carousel behavior

The ES-X gamelist carousel uses layout and scaling concepts inspired by ES-DE's carousel behavior, including selected-item scaling, neighboring-item spacing, horizontal/vertical orientation, and alignment-aware placement.

ES-X themes are **not directly compatible with ES-DE themes**. The syntax remains based on the classic EmulationStation `textlist` element, with additional ES-X-specific properties.

### Tile and artwork geometry

A useful way to understand the gamelist carousel is to separate the **carousel tile** from the **artwork inside the tile**:

- `logoSize` defines the base tile area for each carousel item.
- `logoScale` controls how much the selected tile grows.
- `logoAlignment` controls how the tile is anchored or positioned.
- Artwork is centered inside the tile.
- `carouselImageFit` controls how the artwork itself is fitted inside that area.

Supported image fit modes:

- `contain` — keeps the complete image visible while preserving aspect ratio.
- `cover` — fills the available tile area and crops overflow while preserving aspect ratio.
- `stretch` — fills the complete area without preserving aspect ratio.

This separation allows carousel alignment to change without incorrectly shifting the internal artwork.

### Horizontal carousel

```xml
<textlist name="gamelist">
  <orientation>horizontal</orientation>
  <carouselMode>true</carouselMode>

  <carouselImage>true</carouselImage>
  <carouselImageType>image</carouselImageType>
  <carouselImageFit>contain</carouselImageFit>

  <logoSize>0.132 0.733</logoSize>
  <logoScale>1.40</logoScale>
  <logoSpacingX>0.13</logoSpacingX>
  <scaledLogoSpacing>0.58</scaledLogoSpacing>

  <maxLogoCount>9</maxLogoCount>
  <minLogoOpacity>0.75</minLogoOpacity>

  <carouselShowText>false</carouselShowText>
</textlist>
```

### Vertical carousel

```xml
<textlist name="gamelist">
  <pos>0.03 0.16</pos>
  <size>0.42 0.74</size>
  <origin>0 0</origin>

  <orientation>vertical</orientation>
  <carouselMode>true</carouselMode>
  <carouselLoop>true</carouselLoop>
  <carouselShowText>false</carouselShowText>

  <carouselImage>true</carouselImage>
  <carouselImageType>image</carouselImageType>
  <carouselImageFit>contain</carouselImageFit>

  <logoAlignment>center</logoAlignment>

  <logoSize>0.30 0.105</logoSize>
  <logoScale>2.25</logoScale>

  <logoOffsetX>-0.06</logoOffsetX>
  <logoOffsetY>0.00</logoOffsetY>

  <logoSpacingY>0.125</logoSpacingY>

  <maxLogoCount>5</maxLogoCount>
  <minLogoOpacity>0.55</minLogoOpacity>
  <scaledLogoSpacing>0.45</scaledLogoSpacing>

  <carouselItemColor>00000000</carouselItemColor>
  <carouselSelectedItemColor>00000000</carouselSelectedItemColor>

  <carouselTextMaxLines>2</carouselTextMaxLines>
</textlist>
```

### Wheel transformation

The optional wheel transformation uses the carousel orientation and rotates every item according
to its distance from the selected item. An origin outside the tile creates the curved wheel path.

```xml
<textlist name="gamelist">
  <orientation>vertical</orientation>
  <carouselMode>true</carouselMode>

  <carouselItemRotation>-7</carouselItemRotation>
  <carouselItemRotationOrigin>4.03 0.5</carouselItemRotationOrigin>
</textlist>
```

Set `carouselItemRotation` to `0` (the default) to retain the regular straight carousel. The same
properties also work with horizontal orientation. `contain` is recommended for wheel artwork;
axis-aligned `cover` clipping is disabled on rotated items so it cannot cut them incorrectly.

### Carousel property reference

| Property | Description |
|---|---|
| `carouselMode` | Enables gamelist carousel mode. |
| `carouselLoop` | Enables continuous wrapping through the game list. |
| `orientation` | Carousel direction: `horizontal` or `vertical`. |
| `maxLogoCount` | Controls the intended number of visible carousel items. |
| `logoSize` | Base tile size relative to the `textlist`. |
| `logoScale` | Scale applied to the selected/center item. |
| `logoAlignment` | Tile alignment: `left`, `center`, `right`, `top`, or `bottom`. |
| `logoSpacing` | Sets X/Y spacing together. |
| `logoSpacingX` | Horizontal item spacing. |
| `logoSpacingY` | Vertical item spacing. |
| `scaledLogoSpacing` | Extra spacing caused by selected-item scaling. |
| `logoPos` / `carouselLogoPos` | Absolute selected-logo anchor inside the SystemView carousel. It does not move or resize the carousel clip. |\n| `logoOffset` | Fine X/Y adjustment of the carousel anchor. |
| `logoOffsetX` | Fine horizontal carousel adjustment. |
| `logoOffsetY` | Fine vertical carousel adjustment. |
| `carouselLogoPos` / `logoPos` | Optional absolute carousel anchor inside the `textlist`; offsets can still be used for fine adjustment. |
| `selectedLogoOffsetX` | Additional horizontal offset applied to the selected item. |
| `selectedLogoOffsetY` | Additional vertical offset applied to the selected item. |
| `minLogoOpacity` | Minimum opacity for non-selected items. |
| `carouselItemColor` | Background color for normal carousel tiles. |
| `carouselSelectedItemColor` | Background color for the selected tile. |
| `carouselShowText` | Shows game names only for items with neither real art nor a fallback. |
| `carouselTextMaxLines` | Maximum title lines; supported range is 1–3. |
| `carouselItemRotation` | Rotation in degrees per item of distance from the selected item; `0` disables the wheel transformation. |
| `carouselItemRotationOrigin` | Normalized rotation origin; values outside `0–1` place the wheel pivot outside the tile. |

### Carousel artwork properties

| Property | Description |
|---|---|
| `carouselImage` | Enables artwork rendering inside carousel items. |
| `carouselImageType` | Selects the artwork source. |
| `carouselImageFit` | Artwork fitting mode: `contain`, `cover`, or `stretch`. |
| `carouselImageCornerRadius` / `imageCornerRadius` | **TEST:** Rounded image crop. Radius as a fraction of **screen width**, e.g. `0.010` = 1%. No shader/stencil; defaults to `0`. The explicit `carouselImageCornerRadius` takes precedence. |
| `carouselImagePadding` | Internal padding between artwork and tile bounds. |
| `carouselImageZIndex` | Internal carousel-layer order for the artwork. Default: `10`. Higher values render later/above lower values. |
| `carouselFallbackImage` | Fallback image used when the requested artwork is missing. |
| `carouselShowFallbackName` | **TEST / opt-in:** overlays the game name on `carouselFallbackImage` only when the game artwork is missing; independent of `carouselShowText` and disabled by default. Real artwork is never labeled. |
| `carouselImageLockSize` | Enables use of a fixed artwork box when `carouselImageBoxSize` is defined. |
| `carouselImageBoxSize` | Optional fixed artwork box relative to the carousel area. |
| `carouselImageBorder` | PNG overlay/frame used for normal carousel items. |
| `carouselImageBorderSelected` | PNG overlay/frame used for the selected item. |
| `carouselImageBorderScale` | Scale of the artwork border/frame relative to the tile. |
| `carouselImageBorderColor` | Tint/color shift applied to the border image. |
| `carouselImageBorderFit` | Border/card PNG fitting mode: `stretch` (legacy default) or `contain` (preserves aspect ratio). |
| `carouselImageBorderZIndex` | Internal carousel-layer order for the border/overlay. Default: `20`. Set it below `carouselImageZIndex` to place the PNG behind the artwork. |

### Game names over fallback images

The system carousel's `showFallbackName` applies to systems, not to games.
For a **gamelist** with `<carouselMode>true</carouselMode>`, use:

```xml
<textlist name="gamelist">
  <carouselMode>true</carouselMode>
  <carouselImage>true</carouselImage>
  <carouselImageType>texture</carouselImageType>
  <carouselFallbackImage>./_inc/physical-media/_default.png</carouselFallbackImage>
  <carouselShowText>false</carouselShowText>
  <carouselShowFallbackName>true</carouselShowFallbackName>
  <carouselTextMaxLines>2</carouselTextMaxLines>
</textlist>
```

- When a game has its own artwork, only that image is rendered.
- When a game has no artwork and the generic PNG exists, the PNG appears
  **with the game's title centered over it** in white, following the item's
  scale, rotation, opacity, and the existing `carouselTextMaxLines` limit.
- If neither image nor PNG exists, the existing `carouselShowText` behavior
  is preserved. `carouselShowFallbackName` alone does not force text-only
  games to appear.
- Default is `false`: existing themes and game lists behave unchanged.
  This opt-in property is available in ES-X `main` since PR #67. It was
  visually validated in Pi-Station-X's **PS4 Carousel Gamelist** with missing
  artwork and existing artwork; other layouts (notably PS3/XMB) have not been
  visually validated with this option. Pi-Station-X activates it **only** in
  `layout/ps4-carrousel gamelist/Swatch.xml`.

Supported `carouselImageType` values:

```text
auto
image
thumbnail
marquee
cover
boxart
screenshot
wheel
texture
fanart
none
```

`none` disables carousel artwork.

### Rounded carousel images (TEST; GL 2.1 and GLES 1.0)

> **TEST (2026-10-27):** desktop visual test passed with PS5 gamelist artwork,
> including the selected card and removal of decorative border PNGs. The
> author also confirmed it worked perfectly on the Orange Pi 4A. The exact
> GL/GLES backend used there was not identified; alternate image fits and
> fallback media still need broader validation.


ES-DE's PS5 theme uses `<imageCornerRadius>0.010</imageCornerRadius>`
for compact cards and `0.015` for larger ones. ES-X's classic
`textlist` carousel now accepts both this alias and the explicit
`carouselImageCornerRadius`:

```xml
<textlist name="gamelist">
  <carouselMode>true</carouselMode>
  <carouselImage>true</carouselImage>
  <carouselImageType>cover</carouselImageType>
  <carouselImageFit>cover</carouselImageFit>
  <carouselImageCornerRadius>0.010</carouselImageCornerRadius>
</textlist>
```

- The **System View** image carousel accepts the same
  `imageCornerRadius` or `carouselImageCornerRadius` option inside
  `<carousel name="systemcarousel">`. Text-only carousel entries are unchanged.
- The artwork and its optional fallback are clipped to rounded corners
  **using texture-mapped geometry**, not a decorative overlay PNG.
- Radius is normalized to screen width, then limited to half the smaller
  visible image dimension. `0` is the original fast four-vertex path.
- `contain` rounds the actual fitted image; `cover` rounds the centered
  cropped image and also works on rotated items; `stretch` rounds the full
  stretched image. PNG frames and borders remain separate and unchanged.
- Applies to images displayed by the game carousel, not to video playback.
  Standalone theme images can also opt in with `<cornerRadius>0.010</cornerRadius>`.
- Uses fixed-function triangle-strip rendering intended for OpenGL 2.1
  and OpenGL ES 1.0. Desktop and Orange Pi 4A visual tests passed, but the
  precise active backend and wider GLES compatibility remain to be checked.
  Edges are polygonal and may show light aliasing at small resolutions.

### Example: framed fanart carousel

```xml
<textlist name="gamelist">
  <orientation>vertical</orientation>
  <carouselMode>true</carouselMode>
  <carouselLoop>true</carouselLoop>

  <carouselImage>true</carouselImage>
  <carouselImageType>fanart</carouselImageType>
  <carouselImageFit>cover</carouselImageFit>

  <carouselImageBorder>./_inc/images/card_border.png</carouselImageBorder>
  <carouselImageBorderSelected>./_inc/images/card_border.png</carouselImageBorderSelected>
  <carouselImageBorderScale>0.965</carouselImageBorderScale>
  <carouselImageBorderColor>FFFFFFFF</carouselImageBorderColor>
  <carouselImageBorderFit>contain</carouselImageBorderFit>

  <!-- Optional internal ordering. Defaults are image=10, border=20. -->
  <carouselImageZIndex>10</carouselImageZIndex>
  <carouselImageBorderZIndex>20</carouselImageBorderZIndex>

  <logoAlignment>left</logoAlignment>
  <logoSize>0.164 0.164</logoSize>
  <logoScale>2.25</logoScale>
  <logoSpacingY>0.25</logoSpacingY>

  <maxLogoCount>3</maxLogoCount>
  <carouselShowText>false</carouselShowText>
</textlist>
```

This example demonstrates an important distinction: `logoAlignment="left"` anchors the **tile**, while the fanart remains centered and cropped inside that tile when `carouselImageFit="cover"` is used.

---

# Theme Options (ES-X)

Themes can expose configurable options in the UI.

Examples:

- Layout selection (`ps4` / `ps3` / `lite`)
- Artwork source selection
- Performance toggles
- Density modes

If a theme defines no options, the Theme Options menu can remain hidden.

---

# `theme.ini` (ES-X)

`theme.ini` is optional but powerful.

It allows themes to:

- centralize configuration
- reduce XML duplication
- control layout variations
- expose user-selectable options

## Syntax

```ini
[section]
key=value
```

Comments:

```ini
; comment
# comment
```

## Using INI values in `theme.xml`

INI keys become variables.

Example:

```xml
<pos>${systemNamePos}</pos>
<maxSize>${mostPlayedMaxSize}</maxSize>
```

## Layout Sections

Common structure:

```ini
[layout.ps4]
[layout.ps3]
[layout.lite]
```

Theme Options selects which section becomes active.

---

# Clock Element (ES-X)

ES-X includes a built-in system clock that can be styled by themes.

The clock is controlled from:

```text
UI Settings → Clock
UI Settings → Clock Format
```

Themes control visual styling only, not time format.

## Theme Definition

The clock is defined as a text element inside the screen view.

Example:

```xml
<view name="screen">
  <text name="clock">
    <pos>0.984 0.03</pos>
    <origin>1 0.5</origin>
    <color>FFFFFFFF</color>
    <fontPath>./_inc/fonts/Exo2-Medium.ttf</fontPath>
    <fontSize>0.018</fontSize>
  </text>
</view>
```

### Properties

| Property | Description |
|---|---|
| `pos` | Normalized screen position |
| `origin` | Anchor point |
| `color` | Text color |
| `fontPath` | Optional font |
| `fontSize` | Optional size |

If the clock element is not defined, ES-X uses an internal fallback position.

## Typical Placement

Most console-style themes place the clock in the top-right corner.

```xml
<pos>0.984 0.03</pos>
<origin>1 0.5</origin>
```

## Format Handling

Clock format is controlled by the user.

| Setting | Output |
|---|---|
| 24H | `18:42` |
| 12H | `6:42 PM` |

Themes cannot override the time format.

## Rendering Model

Although declared as:

```xml
<text name="clock">
```

the clock is rendered internally by ES-X.

This provides:

- smooth updates (once per second)
- minimal CPU usage
- consistent behavior across themes

---

# Design Philosophy

ES-X separates responsibilities:

```text
theme.xml     → structure
theme.ini     → configuration
Theme Options → user experience
```

This makes themes:

- easier to maintain
- easier to expand
- easier to reuse
- adaptable to different hardware

Modern themes are not about complexity.

They are about flexibility without chaos.

---

# Reference

## Property Types

```text
RESOLUTION_RECT
RESOLUTION_PAIR
RESOLUTION_FLOAT
NORMALIZED_PAIR
PATH
STRING
COLOR
FLOAT
BOOLEAN
```

## Common Element Types

```text
image
text
textlist
video
carousel
rating
datetime
helpsystem
sound
ninepatch
imagegrid
```

---

## Dynamic player-count image (ES-X)

A theme may opt into a **per-game player badge** in the `detailed`, `video`,
or `grid` view. Declare a regular `image` called `md_players_icon`:

```xml
<view name="detailed, video">
  <image name="md_players_icon">
    <origin>0 0.5</origin>
    <pos>0.733 0.8095</pos>
    <maxSize>0.032 0.032</maxSize>
    <path>./resources/systemicons/{game:players}p.png</path>
    <default>./resources/icons/Controllers.png</default>
    <zIndex>105</zIndex>
  </image>
</view>
```

On selection changes, ES-X resolves **only** `{game:players}` in this image's
path against the game's existing `players` gamelist metadata. For example:
`2` loads `2p.png`; a range such as `1-4` uses `4p.png`. Missing or
unrecognized metadata, and missing icons, use `<default>` when configured.
The unmodified `md_players` text remains available for themes that want it.

This is opt-in. Other `image` elements and all existing theme paths are
unaffected. ES-X does not implement general Batocera expression bindings.

---

## TEST: system hardware tabs (ES-X)

An opt-in `<text extra="true">` in the `system` view can be highlighted
once per system without Batocera's expression evaluator:

```xml
<text name="platform_console" extra="true">
    <pos>0.24 0.041</pos>
    <size>0.11 0.05</size>
    <text>Consoles</text>
    <fontPath>./resources/fonts/OpenSans-Light.ttf</fontPath>
    <color>BEBEBEFF</color>
    <activeFor>console</activeFor>
    <activeFontPath>./resources/fonts/OpenSans-SemiBold.ttf</activeFontPath>
    <activeColor>FFFFFFFF</activeColor>
</text>
```

Supported: `arcade`, `computer`, `console`, `extension`, `pinball`,
`port`, `portable`. ES-X chooses the category using RetroPie special system
names, optional theme `systemHardwareType` variables (PS5/Swatch-style),
`<platform>` IDs, and common system aliases. Generic `Folder` /
`Collection` types are not assumed to be a hardware category. An unknown
system keeps all labels in their inactive style. No runtime polling.

**TEST:** visual placement, edge-case platform classifications and build
behavior are pending evaluation on PC / Orange Pi 4A.
