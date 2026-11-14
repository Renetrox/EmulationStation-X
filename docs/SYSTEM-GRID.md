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
