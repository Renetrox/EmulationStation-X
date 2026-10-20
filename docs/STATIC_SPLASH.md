# Static theme loading splash

Put `splash.xml` in the selected theme's root directory. The configuration is
read independently before the systems are scanned. No per-system theme needs
to be loaded first. See [the example](examples/splash.xml).

Supported elements in `<view name="splash">`:

- `<splash name="splash">`: `backgroundColor`.
- `<image name="background">`: image, position, origin, size/maxSize/minSize.
- `<text name="label">`: live loading text, position, size, font and color.
  The text comes from ES-X and cannot be replaced by the XML.
- `<image name="progressbar">` and `<image name="progressbar:active">`:
  solid rectangles with position, size, origin, rotation, color/colorEnd and
  gradientType. Image textures on these two bars are not supported.
- Extra static images/text (`extra="true"` or `extra="static"`), ordered by
  `zIndex`. Text can be vertically aligned with `verticalAlignment` on `label`.

The active bar inherits the inactive bar's geometry, then applies its own
properties. Progress changes its visible width without moving its origin.
Negative progress hides both bars. Values above one are clamped.

Riescade's standalone `splash.xml` is the reference: its full-height progress
rectangles must remain behind its extra `resources/_parts/splash.png` image.
The renderer uses stable ascending zIndex order, preserving ties.

This is static support, not full Batocera theme compatibility. Glow, shadows,
smoothing options and unknown properties are ignored only by this loader.
Pulse/frame-animation activation and text strokes are disabled. Conditional
nodes (`ifSubset`, `if`, `tinyScreen`, region) are skipped; use a standalone,
adapted XML with explicit choices instead of relying on Batocera options.
`${themePath}` is supplied, and ES-X's existing variable/include and theme.ini
loading remain available. Native PlayStation-X imports its full theme and
requires adaptation of its subset variables and conditional paths. Unresolved
variables, invalid XML and a missing background image fall back to the original
ES-X splash. Missing extra images follow the existing image-loader behavior.
Other views are skipped while parsing splash includes; normal theme parsing
remains strict. Include depth is bounded in splash mode.

Resources are created once per loading operation, loaded synchronously and
released when normal rendering resumes, at the end of startup or on renderer
shutdown. No video, animation loop, external process or timed delay is added.
The original splash remains available for themes without a valid splash.xml.

## Manual verification

1. Start with a theme without splash.xml: original ES-X splash should appear.
2. Install the example and your image: verify text, aspect ratio and progress.
3. Test Riescade with its original resources: verify its mask/layer composition.
4. Try a malformed XML, unresolved variable and missing background: verify a
   warning and the original splash, without interrupting startup.
5. Reload/change themes and return from a game: verify the new theme is used on
   the next loading operation and no splash resources survive normal rendering.
6. Test 16:9 and 4:3 on the target renderer, especially the Orange Pi GLES path.

## Automated parser checks

On Linux with the normal SDL2/FreeType development dependencies and initialized
submodules, run `tests/run_theme_splash_parser.sh`. Optionally pass the local
path to Riescade's original splash.xml to check that reference too.

Development validation: full Linux desktop OpenGL build passed. Offscreen Mesa
pixel checks passed for partial/full/hidden progress, centered bar origin and
foreground image layering; malformed XML fell back to the original splash.
Texture size returned to its pre-splash baseline after destruction. Target
Orange Pi/GLES and original Riescade artwork still need visual verification.
