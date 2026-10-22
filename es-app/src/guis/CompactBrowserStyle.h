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
    inline int titleSize()
    {
        // Tarave-like compact typography without changing ES-X global fonts.
        return std::max(16, static_cast<int>(Renderer::getScreenHeight() * 0.026f));
    }

    inline int detailSize()
    {
        return std::max(13, static_cast<int>(Renderer::getScreenHeight() * 0.019f));
    }

    inline std::shared_ptr<Font> titleFont()
    {
        return Font::get(titleSize());
    }

    inline std::shared_ptr<Font> iconFont()
    {
        // Font Awesome is already bundled in ES-X.
        return Font::get(titleSize(), ":/fonts/fontawesome-webfont.ttf");
    }

    inline std::shared_ptr<Font> detailFont()
    {
        // Icons use Font Awesome; Latin text uses the usual fallback faces.
        return Font::get(detailSize(), ":/fonts/fontawesome-webfont.ttf");
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
            titleFont()->getLetterHeight() +
            detailFont()->getLetterHeight() + 12.0f,
            Renderer::getScreenHeight() * 0.054f);

        grid->setColWidthPerc(0, 0.085f, false);
        grid->setRowHeightPerc(0, 0.55f, false);

        const char* glyph = updateAvailable ? u8"\uf021" :
            (installed ? u8"\uf058" : u8"\uf019");
        auto status = std::make_shared<TextComponent>(
            window, glyph, iconFont(), 0xD3D6D9FF, ALIGN_CENTER);
        status->setVerticalAlignment(ALIGN_CENTER);

        auto headline = std::make_shared<TextComponent>(
            window, title, titleFont(), 0xEEEEEEFF, ALIGN_LEFT);
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
