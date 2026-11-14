# ES-X: system grid (experimental)

Branch: `test/system-grid-view-20261114`

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

## Test checklist (before merging)

- [ ] Compile ES-X on desktop and Orange Pi 4A.
- [ ] Verify old horizontal and vertical themes behave exactly as in main.
- [ ] Verify 4×3 and 3×3 system grids, including an incomplete final row.
- [ ] Verify game launch, back, favorite/automatic collections, and random selection.
- [ ] Verify logos (SVG/PNG), fallback and missing logo text.
- [ ] Verify transition/background extras and video at system switching.
- [ ] Observe VRAM use and scrolling behavior on the Orange Pi 4A.
- [ ] Adapt BatWiiCera's layout/assets and check its CC BY-NC-SA 4.0 credits.

This branch has not been compiled or tested on an end-user device yet. Do not merge to main before validating the checklist.

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
- Add visible `<image name="md_image">` with image-space dimensions for
  both `detailed` and `video` views. The original theme's shared
  rules hid both `md_image` and `md_thumbnail`.
- Place `md_video` only in the `video` view above the image,
  and use a distinct `extra="true"` static image for missing media.
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

Check language switching (`es`, `en`, `pt`, `gn_PY`), missing logos,
genre collections and card selection on desktop and Orange Pi 4A before merging.
