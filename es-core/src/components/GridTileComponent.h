#pragma once
#ifndef ES_APP_COMPONENTS_GRID_TILE_COMPONENT_H
#define ES_APP_COMPONENTS_GRID_TILE_COMPONENT_H

#include "GuiComponent.h"

#include "components/ImageComponent.h"
#include "components/NinePatchComponent.h"

#include <memory>
#include <string>

class TextureResource;
class ThemeData;

struct GridTileProperties
{
	Vector2f      mSize;
	Vector2f      mPadding;
	unsigned int  mImageColor;

	// Default (legacy) background (NinePatch)
	std::string   mBackgroundImage;
	Vector2f      mBackgroundCornerSize;
	unsigned int  mBackgroundCenterColor;
	unsigned int  mBackgroundEdgeColor;

	// Optional proportional frame (ImageComponent)
	std::string   mFrameImage;       // if empty -> use NinePatch background
	Vector2f      mFramePadding;     // extra padding around frame inside tile (pixels)
	Vector2f      mFrameMaxSize;     // if (0,0) -> use tile size
	unsigned int  mFrameColor;       // tint color (default white)
};

class GridTileComponent : public GuiComponent
{
public:
	GridTileComponent(Window* window);

	void render(const Transform4x4f& parentTrans) override;
	void update(int deltaTime) override;

	void applyTheme(const std::shared_ptr<ThemeData>& theme,
	                const std::string& view,
	                const std::string& element,
	                unsigned int properties) override;

	static Vector2f getDefaultTileSize();
	Vector2f getSelectedTileSize() const;

	bool isSelected() const;

	void reset();
	void setImage(const std::string& path);
	void setImage(const std::shared_ptr<TextureResource>& texture);

	// New/extended API
	void setSelected(bool selected, bool allowAnimation, Vector3f* pPosition = nullptr, bool force = false);

	// ✅ Compatibility overload: ImageGridComponent calls setSelected(bool)
	void setSelected(bool selected)
	{
		setSelected(selected, false, nullptr, false);
	}

	void setSelectedZoom(float percent);
	void setVisible(bool visible);

	Vector3f getBackgroundPosition();
	std::shared_ptr<TextureResource> getTexture();

	void forceSize(Vector2f size, float selectedZoom);

private:
	void resize();
	void calcCurrentProperties();

	GridTileProperties mDefaultProperties;
	GridTileProperties mSelectedProperties;
	GridTileProperties mCurrentProperties;

	NinePatchComponent mBackground;

	// Optional proportional frame
	std::shared_ptr<ImageComponent> mFrame;

	std::shared_ptr<ImageComponent> mImage;

	// Keep the currently bound path so repeated cursor updates do not
	// drop/reacquire the same texture. This is especially important for
	// dynamically loaded SVG/default artwork on low-power SBCs.
	std::string mImagePath;

	bool mSelected = false;
	bool mVisible = true;

	float mSelectedZoomPercent = 0.0f;
	Vector3f mAnimPosition;
};

#endif // ES_APP_COMPONENTS_GRID_TILE_COMPONENT_H
