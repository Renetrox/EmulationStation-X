#include "components/VolumeInfoComponent.h"

#include "PowerSaver.h"
#include "Settings.h"
#include "VolumeControl.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"
#include "resources/Font.h"
#include "renderers/Renderer.h"

#define PADDING_PX (Renderer::getScreenWidth() * 0.006f)
#define PADDING_BAR (Renderer::getScreenWidth() * 0.006f)
#define VISIBLE_TIME 2650
#define FADE_TIME 350
#define BASE_OPACITY 210
#define CHECK_VOLUME_DELAY 40

VolumeInfoComponent::VolumeInfoComponent(Window* window)
	: GuiComponent(window)
	, mFrame(nullptr)
	, mLabel(nullptr)
	, mVolume(-1)
	, mCheckTime(0)
	, mDisplayTime(-1)
{
	const bool darkMenu = Settings::getInstance()->getBool("MenuDark");
	const unsigned int textColor = darkMenu ? 0xFFFFFFFF : 0x555555FF;
	const unsigned int edgeColor = darkMenu ? 0xFFFFFFFF : 0xFFFFFFFF;
	const unsigned int centerColor = darkMenu ? 0x2A2A2AEA : 0xF2F2F2EA;

	auto font = Font::get(FONT_SIZE_SMALL);
	const float width = 2.0f * PADDING_PX + font->sizeText("100%").x();
	Vector2f fullSize(width, width * 2.5f);
	setSize(fullSize);

	mFrame = new NinePatchComponent(window);
	mFrame->setImagePath(darkMenu ? ":/frame_dark.png" : ":/frame.png");
	mFrame->setEdgeColor(edgeColor);
	mFrame->setCenterColor(centerColor);
	mFrame->fitTo(mSize, Vector3f::Zero(), Vector2f(-32.0f, -32.0f));
	addChild(mFrame);

	mLabel = new TextComponent(mWindow, "", font, textColor, ALIGN_CENTER);

	const float labelHeight = font->sizeText("100%").y() + PADDING_PX;
	mLabel->setPosition(0.0f, fullSize.y() - labelHeight);
	mLabel->setSize(fullSize.x(), labelHeight);
	addChild(mLabel);

	setPosition(Renderer::getScreenWidth() * 0.02f, Renderer::getScreenHeight() * 0.04f, 0.0f);
	setOpacity(BASE_OPACITY);
	setVisible(false);
}

VolumeInfoComponent::~VolumeInfoComponent()
{
	delete mLabel;
	delete mFrame;
}

void VolumeInfoComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (mDisplayTime >= 0)
	{
		mDisplayTime += deltaTime;
		if (mDisplayTime > VISIBLE_TIME + FADE_TIME)
		{
			mDisplayTime = -1;

			if (isVisible())
			{
				setVisible(false);
				PowerSaver::resume();
			}
		}
	}

	mCheckTime += deltaTime;
	if (mCheckTime < CHECK_VOLUME_DELAY)
		return;

	mCheckTime = 0;

	const int volume = VolumeControl::getInstance()->getVolume();
	if (volume == mVolume)
		return;

	const bool firstTime = (mVolume < 0);
	mVolume = volume;
	mLabel->setText(mVolume == 0 ? "X" : std::to_string(mVolume) + "%");

	if (!firstTime)
	{
		mDisplayTime = 0;

		if (!isVisible())
		{
			setVisible(true);
			PowerSaver::pause();
		}
	}
}

void VolumeInfoComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible || mDisplayTime < 0)
		return;

	const int opacity = BASE_OPACITY - Math::max(0, (mDisplayTime - VISIBLE_TIME) * BASE_OPACITY / FADE_TIME);
	setOpacity(opacity);

	GuiComponent::render(parentTrans);

	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);

	const bool darkMenu = Settings::getInstance()->getBool("MenuDark");
	const unsigned int trackColor = darkMenu ? 0xFFFFFFFF : 0x555555FF;
	const unsigned int fillColor = darkMenu ? 0xFFFFFFFF : 0x555555FF;

	const float x = PADDING_PX + PADDING_BAR;
	const float y = PADDING_PX * 2.0f;
	const float w = getSize().x() - 2.0f * PADDING_PX - 2.0f * PADDING_BAR;
	const float h = getSize().y() - mLabel->getSize().y() - PADDING_PX * 3.0f;

	const unsigned int track = (trackColor & 0xFFFFFF00) | Math::min(opacity / 2, 0xFF);
	Renderer::drawRect(x, y, w, h, track, track);

	const float filled = (h * mVolume) / 100.0f;
	const unsigned int fill = (fillColor & 0xFFFFFF00) | Math::min(opacity, 0xFF);
	Renderer::drawRect(x, y + h - filled, w, filled, fill, fill);
}
