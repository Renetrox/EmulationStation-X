// Lightweight C++11 regression checks for Issue #19 and genre auto collections.
#include "../es-app/src/GenreCollections.h"
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>
#include <vector>

static bool contains(const std::vector<std::string>& names, const std::string& needle)
{
    return std::find(names.begin(), names.end(), needle) != names.end();
}

int main()
{
    using GenreCollections::classify;
    using GenreCollections::matches;

    // DjDiabolik's original request: existing gamelist.xml tags.
    assert(matches("Puzzle", "genre-puzzle"));
    assert(matches("Action", "genre-action"));
    assert(matches("Sports", "genre-sports"));
    assert(!matches("Puzzle", "genre-action"));

    // Metadata from multilingual scrapers and hierarchical subgenres.
    assert(matches("Acción / Aventura", "genre-action"));
    assert(matches("Acción / Aventura", "genre-adventure"));
    assert(matches("Action / Platform", "genre-platform"));
    assert(matches("Carreras, Conducción", "genre-racing"));
    assert(matches("Jeux de rôle", "genre-rpg"));
    assert(matches("Música y Baile", "genre-music"));
    assert(matches("Adventure / Survival Horror", "genre-horror"));
    assert(matches("Action / Beat'em Up", "genre-fighting"));
    assert(matches("Shoot'em Up", "genre-shooter"));

    // Do not rely on simple substrings: avoid spurious classification.
    assert(!matches("Interaction", "genre-action"));
    assert(!matches("Airport", "genre-rpg"));
    assert(classify("").empty());
    assert(classify("unknown").empty());

    auto many = classify("Action / Adventure");
    assert(contains(many, "genre-action") && contains(many, "genre-adventure"));
    assert(!contains(many, "genre-racing"));

    std::cout << "Genre collection classifier: OK\n";
    return 0;
}
