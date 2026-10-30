#pragma once
#ifndef ES_APP_GENRE_COLLECTIONS_H
#define ES_APP_GENRE_COLLECTIONS_H

// Compact, dependency-free genre mapping for ES-X. Unlike Batocera's large
// genre hierarchy, only commonly used top-level categories are exposed as
// collections. Scraper subgenres and localized labels map to those categories.
#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace GenreCollections
{
struct Category
{
    const char* name;        // Stable setting/collection ID.
    const char* label;       // Translatable display label.
    const char* themeFolder; // Optional theme-specific art.
    const char* aliases;     // Case/accent insensitive phrases, separated by '|'.
};

inline const std::vector<Category>& categories()
{
    static const std::vector<Category> list = {
        { "genre-action",      "ACTION",       "auto-action",      "action|accion|acao|action adventure|run and gun" },
        { "genre-adventure",   "ADVENTURE",    "auto-adventure",   "adventure|aventura|aventure|abenteuer|action adventure|point and click|visual novel|interactive movie" },
        { "genre-platform",    "PLATFORM",     "auto-platform",    "platform|platformer|platform games|plataforma|plataformas|plateforme|jump and run|jump n run" },
        { "genre-puzzle",      "PUZZLE",       "auto-puzzle",      "puzzle|puzzle game|puzzle games|puzle|rompecabezas|quebracabeca|reflexion|logic|labyrinth" },
        { "genre-racing",      "RACING",       "auto-racing",      "racing|race|race driving|race driving games|driving|carreras|conduccion|course|corrida" },
        { "genre-sports",      "SPORTS",       "auto-sports",      "sports|sport|deportes|deporte|esportes|sports with animals" },
        { "genre-rpg",         "RPG",          "auto-rpg",         "rpg|jrpg|role playing|role playing game|role playing games|roleplaying|juegos de rol|jeu de role|jeux de role" },
        { "genre-shooter",     "SHOOTER",      "auto-shooter",     "shooter|shooters|shoot em up|shoot em up games|shmup|fps|first person shooter|third person shooter|disparos|tiros|run and gun" },
        { "genre-fighting",    "FIGHTING",     "auto-fighting",    "fighting|fight|fighter|lucha|combat|beat em up|beat them up|beat em all" },
        { "genre-strategy",    "STRATEGY",     "auto-strategy",    "strategy|estrategia|strategie|rts|real time strategy|turn based strategy" },
        { "genre-simulation",  "SIMULATION",   "auto-simulation",  "simulation|simulacion|simulator|simulador" },
        { "genre-horror",      "HORROR",       "auto-horror",      "horror|survival horror|terror|survie horreur" },
        { "genre-music",       "MUSIC",        "auto-music",       "music|musica|dance|baile|music and dance|musique" },
        { "genre-educational", "EDUCATIONAL",  "auto-educational", "educational|education|educacion|educacional|educativo|quiz|trivia" },
        { "genre-pinball",     "PINBALL",      "auto-pinball",     "pinball|flipper" }
    };
    return list;
}

// Only ASCII is passed to std::tolower; common UTF-8 Latin accents are folded.
inline std::string normalize(const std::string& value)
{
    std::string out;
    for (size_t i = 0; i < value.size(); ++i)
    {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        char ch = 0;
        if (c == 0xC3 && i + 1 < value.size())
        {
            const unsigned char n = static_cast<unsigned char>(value[++i]);
            if (n == 0x81 || n == 0x80 || n == 0x82 || n == 0x84 || n == 0xA1 || n == 0xA0 || n == 0xA2 || n == 0xA4) ch = 'a';
            else if (n == 0x89 || n == 0x88 || n == 0x8A || n == 0x8B || n == 0xA9 || n == 0xA8 || n == 0xAA || n == 0xAB) ch = 'e';
            else if (n == 0x8D || n == 0x8C || n == 0x8E || n == 0x8F || n == 0xAD || n == 0xAC || n == 0xAE || n == 0xAF) ch = 'i';
            else if (n == 0x93 || n == 0x92 || n == 0x94 || n == 0x96 || n == 0xB3 || n == 0xB2 || n == 0xB4 || n == 0xB6) ch = 'o';
            else if (n == 0x9A || n == 0x99 || n == 0x9B || n == 0x9C || n == 0xBA || n == 0xB9 || n == 0xBB || n == 0xBC) ch = 'u';
            else if (n == 0x91 || n == 0xB1) ch = 'n';
            else if (n == 0x87 || n == 0xA7) ch = 'c';
        }
        else if (c < 128 && std::isalnum(c))
            ch = static_cast<char>(std::tolower(c));

        if (ch)
            out += ch;
        else if (!out.empty() && out.back() != ' ')
            out += ' ';
    }
    if (!out.empty() && out.back() == ' ')
        out.pop_back();
    return out;
}

// A game may belong to several categories (e.g. "Action / Adventure").
// Matching uses word boundaries, not raw substring searches.
inline std::vector<std::string> classify(const std::string& genre)
{
    const std::string text = " " + normalize(genre) + " ";
    std::vector<std::string> matches;
    if (text == "  " || normalize(genre) == "unknown")
        return matches;

    for (const auto& category : categories())
    {
        const std::string aliases = category.aliases;
        size_t start = 0;
        while (start <= aliases.size())
        {
            const size_t end = aliases.find('|', start);
            const std::string alias = normalize(aliases.substr(start, end == std::string::npos ? end : end - start));
            if (!alias.empty() && text.find(" " + alias + " ") != std::string::npos)
            {
                matches.push_back(category.name);
                break;
            }
            if (end == std::string::npos)
                break;
            start = end + 1;
        }
    }
    return matches;
}

inline bool matches(const std::string& genre, const std::string& collectionName)
{
    const auto groups = classify(genre);
    return std::find(groups.begin(), groups.end(), collectionName) != groups.end();
}
} // namespace GenreCollections

#endif // ES_APP_GENRE_COLLECTIONS_H
