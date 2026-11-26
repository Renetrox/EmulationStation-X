#pragma once
#include "RetroPieDecorations.h"
#include "guis/GuiSettings.h"
#include "components/MenuComponent.h"
#include "components/TextComponent.h"
#include "components/ImageComponent.h"
#include "resources/Font.h"
#include "renderers/Renderer.h"
#include "utils/StringUtil.h"
#include "components/OptionListComponent.h"
#include "LocaleES.h"

namespace RetroPieDecorations {
inline std::string previewFor(const std::string& name, const std::string& system) {
    const auto folder = root() + "/" + name;
    if (!system.empty() && pair(folder + "/systems/" + system))
        return folder + "/systems/" + system + ".png";
    if (pair(folder + "/default")) return folder + "/default.png";
    if (pair(folder + "/systems/gba")) return folder + "/systems/gba.png";
    for (const auto& file : Utils::FileSystem::getDirContent(folder + "/systems"))
        if (file.size() > 4 && file.substr(file.size()-4) == ".png" &&
            pair(file.substr(0,file.size()-4))) return file;
    return "";
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
        auto label = name;
        std::replace(label.begin(), label.end(), '_', ' ');
        options->add(label,name,stored == name,previewFor(name,system));
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
