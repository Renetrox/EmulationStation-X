# Automatic genre collections (experimental, Issue #19)

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
- Optional theme art folders use `auto-` names, e.g. `auto-puzzle`.
  Missing dedicated art falls back to the existing theme behavior.
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
