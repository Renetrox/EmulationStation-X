# Automatic genre collections (ES-X main; Issue #19)

This implementation is native to ES-X and preserves RetroPie's existing gamelists,
ROM directories, custom collections and existing per-system filters.

## Using it

1. Scrape games or provide a stock RetroPie `gamelist.xml` with `<genre>` metadata.
2. Enter **Main Menu → Game Collection Settings → Genre Collections**.
3. Select categories such as **Puzzle**, **Action**, **Sports** or **Racing**.
4. Return to the systems carousel. Only enabled, non-empty genre collections
   appear, combining games from all consoles.
5. To filter **within one console** (the exact request in Issue #19), open that
   console, enter **Options → Filter Gamelist → Genre**. This existing ES-X
   feature works independently of automatic collections.

Genres are mapped using case-insensitive, accent-insensitive aliases and common
subgenre phrases (e.g. `Action / Adventure`, `Carreras, Conducción`).
One game may belong to more than one collection.

## Compatibility and configuration

- Automatic collection IDs use the `genre-` prefix, e.g. `genre-puzzle`.
- Automatic collection IDs are independent of visual identifiers. Optional theme
  art keys use Batocera-compatible `auto-` names wherever practical, such as
  `auto-puzzle`. In particular, the stable settings IDs `genre-racing`,
  `genre-rpg`, `genre-fighting`, and `genre-music` load artwork through
  `auto-racedriving`, `auto-roleplayings`, `auto-fight`, and
  `auto-musicanddance`, respectively. Existing enabled selections do not change.
- Pi-Station-X already ships PNG covers and JPG backgrounds for many supported
  genre IDs, referenced by `${system.theme}`. Some new genres have no dedicated
  cover, so the theme uses a generic logo plus the localized collection name.
  The compatibility fixes for missing per-system metadata XML files and the
  generic logo are included in Pi-Station-X `main` (PRs #4 and #5).
- Themes without matching artwork retain their existing fallback. ES-X already
  supports a generic logo plus caption when a theme provides a `logo`
  `<default>` and enables `<showFallbackName>true</showFallbackName>` in its
  system carousel. This change does not add assets or require every theme to
  configure that optional fallback.
- Selected IDs are stored with existing automatic collections in
  `CollectionSystemsAuto` (no new settings format).
- Genres without metadata remain outside genre collections.
- Categories are **disabled by default**.
- The first enabled genre batch is populated in a single scan of game systems.
  Changing metadata updates collection membership, and manual **Recreate
  Collection** rebuilds the selected virtual collection.
- Collection entries are virtual references. ROM files and `gamelist.xml` are
  not duplicated or rewritten by the classification itself.
- All existing custom collections and random collections remain unchanged.

### Optional per-system metadata includes (Pi-Station-X)

Some Batocera-derived themes, including Pi-Station-X, use both a common
`_default.xml` and a system-specific metadata include. Most original console
and legacy automatic-collection metadata files exist, but freshly introduced
`auto-action`, `auto-horror`, etc. metadata XML files do not. Historically ES-X
throws on **every** missing `<include>`, aborting that system's entire theme
load; this produces unstyled, oversized `genre-*` text instead of any logos.

The theme parser now supports explicit `optional="true"` on an include in **both**
its variable and view passes. Mark only the optional second line in the theme's
`Swatch.xml`:

```xml
<include>./_inc/metadata-global/_default.xml</include>
<include optional="true">./_inc/metadata-global/${system.theme}.xml</include>
```

Existing collection and console metadata still loads when present, while an
unknown system now keeps the default metadata and all graphics. Missing
**required** includes remain errors; do not change the default file to optional.
This is available in ES-X `main` (PR #66) and the corresponding Pi-Station-X
layout templates already contain the change (PRs #4 and #5). For other themes,
mark the system-specific include as optional only when appropriate.
A valid `<default>` in the system `logo` and `showFallbackName` remain the
separate mechanism for a genuinely missing carousel cover image.

ES-X also exposes `${systemMetaLine}` for a clean combination of the available
hardware type, release year and manufacturer. Missing fields are omitted rather
than leaving literal placeholders or dangling separators. In Pi-Station-X a
new genre collection normally shows only **Colección**; traditional consoles
retain their actual year and manufacturer.

### Pi-Station-X generic cover fallback

Pi-Station-X's `theme.xml` references
`./_inc/caratulas/${system.theme}.png` as its normal `logo`.
For any category without a dedicated image (e.g. `auto-horror`), add a
`<default>./_inc/caratulas/default.png</default>` **inside that same
`<image name="logo">`**. This invokes the existing `ImageComponent` fallback;
a separate full-screen `<image extra="true">` does not act as a carousel logo.

In the active `<carousel name="systemcarousel">` (usually `Swatch.xml` for
the default Pi-Station-X layout), add
`<showFallbackName>true</showFallbackName>`. Configure optional
`<text name="logoText">` within the system view (for example, normalized
`<pos>0.5 0.5</pos>`, `<size>0.9 0.32</size>`, `<color>FFFFFFFF</color>`).
Only the generic fallback receives that text; existing genre-specific PNG covers
remain unchanged. ES-X now uses the genre collection's localized `fullName`
instead of exposing raw `genre-*` internal IDs for both image+caption and
plain text-only fallbacks.

This also works for other themes opting into the same existing logo-fallback
interface, with no changes to collection creation or metadata matching.

**Do not confuse system and game fallbacks:** `<showFallbackName>` belongs to
`systemcarousel`. To label games over a generic `carouselFallbackImage` in a
`textlist` gamelist, the separate `carouselShowFallbackName` option is now in
ES-X `main` (PR #67). Pi-Station-X enables that game-level option only in its
**PS4 Carousel Gamelist** layout (PR #7). Other layouts retain their usual
behavior. See [THEMES.md](../THEMES.md#game-names-over-fallback-images) for XML.

## Testing

Run a standalone classifier regression test:

```sh
g++ -std=c++11 -Wall -Wextra -Werror -pedantic \
  tests/test_genre_collections.cpp -o /tmp/esx-genres
/tmp/esx-genres
```

The companion GitHub Actions workflow also compiles the whole ES-X frontend.

### Manual QA for RetroPie

- With just GB games labeled `Puzzle` and `Action`, confirm **Options →
  Filter Gamelist → Genre** contains the expected original genre values.
- Enable Puzzle and Action collections; verify games appear only in the
  appropriate collections, and empty genres do not appear.
- Check an `Action / Adventure` game belongs to both categories.
- Change a game's genre in **Edit Metadata**, then verify membership updates;
  also test removing its genre.
- Disable a genre and re-enter settings; confirm the choice persists.
- Check themes with and without `auto-puzzle`/genre-specific assets,
  including Pi-Station-X, Riescade and PS5.
- Check launch/return via RetroPie runcommand and use with a gamepad.

*Note:* This curated initial set of 15 categories is intentionally smaller than
Tarave's full multi-level genre taxonomy, to avoid a sprawling systems carousel.
Additional aliases and categories can be added to `GenreCollections.h`.
