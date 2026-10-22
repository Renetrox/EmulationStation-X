// Shared presentation only for the ES-X theme and X-TRAS browsers.
// Inspired by Tarave's compact two-level rows; no changes to catalogs,
// downloads, input handling or the image preview backend.
#pragma once

#include "Window.h"
#include "components/ComponentGrid.h"
#include "components/TextComponent.h"
#include "resources/Font.h"

#include <algorithm>
#include <memory>
#include <string>

namespace CompactBrowserStyle
{
    inline std::shared_ptr<Font> iconFont()
    {
        // Font Awesome is already bundled in ES-X.
        return Font::get(FONT_SIZE_SMALL, ":/fonts/fontawesome-webfont.ttf");
    }

    inline std::shared_ptr<Font> detailFont()
    {
        // The glyphs come from Font Awesome; letters use the bundled
        // Unicode fallback faces. This retains distinct metadata icons.
        return Font::get(FONT_SIZE_MINI, ":/fonts/fontawesome-webfont.ttf");
    }

    inline std::shared_ptr<ComponentGrid> makeRow(
        Window* window,
        const std::string& title,
        const std::string& details,
        bool installed,
        bool updateAvailable = false)
    {
        auto grid = std::make_shared<ComponentGrid>(window, Vector2i(2, 2));
        const float rowHeight = std::max(
            Font::get(FONT_SIZE_SMALL)->getHeight() +
            Font::get(FONT_SIZE_MINI)->getHeight() + 8.0f,
            Renderer::getScreenHeight() * 0.064f);

        grid->setColWidthPerc(0, 0.085f, false);
        grid->setRowHeightPerc(0, 0.53f, false);

        const char* glyph = updateAvailable ? u8"\uf021" :
            (installed ? u8"\uf058" : u8"\uf019");
        auto status = std::make_shared<TextComponent>(
            window, glyph, iconFont(), 0xD3D6D9FF, ALIGN_CENTER);
        status->setVerticalAlignment(ALIGN_CENTER);

        auto headline = std::make_shared<TextComponent>(
            window, title, Font::get(FONT_SIZE_SMALL), 0xEEEEEEFF, ALIGN_LEFT);
        headline->setVerticalAlignment(ALIGN_CENTER);

        auto metadata = std::make_shared<TextComponent>(
            window, details, detailFont(), 0xABB0B5FF, ALIGN_LEFT);
        metadata->setVerticalAlignment(ALIGN_CENTER);

        grid->setEntry(status, Vector2i(0, 0), false, true, Vector2i(1, 2));
        grid->setEntry(headline, Vector2i(1, 0), false, true);
        grid->setEntry(metadata, Vector2i(1, 1), false, true);
        grid->setSize(Vector2f(0.0f, rowHeight));
        return grid;
    }
}
