#include "components/ScrollableContainer.h"

#include "components/TextComponent.h"
#include "math/Vector2i.h"
#include "renderers/Renderer.h"

#include <algorithm> // std::max

#define AUTO_SCROLL_RESET_DELAY 3000 // ms to reset to top after we reach the bottom
#define AUTO_SCROLL_DELAY       1000 // ms to wait before we start to scroll (legacy default)
#define AUTO_SCROLL_SPEED       50   // ms between scrolls

ScrollableContainer::ScrollableContainer(Window* window, int scrollDelay)
	: GuiComponent(window),
	  mAutoScrollDelay(scrollDelay),
	  mAutoScrollSpeed(0),
	  mAutoScrollAccumulator(0),
	  mScrollPos(0, 0),
	  mScrollDir(0, 0),
	  mAutoScrollResetAccumulator(0),
	  mAtEnd(false),
	  mAutoScroll(false)
{
	mAutoScrollResetDelayConstant = AUTO_SCROLL_RESET_DELAY;
	mAutoScrollDelayConstant = AUTO_SCROLL_DELAY;
	mAutoScrollSpeedConstant = AUTO_SCROLL_SPEED;
}

void ScrollableContainer::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	auto* text = dynamic_cast<TextComponent*>(mChildren.empty() ? nullptr : mChildren.front());
	if (text && text->getValue().empty())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	Vector2i clipPos((int)trans.translation().x(), (int)trans.translation().y());

	Vector3f dimScaled = trans * Vector3f(mSize.x(), mAdjustedHeight > 0.0f ? mAdjustedHeight : mSize.y(), 0);
	Vector2i clipDim(
		(int)(dimScaled.x() - trans.translation().x()),
		(int)(dimScaled.y() - trans.translation().y())
	);

	clipPos[1] += (int)mClipSpacing;
	clipDim[1] -= (int)mClipSpacing;

	Renderer::pushClipRect(clipPos, clipDim);

	trans.translate(-Vector3f(mScrollPos.x(), mScrollPos.y(), 0));
	Renderer::setMatrix(trans);

	GuiComponent::renderChildren(trans);

	Renderer::popClipRect();
}

// Compatibilidad con código viejo (si el delay es 0, usamos default legacy 1000ms)
void ScrollableContainer::setAutoScroll(bool autoScroll)
{
	int delay = mAutoScrollDelay;
	if (delay <= 0)
		delay = AUTO_SCROLL_DELAY;

	setAutoScroll(autoScroll, delay);
}

// NUEVO: delay mutable (respeta 0 = arranque inmediato)
void ScrollableContainer::setAutoScroll(bool autoScroll, int delayMs)
{
	if (delayMs < 0)
		delayMs = 0;

	mAutoScroll = autoScroll;
	mAutoScrollDelay = delayMs;

	if (autoScroll)
	{
		mScrollDir = Vector2f(0, 1);
		mAutoScrollSpeed = (int)mAutoScrollSpeedConstant;
		reset();
	}
	else
	{
		mScrollDir = Vector2f(0, 0);
		mAutoScrollSpeed = 0;
		mAutoScrollAccumulator = 0;
		mAutoScrollResetAccumulator = 0;
		mAtEnd = false;
	}
}

Vector2f ScrollableContainer::getScrollPos() const
{
	return mScrollPos;
}

void ScrollableContainer::setScrollPos(const Vector2f& pos)
{
	mScrollPos = pos;
}

void ScrollableContainer::update(int deltaTime)
{
	if (!isVisible() || mSize == Vector2f(0, 0))
		return;

	auto* text = dynamic_cast<TextComponent*>(mChildren.empty() ? nullptr : mChildren.front());
	if (!text || !text->getFont())
	{
		GuiComponent::update(deltaTime);
		return;
	}

	const Vector2f contentSize = getContentSize();
	const float lineSpacing = text->getLineSpacing();
	const float lineHeight = text->getFont()->getHeight(lineSpacing);
	if (lineHeight <= 0.0f)
		return;

	float rowModifier = 1.0f;

	if (lineSpacing > 1.2f && mClipSpacing == 0.0f)
	{
		const float minimumSpacing = text->getFont()->getHeight(1.2f);
		const float currentSpacing = text->getFont()->getHeight(lineSpacing);
		mClipSpacing = Math::round((currentSpacing - minimumSpacing) / 2.0f);
	}

	if (!mUpdatedSize)
	{
		if (mVerticalSnap)
		{
			float numLines = Math::floorf(mSize.y() / lineHeight);
			if (numLines == 0)
				numLines = 1;
			mAdjustedHeight = Math::ceilf(numLines * lineHeight);
		}
		else
		{
			mAdjustedHeight = mSize.y();
		}
		mUpdatedSize = true;
	}

	if (!mAdjustedAutoScrollSpeed)
	{
		const float fontSize = (float)text->getFont()->getSize();
		float width = contentSize.x() / (fontSize * 1.3f);
		float speedModifier = Math::clamp(width, 10.0f, 40.0f);
		speedModifier *= mAutoScrollSpeedConstant;
		mAdjustedAutoScrollSpeed = (int)speedModifier;
	}

	const float lines = mAdjustedHeight / lineHeight;
	if (lines < 8.0f)
		rowModifier = lines / 8.0f;

	if (mAdjustedAutoScrollSpeed < 0)
		mAdjustedAutoScrollSpeed = 1;

	if (mAdjustedAutoScrollSpeed != 0)
	{
		mAutoScrollAccumulator += deltaTime;

		while (mAutoScrollAccumulator >= (int)(rowModifier * (float)mAdjustedAutoScrollSpeed))
		{
			if (!mAtEnd && contentSize.y() > mAdjustedHeight)
				mScrollPos += mScrollDir;
			mAutoScrollAccumulator -= (int)(rowModifier * (float)mAdjustedAutoScrollSpeed);
		}
	}

	// límites
	if (mScrollPos.x() < 0)
		mScrollPos[0] = 0;
	if (mScrollPos.y() < 0)
		mScrollPos[1] = 0;

	if (contentSize.y() < mAdjustedHeight)
	{
		mScrollPos[1] = 0;
		mAtEnd = false;
	}
	else if (mScrollPos.y() + mAdjustedHeight > contentSize.y())
	{
		mScrollPos[1] = contentSize.y() - mAdjustedHeight;
		mAtEnd = true;
	}

	if (mAtEnd)
	{
		mAutoScrollResetAccumulator += deltaTime;
		if (mAutoScrollResetAccumulator >= (int)mAutoScrollResetDelayConstant)
			reset();
	}

	GuiComponent::update(deltaTime);
}

Vector2f ScrollableContainer::getContentSize()
{
	Vector2f max(0, 0);
	for (auto* child : mChildren)
	{
		Vector2f pos(child->getPosition().x(), child->getPosition().y());
		Vector2f br = child->getSize() + pos;
		max.x() = std::max(max.x(), br.x());
		max.y() = std::max(max.y(), br.y());
	}
	return max;
}

void ScrollableContainer::reset()
{
	mScrollPos = Vector2f(0, 0);
	mAutoScrollResetAccumulator = 0;

	// Delay real antes de arrancar: acumulador negativo
	// Si delay=0 => arranca inmediato
	mAutoScrollAccumulator = -mAutoScrollDelay + mAutoScrollSpeed;

	mAtEnd = false;
	mUpdatedSize = false;
	mAdjustedAutoScrollSpeed = 0;
	mClipSpacing = 0.0f;
}
