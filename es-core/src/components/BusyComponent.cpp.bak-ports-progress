#include "BusyComponent.h"

#include "LocaleES.h"

#include "components/AnimatedImageComponent.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"

#include "renderers/Renderer.h"
#include "resources/Font.h"

// animation definition
static AnimationFrame BUSY_ANIMATION_FRAMES[] = {
	{":/busy_0.svg", 300},
	{":/busy_1.svg", 300},
	{":/busy_2.svg", 300},
	{":/busy_3.svg", 300},
};

static const AnimationDef BUSY_ANIMATION_DEF = { BUSY_ANIMATION_FRAMES, 4, true };

BusyComponent::BusyComponent(Window* window)
	: GuiComponent(window)
	, mBackground(window, ":/frame.png")
	, mGrid(window, Vector2i(5, 3))
{
	mAnimation = std::make_shared<AnimatedImageComponent>(mWindow);
	mAnimation->load(&BUSY_ANIMATION_DEF);

	mText = std::make_shared<TextComponent>(
		mWindow,
		LocaleES::get("WORKING"),
		Font::get(FONT_SIZE_MEDIUM),
		0x777777FF
	);

	// col 1 = animation, col 2 = spacer, col 3 = text
	mGrid.setEntry(mAnimation, Vector2i(1, 1), false, true);
	mGrid.setEntry(mText,      Vector2i(3, 1), false, true);

	addChild(&mBackground);
	addChild(&mGrid);
}

void BusyComponent::onSizeChanged()
{
	mGrid.setSize(mSize);

	if (mSize.x() == 0 || mSize.y() == 0)
		return;

	const float middleSpacerWidth = 0.01f * Renderer::getScreenWidth();
	const float textHeight = mText->getFont()->getLetterHeight();

	// Recalcular ancho del texto
	mText->setSize(0, textHeight);
	const float textWidth = mText->getSize().x() + 4;

	// animation is square
	mGrid.setColWidthPerc(1, textHeight / mSize.x());
	mGrid.setColWidthPerc(2, middleSpacerWidth / mSize.x());
	mGrid.setColWidthPerc(3, textWidth / mSize.x());

	mGrid.setRowHeightPerc(1, textHeight / mSize.y());

	// Ajustar el frame al contenido real
	mBackground.fitTo(
		Vector2f(
			mGrid.getColWidth(1) + mGrid.getColWidth(2) + mGrid.getColWidth(3),
			textHeight + 2
		),
		mAnimation->getPosition(),
		Vector2f(0, 0)
	);
}

void BusyComponent::reset()
{
	// ✅ Reinicia la animación sin depender de AnimatedImageComponent::reset()
	// Recargar el AnimationDef reinicia frame/timer en prácticamente todos los forks.
	if (mAnimation)
		mAnimation->load(&BUSY_ANIMATION_DEF);
}