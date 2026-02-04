#pragma once

#include "GuiComponent.h"
#include "math/Vector2f.h"

class ScrollableContainer : public GuiComponent
{
public:
	explicit ScrollableContainer(Window* window, int scrollDelay = 0);

	void render(const Transform4x4f& parentTrans) override;
	void update(int deltaTime) override;

	// Firma original (compatibilidad)
	void setAutoScroll(bool autoScroll);

	// NUEVO: delay en runtime
	void setAutoScroll(bool autoScroll, int delayMs);

	Vector2f getScrollPos() const;
	void setScrollPos(const Vector2f& pos);

	Vector2f getContentSize();
	void reset();

private:
	int mAutoScrollDelay;              // ms antes de empezar
	int mAutoScrollSpeed;              // ms entre pasos
	int mAutoScrollAccumulator;

	Vector2f mScrollPos;
	Vector2f mScrollDir;

	int  mAutoScrollResetAccumulator;
	bool mAtEnd = false;

	bool mAutoScroll = false;

	float mAdjustedHeight = 0.0f;
	float mClipSpacing = 0.0f;

	float mAutoScrollResetDelayConstant = 0.0f;
	float mAutoScrollDelayConstant = 0.0f;
	float mAutoScrollSpeedConstant = 0.0f;

	int  mAdjustedAutoScrollSpeed = 0;
	bool mVerticalSnap = true;
	bool mUpdatedSize = false;
};
