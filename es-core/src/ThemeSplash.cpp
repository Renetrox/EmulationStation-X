#include "ThemeSplash.h"

#include "ThemeData.h"
#include "Window.h"
#include "Log.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "math/Misc.h"
#include <algorithm>
#include <cmath>
#ifdef WIN32
#include <SDL_events.h>
#endif

namespace
{
// A solid progress bar needs no texture or external image. Its full geometry
// stays fixed as progress changes, so centered origins do not move the bar.
class SplashProgress : public GuiComponent
{
public:
    explicit SplashProgress(Window* window) : GuiComponent(window) {}
    float fraction = 1.0f;
    unsigned int color = 0x252525FF;
    unsigned int colorEnd = 0x252525FF;
    bool horizontal = true;

    void apply(const std::shared_ptr<ThemeData>& theme, const std::string& name)
    {
        auto elem = theme->getElement("splash", name, "image");
        if (!elem) return;
        const float oldZ = getZIndex();
        const bool oldVisible = isVisible();
        GuiComponent::applyTheme(theme, "splash", name, ThemeFlags::ALL);
        if (!elem->has("zIndex")) setZIndex(oldZ);
        if (!elem->has("visible")) setVisible(oldVisible);
        if (elem->has("color")) color = colorEnd = elem->get<unsigned int>("color");
        if (elem->has("colorEnd")) colorEnd = elem->get<unsigned int>("colorEnd");
        if (elem->has("gradientType")) horizontal = elem->get<std::string>("gradientType") == "horizontal";
    }

    void render(const Transform4x4f& parentTrans) override
    {
        if (!isVisible() || fraction <= 0) return;
        Renderer::setMatrix(parentTrans * getTransform());
        auto alpha = [this](unsigned int c) {
            return (c & 0xFFFFFF00) | (((c & 255) * getOpacity()) / 255);
        };
        Renderer::drawRect(0, 0, getSize().x() * fraction, getSize().y(),
            alpha(color), alpha(colorEnd), horizontal);
    }
};
}

ThemeSplash::~ThemeSplash() = default;

std::unique_ptr<ThemeSplash> ThemeSplash::create(Window* window)
{
    try
    {
        return std::unique_ptr<ThemeSplash>(new ThemeSplash(window));
    }
    catch (const ThemeException& e)
    {
        LOG(LogWarning) << "Theme splash unavailable; using original splash: " << e.what();
    }
    catch (const std::exception& e)
    {
        LOG(LogWarning) << "Theme splash unavailable; using original splash: " << e.what();
    }
    return nullptr;
}

ThemeSplash::ThemeSplash(Window* window)
{
    ThemeException error;
    auto sets = ThemeData::getThemeSets();
    auto selected = sets.find(Settings::getInstance()->getString("ThemeSet"));
    if (selected == sets.end() || !Utils::FileSystem::exists(selected->second.path + "/splash.xml"))
        throw error << "No splash.xml in selected theme";

    const std::string directory = selected->second.path;
    auto theme = std::make_shared<ThemeData>();
    theme->loadFile({{"themePath", directory}}, directory + "/splash.xml", true);
    if (!theme->hasView("splash"))
        throw error << "Missing splash view";

    const float width = Renderer::getScreenWidth();
    const float height = Renderer::getScreenHeight();
    auto config = theme->getElement("splash", "splash", "splash");
    if (config && config->has("backgroundColor"))
        mBackgroundColor = config->get<unsigned int>("backgroundColor");

    auto background = std::unique_ptr<ImageComponent>(new ImageComponent(window, true));
    background->setOrigin(0.5f, 0.5f);
    background->setPosition(width / 2, height / 2);
    background->setMaxSize(width, height);
    background->setDefaultZIndex(0);
    background->applyTheme(theme, "splash", "background", ThemeFlags::ALL);
    auto image = theme->getElement("splash", "background", "image");
    if (image && image->has("path") &&
        !ResourceManager::getInstance()->fileExists(image->get<std::string>("path")))
        throw error << "Splash background image is missing";
    mElements.push_back(std::move(background));

    auto inactive = std::unique_ptr<SplashProgress>(new SplashProgress(window));
    inactive->setPosition(width * 0.25f, height * 0.88f);
    inactive->setSize(width * 0.5f, height * 0.04f);
    inactive->setDefaultZIndex(5);
    inactive->setZIndex(5);
    inactive->apply(theme, "progressbar");
    auto active = std::unique_ptr<SplashProgress>(new SplashProgress(window));
    active->setPosition(width * 0.25f, height * 0.88f);
    active->setSize(width * 0.5f, height * 0.04f);
    active->setDefaultZIndex(5);
    active->setZIndex(5);
    active->apply(theme, "progressbar");
    active->color = active->colorEnd = 0x006C9EFF;
    active->apply(theme, "progressbar:active");
    mInactive = inactive.get();
    mActive = active.get();
    mElements.push_back(std::move(inactive));
    mElements.push_back(std::move(active));

    auto label = std::unique_ptr<TextComponent>(new TextComponent(window));
    label->setHorizontalAlignment(ALIGN_CENTER);
    label->setColor(0x656565FF);
    label->setPosition(0, height * 0.78f);
    label->setSize(width, 0);
    label->setDefaultZIndex(10);
    label->setZIndex(10);
    label->applyTheme(theme, "splash", "label", ThemeFlags::ALL & ~ThemeFlags::TEXT);
    auto labelTheme = theme->getElement("splash", "label", "text");
    if (labelTheme && labelTheme->has("verticalAlignment"))
    {
        auto alignment = labelTheme->get<std::string>("verticalAlignment");
        label->setVerticalAlignment(alignment == "top" ? ALIGN_TOP : alignment == "bottom" ? ALIGN_BOTTOM : ALIGN_CENTER);
    }
    mLabel = label.get();
    mElements.push_back(std::move(label));

    // Own each extra immediately, including on later construction failure.
    auto extras = ThemeData::makeExtras(theme, "splash", window, true);
    for (auto extra : extras) mElements.emplace_back(extra);
    std::stable_sort(mElements.begin(), mElements.end(),
        [](const std::unique_ptr<GuiComponent>& a, const std::unique_ptr<GuiComponent>& b) {
            return a->getZIndex() < b->getZIndex();
        });
    LOG(LogInfo) << "Static theme splash loaded: " << directory + "/splash.xml";
}

void ThemeSplash::render(const std::string& text, float percent, unsigned char opacity)
{
    auto transform = Transform4x4f::Identity();
    Renderer::setMatrix(transform);
    // Keep the clearing background opaque, matching the original loading screen.
    Renderer::drawRect(0, 0, Renderer::getScreenWidth(), Renderer::getScreenHeight(),
        mBackgroundColor, mBackgroundColor);
    mLabel->setText(text);
    const bool showProgress = std::isfinite(percent) && percent >= 0;
    static_cast<SplashProgress*>(mActive)->fraction = showProgress ? Math::clamp(0.0f, percent, 1.0f) : 0;
    for (const auto& element : mElements)
    {
        if (!showProgress && (element.get() == mInactive || element.get() == mActive)) continue;
        element->setOpacity(opacity);
        element->render(transform);
    }
    Renderer::swapBuffers();
#ifdef WIN32
    SDL_Event event;
    while (SDL_PollEvent(&event)) {}
#endif
}
