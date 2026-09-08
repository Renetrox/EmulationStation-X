#include "resources/TextureData.h"

#include "math/Misc.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "ImageIO.h"
#include "Settings.h"
#include "Log.h"
#include <plutosvg.h>
#include <limits.h>
#include <string.h>

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
	mPendingRasterization(false),
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
	std::unique_lock<std::mutex> lock(mMutex);

	// If this SVG has already been rasterized at the requested size, keep it.
	if (mDataRGBA && !mPendingRasterization)
		return true;

	if (!fileData || length == 0 || length > (size_t)INT_MAX)
	{
		LOG(LogError) << "Error parsing SVG image: invalid input buffer.";
		return false;
	}

	// PlutoSVG keeps references into the input buffer for the lifetime of the
	// document. ResourceData remains alive for this call, and the document is
	// always destroyed before returning from this function.
	plutosvg_document_t* svgImage = plutosvg_document_load_from_data(
		reinterpret_cast<const char*>(fileData),
		(int)length,
		-1.0f,
		-1.0f,
		nullptr,
		nullptr);

	if (!svgImage)
	{
		LOG(LogError) << "Error parsing SVG image with PlutoSVG.";
		return false;
	}

	const float svgWidth = plutosvg_document_get_width(svgImage);
	const float svgHeight = plutosvg_document_get_height(svgImage);

	if (svgWidth <= 0.0f || svgHeight <= 0.0f)
	{
		LOG(LogError) << "Error parsing SVG image: invalid intrinsic size.";
		plutosvg_document_destroy(svgImage);
		return false;
	}

	// ES-DE style first pass: if the component has not told us the final raster
	// size yet, expose a small aspect-correct temporary size but do not rasterize.
	// ImageComponent can use this to calculate its final visual dimensions first.
	if (mSourceWidth == 0.0f && mSourceHeight == 0.0f)
	{
		mSourceWidth = 64.0f;
		mSourceHeight = 64.0f * (svgHeight / svgWidth);
		mWidth = (size_t)Math::round(mSourceWidth);
		mHeight = (size_t)Math::round(mSourceHeight);
		mBaseSize = Vector2i((int)Math::round(svgWidth), (int)Math::round(svgHeight));
		mPackedSize = Vector2i::Zero();
		mPendingRasterization = true;
		plutosvg_document_destroy(svgImage);
		return true;
	}

	// A zero axis means "preserve aspect ratio".
	if (mSourceWidth == 0.0f)
		mSourceWidth = (mSourceHeight / svgHeight) * svgWidth;
	else if (mSourceHeight == 0.0f)
		mSourceHeight = (mSourceWidth / svgWidth) * svgHeight;

	mWidth = (size_t)Math::round(mSourceWidth);
	mHeight = (size_t)Math::round(mSourceHeight);

	if (mWidth == 0 || mHeight == 0 || mWidth > (size_t)INT_MAX || mHeight > (size_t)INT_MAX)
	{
		LOG(LogError) << "Error rasterizing SVG image at invalid size.";
		plutosvg_document_destroy(svgImage);
		return false;
	}

	mBaseSize = Vector2i((int)Math::round(svgWidth), (int)Math::round(svgHeight));
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

	plutovg_surface_t* surface = plutosvg_document_render_to_surface(
		svgImage,
		nullptr,
		(int)mWidth,
		(int)mHeight,
		nullptr,
		nullptr,
		nullptr);

	if (!surface)
	{
		LOG(LogError) << "Error rasterizing SVG image with PlutoSVG.";
		plutosvg_document_destroy(svgImage);
		return false;
	}

	const int surfaceWidth = plutovg_surface_get_width(surface);
	const int surfaceHeight = plutovg_surface_get_height(surface);
	const int surfaceStride = plutovg_surface_get_stride(surface);
	unsigned char* surfaceData = plutovg_surface_get_data(surface);

	if (!surfaceData || surfaceWidth != (int)mWidth || surfaceHeight != (int)mHeight ||
		surfaceStride < surfaceWidth * 4)
	{
		LOG(LogError) << "Error rasterizing SVG image: unexpected PlutoVG surface layout.";
		plutovg_surface_destroy(surface);
		plutosvg_document_destroy(svgImage);
		return false;
	}

	// PlutoVG stores premultiplied native-endian ARGB. ES-X's renderer expects
	// non-premultiplied RGBA, so convert in place before copying to our compact
	// texture buffer. The conversion API explicitly supports overlapping buffers.
	plutovg_convert_argb_to_rgba(
		surfaceData,
		surfaceData,
		surfaceWidth,
		surfaceHeight,
		surfaceStride);

	unsigned char* dataRGBA = new unsigned char[mWidth * mHeight * 4];
	const size_t rowBytes = mWidth * 4;
	for (size_t y = 0; y < mHeight; ++y)
	{
		memcpy(
			dataRGBA + y * rowBytes,
			surfaceData + y * (size_t)surfaceStride,
			rowBytes);
	}

	plutovg_surface_destroy(surface);
	plutosvg_document_destroy(svgImage);

	ImageIO::flipPixelsVert(dataRGBA, mWidth, mHeight);

	mDataRGBA = dataRGBA;
	mPendingRasterization = false;

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
	mPendingRasterization = false;

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
	mPendingRasterization = false;
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
		if (mPath.size() >= 4 && mPath.substr(mPath.size() - 4, std::string::npos) == ".svg")
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
	if (mDataRGBA || (mTextureID != 0) || mPendingRasterization)
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
		// A first-pass SVG intentionally has no raster data yet.
		if (mPendingRasterization)
			return false;

		// Load it if necessary
		if (!mDataRGBA)
			return false;

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
		// A pending SVG must be promoted to a real raster even if the requested
		// size happens to match the temporary first-pass dimensions.
		if (mPendingRasterization || (mSourceWidth != width) || (mSourceHeight != height))
		{
			mSourceWidth = width;
			mSourceHeight = height;

			// Mark it as needing real data so TextureDataManager will queue/reload it.
			mPendingRasterization = false;
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
