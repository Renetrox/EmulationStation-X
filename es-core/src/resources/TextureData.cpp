#include "resources/TextureData.h"

#include "math/Misc.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "ImageIO.h"
#include "Settings.h"
#include "Log.h"
#include <nanosvg/nanosvg.h>
#include <nanosvg/nanosvgrast.h>
#include <assert.h>
#include <string.h>

#define DPI 96

// ES-X: ON by default for the OPi-friendly build.
// Set to false here if you want to compare behavior with the classic path.
bool TextureData::OPTIMIZEVRAM = true;

static bool isOptimizeImageVRAMEnabled()
{
	return TextureData::OPTIMIZEVRAM &&
		Settings::getInstance()->getBool("OptimizeImageVRAM");
}

TextureData::TextureData(bool tile) :
	mTile(tile),
	mTextureID(0),
	mDataRGBA(nullptr),
	mScalable(false),
	mWidth(0),
	mHeight(0),
	mSourceWidth(0.0f),
	mSourceHeight(0.0f),
	mReloadable(false),
	mBaseSize(0, 0),
	mPackedSize(0, 0),
	mMaxSize()
{
}

TextureData::~TextureData()
{
	releaseVRAM();
	releaseRAM();
}

void TextureData::initFromPath(const std::string& path)
{
	// Just set the path. It will be loaded later
	mPath = path;
	// Only textures with paths are reloadable
	mReloadable = true;
}

bool TextureData::initSVGFromMemory(const unsigned char* fileData, size_t length)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA)
		return true;

	// nsvgParse excepts a modifiable, null-terminated string
	char* copy = (char*)malloc(length + 1);
	assert(copy != NULL);
	memcpy(copy, fileData, length);
	copy[length] = '\0';

	NSVGimage* svgImage = nsvgParse(copy, "px", DPI);
	free(copy);
	if (!svgImage || (svgImage->width == 0) || (svgImage->height == 0))
	{
		LOG(LogError) << "Error parsing SVG image.";
		nsvgDelete(svgImage);
		return false;
	}

	// We want to rasterise this texture at a specific resolution. If the source size
	// variables are set then use them otherwise set them from the parsed file
	if (mSourceHeight == 0.0f)
		mSourceHeight = svgImage->height;

	mSourceWidth = (mSourceHeight * svgImage->width) / svgImage->height;

	mWidth = (size_t)Math::round(mSourceWidth);
	mHeight = (size_t)Math::round(mSourceHeight);

	mBaseSize = Vector2i((int)mWidth, (int)mHeight);
	mPackedSize = Vector2i::Zero();

	if (isOptimizeImageVRAMEnabled() && !mMaxSize.empty() &&
		(mWidth > (size_t)mMaxSize.x() || mHeight > (size_t)mMaxSize.y()))
	{
		Vector2i sz = ImageIO::adjustPictureSize(
			Vector2i((int)mWidth, (int)mHeight),
			Vector2i((int)mMaxSize.x(), (int)mMaxSize.y()),
			mMaxSize.externalZoom());

		if (sz.x() > 0 && sz.y() > 0)
		{
			mWidth = (size_t)sz.x();
			mHeight = (size_t)sz.y();
			mSourceWidth = (float)mWidth;
			mSourceHeight = (float)mHeight;
			mPackedSize = sz;
		}
	}

	unsigned char* dataRGBA = new unsigned char[mWidth * mHeight * 4];

	NSVGrasterizer* rast = nsvgCreateRasterizer();
	float scale = Math::min(mHeight / svgImage->height, mWidth / svgImage->width);
	nsvgRasterize(rast, svgImage, 0, 0, scale, dataRGBA, (int)mWidth, (int)mHeight, (int)mWidth * 4);
	nsvgDeleteRasterizer(rast);
	nsvgDelete(svgImage);

	ImageIO::flipPixelsVert(dataRGBA, mWidth, mHeight);

	mDataRGBA = dataRGBA;

	return true;
}

void TextureData::setMaxSize(MaxSizeInfo maxSize)
{
	if (maxSize.empty())
		return;

	if (mMaxSize.empty())
	{
		mMaxSize = maxSize;
		return;
	}

	Vector2i baseSize = mBaseSize;
	if (baseSize == Vector2i::Zero())
		baseSize = Vector2i((int)mSourceWidth, (int)mSourceHeight);

	if (baseSize == Vector2i::Zero())
	{
		// Not loaded yet. Keep the larger raw box.
		if (maxSize.x() > mMaxSize.x() || maxSize.y() > mMaxSize.y())
			mMaxSize = maxSize;
		return;
	}

	Vector2i currentRequired = ImageIO::adjustPictureSize(
		baseSize,
		Vector2i((int)mMaxSize.x(), (int)mMaxSize.y()),
		mMaxSize.externalZoom());

	Vector2i newRequired = ImageIO::adjustPictureSize(
		baseSize,
		Vector2i((int)maxSize.x(), (int)maxSize.y()),
		maxSize.externalZoom());

	if (newRequired.x() > currentRequired.x() ||
		newRequired.y() > currentRequired.y())
	{
		mMaxSize = maxSize;
	}
}

bool TextureData::isRequiredTextureSizeOk()
{
	if (!isOptimizeImageVRAMEnabled())
		return true;

	if (mPackedSize == Vector2i::Zero())
		return true;

	if (mBaseSize == Vector2i::Zero())
		return true;

	if (mMaxSize.empty())
		return true;

	Vector2i required = ImageIO::adjustPictureSize(
		mBaseSize,
		Vector2i((int)mMaxSize.x(), (int)mMaxSize.y()),
		mMaxSize.externalZoom());

	if (required.x() <= mPackedSize.x() &&
		required.y() <= mPackedSize.y())
	{
		return true;
	}

	if (mBaseSize.x() <= mPackedSize.x() &&
		mBaseSize.y() <= mPackedSize.y())
	{
		return true;
	}

	return false;
}

bool TextureData::initImageFromMemory(const unsigned char* fileData, size_t length)
{
	size_t width, height;

	// If already initialised then don't read again
	{
		std::unique_lock<std::mutex> lock(mMutex);
		if (mDataRGBA)
			return true;
	}

	int maxWidth = 0;
	int maxHeight = 0;
	bool externalZoom = false;

	if (isOptimizeImageVRAMEnabled())
	{
		if (!mMaxSize.empty())
		{
			maxWidth = (int)mMaxSize.x();
			maxHeight = (int)mMaxSize.y();
			externalZoom = mMaxSize.externalZoom();
		}
		else
		{
			// General safety net: never decode non-SVG images larger than the screen
			// unless a caller asks for a specific larger size.
			maxWidth = Renderer::getScreenWidth();
			maxHeight = Renderer::getScreenHeight();
		}

		if (maxWidth > Renderer::getScreenWidth())
			maxWidth = Renderer::getScreenWidth();

		if (maxHeight > Renderer::getScreenHeight())
			maxHeight = Renderer::getScreenHeight();
	}

	std::vector<unsigned char> imageRGBA = ImageIO::loadFromMemoryRGBA32Ex(
		(const unsigned char*)fileData,
		length,
		width,
		height,
		maxWidth,
		maxHeight,
		externalZoom,
		mBaseSize,
		mPackedSize);

	if (imageRGBA.size() == 0)
	{
		LOG(LogError) << "Could not initialize texture from memory, invalid data!  (file path: " << mPath << ", data ptr: " << (size_t)fileData << ", reported size: " << length << ")";
		return false;
	}

	mSourceWidth = (float)width;
	mSourceHeight = (float)height;
	mScalable = false;

	return initFromRGBA(imageRGBA.data(), width, height);
}

bool TextureData::initFromRGBA(const unsigned char* dataRGBA, size_t width, size_t height)
{
	// If already initialised then don't read again
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA)
		return true;

	// Take a copy
	mDataRGBA = new unsigned char[width * height * 4];
	memcpy(mDataRGBA, dataRGBA, width * height * 4);
	mWidth = width;
	mHeight = height;
	return true;
}

bool TextureData::load()
{
	bool retval = false;

	// Need to load. See if there is a file
	if (!mPath.empty())
	{
		std::shared_ptr<ResourceManager>& rm = ResourceManager::getInstance();
		const ResourceData& data = rm->getFileData(mPath);
		// is it an SVG?
		if (mPath.substr(mPath.size() - 4, std::string::npos) == ".svg")
		{
			mScalable = true;
			retval = initSVGFromMemory((const unsigned char*)data.ptr.get(), data.length);
		}
		else
			retval = initImageFromMemory((const unsigned char*)data.ptr.get(), data.length);
	}
	return retval;
}

bool TextureData::isLoaded()
{
	std::unique_lock<std::mutex> lock(mMutex);
	if (mDataRGBA || (mTextureID != 0))
		return true;
	return false;
}

bool TextureData::uploadAndBind()
{
	// See if it's already been uploaded
	std::unique_lock<std::mutex> lock(mMutex);
	if (mTextureID != 0)
	{
		Renderer::bindTexture(mTextureID);
	}
	else
	{
		// Load it if necessary
		if (!mDataRGBA)
		{
			return false;
		}
		// Make sure we're ready to upload
		if ((mWidth == 0) || (mHeight == 0) || (mDataRGBA == nullptr))
			return false;

		// Upload texture
		mTextureID = Renderer::createTexture(Renderer::Texture::RGBA, true, mTile, (int)mWidth, (int)mHeight, mDataRGBA);
	}
	return true;
}

void TextureData::releaseVRAM()
{
	std::unique_lock<std::mutex> lock(mMutex);
	if (mTextureID != 0)
	{
		Renderer::destroyTexture(mTextureID);
		mTextureID = 0;
	}
}

void TextureData::releaseRAM()
{
	std::unique_lock<std::mutex> lock(mMutex);
	delete[] mDataRGBA;
	mDataRGBA = 0;
}

size_t TextureData::width()
{
	if (mWidth == 0)
		load();
	return mWidth;
}

size_t TextureData::height()
{
	if (mHeight == 0)
		load();
	return mHeight;
}

float TextureData::sourceWidth()
{
	if (mSourceWidth == 0)
		load();
	return mSourceWidth;
}

float TextureData::sourceHeight()
{
	if (mSourceHeight == 0)
		load();
	return mSourceHeight;
}

void TextureData::setSourceSize(float width, float height)
{
	if (mScalable)
	{
		if ((mSourceWidth != width) || (mSourceHeight != height))
		{
			mSourceWidth = width;
			mSourceHeight = height;
			releaseVRAM();
			releaseRAM();
		}
	}
}

size_t TextureData::getVRAMUsage()
{
	if ((mTextureID != 0) || (mDataRGBA != nullptr))
		return mWidth * mHeight * 4;
	else
		return 0;
}
