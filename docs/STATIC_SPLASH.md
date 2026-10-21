# Native theme loading splash

ES-X can read a `splash.xml` from the root of the selected theme. The splash
is implemented as a normal `ThemeData` view, following the same architecture
used by Tarave/Batocera rather than maintaining a second splash-only parser.

## Target format

A theme may define:

- `<view name="splash">`
- `<splash name="splash">` with `backgroundColor`
- `<image name="background">`
- `<text name="label">` for ES-X's live loading text
- `<image name="progressbar">`
- `<image name="progressbar:active">`
- ordinary image/text extras with `extra="true"`, ordered by `zIndex`

The progress bar accepts a real image path as well as solid colors. This is
important for PiStation-X, whose startup splash uses
`./main/media/progress.png`.

The splash components are force-loaded during startup and released explicitly
when startup loading finishes. The classic ES-X splash remains the fallback
when the selected theme has no usable `splash.xml`.

## Compatibility scope

The first compatibility target is PiStation-X's standalone `splash.xml`,
which uses normal theme properties and relative resources.

This does **not** yet make ES-X fully compatible with Batocera's advanced
theme-option/subset language. Themes such as newer PlayStation-X builds may
reference `${subset.*}`, conditional `ifSubset` expressions, Batocera
globals, glow properties, and other syntax not currently implemented by
ES-X's normal theme parser. Such themes need either an adapted splash or later
theme-engine support; they fall back to the classic ES-X splash instead of
breaking startup.

## Manual verification

1. Build the draft branch and select PiStation-X.
2. Start ES-X and verify the theme background and themed progress bar appear.
3. Confirm the live loading label is visible.
4. Remove/rename the theme's `splash.xml` and verify the classic ES-X splash returns.
5. Test again on Orange Pi ARM64/GLES after desktop validation.

The parser regression test is available through
`tests/run_theme_splash_parser.sh`. An optional second argument can point to a
local `splash.xml` such as PiStation-X for an additional parser check.
