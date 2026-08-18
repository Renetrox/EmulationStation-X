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

Depth and spacing can be fully adjusted without modifying engine code.

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
| `logoOffset` | Fine X/Y adjustment of the carousel anchor. |
| `logoOffsetX` | Fine horizontal carousel adjustment. |
| `logoOffsetY` | Fine vertical carousel adjustment. |
| `carouselLogoPos` / `logoPos` | Optional absolute carousel anchor inside the `textlist`; offsets can still be used for fine adjustment. |
| `selectedLogoOffsetX` | Additional horizontal offset applied to the selected item. |
| `selectedLogoOffsetY` | Additional vertical offset applied to the selected item. |
| `minLogoOpacity` | Minimum opacity for non-selected items. |
| `carouselItemColor` | Background color for normal carousel tiles. |
| `carouselSelectedItemColor` | Background color for the selected tile. |
| `carouselShowText` | Shows or hides game names inside carousel items. |
| `carouselTextMaxLines` | Maximum title lines; supported range is 1–3. |

### Carousel artwork properties

| Property | Description |
|---|---|
| `carouselImage` | Enables artwork rendering inside carousel items. |
| `carouselImageType` | Selects the artwork source. |
| `carouselImageFit` | Artwork fitting mode: `contain`, `cover`, or `stretch`. |
| `carouselImagePadding` | Internal padding between artwork and tile bounds. |
| `carouselFallbackImage` | Fallback image used when the requested artwork is missing. |
| `carouselImageLockSize` | Enables use of a fixed artwork box when `carouselImageBoxSize` is defined. |
| `carouselImageBoxSize` | Optional fixed artwork box relative to the carousel area. |
| `carouselImageBorder` | PNG overlay/frame used for normal carousel items. |
| `carouselImageBorderSelected` | PNG overlay/frame used for the selected item. |
| `carouselImageBorderScale` | Scale of the artwork border/frame relative to the tile. |
| `carouselImageBorderColor` | Tint/color shift applied to the border image. |

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
