// es-app/src/views/SystemView.h
#pragma once
#ifndef ES_APP_VIEWS_SYSTEM_VIEW_H
#define ES_APP_VIEWS_SYSTEM_VIEW_H

#include "components/IList.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "GuiComponent.h"
#include "Sound.h"
#include "Window.h"
#include <memory>
#include <string>
#include <vector>

class AnimatedImageComponent;
class SystemData;
class VideoComponent;

enum CarouselType : unsigned int
{
	HORIZONTAL = 0,
	VERTICAL = 1,
	VERTICAL_WHEEL = 2,
	HORIZONTAL_WHEEL = 3
};

struct SystemViewData
{
	std::shared_ptr<GuiComponent> logo;
	std::vector<GuiComponent*> backgroundExtras;

	// InfoButton (temable)
	GuiComponent* infoButton = nullptr;          // idle
	GuiComponent* infoButtonSelected = nullptr;  // focus

	// Descripción resuelta desde ThemeData:
	bool hasDescription = false;
	std::string descriptionText;
};

struct SystemViewCarousel
{
	CarouselType type;
	Vector2f pos;
	Vector2f size;
	Vector2f origin;
	float logoScale;
	float logoRotation;
	Vector2f logoRotationOrigin;
	Alignment logoAlignment;
	unsigned int color;
	unsigned int colorEnd;
	bool colorGradientHorizontal;
	int maxLogoCount;
	Vector2f logoSize;
	float zIndex;

	// mejoras visuales (compat Batocera)
	float minLogoOpacity;
	float scaledLogoSpacing;
};

class SystemView : public IList<SystemViewData, SystemData*>
{
public:
	SystemView(Window* window);
	~SystemView() override;

	void onShow() override;
	void onHide() override;

	void goToSystem(SystemData* system, bool animate);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	void onThemeChanged(const std::shared_ptr<ThemeData>& theme);

	std::vector<HelpPrompt> getHelpPrompts() override;
	HelpStyle getHelpStyle() override;

protected:
	void onCursorChanged(const CursorState& state) override;

private:
	void populate();
	void getViewElements(const std::shared_ptr<ThemeData>& theme);
	void getDefaultElements(void);
	void getCarouselFromTheme(const ThemeData::ThemeElement* elem);
	void refreshMostPlayedVideo();

	void renderCarousel(const Transform4x4f& parentTrans);
	void renderExtras(const Transform4x4f& parentTrans, float lower, float upper);
	void renderInfoBar(const Transform4x4f& trans);
	void renderFade(const Transform4x4f& trans);

	// foco del infoButton
	bool selectedHasInfo() const;
	void setInfoFocus(bool focus);

	SystemViewCarousel mCarousel;
	TextComponent mSystemInfo;
	std::unique_ptr<VideoComponent> mMostPlayedVideo;

	float mCamOffset;
	float mExtrasCamOffset;
	float mExtrasFadeOpacity;

	bool mViewNeedsReload;
	bool mShowing;

	// scroll sound opcional desde <carousel scrollSound="...">
	std::shared_ptr<Sound> mScrollSnd;

	// foco actual en el botón info
	bool mInfoFocus;
};

#endif // ES_APP_VIEWS_SYSTEM_VIEW_H
