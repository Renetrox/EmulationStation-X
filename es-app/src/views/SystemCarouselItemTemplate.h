#pragma once

#include "GuiComponent.h"
#include "ThemeData.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

// Scoped port of Tarave's carousel itemTemplate presentation.
// Composes image/text elements in each logo slot. No new theme backend,
// game metadata engine, or modification of the normal logo fallback path.
class SystemCarouselItemTemplate : public GuiComponent
{
public:
    explicit SystemCarouselItemTemplate(Window* window) : GuiComponent(window) {}

    // Carousel fading must affect the complete composed item, not just
    // the empty parent; otherwise inactive cards remain fully opaque.
    void setOpacity(unsigned char opacity) override
    {
        GuiComponent::setOpacity(opacity);
        for (const auto& child : mParts)
            child->setOpacity(opacity);
    }

    bool load(const ThemeData::ThemeElement& templ, const Vector2f& size,
              const std::string& name, const std::string& fullName)
    {
        setSize(size);
        for (const auto& pair : templ.children)
        {
            const ThemeData::ThemeElement& elem = pair.second;

            if (elem.type == "image")
            {
                const std::string path = elem.has("path") ?
                    elem.get<std::string>("path") : "";
                const std::string fallback = elem.has("default") ?
                    elem.get<std::string>("default") : "";
                const std::string chosen = ResourceManager::getInstance()->fileExists(path) ?
                    path : (ResourceManager::getInstance()->fileExists(fallback) ? fallback : "");
                if (chosen.empty())
                    continue;

                auto image = std::make_shared<ImageComponent>(mWindow, false, false);
                const Vector2f itemSize = normalizedSize(elem, size);
                const Vector2f itemPos = normalizedPosition(elem, size);
                image->setResize(itemSize);
                image->setImage(chosen);
                if (elem.has("color"))
                    image->setColorShift(elem.get<unsigned int>("color"));
                if (elem.has("zIndex"))
                    image->setZIndex(elem.get<float>("zIndex"));
                if (elem.has("visible"))
                    image->setVisible(elem.get<bool>("visible"));
                if (elem.has("origin"))
                    image->setOrigin(elem.get<Vector2f>("origin"));
                if (elem.has("rotation"))
                    image->setRotationDegrees(elem.get<float>("rotation"));
                image->setPosition(itemPos.x(), itemPos.y());
                attach(image);
            }
            else if (elem.type == "text")
            {
                std::string value = elem.has("text") ?
                    elem.get<std::string>("text") : "{system:name}";
                replaceAll(value, "{system:fullName}", fullName);
                replaceAll(value, "{system:name}", name);
                // Batocera themes can leave the text property empty to bind
                // the current system name automatically.
                if (value.empty())
                    value = name;

                std::string fontPath = elem.has("fontPath") ?
                    elem.get<std::string>("fontPath") : Font::getDefaultPath();
                const int fontSize = elem.has("fontSize")
                    ? std::max(8, (int)(elem.get<float>("fontSize") *
                                        Renderer::getScreenHeight()))
                    : (int)FONT_SIZE_MEDIUM;
                auto text = std::make_shared<TextComponent>(
                    mWindow, value, Font::get(fontSize, fontPath),
                    elem.has("color") ? elem.get<unsigned int>("color") : 0xFFFFFFFF,
                    ALIGN_CENTER);

                const Vector2f itemSize = normalizedSize(elem, size);
                const Vector2f itemPos = normalizedPosition(elem, size);
                text->setSize(itemSize);
                if (elem.has("origin"))
                    text->setOrigin(elem.get<Vector2f>("origin"));
                if (elem.has("forceUppercase"))
                    text->setUppercase(elem.get<bool>("forceUppercase"));
                if (elem.has("backgroundColor"))
                    text->setBackgroundColor(elem.get<unsigned int>("backgroundColor"));
                if (elem.has("textStrokeColor"))
                    text->setTextStrokeColor(elem.get<unsigned int>("textStrokeColor"));
                if (elem.has("textStrokeSize"))
                    text->setTextStrokeSize(elem.get<float>("textStrokeSize"));
                if (elem.has("zIndex"))
                    text->setZIndex(elem.get<float>("zIndex"));
                if (elem.has("lineSpacing"))
                    text->setLineSpacing(elem.get<float>("lineSpacing"));
                if (elem.has("visible"))
                    text->setVisible(elem.get<bool>("visible"));
                if (elem.has("rotation"))
                    text->setRotationDegrees(elem.get<float>("rotation"));
                if (elem.has("alignment"))
                {
                    const std::string alignment = elem.get<std::string>("alignment");
                    if (alignment == "left") text->setHorizontalAlignment(ALIGN_LEFT);
                    else if (alignment == "right") text->setHorizontalAlignment(ALIGN_RIGHT);
                }
                text->setPosition(itemPos.x(), itemPos.y());
                attach(text);
            }
        }
        sortChildren();
        return !mParts.empty();
    }

private:
    static Vector2f normalizedSize(const ThemeData::ThemeElement& e,
                                   const Vector2f& area)
    {
        return e.has("size") ?
            e.get<Vector2f>("size") * area : area;
    }

    static Vector2f normalizedPosition(const ThemeData::ThemeElement& e,
                                       const Vector2f& area)
    {
        return e.has("pos") ?
            e.get<Vector2f>("pos") * area : Vector2f::Zero();
    }

    static void replaceAll(std::string& s, const std::string& key,
                           const std::string& replacement)
    {
        if (key.empty()) return;
        size_t pos = 0;
        while ((pos = s.find(key, pos)) != std::string::npos)
        {
            s.replace(pos, key.size(), replacement);
            pos += replacement.size();
        }
    }

    void attach(const std::shared_ptr<GuiComponent>& child)
    {
        addChild(child.get());
        mParts.push_back(child);
    }

    std::vector<std::shared_ptr<GuiComponent>> mParts;
};
