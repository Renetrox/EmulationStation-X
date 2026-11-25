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
