#include "GridTileComponent.h"

#include "animations/LambdaAnimation.h"
#include "resources/TextureResource.h"
#include "ThemeData.h"
#include "renderers/Renderer.h"

static inline bool isZero(const Vector2f& v)
{
	return v.x() == 0.0f && v.y() == 0.0f;
}

static inline Vector2f lerpVec2(const Vector2f& a, const Vector2f& b, float t)
{
	return a * (1.0f - t) + b * t;
}

static unsigned int mixColors(unsigned int first, unsigned int second, float percent)
{
	unsigned char alpha0 = (first >> 24) & 0xFF;
	unsigned char blue0  = (first >> 16) & 0xFF;
	unsigned char green0 = (first >> 8)  & 0xFF;
	unsigned char red0   = first & 0xFF;

	unsigned char alpha1 = (second >> 24) & 0xFF;
	unsigned char blue1  = (second >> 16) & 0xFF;
	unsigned char green1 = (second >> 8)  & 0xFF;
	unsigned char red1   = second & 0xFF;

	unsigned char alpha = (unsigned char)(alpha0 * (1.0f - percent) + alpha1 * percent);
	unsigned char blue  = (unsigned char)(blue0  * (1.0f - percent) + blue1 * percent);
	unsigned char green = (unsigned char)(green0 * (1.0f - percent) + green1 * percent);
	unsigned char red   = (unsigned char)(red0   * (1.0f - percent) + red1 * percent);

	return (alpha << 24) | (blue << 16) | (green << 8) | red;
}

GridTileComponent::GridTileComponent(Window* window)
	: GuiComponent(window)
	, mBackground(window, ":/frame.png")
{
	// ----------------------------
	// Defaults
	// ----------------------------
	mDefaultProperties.mSize = getDefaultTileSize();
	mDefaultProperties.mPadding = Vector2f(16.0f, 16.0f);
	mDefaultProperties.mImageColor = 0xAAAAAABB;

	// Legacy NinePatch background
	mDefaultProperties.mBackgroundImage = ":/frame.png";
	mDefaultProperties.mBackgroundCornerSize = Vector2f(16, 16);
	mDefaultProperties.mBackgroundCenterColor = 0xAAAAEEFF;
	mDefaultProperties.mBackgroundEdgeColor = 0xAAAAEEFF;

	// Optional proportional frame (OFF by default)
	mDefaultProperties.mFrameImage = "";
	mDefaultProperties.mFramePadding = Vector2f(0.0f, 0.0f);
	mDefaultProperties.mFrameMaxSize = Vector2f(0.0f, 0.0f);
	mDefaultProperties.mFrameColor = 0xFFFFFFFF;

	// Selected defaults
	mSelectedProperties = mDefaultProperties;
	mSelectedProperties.mSize = getSelectedTileSize();
	mSelectedProperties.mImageColor = 0xFFFFFFFF;
	mSelectedProperties.mBackgroundCenterColor = 0xFFFFFFFF;
	mSelectedProperties.mBackgroundEdgeColor = 0xFFFFFFFF;
	mSelectedProperties.mFrameColor = 0xFFFFFFFF;

	// ----------------------------
	// Components
	// ----------------------------
	mImage = std::make_shared<ImageComponent>(mWindow);
	mImage->setOrigin(0.5f, 0.5f);

	// Optional proportional frame component
	mFrame = std::make_shared<ImageComponent>(mWindow);
	mFrame->setOrigin(0.5f, 0.5f);
	mFrame->setImage(""); // inactive by default

	mBackground.setOrigin(0.5f, 0.5f);

	// Keep children, but we will render conditionally
	addChild(&mBackground);
	addChild(&(*mFrame));
	addChild(&(*mImage));

	mSelectedZoomPercent = 0.0f;

	setSelected(false, false);
	setVisible(true);
}

void GridTileComponent::render(const Transform4x4f& parentTrans)
{
	if (!mVisible)
		return;

	Transform4x4f trans = getTransform() * parentTrans;

	// We render manually to avoid drawing both frame styles at the same time.
	const bool useFrameImage = !mCurrentProperties.mFrameImage.empty();

	if (useFrameImage)
	{
		mFrame->render(trans);
		mImage->render(trans);
	}
	else
	{
		mBackground.render(trans);
		mImage->render(trans);
	}
}

void GridTileComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	calcCurrentProperties();

	// Decide frame mode
	const bool useFrameImage = !mCurrentProperties.mFrameImage.empty();

	// Update image and colors (always)
	mImage->setColorShift(mCurrentProperties.mImageColor);

	if (useFrameImage)
	{
		// Proportional frame
		mFrame->setImage(mCurrentProperties.mFrameImage);
		mFrame->setColorShift(mCurrentProperties.mFrameColor);
	}
	else
	{
		// Legacy NinePatch background
		mBackground.setImagePath(mCurrentProperties.mBackgroundImage);
		mBackground.setCenterColor(mCurrentProperties.mBackgroundCenterColor);
		mBackground.setEdgeColor(mCurrentProperties.mBackgroundEdgeColor);

		// Ensure frame is inactive
		mFrame->setImage("");
	}

	resize();
}

static void applyThemeToProperties(const ThemeData::ThemeElement* elem, GridTileProperties* properties)
{
	Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	if (elem->has("size"))
		properties->mSize = elem->get<Vector2f>("size") * screen;

	if (elem->has("padding"))
	{
		properties->mPadding = elem->get<Vector2f>("padding") * screen;

		// hack to fix broken themes now that this uses percentage rather than pixels
		if (properties->mPadding.x() > screen.x())
			properties->mPadding /= screen;
	}

	if (elem->has("imageColor"))
		properties->mImageColor = elem->get<unsigned int>("imageColor");

	// Legacy NinePatch background
	if (elem->has("backgroundImage"))
		properties->mBackgroundImage = elem->get<std::string>("backgroundImage");

	if (elem->has("backgroundCornerSize"))
	{
		properties->mBackgroundCornerSize = elem->get<Vector2f>("backgroundCornerSize") * screen;

		// hack to fix broken themes now that this uses percentage rather than pixels
		if (properties->mBackgroundCornerSize.x() > screen.x())
			properties->mBackgroundCornerSize /= screen;
	}

	if (elem->has("backgroundColor"))
	{
		properties->mBackgroundCenterColor = elem->get<unsigned int>("backgroundColor");
		properties->mBackgroundEdgeColor   = elem->get<unsigned int>("backgroundColor");
	}

	if (elem->has("backgroundCenterColor"))
		properties->mBackgroundCenterColor = elem->get<unsigned int>("backgroundCenterColor");

	if (elem->has("backgroundEdgeColor"))
		properties->mBackgroundEdgeColor = elem->get<unsigned int>("backgroundEdgeColor");

	// ----------------------------
	// NEW: proportional frame options
	// ----------------------------
	if (elem->has("frameImage"))
		properties->mFrameImage = elem->get<std::string>("frameImage");

	if (elem->has("frameColor"))
		properties->mFrameColor = elem->get<unsigned int>("frameColor");

	if (elem->has("framePadding"))
	{
		properties->mFramePadding = elem->get<Vector2f>("framePadding") * screen;
		if (properties->mFramePadding.x() > screen.x())
			properties->mFramePadding /= screen;
	}

	if (elem->has("frameMaxSize"))
	{
		properties->mFrameMaxSize = elem->get<Vector2f>("frameMaxSize") * screen;
		if (properties->mFrameMaxSize.x() > screen.x())
			properties->mFrameMaxSize /= screen;
	}
}

void GridTileComponent::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                  const std::string& view,
                                  const std::string& /*element*/,
                                  unsigned int /*properties*/)
{
	// Apply theme to the default gridtile
	const ThemeData::ThemeElement* elem = theme->getElement(view, "default", "gridtile");
	if (elem)
		applyThemeToProperties(elem, &mDefaultProperties);

	// Apply theme to the selected gridtile
	// NOTE that some of the default gridtile properties influence on the selected gridtile properties
	// See THEMES.md for more informations
	elem = theme->getElement(view, "selected", "gridtile");

	// Reset selected from default (keeps compatibility)
	mSelectedProperties = mDefaultProperties;
	mSelectedProperties.mSize = getSelectedTileSize();

	// If selected has overrides, apply them
	if (elem)
		applyThemeToProperties(elem, &mSelectedProperties);
}

Vector2f GridTileComponent::getDefaultTileSize()
{
	Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	return screen * 0.22f;
}

Vector2f GridTileComponent::getSelectedTileSize() const
{
	return mDefaultProperties.mSize * 1.2f;
}

bool GridTileComponent::isSelected() const
{
	return mSelected;
}

void GridTileComponent::reset()
{
	// Keep the currently bound texture resident. Both grid implementations hide
	// a tile before rebinding it, so clearing the texture here only creates a
	// needless release/reacquire cycle and can make dynamic/default artwork blink.
}

void GridTileComponent::setImage(const std::string& path)
{
	if (path == mImagePath)
		return;

	mImagePath = path;
	mImage->setImage(path);
	resize(); // prevent flickering
}

void GridTileComponent::setImage(const std::shared_ptr<TextureResource>& texture)
{
	// A raw texture binding has no stable path identity, so invalidate the
	// path cache to ensure a later path-based binding is not skipped.
	mImagePath.clear();
	mImage->setImage(texture);
	resize(); // prevent flickering
}

void GridTileComponent::setSelected(bool selected, bool allowAnimation, Vector3f* pPosition, bool force)
{
	if (mSelected == selected && !force)
		return;

	mSelected = selected;

	if (selected)
	{
		if (pPosition == nullptr || !allowAnimation)
		{
			cancelAnimation(3);
			setSelectedZoom(1.0f);
			mAnimPosition = Vector3f(0, 0, 0);
			resize();
		}
		else
		{
			mAnimPosition = Vector3f(pPosition->x(), pPosition->y(), pPosition->z());

			auto func = [this](float t)
			{
				t -= 1.0f; // cubic ease out
				float pct = Math::lerp(0.0f, 1.0f, t * t * t + 1.0f);
				setSelectedZoom(pct);
			};

			cancelAnimation(3);
			setAnimation(new LambdaAnimation(func, 250), 0, [this] {
				setSelectedZoom(1.0f);
				mAnimPosition = Vector3f(0, 0, 0);
			}, false, 3);
		}
	}
	else
	{
		if (!allowAnimation)
		{
			cancelAnimation(3);
			setSelectedZoom(0.0f);
			resize();
		}
		else
		{
			setSelectedZoom(1.0f);

			auto func = [this](float t)
			{
				t -= 1.0f; // cubic ease out
				float pct = Math::lerp(0.0f, 1.0f, t * t * t + 1.0f);
				setSelectedZoom(1.0f - pct);
			};

			cancelAnimation(3);
			setAnimation(new LambdaAnimation(func, 250), 0, [this] {
				setSelectedZoom(0.0f);
			}, false, 3);
		}
	}
}

void GridTileComponent::setSelectedZoom(float percent)
{
	if (mSelectedZoomPercent == percent)
		return;

	mSelectedZoomPercent = percent;
	resize();
}

void GridTileComponent::setVisible(bool visible)
{
	mVisible = visible;
}

void GridTileComponent::resize()
{
	calcCurrentProperties();

	// Image always keeps aspect within tile
	mImage->setMaxSize(mCurrentProperties.mSize - mCurrentProperties.mPadding * 2);

	const bool useFrameImage = !mCurrentProperties.mFrameImage.empty();

	if (useFrameImage)
	{
		// Proportional frame keeps aspect using maxSize
		Vector2f frameBox = isZero(mCurrentProperties.mFrameMaxSize) ? mCurrentProperties.mSize : mCurrentProperties.mFrameMaxSize;
		Vector2f frameMax = frameBox - (mCurrentProperties.mFramePadding * 2.0f);

		if (frameMax.x() < 0.0f) frameMax.x() = 0.0f;
		if (frameMax.y() < 0.0f) frameMax.y() = 0.0f;

		mFrame->setMaxSize(frameMax);
	}
	else
	{
		// Legacy NinePatch stretches to tile
		mBackground.setCornerSize(mCurrentProperties.mBackgroundCornerSize);
		mBackground.fitTo(mCurrentProperties.mSize - mBackground.getCornerSize() * 2);
	}
}

void GridTileComponent::calcCurrentProperties()
{
	// Base selection
	mCurrentProperties = mSelected ? mSelectedProperties : mDefaultProperties;

	// Mid animation: interpolate numeric fields
	if (mSelectedZoomPercent != 0.0f && mSelectedZoomPercent != 1.0f)
	{
		float t = mSelectedZoomPercent;

		if (mDefaultProperties.mSize != mSelectedProperties.mSize)
			mCurrentProperties.mSize = lerpVec2(mDefaultProperties.mSize, mSelectedProperties.mSize, t);

		if (mDefaultProperties.mPadding != mSelectedProperties.mPadding)
			mCurrentProperties.mPadding = lerpVec2(mDefaultProperties.mPadding, mSelectedProperties.mPadding, t);

		if (mDefaultProperties.mImageColor != mSelectedProperties.mImageColor)
			mCurrentProperties.mImageColor = mixColors(mDefaultProperties.mImageColor, mSelectedProperties.mImageColor, t);

		if (mDefaultProperties.mBackgroundCornerSize != mSelectedProperties.mBackgroundCornerSize)
			mCurrentProperties.mBackgroundCornerSize = lerpVec2(mDefaultProperties.mBackgroundCornerSize, mSelectedProperties.mBackgroundCornerSize, t);

		if (mDefaultProperties.mBackgroundCenterColor != mSelectedProperties.mBackgroundCenterColor)
			mCurrentProperties.mBackgroundCenterColor = mixColors(mDefaultProperties.mBackgroundCenterColor, mSelectedProperties.mBackgroundCenterColor, t);

		if (mDefaultProperties.mBackgroundEdgeColor != mSelectedProperties.mBackgroundEdgeColor)
			mCurrentProperties.mBackgroundEdgeColor = mixColors(mDefaultProperties.mBackgroundEdgeColor, mSelectedProperties.mBackgroundEdgeColor, t);

		// Frame numeric fields
		if (mDefaultProperties.mFramePadding != mSelectedProperties.mFramePadding)
			mCurrentProperties.mFramePadding = lerpVec2(mDefaultProperties.mFramePadding, mSelectedProperties.mFramePadding, t);

		if (mDefaultProperties.mFrameMaxSize != mSelectedProperties.mFrameMaxSize)
			mCurrentProperties.mFrameMaxSize = lerpVec2(mDefaultProperties.mFrameMaxSize, mSelectedProperties.mFrameMaxSize, t);

		if (mDefaultProperties.mFrameColor != mSelectedProperties.mFrameColor)
			mCurrentProperties.mFrameColor = mixColors(mDefaultProperties.mFrameColor, mSelectedProperties.mFrameColor, t);

		// Frame image string: choose a stable side to avoid flicker
		if (mDefaultProperties.mFrameImage != mSelectedProperties.mFrameImage)
			mCurrentProperties.mFrameImage = (t >= 0.5f) ? mSelectedProperties.mFrameImage : mDefaultProperties.mFrameImage;

		// Background image string: same idea
		if (mDefaultProperties.mBackgroundImage != mSelectedProperties.mBackgroundImage)
			mCurrentProperties.mBackgroundImage = (t >= 0.5f) ? mSelectedProperties.mBackgroundImage : mDefaultProperties.mBackgroundImage;
	}
}

Vector3f GridTileComponent::getBackgroundPosition()
{
	return mBackground.getPosition() + mPosition;
}

std::shared_ptr<TextureResource> GridTileComponent::getTexture()
{
	if (mImage != nullptr)
		return mImage->getTexture();

	return nullptr;
}

void GridTileComponent::forceSize(Vector2f size, float selectedZoom)
{
	mDefaultProperties.mSize = size;
	mSelectedProperties.mSize = size * selectedZoom;
}
