#pragma once
#include "RetroPieRuncommand.h"
#include "Settings.h"
#include "utils/FileSystemUtil.h"
#include "components/OptionListComponent.h"
#include "guis/GuiSettings.h"
#include "LocaleES.h"

namespace RetroPieDecorations {
inline std::string root() {
    return Utils::FileSystem::getHomePath() + "/RetroPie/Overlays";
}
inline std::string gameKey(const std::string& system, const std::string& rom) {
    return "Decoration.Game." + system + ":" + rom;
}
inline std::string systemKey(const std::string& system) {
    return "Decoration.System." + system;
}
inline bool pair(const std::string& stem) {
    return Utils::FileSystem::isRegularFile(stem + ".png") &&
           Utils::FileSystem::isRegularFile(stem + ".info");
}
inline bool compatible(const std::string& name, const std::string& system) {
    if (name.empty() || name == "." || name == ".." ||
        name.find_first_not_of("ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789_. -") != std::string::npos)
        return false;
    const auto folder = root() + "/" + name;
    if (!Utils::FileSystem::isDirectory(folder)) return false;
    if (system.empty()) {
        if (pair(folder + "/default")) return true;
        for (const auto& file : Utils::FileSystem::getDirContent(folder + "/systems"))
            if (file.size() > 4 && file.substr(file.size()-4) == ".png" &&
                pair(file.substr(0,file.size()-4))) return true;
        return false;
    }
    if (pair(folder + "/systems/" + system)) return true;
    if (name.find("vertical") != std::string::npos) return false;
    const std::vector<std::string> arcade = {"arcade","mame","mame-libretro","fba","fbneo","neogeo"};
    const std::vector<std::string> console = {"nes","snes","n64","psx","mastersystem","megadrive","segacd","sega32x","3do","saturn","pcengine","pcenginecd","dreamcast"};
    const bool a = std::find(arcade.begin(), arcade.end(), system) != arcade.end();
    const bool c = std::find(console.begin(), console.end(), system) != console.end();
    return (a || (c && name.compare(0,7,"arcade_") != 0)) && pair(folder + "/default");
}
inline std::string selected(const std::string& system, const std::string& rom) {
    for (const auto& key : {gameKey(system,rom), systemKey(system), std::string("Decoration.Global")}) {
        const auto value = Settings::getInstance()->getString(key);
        if (value == "none") return "";
        if (compatible(value,system)) return value;
    }
    return "";
}
inline std::string shellQuote(const std::string& value) {
    std::string result = "'";
    for (char c : value) result += c == '\'' ? "'\\''" : std::string(1,c);
    return result + "'";
}
inline void addSelector(Window* window, GuiSettings* gui, const std::string& key, const std::string& system = "") {
    const auto stored = Settings::getInstance()->getString(key);
    auto options = std::make_shared<OptionListComponent<std::string>>(window, es_translate("DECORATION SET"), false);
    options->add(es_translate("AUTO"), "", stored.empty() || stored == "auto");
    options->add(es_translate("NONE"), "none", stored == "none");
    bool found = stored.empty() || stored == "auto" || stored == "none";
    for (const auto& path : Utils::FileSystem::getDirContent(root())) {
        const auto name = Utils::FileSystem::getFileName(path);
        if (!compatible(name,system)) continue;
        options->add(name,name,stored == name);
        if (stored == name) found = true;
    }
    if (!found) options->add(es_translate("UNAVAILABLE (CURRENT)") + ": " + stored,stored,true);
    gui->addWithLabel(es_translate("DECORATION SET"),options);
    gui->addSaveFunc([key,options,stored] {
        if (options->getSelected() != stored) {
            Settings::getInstance()->setString(key,options->getSelected());
            Settings::getInstance()->saveFile();
        }
    });
}
}
