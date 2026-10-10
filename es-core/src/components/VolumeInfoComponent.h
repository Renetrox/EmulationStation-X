#pragma once

#ifndef ES_CORE_COMPONENTS_VOLUME_INFO_COMPONENT_H
#define ES_CORE_COMPONENTS_VOLUME_INFO_COMPONENT_H

#include "GuiComponent.h"

class NinePatchComponent;
class TextComponent;
class Window;

class VolumeInfoComponent : public GuiComponent
{
public:
	VolumeInfoComponent(Window* window);
	~VolumeInfoComponent();

	void render(const Transform4x4f& parentTrans) override;
	void update(int deltaTime) override;

	void reset() { mVolume = -1; }

private:
	NinePatchComponent* mFrame;
	TextComponent* mLabel;

	int mVolume;
	int mCheckTime;
	int mDisplayTime;
};

#endif // ES_CORE_COMPONENTS_VOLUME_INFO_COMPONENT_H
