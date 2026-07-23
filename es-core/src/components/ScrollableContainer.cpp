#include "components/ScrollableContainer.h"

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
}

void ScrollableContainer::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans = parentTrans * getTransform();

	Vector2i clipPos((int)trans.translation().x(), (int)trans.translation().y());

	Vector3f dimScaled = trans * Vector3f(mSize.x(), mSize.y(), 0);
	Vector2i clipDim(
		(int)(dimScaled.x() - trans.translation().x()),
		(int)(dimScaled.y() - trans.translation().y())
	);

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
		mAutoScrollSpeed = AUTO_SCROLL_SPEED;
		reset();
	}
	else
	{
		mScrollDir = Vector2f(0, 0);
		mAutoScrollSpeed = 0;
		mAutoScrollAccumulator = 0;
		mAutoScrollResetAccumulator = 0;
		mScrollPos = Vector2f(0, 0);
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
if (mAutoScrollSpeed != 0)
{
    mAutoScrollAccumulator += deltaTime;

    const int steps = mAutoScrollAccumulator / mAutoScrollSpeed;
    if (steps > 0)
    {
        mScrollPos += mScrollDir * (float)steps;
        mAutoScrollAccumulator -= steps * mAutoScrollSpeed;
    }
}


	// límites
	if (mScrollPos.x() < 0)
		mScrollPos[0] = 0;
	if (mScrollPos.y() < 0)
		mScrollPos[1] = 0;

	const Vector2f contentSize = getContentSize();

	if (contentSize.y() < getSize().y())
	{
		mScrollPos[1] = 0;
		mAtEnd = false;
	}
	else if (mScrollPos.y() + getSize().y() > contentSize.y())
	{
		mScrollPos[1] = contentSize.y() - getSize().y();
		mAtEnd = true;
	}

	if (mAtEnd)
	{
		mAutoScrollResetAccumulator += deltaTime;
		if (mAutoScrollResetAccumulator >= AUTO_SCROLL_RESET_DELAY)
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
}
