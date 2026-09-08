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
#include <string>
#include <vector>

// ES-X: ON by default for the OPi-friendly build.
// Set to false here if you want to compare behavior with the classic path.
bool TextureData::OPTIMIZEVRAM = true;

static bool isOptimizeImageVRAMEnabled()
{
	return TextureData::OPTIMIZEVRAM &&
		Settings::getInstance()->getBool("OptimizeImageVRAM");
}

namespace
{
	struct SvgClassRule
	{
		std::string className;
		std::string declarations;
	};

	static bool isSvgCssSpace(char c)
	{
		return c == ' ' || c == '\t' || c == '\r' || c == '\n';
	}

	static void trimSvgCss(std::string& value)
	{
		size_t start = 0;
		while (start < value.size() && isSvgCssSpace(value[start]))
			++start;

		size_t end = value.size();
		while (end > start && isSvgCssSpace(value[end - 1]))
			--end;

		value = value.substr(start, end - start);
	}

	static void replaceAll(std::string& value, const std::string& from, const std::string& to)
	{
		if (from.empty())
			return;

		size_t pos = 0;
		while ((pos = value.find(from, pos)) != std::string::npos)
		{
			value.replace(pos, from.size(), to);
			pos += to.size();
		}
	}

	static std::string normalizeSvgCss(std::string css)
	{
		// Illustrator commonly writes indentation in <style> as XML numeric
		// character references. Decode the whitespace references we care about
		// before looking for simple class selectors.
		replaceAll(css, "&#x9;", " ");
		replaceAll(css, "&#X9;", " ");
		replaceAll(css, "&#9;", " ");
		replaceAll(css, "&#xA;", " ");
		replaceAll(css, "&#xa;", " ");
		replaceAll(css, "&#10;", " ");
		replaceAll(css, "&#xD;", " ");
		replaceAll(css, "&#xd;", " ");
		replaceAll(css, "&#13;", " ");

		// Strip CSS comments so they cannot become part of a selector.
		size_t pos = 0;
		while ((pos = css.find("/*", pos)) != std::string::npos)
		{
			const size_t end = css.find("*/", pos + 2);
			if (end == std::string::npos)
			{
				css.erase(pos);
				break;
			}
			css.erase(pos, end + 2 - pos);
		}

		return css;
	}

	static bool isSimpleClassSelector(std::string selector, std::string& className)
	{
		trimSvgCss(selector);
		if (selector.size() < 2 || selector[0] != '.')
			return false;

		for (size_t i = 1; i < selector.size(); ++i)
		{
			const char c = selector[i];
			const bool valid =
				(c >= 'a' && c <= 'z') ||
				(c >= 'A' && c <= 'Z') ||
				(c >= '0' && c <= '9') ||
				c == '_' || c == '-';

			if (!valid)
				return false;
		}

		className = selector.substr(1);
		return true;
	}

	static void collectSimpleSvgClassRules(const std::string& cssText, std::vector<SvgClassRule>& rules)
	{
		const std::string css = normalizeSvgCss(cssText);
		size_t pos = 0;

		while (pos < css.size())
		{
			const size_t open = css.find('{', pos);
			if (open == std::string::npos)
				break;

			const size_t close = css.find('}', open + 1);
			if (close == std::string::npos)
				break;

			std::string selectors = css.substr(pos, open - pos);
			std::string declarations = css.substr(open + 1, close - open - 1);
			trimSvgCss(declarations);

			if (!declarations.empty())
			{
				size_t selectorPos = 0;
				while (selectorPos <= selectors.size())
				{
					const size_t comma = selectors.find(',', selectorPos);
					const size_t selectorEnd = comma == std::string::npos ? selectors.size() : comma;
					std::string selector = selectors.substr(selectorPos, selectorEnd - selectorPos);
					std::string className;

					if (isSimpleClassSelector(selector, className))
					{
						SvgClassRule rule;
						rule.className = className;
						rule.declarations = declarations;
						rules.push_back(rule);
					}

					if (comma == std::string::npos)
						break;
					selectorPos = comma + 1;
				}
			}

			pos = close + 1;
		}
	}

	static bool bufferContains(const unsigned char* data, size_t length, const char* needle)
	{
		const size_t needleLength = strlen(needle);
		if (!data || needleLength == 0 || needleLength > length)
			return false;

		for (size_t i = 0; i + needleLength <= length; ++i)
		{
			if (memcmp(data + i, needle, needleLength) == 0)
				return true;
		}
		return false;
	}

	static bool classListContains(const std::string& classList, const std::string& className)
	{
		size_t pos = 0;
		while (pos < classList.size())
		{
			while (pos < classList.size() && isSvgCssSpace(classList[pos]))
				++pos;

			const size_t start = pos;
			while (pos < classList.size() && !isSvgCssSpace(classList[pos]))
				++pos;

			if (pos > start && classList.compare(start, pos - start, className) == 0)
				return true;
		}
		return false;
	}

	static bool findQuotedAttribute(const std::string& tag, const char* attribute,
		size_t& valueStart, size_t& valueEnd, char& quote)
	{
		const size_t attributeLength = strlen(attribute);
		size_t pos = 0;

		while ((pos = tag.find(attribute, pos)) != std::string::npos)
		{
			const bool validStart = pos > 0 && (isSvgCssSpace(tag[pos - 1]) || tag[pos - 1] == '<');
			if (!validStart)
			{
				pos += attributeLength;
				continue;
			}

			size_t cursor = pos + attributeLength;
			while (cursor < tag.size() && isSvgCssSpace(tag[cursor]))
				++cursor;

			if (cursor >= tag.size() || tag[cursor] != '=')
			{
				pos += attributeLength;
				continue;
			}

			++cursor;
			while (cursor < tag.size() && isSvgCssSpace(tag[cursor]))
				++cursor;

			if (cursor >= tag.size() || (tag[cursor] != '"' && tag[cursor] != '\''))
			{
				pos += attributeLength;
				continue;
			}

			quote = tag[cursor];
			valueStart = cursor + 1;
			valueEnd = tag.find(quote, valueStart);
			return valueEnd != std::string::npos;
		}

		return false;
	}

	static size_t findSvgTagEnd(const std::string& svg, size_t start)
	{
		char quote = 0;
		for (size_t i = start + 1; i < svg.size(); ++i)
		{
			const char c = svg[i];
			if (quote != 0)
			{
				if (c == quote)
					quote = 0;
				continue;
			}

			if (c == '"' || c == '\'')
				quote = c;
			else if (c == '>')
				return i;
		}
		return std::string::npos;
	}

	static std::string escapeSvgStyleForQuote(std::string value, char quote)
	{
		if (quote == '"')
			replaceAll(value, "\"", "&quot;");
		else if (quote == '\'')
			replaceAll(value, "'", "&apos;");
		return value;
	}


	static bool isPlutoSvgStyleProperty(const std::string& name)
	{
		static const char* supported[] =
		{
			"clip-path",
			"clip-rule",
			"color",
			"display",
			"fill",
			"fill-opacity",
			"fill-rule",
			"opacity",
			"stop-color",
			"stop-opacity",
			"stroke",
			"stroke-dasharray",
			"stroke-dashoffset",
			"stroke-linecap",
			"stroke-linejoin",
			"stroke-miterlimit",
			"stroke-opacity",
			"stroke-width",
			"visibility"
		};

		for (size_t i = 0; i < sizeof(supported) / sizeof(supported[0]); ++i)
		{
			if (name == supported[i])
				return true;
		}

		return false;
	}

	static std::string filterPlutoSvgStyle(const std::string& style)
	{
		std::string result;
		size_t pos = 0;

		while (pos <= style.size())
		{
			const size_t semi = style.find(';', pos);
			const size_t end = semi == std::string::npos ? style.size() : semi;

			std::string declaration = style.substr(pos, end - pos);
			const size_t colon = declaration.find(':');

			if (colon != std::string::npos)
			{
				std::string name = declaration.substr(0, colon);
				std::string value = declaration.substr(colon + 1);

				trimSvgCss(name);
				trimSvgCss(value);

				if (!name.empty() && !value.empty() &&
					isPlutoSvgStyleProperty(name))
				{
					if (!result.empty())
						result += ';';

					result += name;
					result += ':';
					result += value;
				}
			}

			if (semi == std::string::npos)
				break;

			pos = semi + 1;
		}

		return result;
	}

	static bool sanitizeInlineSvgStyles(std::string& svg)
	{
		bool changed = false;
		size_t pos = 0;

		while ((pos = svg.find('<', pos)) != std::string::npos)
		{
			if (pos + 1 >= svg.size())
				break;

			const size_t end = findSvgTagEnd(svg, pos);
			if (end == std::string::npos)
				break;

			const char next = svg[pos + 1];
			if (next == '/' || next == '!' || next == '?')
			{
				pos = end + 1;
				continue;
			}

			std::string tag = svg.substr(pos, end - pos + 1);

			size_t styleStart = 0;
			size_t styleEnd = 0;
			char styleQuote = 0;

			if (!findQuotedAttribute(tag, "style",
				styleStart, styleEnd, styleQuote))
			{
				pos = end + 1;
				continue;
			}

			const std::string original =
				tag.substr(styleStart, styleEnd - styleStart);

			std::string filtered = filterPlutoSvgStyle(original);

			if (filtered != original)
			{
				filtered = escapeSvgStyleForQuote(filtered, styleQuote);
				tag.replace(styleStart, styleEnd - styleStart, filtered);

				svg.replace(pos, end - pos + 1, tag);
				changed = true;
			}

			pos += tag.size();
		}

		return changed;
	}

	static std::string inlineSimpleSvgCssClasses(const unsigned char* fileData, size_t length)
	{
		// Most SVGs do not use stylesheet classes. Avoid copying large SVGs
		// (especially base64-image Help icons) unless this compatibility path
		// is actually needed.
		if (!bufferContains(fileData, length, "<style") ||
			!bufferContains(fileData, length, "class="))
		{
			return std::string();
		}

		std::string svg(reinterpret_cast<const char*>(fileData), length);
		std::vector<SvgClassRule> rules;

		// PlutoSVG understands inline style="..." properties but not stylesheet
		// class selectors. Collect simple .class{...} rules and remove <style>
		// blocks only from this temporary in-memory copy.
		size_t stylePos = 0;
		while ((stylePos = svg.find("<style", stylePos)) != std::string::npos)
		{
			const size_t openEnd = svg.find('>', stylePos);
			if (openEnd == std::string::npos)
				break;

			const size_t close = svg.find("</style>", openEnd + 1);
			if (close == std::string::npos)
				break;

			collectSimpleSvgClassRules(svg.substr(openEnd + 1, close - openEnd - 1), rules);
			svg.erase(stylePos, close + 8 - stylePos);
		}

		if (rules.empty())
			return std::string();

		bool changed = false;
		size_t pos = 0;

		while ((pos = svg.find('<', pos)) != std::string::npos)
		{
			if (pos + 1 >= svg.size())
				break;

			const char next = svg[pos + 1];
			if (next == '/' || next == '!' || next == '?')
			{
				const size_t end = findSvgTagEnd(svg, pos);
				if (end == std::string::npos)
					break;
				pos = end + 1;
				continue;
			}

			const size_t end = findSvgTagEnd(svg, pos);
			if (end == std::string::npos)
				break;

			std::string tag = svg.substr(pos, end - pos + 1);
			size_t classStart = 0;
			size_t classEnd = 0;
			char classQuote = 0;

			if (!findQuotedAttribute(tag, "class", classStart, classEnd, classQuote))
			{
				pos = end + 1;
				continue;
			}

			const std::string classList = tag.substr(classStart, classEnd - classStart);
			std::string classDeclarations;

			// Preserve stylesheet source order when more than one class rule matches.
			for (const auto& rule : rules)
			{
				if (!classListContains(classList, rule.className))
					continue;

				if (!classDeclarations.empty() && classDeclarations[classDeclarations.size() - 1] != ';')
					classDeclarations += ';';
				classDeclarations += rule.declarations;
			}

			if (classDeclarations.empty())
			{
				pos = end + 1;
				continue;
			}

			size_t styleStart = 0;
			size_t styleEnd = 0;
			char styleQuote = 0;

			if (findQuotedAttribute(tag, "style", styleStart, styleEnd, styleQuote))
			{
				std::string merged = classDeclarations;
				if (!merged.empty() && merged[merged.size() - 1] != ';')
					merged += ';';
				merged += tag.substr(styleStart, styleEnd - styleStart);
				merged = escapeSvgStyleForQuote(merged, styleQuote);
				tag.replace(styleStart, styleEnd - styleStart, merged);
			}
			else
			{
				const std::string escaped = escapeSvgStyleForQuote(classDeclarations, '"');
				size_t insertPos = tag.size() - 1;
				if (insertPos > 0 && tag[insertPos - 1] == '/')
					--insertPos;
				tag.insert(insertPos, " style=\"" + escaped + "\"");
			}

			svg.replace(pos, end - pos + 1, tag);
			pos += tag.size();
			changed = true;
		}

		return changed ? svg : std::string();
	}
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

	// PlutoSVG compatibility pass:
	// 1. Convert simple stylesheet classes to inline styles.
	// 2. Strip unsupported Inkscape/Illustrator CSS properties from inline
	//    styles while preserving the graphical properties PlutoSVG understands.
	std::string svgCssInlined = inlineSimpleSvgCssClasses(fileData, length);

	if (svgCssInlined.empty() && bufferContains(fileData, length, "style"))
		svgCssInlined.assign(reinterpret_cast<const char*>(fileData), length);

	if (!svgCssInlined.empty())
		sanitizeInlineSvgStyles(svgCssInlined);

	const char* svgData = reinterpret_cast<const char*>(fileData);
	int svgLength = (int)length;

	if (!svgCssInlined.empty())
	{
		if (svgCssInlined.size() > (size_t)INT_MAX)
		{
			LOG(LogError) << "Error parsing SVG image: preprocessed input is too large.";
			return false;
		}

		svgData = svgCssInlined.data();
		svgLength = (int)svgCssInlined.size();
	}

	// PlutoSVG keeps references into the input buffer for the lifetime of the
	// document. Both ResourceData and svgCssInlined remain alive for this call,
	// and the document is always destroyed before returning from this function.
	plutosvg_document_t* svgImage = plutosvg_document_load_from_data(
		svgData,
		svgLength,
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
