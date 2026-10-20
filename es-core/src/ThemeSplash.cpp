#include "ThemeSplash.h"

#include "ThemeData.h"
#include "Window.h"
#include "Log.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"

#include <algorithm>
#include <cmath>
#ifdef WIN32
#include <SDL_events.h>
#endif

namespace
{
class SplashProgress : public GuiComponent
{
public:
	SplashProgress(Window* window, unsigned int color, unsigned int colorEnd)
		: GuiComponent(window),
		  mImage(window, true),
		  mColor(color),
		  mColorEnd(colorEnd)
	{
	}

	void inheritGeometry(const SplashProgress& other)
	{
		setPosition(other.getPosition());
		setSize(other.getSize());
		setOrigin(other.getOrigin());
		setRotation(other.getRotation());
		setRotationOrigin(other.getRotationOrigin());
		setZIndex(other.getZIndex());
		setVisible(other.isVisible());
	}

	void apply(const std::shared_ptr<ThemeData>& theme, const std::string& name)
	{
		const ThemeData::ThemeElement* elem =
			theme->getElement("splash", name, "image");
		if (!elem)
			return;

		GuiComponent::applyTheme(theme, "splash", name, ThemeFlags::ALL);

		if (elem->has("color"))
			mColor = mColorEnd = elem->get<unsigned int>("color");
		if (elem->has("colorEnd"))
			mColorEnd = elem->get<unsigned int>("colorEnd");
		if (elem->has("gradientType"))
			mHorizontalGradient =
				elem->get<std::string>("gradientType") == "horizontal";

		if (elem->has("path"))
		{
			const std::string path = elem->get<std::string>("path");
			if (ResourceManager::getInstance()->fileExists(path))
			{
				const bool tile = elem->has("tile") && elem->get<bool>("tile");
				mImage.setImage(path, tile);
				mHasImage = true;
			}
		}
	}

	void setFraction(float value)
	{
		mFraction = std::max(0.0f, std::min(1.0f, value));
	}

	void render(const Transform4x4f& parentTrans) override
	{
		if (!isVisible() || mFraction <= 0.0f)
			return;

		const Transform4x4f trans = parentTrans * getTransform();
		const float width = getSize().x() * mFraction;
		const float height = getSize().y();

		if (mHasImage)
		{
			mImage.setPosition(0.0f, 0.0f);
			mImage.setOrigin(0.0f, 0.0f);
			mImage.setResize(width, height);
			mImage.setColorShift(mColor);
			mImage.setColorShiftEnd(mColorEnd);
			mImage.setColorGradientHorizontal(mHorizontalGradient);
			mImage.setOpacity(getOpacity());
			mImage.render(trans);
			return;
		}

		auto withOpacity = [this](unsigned int color) {
			return (color & 0xFFFFFF00) |
				(((color & 0xFF) * getOpacity()) / 255);
		};

		Renderer::setMatrix(trans);
		Renderer::drawRect(
			0.0f, 0.0f, width, height,
			withOpacity(mColor), withOpacity(mColorEnd),
			mHorizontalGradient);
	}

private:
	ImageComponent mImage;
	float mFraction = 1.0f;
	unsigned int mColor;
	unsigned int mColorEnd;
	bool mHorizontalGradient = true;
	bool mHasImage = false;
};

bool isReservedSplashElement(const std::string& name)
{
	return name == "background" || name == "label" ||
		name == "progressbar" || name == "progressbar:active";
}
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
	const std::map<std::string, ThemeSet> sets = ThemeData::getThemeSets();
	const auto selected = sets.find(Settings::getInstance()->getString("ThemeSet"));
	if (selected == sets.end())
		throw error << "Selected theme was not found";

	const std::string directory = selected->second.path;
	const std::string splashPath = directory + "/splash.xml";
	if (!Utils::FileSystem::exists(splashPath))
		throw error << "No splash.xml in selected theme";

	// Follow Tarave/Batocera's model: splash is a normal ThemeData view.
	// This deliberately avoids a second, splash-only parser with different rules.
	const float width = (float)Renderer::getScreenWidth();
	const float height = (float)Renderer::getScreenHeight();

	auto theme = std::make_shared<ThemeData>();
	std::map<std::string, std::string> splashVars;
	splashVars["themePath"] = directory;
	splashVars["subset.frontend"] = "EmulationStation";
	splashVars["screen.width"] = std::to_string((int)width);
	splashVars["screen.height"] = std::to_string((int)height);

	const float ratio = height > 0.0f ? width / height : (16.0f / 9.0f);
	if (std::fabs(ratio - (4.0f / 3.0f)) < 0.05f)
		splashVars["subset.aspect-ratio"] = "4-3";
	else if (std::fabs(ratio - (5.0f / 4.0f)) < 0.05f)
		splashVars["subset.aspect-ratio"] = "5-4";
	else if (std::fabs(ratio - (16.0f / 10.0f)) < 0.05f)
		splashVars["subset.aspect-ratio"] = "16-10";
	else
		splashVars["subset.aspect-ratio"] = "16-9";

	theme->loadFile(splashVars, splashPath, "splash");
	if (!theme->hasView("splash"))
		throw error << "Missing splash view";

	const ThemeData::ThemeElement* config =
		theme->getElement("splash", "splash", "splash");
	if (config && config->has("backgroundColor"))
		mBackgroundColor = config->get<unsigned int>("backgroundColor");

	auto background = std::unique_ptr<ImageComponent>(new ImageComponent(window, true));
	background->setOrigin(0.5f, 0.5f);
	background->setPosition(width * 0.5f, height * 0.5f);
	background->setMaxSize(width, height);
	background->setDefaultZIndex(0);
	background->setZIndex(0);
	background->applyTheme(theme, "splash", "background", ThemeFlags::ALL);

	const ThemeData::ThemeElement* backgroundTheme =
		theme->getElement("splash", "background", "image");
	if (backgroundTheme && backgroundTheme->has("path") &&
		!ResourceManager::getInstance()->fileExists(
			backgroundTheme->get<std::string>("path")))
		throw error << "Splash background image is missing";

	mElements.push_back(std::move(background));

	auto inactive = std::unique_ptr<SplashProgress>(
		new SplashProgress(window, 0x6060607F, 0x9090907F));
	inactive->setPosition(width * 0.25f, height * 0.88f);
	inactive->setSize(width * 0.5f, height * 0.04f);
	inactive->setDefaultZIndex(5);
	inactive->setZIndex(5);
	inactive->apply(theme, "progressbar");

	auto active = std::unique_ptr<SplashProgress>(
		new SplashProgress(window, 0x006C9EFF, 0x003E5CFF));
	active->inheritGeometry(*inactive);
	active->setDefaultZIndex(5);
	active->apply(theme, "progressbar:active");

	mInactive = inactive.get();
	mActive = active.get();
	mElements.push_back(std::move(inactive));
	mElements.push_back(std::move(active));

	auto label = std::unique_ptr<TextComponent>(new TextComponent(window));
	label->setHorizontalAlignment(ALIGN_CENTER);
	label->setColor(0x656565FF);
	label->setPosition(0.0f, height * 0.78f);
	label->setSize(width, 0.0f);
	label->setDefaultZIndex(10);
	label->setZIndex(10);
	label->applyTheme(
		theme, "splash", "label",
		ThemeFlags::ALL & ~ThemeFlags::TEXT);
	mLabel = label.get();
	mElements.push_back(std::move(label));

	// Render ordinary theme extras through the same ThemeData machinery as ES-X.
	// Force-load images because the splash exists only during startup.
	std::vector<ThemeData::ThemeExtra> extras =
		ThemeData::makeExtrasWithMetadata(theme, "splash", window, true);
	for (ThemeData::ThemeExtra& extra : extras)
	{
		if (!extra.component)
			continue;
		if (isReservedSplashElement(extra.name))
		{
			delete extra.component;
			continue;
		}
		mElements.emplace_back(extra.component);
	}

	std::stable_sort(
		mElements.begin(), mElements.end(),
		[](const std::unique_ptr<GuiComponent>& a,
		   const std::unique_ptr<GuiComponent>& b) {
			return a->getZIndex() < b->getZIndex();
		});

	LOG(LogInfo) << "Native theme splash loaded: " << splashPath;
}

void ThemeSplash::render(
	const std::string& text,
	float percent,
	unsigned char opacity)
{
	const Transform4x4f transform = Transform4x4f::Identity();
	Renderer::setMatrix(transform);
	Renderer::drawRect(
		0.0f, 0.0f,
		(float)Renderer::getScreenWidth(),
		(float)Renderer::getScreenHeight(),
		mBackgroundColor, mBackgroundColor);

	mLabel->setText(text);

	const bool showProgress = std::isfinite(percent) && percent >= 0.0f;
	static_cast<SplashProgress*>(mInactive)->setFraction(showProgress ? 1.0f : 0.0f);
	static_cast<SplashProgress*>(mActive)->setFraction(showProgress ? percent : 0.0f);

	for (const auto& element : mElements)
	{
		element->setOpacity(opacity);
		element->render(transform);
	}

	Renderer::swapBuffers();

#ifdef WIN32
	SDL_Event event;
	while (SDL_PollEvent(&event)) {}
#endif
}
