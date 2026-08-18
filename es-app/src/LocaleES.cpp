#include "LocaleES.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "Log.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

// --------------------
// Helpers
// --------------------
namespace
{
    static inline void trim(std::string& s)
    {
        size_t start = 0;
        while (start < s.size() &&
               (s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n'))
            ++start;

        size_t end = s.size();
        while (end > start &&
               (s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n'))
            --end;

        s = s.substr(start, end - start);
    }

    static inline std::string toLowerCopy(const std::string& s)
    {
        std::string out = s;
        std::transform(out.begin(), out.end(), out.begin(),
            [](unsigned char c) { return (char)std::tolower(c); });
        return out;
    }

    static inline std::string normalizeKey(std::string key)
    {
        trim(key);

        // compactar espacios internos múltiples a uno solo
        std::string compact;
        compact.reserve(key.size());

        bool lastWasSpace = false;
        for (char ch : key)
        {
            bool isSpace = (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n');
            if (isSpace)
            {
                if (!lastWasSpace)
                    compact += ' ';
                lastWasSpace = true;
            }
            else
            {
                compact += ch;
                lastWasSpace = false;
            }
        }

        trim(compact);
        return toLowerCopy(compact);
    }
}

// --------------------
// Singleton
// --------------------
LocaleES& LocaleES::getInstance()
{
    static LocaleES instance;
    return instance;
}

LocaleES::LocaleES()
    : mCurrentLanguage("en")
{
}

// --------------------
// Lectura de archivos .ini
// --------------------
bool LocaleES::loadLanguageFile(const std::string& filePath)
{
    std::ifstream in(filePath);
    if (!in.is_open())
    {
        LOG(LogWarning) << "LocaleES: could not open language file: " << filePath;
        return false;
    }

    std::string line;
    int loaded = 0;

    while (std::getline(in, line))
    {
        trim(line);

        // Saltar líneas vacías, comentarios o secciones [XXX]
        if (line.empty() || line[0] == '#' || line[0] == ';' || line[0] == '[')
            continue;

        const auto pos = line.find('=');
        if (pos == std::string::npos)
            continue;

        std::string key   = line.substr(0, pos);
        std::string value = line.substr(pos + 1);

        trim(key);
        trim(value);

        if (!key.empty())
        {
            mTranslations[normalizeKey(key)] = value;
            ++loaded;
        }
    }

    LOG(LogInfo) << "LocaleES: loaded " << loaded
                 << " entries from " << filePath;
    return true;
}

// --------------------
// Nombre visible del idioma
// --------------------
std::string LocaleES::getLanguageName(const std::string& code) const
{
    auto readNameFromFile = [](const std::string& filePath) -> std::string
    {
        std::ifstream in(filePath);
        if (!in.is_open())
            return "";

        std::string line;
        bool inMetaSection = false;

        while (std::getline(in, line))
        {
            trim(line);

            if (line.empty() || line[0] == '#' || line[0] == ';')
                continue;

            if (line.front() == '[' && line.back() == ']')
            {
                std::string section = line.substr(1, line.size() - 2);
                inMetaSection = (normalizeKey(section) == "meta");
                continue;
            }

            if (!inMetaSection)
                continue;

            const auto pos = line.find('=');
            if (pos == std::string::npos)
                continue;

            std::string key = line.substr(0, pos);
            std::string value = line.substr(pos + 1);

            trim(key);
            trim(value);

            if (normalizeKey(key) == "name" && !value.empty())
                return value;
        }

        return "";
    };

    const std::string home = Utils::FileSystem::getHomePath();
    const std::string exePath = Utils::FileSystem::getExePath();

    std::string name = readNameFromFile(
        home + "/.emulationstation/lang/" + code + ".ini");

    if (name.empty())
        name = readNameFromFile(exePath + "/lang/" + code + ".ini");

    return name;
}

// --------------------
// Cargar según Settings
// --------------------
void LocaleES::loadFromSettings()
{
    std::string lang = Settings::getInstance()->getString("Language");
    if (lang.empty())
        lang = "en";

    if (lang == mCurrentLanguage && !mTranslations.empty() && !mFallbackTranslations.empty())
        return;

    mCurrentLanguage = lang;
    mTranslations.clear();
    mFallbackTranslations.clear();

    const std::string home = Utils::FileSystem::getHomePath();
    const std::string exePath = Utils::FileSystem::getExePath();

    const std::string userLangPath = home + "/.emulationstation/lang/" + lang + ".ini";
    const std::string appLangPath  = exePath + "/lang/" + lang + ".ini";

    const std::string userEnPath = home + "/.emulationstation/lang/en.ini";
    const std::string appEnPath  = exePath + "/lang/en.ini";

    // cargar fallback inglés primero en mapa temporal
    {
        std::map<std::string, std::string> backup;
        mTranslations.clear();

        if (!loadLanguageFile(userEnPath))
            loadLanguageFile(appEnPath);

        mFallbackTranslations = mTranslations;
        mTranslations.clear();
    }

    // luego cargar idioma actual
    if (!loadLanguageFile(userLangPath))
    {
        if (!loadLanguageFile(appLangPath))
        {
            LOG(LogWarning) << "LocaleES: no language file found for '" << lang << "'";
        }
    }
}

// --------------------
// Traducciones
// --------------------
std::string LocaleES::translate(const std::string& key) const
{
    const std::string normalized = normalizeKey(key);

    auto it = mTranslations.find(normalized);
    if (it != mTranslations.end())
        return it->second;

    auto itFallback = mFallbackTranslations.find(normalized);
    if (itFallback != mFallbackTranslations.end())
        return itFallback->second;

    return key;
}

std::string LocaleES::get(const std::string& key)
{
    return getInstance().translate(key);
}

// Función global para usar desde es-core
std::string es_translate(const std::string& key)
{
    return LocaleES::get(key);
}
