# ES-X: system grid (opt-in)

Introduced by [PR #73](https://github.com/Renetrox/EmulationStation-X/pull/73) after desktop testing of BatWiiCera; only themes declaring a system `<imagegrid>` enable it.

The system selector now supports an **opt-in grid** in addition to the existing horizontal/vertical carousels, wheel layouts and textlist. It does not change the gamelist grid.

## Activation

Add this to the **system** view of an ES-X theme. Existing themes without a system `imagegrid` remain untouched.

```xml
<view name="system">
    <imagegrid name="imagegrid">
        <pos>0.075 0.06</pos>
        <size>0.85 0.635</size>
        <autoLayout>4 3</autoLayout>
        <autoLayoutSelectedZoom>1.05</autoLayoutSelectedZoom>
        <margin>0.014 0.028</margin>
        <padding>0.015 0.015 0.015 0.015</padding>
        <zIndex>10</zIndex>
    </imagegrid>

    <gridtile name="default">
        <backgroundColor>D8D8D8FF</backgroundColor>
        <imageColor>FFFFFFFF</imageColor>
    </gridtile>
    <gridtile name="selected">
        <backgroundColor>FFFFFFFF</backgroundColor>
        <imageColor>FFFFFFFF</imageColor>
    </gridtile>
</view>
```

The grid uses each system's existing `<image name="logo">` image and `default` fallback. For missing logos, its full name appears over an optional fallback image. The grid loads only visible tile components, not one component per system.

### Layout

* 16:9: `<autoLayout>4 3</autoLayout>` is the recommended starting point.
* 4:3 or 800×600: `<autoLayout>3 3</autoLayout>` allows larger buttons.
* Four-way navigation uses D-pad/stick; A enters the selected system; X chooses a random system. Where system information is configured, Y opens the information focus, B returns and A opens it.
* Up/down scrolls one whole row, left/right moves inside the current row. The incomplete last row is handled without entering an invalid system.

### Implementation

* `es-app/src/views/SystemView.cpp` selects the grid when the system view defines `imagegrid` and continues to own a single `SystemData*` cursor.
* Reuses existing `GridTileComponent` for tile backgrounds, image rendering and selected zoom.
* A dedicated system tile adapter avoids coupling to `ImageGridComponent<FileData*>` and its `getType()` handling.
* The theme parser already knows `imagegrid` and `gridtile`, so it needs no new XML property.

## Validation and remaining coverage

On 2026-11-14 the maintainer compiled ES-X on a desktop system and interactively
validated system grid navigation, localized system logos, fallback label visibility,
BatWiiCera's media previews, `defaultSnapshot`, and help display.
The original Wii-style gray tile relief was intentionally preserved.

The following are **not yet verified** by those tests:
- Compilation and performance on Orange Pi 4A (including 800×600).
- Runtime display of the proposed 4:3 layout (3×3 system grid) on the target monitor.
- Regression sweep of all horizontal/vertical themes and incomplete-last-row cases.
- Resource use (VRAM) and extended stability on SBC hardware.

The 16:9 and 4:3 BatWiiCera theme XML and its copyrighted assets are distributed
separately from the ES-X engine PR. Keep the original theme license/credits.

## BatWiiCera adapter notes

BatWiiCera's upstream `theme.xml` lives at the theme root, but ES-X currently loads
`<ThemeSet>/<system.theme>/theme.xml`. An adapted theme must provide a system
folder for each active system (including collections), while shared assets can
remain at the theme root and be referenced using `../_inc/...`.

ES-X resolves each `${variable}` only once, so a nested path variable such as
`${logoPath}` whose value itself contains `${system.theme}` should be inlined:

```xml
<image name="logo">
    <path>../_inc/systems/logos/${system.theme}.svg</path>
    <default>../_inc/systems/logos/${system.theme}.png</default>
</image>
```

The uploaded BatWiiCera source is v0.1.6; the original art and licensing must
be kept (CC BY-NC-SA 4.0). The prototype does not implement Batocera `subset`
menus, pointer overlays or online channels; these are theme portability limits,
not prerequisites for native system Grid.


## BatWiiCera multimedia views in ES-X

BatWiiCera's Batocera layout originally placed `<video name="md_video">` in
`<view name="detailed">`. ES-X's DetailedGameListView has no video component:
the `md_video` element must instead be defined in a `<view name="video">`.

For an ES-X adaptation:
- Include `video` in the shared browsing/metadata views and copy the
  `detailed` layout to `video`.
- Define `<image name="md_image">` for `detailed` with a conditional `<default>` image, and hide `md_image` in the `video` view to prevent overlap.
- Place `md_video` only in the `video` view. Set `<defaultSnapshot>` to the fallback artwork and enable `<showSnapshotNoVideo>` and `<showSnapshotDelay>` rather than adding a permanent static `extra` image.
- Do **not** put `no-preview.png` in the video's `<default>`
  property; that property means a default *video*.
- ES-X picks VideoGameListView automatically only if its system has an actual video
  and a `video` theme view. Otherwise it may choose DetailedGameListView
  or BasicGameListView depending on game metadata and settings.
- Visible system labels below the clock are independent of the artwork inside
  a system grid tile. With the current grid code, a label is overlaid on the tile
  only if neither the system's logo nor its fallback image exists.

The tested visual XML adaptation is independent from ES-X's C++ changes and
does not require updating the code for old themes.


## Localized logos and grid fallback label visibility

The optional native system grid now checks for language-specific variants of the
declared `<image name="logo">` path and its `<default>` path:

1. Region-specific variant (e.g. `auto-favorites-es_PY.svg`).
2. Base-language variant (e.g. `auto-favorites-es.svg`).
3. Declared original logo (e.g. `auto-favorites.svg`).
4. Declared fallback image (e.g. `auto-favorites.png`).

SVG and PNG language variants are both supported. Only system Grid changes; the
existing horizontal/vertical carousels retain their historical resolution rules.

The fallback system name is drawn **only** if no logo image was found, **and**
if the theme allows a text label. BatWiiCera explicitly disables tile labels:

```xml
<view name="system">
    <text name="gridtile">
        <visible>false</visible>
    </text>
</view>
```

This prevents a name being painted over a channel's artwork. Themes omitting
that setting still receive readable name-only tiles when media is missing.

Further validation recommended: language switching (`es`, `en`, `pt`, `gn_PY`), missing logos, genre collections and card selection on Orange Pi 4A.


## Conditional "No preview available" artwork

For BatWiiCera, a static `<image name="preview-fallback" extra="true">`
is incorrect: extras do not know if the selected game has video or art and
therefore remain visible all the time. **Remove that extra image.**

ES-X now supports an opt-in `<defaultSnapshot>` property for
`<video>` elements, which supplies the default image to the
video player's built-in snapshot component (not the `<default>` video path):

```xml
<view name="detailed, video">
    <image name="md_image">
        <pos>0.695 0.28</pos><origin>0.5 0.5</origin>
        <maxSize>0.50 0.40</maxSize>
        <default>./_inc/images/no-preview.png</default>
        <zIndex>12</zIndex>
    </image>
</view>
<view name="video">
    <image name="md_image"><visible>false</visible></image>
    <video name="md_video">
        <pos>0.695 0.28</pos><origin>0.5 0.5</origin>
        <maxSize>0.50 0.40</maxSize>
        <delay>0.3</delay>
        <defaultSnapshot>./_inc/images/no-preview.png</defaultSnapshot>
        <showSnapshotNoVideo>true</showSnapshotNoVideo>
        <showSnapshotDelay>true</showSnapshotDelay>
        <zIndex>14</zIndex>
    </video>
</view>
```

The DetailedGameListView uses `md_image` and its `<default>`
fallback when artwork is missing. The VideoGameListView hides `md_image`
and uses one video component for both the thumbnail and the movie: snapshot
during delay; movie during playback; thumbnail or `defaultSnapshot` only when
there is no playable video. Existing themes without the new property retain
their behavior. Validate the theme asset paths and playback behavior on each target device.
