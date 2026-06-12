#include "components/ImageComponent.h"

#include "resources/TextureResource.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"

#include <cmath>

Vector2i ImageComponent::getTextureSize() const
{
	if (mTexture)
		return mTexture->getSize();
	else
		return Vector2i::Zero();
}

Vector2f ImageComponent::getSize() const
{
	return GuiComponent::getSize() * (mBottomRightCrop - mTopLeftCrop);
}

ImageComponent::ImageComponent(Window* window, bool forceLoad, bool dynamic) :
	GuiComponent(window),
	mTargetSize(0, 0),
	mFlipX(false),
	mFlipY(false),
	mTargetIsMax(false),
	mTargetIsMin(false),
	mColorShift(0xFFFFFFFF),
	mColorShiftEnd(0xFFFFFFFF),
	mColorGradientHorizontal(true),
	mFadeOpacity(0),
	mFading(false),
	mForceLoad(forceLoad),
	mDynamic(dynamic),
	mRotateByTargetSize(false),
	mTopLeftCrop(0.0f, 0.0f),
	mBottomRightCrop(1.0f, 1.0f),
	mPulseEnabled(false),
	mPulseOpacityMin(0.45f),
	mPulseOpacityMax(1.0f),
	mPulseDuration(2400.0f),
	mPulseElapsed(0.0f),
	mPulseFactor(1.0f)
{
	updateColors();
}

ImageComponent::~ImageComponent()
{
}

void ImageComponent::resize()
{
	if (!mTexture)
		return;

	const Vector2f textureSize = mTexture->getSourceImageSize();
	if (textureSize == Vector2f::Zero())
		return;

	if (mTexture->isTiled())
	{
		mSize = mTargetSize;
	}
	else
	{
		// SVG rasterization is determined by height (see SVGResource.cpp), and
		// rasterization is done in terms of pixels. If rounding is off enough in
		// the rasterization step, images with extreme aspect ratios may be cut off.
		if (mTargetIsMax)
		{
			mSize = textureSize;

			Vector2f resizeScale(
				(mTargetSize.x() / mSize.x()),
				(mTargetSize.y() / mSize.y()));

			if (resizeScale.x() < resizeScale.y())
			{
				mSize[0] *= resizeScale.x();
				mSize[1] = Math::min(
					Math::round(mSize[1] *= resizeScale.x()),
					mTargetSize.y());
			}
			else
			{
				mSize[1] = Math::round(mSize[1] * resizeScale.y());
				mSize[0] = Math::min(
					(mSize[1] / textureSize.y()) * textureSize.x(),
					mTargetSize.x());
			}
		}
		else if (mTargetIsMin)
		{
			mSize = textureSize;

			Vector2f resizeScale(
				(mTargetSize.x() / mSize.x()),
				(mTargetSize.y() / mSize.y()));

			if (resizeScale.x() > resizeScale.y())
			{
				mSize[0] *= resizeScale.x();
				mSize[1] *= resizeScale.x();

				float cropPercent =
					(mSize.y() - mTargetSize.y()) / (mSize.y() * 2);

				crop(0, cropPercent, 0, cropPercent);
			}
			else
			{
				mSize[0] *= resizeScale.y();
				mSize[1] *= resizeScale.y();

				float cropPercent =
					(mSize.x() - mTargetSize.x()) / (mSize.x() * 2);

				crop(cropPercent, 0, cropPercent, 0);
			}

			mSize[1] = Math::max(Math::round(mSize[1]), mTargetSize.y());
			mSize[0] = Math::max(
				(mSize[1] / textureSize.y()) * textureSize.x(),
				mTargetSize.x());
		}
		else
		{
			// If both components are set, stretch.
			// If no components are set, don't resize.
			mSize =
				mTargetSize == Vector2f::Zero() ? textureSize : mTargetSize;

			// If only one component is set, maintain aspect ratio.
			if (!mTargetSize.x() && mTargetSize.y())
			{
				mSize[1] = Math::round(mTargetSize.y());
				mSize[0] =
					(mSize.y() / textureSize.y()) * textureSize.x();
			}
			else if (mTargetSize.x() && !mTargetSize.y())
			{
				mSize[1] = Math::round(
					(mTargetSize.x() / textureSize.x()) * textureSize.y());

				mSize[0] =
					(mSize.y() / textureSize.y()) * textureSize.x();
			}
		}
	}

	mSize[0] = Math::round(mSize.x());
	mSize[1] = Math::round(mSize.y());

	mTexture->rasterizeAt(
		(size_t)mSize.x(),
		(size_t)mSize.y());

	onSizeChanged();
}

void ImageComponent::onSizeChanged()
{
	updateVertices();
}

void ImageComponent::setDefaultImage(std::string path)
{
	mDefaultPath = path;
}

void ImageComponent::setImage(std::string path, bool tile)
{
	if (path.empty() ||
		!ResourceManager::getInstance()->fileExists(path))
	{
		if (mDefaultPath.empty() ||
			!ResourceManager::getInstance()->fileExists(mDefaultPath))
		{
			mTexture.reset();
		}
		else
		{
			mTexture = TextureResource::get(
				mDefaultPath,
				tile,
				mForceLoad,
				mDynamic);
		}
	}
	else
	{
		mTexture = TextureResource::get(
			path,
			tile,
			mForceLoad,
			mDynamic);
	}

	resize();
}

void ImageComponent::setImage(
	const char* path,
	size_t length,
	bool tile)
{
	mTexture.reset();

	mTexture = TextureResource::get("", tile);
	mTexture->initFromMemory(path, length);

	resize();
}

void ImageComponent::setImage(
	const std::shared_ptr<TextureResource>& texture)
{
	mTexture = texture;
	resize();
}

void ImageComponent::setResize(float width, float height)
{
	mTargetSize = Vector2f(width, height);
	mTargetIsMax = false;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMaxSize(float width, float height)
{
	mTargetSize = Vector2f(width, height);
	mTargetIsMax = true;
	mTargetIsMin = false;
	resize();
}

void ImageComponent::setMinSize(float width, float height)
{
	mTargetSize = Vector2f(width, height);
	mTargetIsMax = false;
	mTargetIsMin = true;
	resize();
}

Vector2f ImageComponent::getRotationSize() const
{
	return mRotateByTargetSize ? mTargetSize : mSize;
}

void ImageComponent::setRotateByTargetSize(bool rotate)
{
	mRotateByTargetSize = rotate;
}

void ImageComponent::cropLeft(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mTopLeftCrop.x() = percent;
}

void ImageComponent::cropTop(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mTopLeftCrop.y() = percent;
}

void ImageComponent::cropRight(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mBottomRightCrop.x() = 1.0f - percent;
}

void ImageComponent::cropBot(float percent)
{
	assert(percent >= 0.0f && percent <= 1.0f);
	mBottomRightCrop.y() = 1.0f - percent;
}

void ImageComponent::crop(
	float left,
	float top,
	float right,
	float bot)
{
	cropLeft(left);
	cropTop(top);
	cropRight(right);
	cropBot(bot);
}

void ImageComponent::uncrop()
{
	crop(0, 0, 0, 0);
}

void ImageComponent::setFlipX(bool flip)
{
	mFlipX = flip;
	updateVertices();
}

void ImageComponent::setFlipY(bool flip)
{
	mFlipY = flip;
	updateVertices();
}

void ImageComponent::setColorShift(unsigned int color)
{
	mColorShift = color;
	mColorShiftEnd = color;
	updateColors();
}

void ImageComponent::setColorShiftEnd(unsigned int color)
{
	mColorShiftEnd = color;
	updateColors();
}

void ImageComponent::setColorGradientHorizontal(bool horizontal)
{
	mColorGradientHorizontal = horizontal;
	updateColors();
}

void ImageComponent::setOpacity(unsigned char opacity)
{
	mOpacity = opacity;
	updateColors();
}

void ImageComponent::update(int deltaTime)
{
	GuiComponent::update(deltaTime);

	if (!mPulseEnabled || mPulseDuration <= 0.0f)
	{
		if (mPulseFactor != 1.0f)
		{
			mPulseFactor = 1.0f;
			updateColors();
		}

		return;
	}

	mPulseElapsed += static_cast<float>(deltaTime);

	while (mPulseElapsed >= mPulseDuration)
		mPulseElapsed -= mPulseDuration;

	const float phase = mPulseElapsed / mPulseDuration;

	// Onda coseno suave:
	// mínimo -> máximo -> mínimo.
	const float wave =
		0.5f - 0.5f *
		std::cos(phase * 6.28318530717958647692f);

	mPulseFactor =
		mPulseOpacityMin +
		(mPulseOpacityMax - mPulseOpacityMin) * wave;

	updateColors();
}

void ImageComponent::updateVertices()
{
	if (!mTexture || !mTexture->isInitialized())
		return;

	const Vector2f topLeft = {
		mSize * mTopLeftCrop
	};

	const Vector2f bottomRight = {
		mSize * mBottomRightCrop
	};

	const float px =
		mTexture->isTiled() ?
		mSize.x() / getTextureSize().x() :
		1.0f;

	const float py =
		mTexture->isTiled() ?
		mSize.y() / getTextureSize().y() :
		1.0f;

	mVertices[0] = {
		{ topLeft.x(), topLeft.y() },
		{ mTopLeftCrop.x(), py - mTopLeftCrop.y() },
		0
	};

	mVertices[1] = {
		{ topLeft.x(), bottomRight.y() },
		{ mTopLeftCrop.x(), 1.0f - mBottomRightCrop.y() },
		0
	};

	mVertices[2] = {
		{ bottomRight.x(), topLeft.y() },
		{ mBottomRightCrop.x() * px, py - mTopLeftCrop.y() },
		0
	};

	mVertices[3] = {
		{ bottomRight.x(), bottomRight.y() },
		{ mBottomRightCrop.x() * px, 1.0f - mBottomRightCrop.y() },
		0
	};

	updateColors();

	for (int i = 0; i < 4; ++i)
		mVertices[i].pos.round();

	if (mFlipX)
	{
		for (int i = 0; i < 4; ++i)
			mVertices[i].tex[0] = px - mVertices[i].tex[0];
	}

	if (mFlipY)
	{
		for (int i = 0; i < 4; ++i)
			mVertices[i].tex[1] = py - mVertices[i].tex[1];
	}
}

void ImageComponent::updateColors()
{
	const float fadeFactor =
		mFading ?
		mFadeOpacity / 255.0f :
		1.0f;

	const float pulseFactor =
		mPulseEnabled ?
		mPulseFactor :
		1.0f;

	const float opacity =
		(mOpacity / 255.0f) *
		fadeFactor *
		pulseFactor;

	const unsigned int color =
		Renderer::convertColor(
			(mColorShift & 0xFFFFFF00) |
			(unsigned char)((mColorShift & 0xFF) * opacity));

	const unsigned int colorEnd =
		Renderer::convertColor(
			(mColorShiftEnd & 0xFFFFFF00) |
			(unsigned char)((mColorShiftEnd & 0xFF) * opacity));

	mVertices[0].col = color;
	mVertices[1].col =
		mColorGradientHorizontal ? colorEnd : color;
	mVertices[2].col =
		mColorGradientHorizontal ? color : colorEnd;
	mVertices[3].col = colorEnd;
}

void ImageComponent::render(const Transform4x4f& parentTrans)
{
	if (!isVisible())
		return;

	Transform4x4f trans =
		parentTrans * getTransform();

	Renderer::setMatrix(trans);

	if (mTexture && mOpacity > 0)
	{
		if (Settings::getInstance()->getBool("DebugImage"))
		{
			Vector2f targetSizePos =
				(mTargetSize - mSize) * mOrigin * -1;

			Renderer::drawRect(
				targetSizePos.x(),
				targetSizePos.y(),
				mTargetSize.x(),
				mTargetSize.y(),
				0xFF000033,
				0xFF000033);

			Renderer::drawRect(
				0.0f,
				0.0f,
				mSize.x(),
				mSize.y(),
				0x00000033,
				0x00000033);
		}

		if (mTexture->isInitialized())
		{
			fadeIn(mTexture->bind());
			Renderer::drawTriangleStrips(&mVertices[0], 4);
		}
		else
		{
			LOG(LogError) << "Image texture is not initialized!";
			mTexture.reset();
		}
	}

	GuiComponent::renderChildren(trans);
}

void ImageComponent::fadeIn(bool textureLoaded)
{
	if (!mForceLoad)
	{
		if (!textureLoaded)
		{
			if (!mFading)
			{
				mFadeOpacity = 0;
				mFading = true;
				updateColors();
			}
		}
		else if (mFading)
		{
			int opacity =
				mFadeOpacity + 255 / 15;

			if (opacity >= 255)
			{
				mFadeOpacity = 255;
				mFading = false;
			}
			else
			{
				mFadeOpacity =
					(unsigned char)opacity;
			}

			updateColors();
		}
	}
}

bool ImageComponent::hasImage()
{
	return (bool)mTexture;
}

void ImageComponent::applyTheme(
	const std::shared_ptr<ThemeData>& theme,
	const std::string& view,
	const std::string& element,
	unsigned int properties)
{
	using namespace ThemeFlags;

	GuiComponent::applyTheme(
		theme,
		view,
		element,
		(properties ^ SIZE) |
		((properties & (SIZE | POSITION)) ? ORIGIN : 0));

	const ThemeData::ThemeElement* elem =
		theme->getElement(view, element, "image");

	if (!elem)
		return;

	Vector2f scale =
		getParent() ?
		getParent()->getSize() :
		Vector2f(
			(float)Renderer::getScreenWidth(),
			(float)Renderer::getScreenHeight());

	if (properties & ThemeFlags::SIZE)
	{
		if (elem->has("size"))
			setResize(elem->get<Vector2f>("size") * scale);
		else if (elem->has("maxSize"))
			setMaxSize(elem->get<Vector2f>("maxSize") * scale);
		else if (elem->has("minSize"))
			setMinSize(elem->get<Vector2f>("minSize") * scale);
	}

	if (elem->has("default"))
		setDefaultImage(
			elem->get<std::string>("default"));

	if (properties & PATH && elem->has("path"))
	{
		bool tile =
			elem->has("tile") &&
			elem->get<bool>("tile");

		setImage(
			elem->get<std::string>("path"),
			tile);
	}

	if (properties & COLOR)
	{
		if (elem->has("color"))
			setColorShift(
				elem->get<unsigned int>("color"));

		if (elem->has("colorEnd"))
			setColorShiftEnd(
				elem->get<unsigned int>("colorEnd"));

		if (elem->has("gradientType"))
		{
			setColorGradientHorizontal(
				!elem->get<std::string>("gradientType")
					.compare("horizontal"));
		}
	}

	// ES-X: pulso suave de opacidad para imágenes.
	if (elem->has("pulse"))
		mPulseEnabled =
			elem->get<bool>("pulse");

	if (elem->has("pulseOpacityMin"))
	{
		mPulseOpacityMin =
			Math::clamp(
				0.0f,
				elem->get<float>("pulseOpacityMin"),
				1.0f);
	}

	if (elem->has("pulseOpacityMax"))
	{
		mPulseOpacityMax =
			Math::clamp(
				0.0f,
				elem->get<float>("pulseOpacityMax"),
				1.0f);
	}

	if (mPulseOpacityMin > mPulseOpacityMax)
	{
		const float temp =
			mPulseOpacityMin;

		mPulseOpacityMin =
			mPulseOpacityMax;

		mPulseOpacityMax =
			temp;
	}

	if (elem->has("pulseDuration"))
	{
		mPulseDuration =
			elem->get<float>("pulseDuration");

		if (mPulseDuration < 100.0f)
			mPulseDuration = 100.0f;
	}

	mPulseElapsed = 0.0f;

	mPulseFactor =
		mPulseEnabled ?
		mPulseOpacityMin :
		1.0f;

	updateColors();
}

std::vector<HelpPrompt> ImageComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> ret;
	ret.push_back(HelpPrompt("a", "select"));
	return ret;
}
