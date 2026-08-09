#pragma once
#ifndef ES_APP_COMPONENTS_TEXT_LIST_COMPONENT_H
#define ES_APP_COMPONENTS_TEXT_LIST_COMPONENT_H

#include "components/IList.h"
#include "components/ImageComponent.h"
#include "animations/LambdaAnimation.h"
#include "math/Misc.h"
#include "utils/StringUtil.h"
#include "Log.h"
#include "Settings.h"
#include "Sound.h"
#include "FileData.h"

#include <cassert>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

class TextCache;
class SystemData;

struct TextListData
{
	unsigned int colorId;
	std::shared_ptr<TextCache> textCache;

	// ES-X: imagen cacheada para modo carrusel.
	std::shared_ptr<ImageComponent> carouselImage;
	std::string carouselImagePath;
};

namespace TextListCarouselArt
{
	inline std::string getImagePath(FileData* file, const std::string& imageType)
	{
		if (!file)
			return "";

		return file->getCarouselImagePath(imageType);
	}

	template <typename U>
	inline std::string getImagePath(const U&, const std::string&)
	{
		return "";
	}
}

// A graphical list. Supports multiple colors for rows and scrolling.
template <typename T>
class TextListComponent : public IList<TextListData, T>
{
protected:
	using IList<TextListData, T>::mEntries;
	using IList<TextListData, T>::listUpdate;
	using IList<TextListData, T>::listInput;
	using IList<TextListData, T>::listRenderTitleOverlay;
	using IList<TextListData, T>::getTransform;
	using IList<TextListData, T>::mSize;
	using IList<TextListData, T>::mCursor;
	using IList<TextListData, T>::mViewportTop;
	using IList<TextListData, T>::mEntry;

public:
	using IList<TextListData, T>::size;
	using IList<TextListData, T>::isScrolling;
	using IList<TextListData, T>::stopScrolling;

	static constexpr int REFRESH_LIST_CURSOR_POS = -1;

	TextListComponent(Window* window);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties) override;

	void add(const std::string& name, const T& obj, unsigned int colorId);

	enum Alignment
	{
		ALIGN_LEFT,
		ALIGN_CENTER,
		ALIGN_RIGHT
	};

	enum Orientation
	{
		ORIENTATION_VERTICAL,
		ORIENTATION_HORIZONTAL
	};

	enum CarouselLogoAlignment
	{
		CAROUSEL_ALIGN_LEFT,
		CAROUSEL_ALIGN_CENTER,
		CAROUSEL_ALIGN_RIGHT,
		CAROUSEL_ALIGN_TOP,
		CAROUSEL_ALIGN_BOTTOM
	};

	inline void setAlignment(Alignment align) { mAlignment = align; }
	inline void setOrientation(Orientation orientation) { mOrientation = orientation; }

	inline bool isHorizontalCarouselMode() const
	{
		return mCarouselMode && mOrientation == ORIENTATION_HORIZONTAL;
	}

	inline bool isCarouselMode() const
	{
		return mCarouselMode &&
			(mOrientation == ORIENTATION_HORIZONTAL || mOrientation == ORIENTATION_VERTICAL);
	}

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func)
	{
		mCursorChangedCallback = func;
	}

	inline void setFont(const std::shared_ptr<Font>& font)
	{
		mFont = font;
		for (auto it = mEntries.begin(); it != mEntries.end(); it++)
			it->data.textCache.reset();
	}

	inline void setUppercase(bool uppercase)
	{
		mUppercase = uppercase;
		for (auto it = mEntries.begin(); it != mEntries.end(); it++)
			it->data.textCache.reset();
	}

	inline void setSelectorHeight(float selectorScale) { mSelectorHeight = selectorScale; }
	inline void setSelectorOffsetY(float selectorOffsetY) { mSelectorOffsetY = selectorOffsetY; }
	inline void setSelectorColor(unsigned int color) { mSelectorColor = color; }
	inline void setSelectorColorEnd(unsigned int color) { mSelectorColorEnd = color; }
	inline void setSelectorColorGradientHorizontal(bool horizontal) { mSelectorColorGradientHorizontal = horizontal; }
	inline void setSelectedColor(unsigned int color) { mSelectedColor = color; }
	inline void setColor(unsigned int id, unsigned int color) { mColors[id] = color; }
	inline void setLineSpacing(float lineSpacing) { mLineSpacing = lineSpacing; }

	inline void setSelectorWidth(float selectorWidth) { mSelectorWidth = selectorWidth; }
	inline void setSelectorHorizontalOffset(float selectorHorizontalOffset) { mSelectorHorizontalOffset = selectorHorizontalOffset; }
	inline void setTopAligned(bool topAligned) { mTopAligned = topAligned; }

	inline void setShowRowNumbers(bool showRowNumbers)
	{
		mShowRowNumbers = showRowNumbers;
		for (auto it = mEntries.begin(); it != mEntries.end(); it++)
			it->data.textCache.reset();
	}

protected:
	virtual void onScroll(int /*amt*/) override
	{
		if (mScrollSound.empty())
			return;

		std::shared_ptr<Sound> sound = Sound::get(mScrollSound);

		// Comportamiento tipo ES-DE: durante el desplazamiento acelerado no
		// reiniciar el sonido mientras la reproducción anterior siga activa.
		if (!isScrolling() || !sound->isPlaying())
			sound->play();
	}

	// En modo carrusel usamos el ritmo SLOW de ES-DE (500 ms iniciales,
	// luego 200 ms entre pasos). Las textlists normales conservan QUICK.
	const ScrollTierList& getScrollTierList() const override
	{
		if (isCarouselMode())
			return LIST_SCROLL_STYLE_SLOW;

		return IList<TextListData, T>::getScrollTierList();
	}

	virtual void onCursorChanged(const CursorState& state) override;

private:
	int mMarqueeOffset;
	int mMarqueeOffset2;
	int mMarqueeTime;

	Alignment mAlignment;
	Orientation mOrientation;
	float mHorizontalMargin;

	int viewportTop();

	// ES-X:
	// Aunque conserva el nombre histórico, esta función ahora renderiza carrusel
	// horizontal o vertical según mOrientation.
	void renderHorizontalCarousel(const Transform4x4f& trans);

	std::function<void(CursorState state)> mCursorChangedCallback;

	std::shared_ptr<Font> mFont;
	bool mUppercase;
	float mLineSpacing;
	float mSelectorHeight;
	float mSelectorOffsetY;
	unsigned int mSelectorColor;
	unsigned int mSelectorColorEnd;
	bool mSelectorColorGradientHorizontal;
	unsigned int mSelectedColor;
	std::string mScrollSound;

	static const unsigned int COLOR_ID_COUNT = 2;
	unsigned int mColors[COLOR_ID_COUNT];

	int mViewportHeight;
	int mCursorPrev;

	float mSelectorWidth;
	float mSelectorHorizontalOffset;
	bool mTopAligned;
	bool mShowRowNumbers;

	// ES-X: fila fantasma visual tipo XMB.
	// No altera navegación, cursor ni lista real; solo desplaza el render vertical.
	bool mVirtualGap;
	float mVirtualGapHeight;
	int mVirtualGapOffset;

	// ES-X: modo carrusel opcional para textlist/gamelist.
	bool mCarouselMode;
	bool mCarouselLoop;
	int mCarouselMaxLogoCount;
	float mCarouselLogoScale;
	float mCarouselMinLogoOpacity;
	float mCarouselLogoSpacingX;
	float mCarouselLogoSpacingY;
	float mCarouselScaledLogoSpacing;

	Vector2f mCarouselLogoSize;
	CarouselLogoAlignment mCarouselLogoAlignment;

	// ES-X: posición absoluta opcional del carrusel dentro del textlist.
	// Si está activa, logoOffset sigue funcionando como ajuste fino.
	bool mCarouselLogoPosSet;
	Vector2f mCarouselLogoPos;

	float mCarouselLogoOffsetX;
	float mCarouselLogoOffsetY;
	float mCarouselSelectedLogoOffsetX;
	float mCarouselSelectedLogoOffsetY;

	// Cámara visual del carrusel.
	// mCursor sigue siendo la selección real; esto solo suaviza el render.
	float mCarouselCamOffset;

	// ES-X: tarjetas y texto controlado del carrusel.
	unsigned int mCarouselItemColor;
	unsigned int mCarouselSelectedItemColor;
	bool mCarouselShowText;
	int mCarouselTextMaxLines;

	// ES-X: imagen interna de tarjeta en modo carrusel.
	bool mCarouselImage;
	std::string mCarouselImageType;
	std::string mCarouselImageFit;
	std::string mCarouselFallbackImage;
	Vector2f mCarouselImagePadding;

	// ES-X: caja fija opcional para blindar artes raros del scraper.
	bool mCarouselImageLockSize;
	Vector2f mCarouselImageBoxSize;

	// ES-X: borde/overlay opcional para imagen de tarjeta en modo carrusel.
	// No recorta la imagen; solo dibuja un PNG encima.
	std::string mCarouselImageBorder;
	std::string mCarouselImageBorderSelected;
	float mCarouselImageBorderScale;
	unsigned int mCarouselImageBorderColor;

	ImageComponent mCarouselImageBorderComponent;
	ImageComponent mCarouselImageBorderSelectedComponent;

	ImageComponent mSelectorImage;
};

template <typename T>
TextListComponent<T>::TextListComponent(Window* window) :
	IList<TextListData, T>(window),
	mCarouselImageBorderComponent(window),
	mCarouselImageBorderSelectedComponent(window),
	mSelectorImage(window)
{
	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;
	mMarqueeTime = 0;

	mHorizontalMargin = 0;
	mAlignment = ALIGN_CENTER;
	mOrientation = ORIENTATION_VERTICAL;

	mFont = Font::get(FONT_SIZE_MEDIUM);
	mUppercase = false;
	mLineSpacing = 1.5f;
	mSelectorHeight = mFont->getSize() * mLineSpacing;
	mSelectorOffsetY = 0;
	mSelectorColor = 0x000000FF;
	mSelectorColorEnd = 0x000000FF;
	mSelectorColorGradientHorizontal = true;
	mSelectedColor = 0;
	mColors[0] = 0x0000FFFF;
	mColors[1] = 0x00FF00FF;

	mViewportHeight = 0;
	mCursorPrev = -1;

	mSelectorWidth = 0.0f;
	mSelectorHorizontalOffset = 0.0f;

	mTopAligned = std::is_same<T, SystemData*>::value;
	mShowRowNumbers = false;

	mVirtualGap = false;
	mVirtualGapHeight = 0.0f;
	mVirtualGapOffset = 0;

	mCarouselMode = false;
	mCarouselLoop = false;
	mCarouselMaxLogoCount = 5;
	mCarouselLogoScale = 1.20f;
	mCarouselMinLogoOpacity = 0.45f;
	mCarouselLogoSpacingX = 0.0f;
	mCarouselLogoSpacingY = 0.0f;
	mCarouselScaledLogoSpacing = 0.0f;

	mCarouselLogoSize = Vector2f::Zero();
	mCarouselLogoAlignment = CAROUSEL_ALIGN_CENTER;

	mCarouselLogoPosSet = false;
	mCarouselLogoPos = Vector2f::Zero();

	mCarouselLogoOffsetX = 0.0f;
	mCarouselLogoOffsetY = 0.0f;
	mCarouselSelectedLogoOffsetX = 0.0f;
	mCarouselSelectedLogoOffsetY = 0.0f;

	mCarouselCamOffset = 0.0f;

	mCarouselItemColor = 0x00000088;
	mCarouselSelectedItemColor = 0x101020CC;
	mCarouselShowText = true;
	mCarouselTextMaxLines = 2;

	mCarouselImage = false;
	mCarouselImageType = "auto";
	mCarouselImageFit = "contain";
	mCarouselFallbackImage = "";
	mCarouselImagePadding = Vector2f(0.04f, 0.04f);

	mCarouselImageLockSize = false;
	mCarouselImageBoxSize = Vector2f::Zero();

	mCarouselImageBorder = "";
	mCarouselImageBorderSelected = "";
	mCarouselImageBorderScale = 1.0f;
	mCarouselImageBorderColor = 0xFFFFFFFF;
}

template <typename T>
void TextListComponent<T>::renderHorizontalCarousel(const Transform4x4f& trans)
{
	std::shared_ptr<Font>& font = mFont;

	if (size() == 0)
		return;

	const bool verticalCarousel = (mOrientation == ORIENTATION_VERTICAL);

	int visibleCount = mCarouselMaxLogoCount;
	if (visibleCount < 1)
		visibleCount = 1;

	if (!mCarouselLoop && visibleCount > size())
		visibleCount = size();

	if (visibleCount > 1 && (visibleCount % 2) == 0)
		visibleCount--;

	if (visibleCount < 1)
		visibleCount = 1;

	const int sideCount = visibleCount / 2;

	// ES-X:
	// En carrusel vertical respetamos maxLogoCount de forma estricta.
	// Antes el buffer agregaba ítems extra, por eso al reducir maxLogoCount
	// parecía que no cambiaba la cantidad visible.
	const int buffer = verticalCarousel ? 0 : ((mCarouselLoop && size() > 1) ? 2 : 1);

	const float baseTextHeight = Math::max(font->getHeight(1.0f), (float)font->getSize());

	float itemWidth = 0.0f;
	float itemHeight = 0.0f;

	if (mCarouselLogoSize.x() > 1.0f)
		itemWidth = mCarouselLogoSize.x();
	else if (mSelectorWidth > 0.0f)
		itemWidth = mSelectorWidth;
	else
		itemWidth = mSize.x() / (float)visibleCount;

	if (mCarouselLogoSize.y() > 1.0f)
		itemHeight = mCarouselLogoSize.y();
	else if (mSelectorHeight > 1.0f && mSelectorHeight <= mSize.y())
		itemHeight = mSelectorHeight;
	else
		itemHeight = baseTextHeight * 4.0f;

	if (itemWidth <= 1.0f)
		itemWidth = verticalCarousel ? (mSize.x() * 0.80f) : (mSize.x() / 5.0f);

	if (itemHeight <= 1.0f)
		itemHeight = baseTextHeight * 4.0f;

	float spacing = verticalCarousel ? mCarouselLogoSpacingY : mCarouselLogoSpacingX;

	// ES-X:
	// Si el tema no define logoSpacingX/Y, calculamos el espaciado
	// según el área disponible y maxLogoCount, como hace CarouselComponent.
	// Esto hace que maxLogoCount afecte realmente la distribución visual.
	if (spacing <= 1.0f)
	{
		const float axisSize = verticalCarousel ? mSize.y() : mSize.x();
		const float itemAxisSize = verticalCarousel ? itemHeight : itemWidth;

		if (visibleCount > 0)
		{
			spacing =
				((axisSize - (itemAxisSize * (float)visibleCount)) / (float)visibleCount) +
				itemAxisSize;
		}
	}

	if (spacing <= 1.0f)
		spacing = verticalCarousel ? itemHeight : itemWidth;

	if (spacing <= 1.0f)
		spacing = 1.0f;

	float centerX = (mSize.x() * 0.5f) + mCarouselLogoOffsetX;
	float anchorY = (mSize.y() * 0.5f) + mSelectorOffsetY + mCarouselLogoOffsetY;

	if (!verticalCarousel)
	{
		if (mCarouselLogoAlignment == CAROUSEL_ALIGN_TOP)
			anchorY = mSelectorOffsetY + mCarouselLogoOffsetY;
		else if (mCarouselLogoAlignment == CAROUSEL_ALIGN_BOTTOM)
			anchorY = mSize.y() + mSelectorOffsetY + mCarouselLogoOffsetY;
	}

	// ES-X:
	// logoPos/carouselLogoPos permite ubicar el centro/ancla del carrusel
	// de forma absoluta dentro del área del textlist.
	// logoOffsetX/Y sigue funcionando como ajuste fino.
	if (mCarouselLogoPosSet)
	{
		centerX = mCarouselLogoPos.x() + mCarouselLogoOffsetX;
		anchorY = mCarouselLogoPos.y() + mCarouselLogoOffsetY;
	}

	Vector3f dim(mSize.x(), mSize.y(), 0);
	dim = trans * dim - trans.translation();

	Renderer::pushClipRect(
		Vector2i((int)trans.translation().x(), (int)trans.translation().y()),
		Vector2i((int)dim.x(), (int)dim.y()));

	int visualCenter = (int)std::round(mCarouselCamOffset);

	if (!mCarouselLoop)
	{
		if (visualCenter < 0)
			visualCenter = 0;
		if (visualCenter >= size())
			visualCenter = size() - 1;
	}

	int start = visualCenter - sideCount - buffer;
	int end = visualCenter + sideCount + buffer;

	if (size() == 1)
	{
		start = 0;
		end = 0;
	}

	auto getRealIndex = [&](int virtualIndex) -> int
	{
		if (size() <= 0)
			return -1;

		if (mCarouselLoop && size() > 1)
		{
			int index = virtualIndex;

			while (index < 0)
				index += size();

			while (index >= size())
				index -= size();

			return index;
		}

		if (virtualIndex < 0 || virtualIndex >= size())
			return -1;

		return virtualIndex;
	};

	auto applyOpacity = [](unsigned int color, float opacity01) -> unsigned int
	{
		if (opacity01 < 0.0f)
			opacity01 = 0.0f;
		if (opacity01 > 1.0f)
			opacity01 = 1.0f;

		unsigned int baseAlpha = color & 0x000000FF;
		unsigned int alpha = (unsigned int)((float)baseAlpha * opacity01);

		if (alpha > 255)
			alpha = 255;

		return (color & 0xFFFFFF00) | alpha;
	};

	auto truncateToWidth = [&](std::string text, float maxWidth) -> std::string
	{
		text = Utils::String::trim(text);

		if (text.empty())
			return "";

		if (font->sizeText(text).x() <= maxWidth)
			return text;

		const std::string dots = "...";

		while (text.size() > 1 && font->sizeText(text + dots).x() > maxWidth)
			text.pop_back();

		return text + dots;
	};

	auto makeLines = [&](const std::string& original, float maxWidth, int maxLines) -> std::vector<std::string>
	{
		std::vector<std::string> lines;

		if (maxLines < 1)
			maxLines = 1;
		if (maxLines > 3)
			maxLines = 3;

		std::string text = Utils::String::trim(original);

		if (text.empty())
		{
			lines.push_back("");
			return lines;
		}

		if (maxLines == 1)
		{
			lines.push_back(truncateToWidth(text, maxWidth));
			return lines;
		}

		if (font->sizeText(text).x() <= maxWidth)
		{
			lines.push_back(text);
			return lines;
		}

		std::vector<std::string> words = Utils::String::delimitedStringToVector(text, " ");
		std::string current;

		for (size_t i = 0; i < words.size(); i++)
		{
			std::string candidate = current.empty() ? words[i] : current + " " + words[i];

			if (font->sizeText(candidate).x() <= maxWidth || current.empty())
			{
				current = candidate;
			}
			else
			{
				lines.push_back(truncateToWidth(current, maxWidth));
				current = words[i];

				if ((int)lines.size() >= maxLines - 1)
				{
					std::string rest = current;
					for (size_t j = i + 1; j < words.size(); j++)
						rest += " " + words[j];

					lines.push_back(truncateToWidth(rest, maxWidth));

					if ((int)lines.size() > maxLines)
						lines.resize((size_t)maxLines);

					return lines;
				}
			}
		}

		if (!current.empty())
			lines.push_back(truncateToWidth(current, maxWidth));

		if (lines.empty())
			lines.push_back(truncateToWidth(text, maxWidth));

		if ((int)lines.size() > maxLines)
			lines.resize((size_t)maxLines);

		return lines;
	};

	int activeVirtualPos = visualCenter;

	if (getRealIndex(activeVirtualPos) < 0)
		activeVirtualPos = mCursor;

	auto renderEntry = [&](int virtualPos, bool /*active*/)
	{
		int realIndex = getRealIndex(virtualPos);
		if (realIndex < 0)
			return;

		typename IList<TextListData, T>::Entry& entry = mEntries.at((unsigned int)realIndex);

		float distance = (float)virtualPos - mCarouselCamOffset;
		float absDistance = distance < 0.0f ? -distance : distance;

		const float influenceRange = 1.0f;
		float influence = Math::max(0.0f, 1.0f - (absDistance / influenceRange));

		float scale = 1.0f + ((mCarouselLogoScale - 1.0f) * influence);
		if (scale < 0.1f)
			scale = 1.0f;

		float minOpacity = mCarouselMinLogoOpacity;
		if (minOpacity < 0.0f)
			minOpacity = 0.0f;
		if (minOpacity > 1.0f)
			minOpacity = 1.0f;

		float opacity01 = minOpacity + ((1.0f - minOpacity) * influence);

		float itemAnchorX = centerX;
		float itemAnchorY = anchorY;

		if (verticalCarousel)
			itemAnchorY = anchorY + (distance * spacing);
		else
			itemAnchorX = centerX + (distance * spacing);

		const float selectedScale = Math::max(1.0f, mCarouselLogoScale);

		if (mCarouselScaledLogoSpacing != 0.0f && absDistance > 0.001f)
		{
			const float pushInfluence = Math::min(absDistance, 1.0f);

			// ES-X:
			// Misma idea que CarouselComponent:
			// calcula cuánto espacio extra ocupa el ítem seleccionado
			// y empuja los laterales proporcionalmente.
			const float logoDiff =
				((spacing * selectedScale) - spacing) *
				0.5f *
				mCarouselScaledLogoSpacing *
				pushInfluence;

			if (verticalCarousel)
			{
				if (distance < 0.0f)
					itemAnchorY -= logoDiff;
				else
					itemAnchorY += logoDiff;
			}
			else
			{
				if (distance < 0.0f)
					itemAnchorX -= logoDiff;
				else
					itemAnchorX += logoDiff;
			}
		}

		if (influence > 0.001f)
		{
			itemAnchorX += mCarouselSelectedLogoOffsetX * influence;
			itemAnchorY += mCarouselSelectedLogoOffsetY * influence;
		}

		// ES-X:
		// Modelo tipo carrusel real: la caja base ya es el tamaño máximo
		// seleccionado y los laterales se achican con una escala normalizada.
		// Así el centro no "nace chico para luego agrandarse"; nace en su
		// tamaño correcto, como hace CarouselComponent.
		const float outerW = itemWidth * selectedScale;
		const float outerH = itemHeight * selectedScale;
		float itemRenderScale = scale / selectedScale;

		if (itemRenderScale < 0.1f)
			itemRenderScale = 0.1f;

		const float scaledW = outerW * itemRenderScale;
		const float scaledH = outerH * itemRenderScale;

		// ES-X:
		// drawX/drawY se usan para imagen/texto escalados.
		// cardX/cardY se usan para la tarjeta/fondo fijo.
		float drawX = itemAnchorX - (scaledW * 0.5f);
		float drawY = itemAnchorY - (scaledH * 0.5f);

		float cardX = itemAnchorX - (itemWidth * 0.5f);
		float cardY = itemAnchorY - (itemHeight * 0.5f);

		if (verticalCarousel)
		{
			if (mCarouselLogoAlignment == CAROUSEL_ALIGN_LEFT)
			{
				drawX = itemAnchorX;
				cardX = itemAnchorX;
			}
			else if (mCarouselLogoAlignment == CAROUSEL_ALIGN_RIGHT)
			{
				drawX = itemAnchorX - scaledW;
				cardX = itemAnchorX - itemWidth;
			}
			else
			{
				drawX = itemAnchorX - (scaledW * 0.5f);
				cardX = itemAnchorX - (itemWidth * 0.5f);
			}

			drawY = itemAnchorY - (scaledH * 0.5f);
			cardY = itemAnchorY - (itemHeight * 0.5f);
		}
		else
		{
			if (mCarouselLogoAlignment == CAROUSEL_ALIGN_TOP)
			{
				drawY = itemAnchorY;
				cardY = itemAnchorY;
			}
			else if (mCarouselLogoAlignment == CAROUSEL_ALIGN_BOTTOM)
			{
				drawY = itemAnchorY - scaledH;
				cardY = itemAnchorY - itemHeight;
			}
		}

		const bool visuallyCentered = (absDistance < 0.5f);

		unsigned int bgColor = visuallyCentered ? mCarouselSelectedItemColor : mCarouselItemColor;
		bgColor = applyOpacity(bgColor, opacity01);

		Renderer::setMatrix(trans);

		// ES-X:
		// La tarjeta/fondo del carrusel queda fija.
		// Antes usaba scaledW/scaledH, por eso el borde se achicaba/crecía
		// al pasar por el centro, especialmente en carrusel vertical.
		Renderer::drawRect(
			cardX,
			cardY,
			itemWidth,
			itemHeight,
			bgColor,
			bgColor);

		if (mCarouselShowText)
		{
			unsigned int textColor;
			if (visuallyCentered && mSelectedColor)
				textColor = mSelectedColor;
			else
				textColor = mColors[entry.data.colorId];

			textColor = applyOpacity(textColor, opacity01);

			std::string displayName = mUppercase ? Utils::String::toUpper(entry.name) : entry.name;

			if (mShowRowNumbers)
				displayName = std::to_string(realIndex + 1) + ". " + displayName;

			const float maxTextWidth = itemWidth * 0.86f;
			std::vector<std::string> lines = makeLines(displayName, maxTextWidth, mCarouselTextMaxLines);

			const float lineGap = baseTextHeight * 0.15f;
			const float totalTextHeight =
				(lines.size() * baseTextHeight) +
				(lines.size() > 1 ? ((lines.size() - 1) * lineGap) : 0.0f);

			float y = -(totalTextHeight * 0.5f);

			if (!verticalCarousel)
			{
				if (mCarouselLogoAlignment == CAROUSEL_ALIGN_TOP)
					y = -(itemHeight * 0.5f) + (itemHeight * 0.08f);
				else if (mCarouselLogoAlignment == CAROUSEL_ALIGN_BOTTOM)
					y = (itemHeight * 0.5f) - totalTextHeight - (itemHeight * 0.08f);
			}

			for (size_t line = 0; line < lines.size(); line++)
			{
				TextCache* cache = font->buildTextCache(lines[line], 0, 0, 0x000000FF);
				cache->setColor(textColor);

				float textX = -(cache->metrics.size.x() * 0.5f);

				if (mCarouselLogoAlignment == CAROUSEL_ALIGN_LEFT || mAlignment == ALIGN_LEFT)
					textX = -(itemWidth * 0.5f) + (itemWidth * 0.06f);
				else if (mCarouselLogoAlignment == CAROUSEL_ALIGN_RIGHT || mAlignment == ALIGN_RIGHT)
					textX = (itemWidth * 0.5f) - cache->metrics.size.x() - (itemWidth * 0.06f);

				Transform4x4f drawTrans = trans;

				drawTrans.translate(Vector3f(
					drawX + (scaledW * 0.5f),
					drawY + (scaledH * 0.5f),
					0.0f));

				drawTrans.scale(Vector3f(itemRenderScale, itemRenderScale, 1.0f));
				drawTrans.translate(Vector3f(textX, y, 0.0f));

				Renderer::setMatrix(drawTrans);
				font->renderTextCache(cache);
				delete cache;

				y += baseTextHeight + lineGap;
			}
		}

		std::string imagePath;
		bool hasRealImage = false;
		bool hasFallbackImage = false;

		if (mCarouselImage)
		{
			imagePath = TextListCarouselArt::getImagePath(entry.object, mCarouselImageType);

			if (!imagePath.empty())
			{
				hasRealImage = true;
			}
			else if (!mCarouselFallbackImage.empty())
			{
				imagePath = mCarouselFallbackImage;
				hasFallbackImage = true;
			}
		}

		const bool hasAnyCarouselImage = hasRealImage || hasFallbackImage;

		if (mCarouselImage && hasAnyCarouselImage && !imagePath.empty())
		{
			if (!entry.data.carouselImage)
				entry.data.carouselImage = std::shared_ptr<ImageComponent>(new ImageComponent(this->mWindow, false, true));

			if (entry.data.carouselImagePath != imagePath)
			{
				entry.data.carouselImagePath = imagePath;
				entry.data.carouselImage->setImage(imagePath);
			}

			// ES-X:
			// Modelo tipo GridTile/md_thumbnail: primero se calcula la caja
			// visual actual del ítem y luego la imagen entra dentro de esa caja.
			// El padding es solo margen interno; no participa como "seguro"
			// contra el escalado del centro.
			const float currentCardW = Math::max(1.0f, scaledW);
			const float currentCardH = Math::max(1.0f, scaledH);

			float imagePadX = currentCardW * mCarouselImagePadding.x();
			float imagePadY = currentCardH * mCarouselImagePadding.y();

			float visualImageW = Math::max(1.0f, currentCardW - (imagePadX * 2.0f));
			float visualImageH = Math::max(1.0f, currentCardH - (imagePadY * 2.0f));

			if (mCarouselImageLockSize && mCarouselImageBoxSize.x() > 1.0f && mCarouselImageBoxSize.y() > 1.0f)
			{
				// El tamaño fijo se considera como caja seleccionada y los laterales
				// se reducen con la misma escala visual del ítem.
				visualImageW = Math::max(1.0f, mCarouselImageBoxSize.x() * itemRenderScale);
				visualImageH = Math::max(1.0f, mCarouselImageBoxSize.y() * itemRenderScale);

				imagePadX = 0.0f;
				imagePadY = 0.0f;
			}

			entry.data.carouselImage->uncrop();

			float imagePosX = std::round(drawX + (scaledW * 0.5f));
			float imagePosY = std::round(drawY + (scaledH * 0.5f));

			if (hasFallbackImage)
			{
				const float fallbackSide = Math::min(visualImageW, visualImageH);
				entry.data.carouselImage->setResize(fallbackSide, fallbackSide);
			}
			else if (mCarouselImageFit == "cover")
			{
				entry.data.carouselImage->setMinSize(visualImageW, visualImageH);
			}
			else if (mCarouselImageFit == "stretch")
			{
				entry.data.carouselImage->setResize(visualImageW, visualImageH);
			}
			else
			{
				entry.data.carouselImage->setMaxSize(visualImageW, visualImageH);
			}

			entry.data.carouselImage->setOrigin(0.5f, 0.5f);
			entry.data.carouselImage->setPosition(imagePosX, imagePosY, 0.0f);
			entry.data.carouselImage->setOpacity((unsigned char)(opacity01 * 255.0f));

			Transform4x4f imageTrans = trans;

			// ES-X:
			// No agregamos un clipRect extra. ImageComponent ya resuelve cover
			// mediante setMinSize()+crop interno; el clip adicional podía
			// comportarse distinto en GLES/Panfrost. Esto imita mejor a un
			// <image> normal del theme, como md_thumbnail.
			entry.data.carouselImage->render(imageTrans);

			// ES-X:
			// Borde/overlay opcional para imagen de tarjeta en modo carrusel.
			// Laterales: tamaño fijo.
			// Centro: tamaño seleccionado estable, adaptado al logo central.
			// El marco sigue el tamaño visual actual del ítem, sin heredar minLogoOpacity.
			ImageComponent* border = nullptr;

			if (visuallyCentered && !mCarouselImageBorderSelected.empty())
				border = &mCarouselImageBorderSelectedComponent;
			else if (!mCarouselImageBorder.empty())
				border = &mCarouselImageBorderComponent;

			if (border)
			{
				float borderScale = mCarouselImageBorderScale;

				if (borderScale <= 0.0f)
					borderScale = 1.0f;

				// ES-X:
				// El borde representa el marco externo del ítem actual,
				// igual que un tile/frame normal. No se calcula desde la
				// imagen interna ni desde un clip.
				float fixedBorderW = Math::max(1.0f, currentCardW * borderScale);
				float fixedBorderH = Math::max(1.0f, currentCardH * borderScale);
				border->setOrigin(0.5f, 0.5f);
				border->setPosition(imagePosX, imagePosY, 0.0f);

				// setResize fuerza el tamaño exacto del PNG.
				border->setResize(fixedBorderW, fixedBorderH);

				// El marco se mantiene sólido, sin heredar minLogoOpacity.
				border->setOpacity(255);

				border->render(imageTrans);
			}
		}
	};

	for (int virtualPos = start; virtualPos <= end; virtualPos++)
	{
		if (virtualPos == activeVirtualPos)
			continue;

		renderEntry(virtualPos, false);
	}

	if (getRealIndex(activeVirtualPos) >= 0)
		renderEntry(activeVirtualPos, true);

	Renderer::popClipRect();

	listRenderTitleOverlay(trans);

	GuiComponent::renderChildren(trans);
}

template <typename T>
void TextListComponent<T>::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();

	std::shared_ptr<Font>& font = mFont;

	if (size() == 0)
		return;

	if (isCarouselMode())
	{
		renderHorizontalCarousel(trans);
		return;
	}

	const float entrySize = Math::max(font->getHeight(1.0f), (float)font->getSize()) * mLineSpacing;

	mViewportHeight = (int)(mSize.y() / entrySize);

	if (mViewportTop == REFRESH_LIST_CURSOR_POS)
	{
		mViewportTop = mCursor - mViewportHeight / 2;
		mCursorPrev = -1;
	}

	if (mCursor != mCursorPrev)
	{
		mViewportTop = (size() > mViewportHeight) ? viewportTop() : 0;
		mCursorPrev = mCursor;
	}

	const bool useVirtualGap =
		mVirtualGap &&
		mOrientation == ORIENTATION_VERTICAL &&
		mVirtualGapHeight > 0.0f;

	unsigned int listCutoff = mViewportTop + mViewportHeight;

	if (useVirtualGap)
		listCutoff++;

	if (listCutoff > size())
		listCutoff = size();

	float y;
	if (mTopAligned)
		y = 0.0f;
	else
		y = (mSize.y() - (mViewportHeight * entrySize)) * 0.5f;

	auto getVirtualGapShift = [&](int index) -> float
	{
		if (!useVirtualGap)
			return 0.0f;

		const int visualIndex = index - mViewportTop;
		const int selectedVisualIndex = mCursor - mViewportTop;
		const int gapVisualIndex = selectedVisualIndex + mVirtualGapOffset;

		if (visualIndex >= gapVisualIndex)
			return mVirtualGapHeight;

		return 0.0f;
	};

	const float selectorWidth = (mSelectorWidth > 0.0f) ? mSelectorWidth : mSize.x();

	const float selectorY =
		y +
		(mCursor - mViewportTop) * entrySize +
		mSelectorOffsetY +
		getVirtualGapShift(mCursor);

	if (mSelectorImage.hasImage())
	{
		mSelectorImage.setSize(selectorWidth, mSelectorHeight);
		mSelectorImage.setPosition(mSelectorHorizontalOffset, selectorY, 0.0f);
		mSelectorImage.render(trans);
	}
	else
	{
		Renderer::setMatrix(trans);
		Renderer::drawRect(
			mSelectorHorizontalOffset,
			selectorY,
			selectorWidth,
			mSelectorHeight,
			mSelectorColor,
			mSelectorColorEnd,
			mSelectorColorGradientHorizontal);
	}

	Vector3f dim(mSize.x(), mSize.y(), 0);
	dim = trans * dim - trans.translation();

	Renderer::pushClipRect(
		Vector2i((int)(trans.translation().x() + mHorizontalMargin), (int)trans.translation().y()),
		Vector2i((int)(dim.x() - mHorizontalMargin * 2), (int)dim.y()));

	for (int i = mViewportTop; i < (int)listCutoff; i++)
	{
		typename IList<TextListData, T>::Entry& entry = mEntries.at((unsigned int)i);

		unsigned int color;
		if (mCursor == i && mSelectedColor)
			color = mSelectedColor;
		else
			color = mColors[entry.data.colorId];

		std::string displayName = mUppercase ? Utils::String::toUpper(entry.name) : entry.name;

		if (mShowRowNumbers)
			displayName = std::to_string(i + 1) + ". " + displayName;

		if (!entry.data.textCache)
			entry.data.textCache = std::unique_ptr<TextCache>(
				font->buildTextCache(displayName, 0, 0, 0x000000FF));

		entry.data.textCache->setColor(color);

		const float drawY = y + getVirtualGapShift(i);

		Vector3f offset(0, drawY, 0);

		switch (mAlignment)
		{
		case ALIGN_LEFT:
			offset[0] = mHorizontalMargin;
			break;
		case ALIGN_CENTER:
			offset[0] = (int)((mSize.x() - entry.data.textCache->metrics.size.x()) / 2);
			if (offset[0] < mHorizontalMargin)
				offset[0] = mHorizontalMargin;
			break;
		case ALIGN_RIGHT:
			offset[0] = (mSize.x() - entry.data.textCache->metrics.size.x());
			offset[0] -= mHorizontalMargin;
			if (offset[0] < mHorizontalMargin)
				offset[0] = mHorizontalMargin;
			break;
		}

		Transform4x4f drawTrans = trans;

		if ((mCursor == i) && (mMarqueeOffset > 0))
			drawTrans.translate(offset - Vector3f((float)mMarqueeOffset, 0, 0));
		else
			drawTrans.translate(offset);

		Renderer::setMatrix(drawTrans);
		font->renderTextCache(entry.data.textCache.get());

		if ((mCursor == i) && (mMarqueeOffset2 < 0))
		{
			drawTrans = trans;
			drawTrans.translate(offset - Vector3f((float)mMarqueeOffset2, 0, 0));
			Renderer::setMatrix(drawTrans);
			font->renderTextCache(entry.data.textCache.get());
		}

		y += entrySize;
	}

	Renderer::popClipRect();

	listRenderTitleOverlay(trans);

	GuiComponent::renderChildren(trans);
}

template <typename T>
int TextListComponent<T>::viewportTop()
{
	int viewportTopMax = size() - mViewportHeight;
	int topNew = mViewportTop;

	if (viewportTopMax < 0)
		viewportTopMax = 0;

	if (mCursorPrev == -1)
		mCursorPrev = mCursor;

	int delta = mCursor - mCursorPrev;

	if (Settings::getInstance()->getBool("UseFullscreenPaging"))
	{
		if (delta <= -mViewportHeight || delta >= mViewportHeight
			|| delta < 0 && mCursor - mViewportTop < mViewportHeight / 2
			|| delta > 0 && mCursor - mViewportTop > mViewportHeight / 2)
		{
			topNew += delta;
		}
	}
	else
	{
		topNew = mCursor - mViewportHeight / 2;
	}

	if (mCursor <= mViewportHeight / 2)
		topNew = 0;
	else if (mCursor >= viewportTopMax + mViewportHeight / 2)
		topNew = viewportTopMax;

	if (topNew < 0)
		topNew = 0;
	else if (topNew > viewportTopMax)
		topNew = viewportTopMax;

	return topNew;
}

template <typename T>
bool TextListComponent<T>::input(InputConfig* config, Input input)
{
	bool isSingleStep = false;

	if (mOrientation == ORIENTATION_HORIZONTAL)
		isSingleStep = config->isMappedLike("right", input) || config->isMappedLike("left", input);
	else
		isSingleStep = config->isMappedLike("down", input) || config->isMappedLike("up", input);

	const bool carouselMode = isCarouselMode();

	bool isPageStep = !carouselMode &&
		(config->isMappedLike("rightshoulder", input) ||
		 config->isMappedLike("leftshoulder", input));

	if (size() > 0 && (isSingleStep || isPageStep))
	{
		if (input.value != 0)
		{
			int delta;
			mCursorPrev = mCursor;

			if (isSingleStep)
			{
				if (mOrientation == ORIENTATION_HORIZONTAL)
					delta = config->isMappedLike("right", input) ? 1 : -1;
				else
					delta = config->isMappedLike("down", input) ? 1 : -1;
			}
			else
			{
				delta = Settings::getInstance()->getBool("UseFullscreenPaging") ? mViewportHeight : 10;
				if (config->isMappedLike("leftshoulder", input))
					delta = -delta;
			}

			listInput(delta);
			return true;
		}
		else
		{
			stopScrolling();
		}
	}

	return GuiComponent::input(config, input);
}

template <typename T>
void TextListComponent<T>::update(int deltaTime)
{
	listUpdate(deltaTime);

	if (!isCarouselMode() && size() > 0)
		mCarouselCamOffset = (float)mCursor;

	if (!isScrolling() && size() > 0)
	{
		mMarqueeOffset = 0;
		mMarqueeOffset2 = 0;

		auto name = mEntries.at((unsigned int)mCursor).name;
		std::string displayName = mUppercase ? Utils::String::toUpper(name) : name;

		if (mShowRowNumbers)
			displayName = std::to_string(mCursor + 1) + ". " + displayName;

		const float textLength = mFont->sizeText(displayName).x();
		const float limit = mSize.x() - mHorizontalMargin * 2;

		if (textLength > limit)
		{
			const float speed = mFont->sizeText("ABCDEFGHIJKLMNOPQRSTUVWXYZ").x() * 0.247f;
			const float delay = 3000;
			const float scrollLength = textLength;
			const float returnLength = speed * 1.5f;
			const float scrollTime = (scrollLength * 1000) / speed;
			const float returnTime = (returnLength * 1000) / speed;
			const int maxTime = (int)(delay + scrollTime + returnTime);

			mMarqueeTime += deltaTime;
			while (mMarqueeTime > maxTime)
				mMarqueeTime -= maxTime;

			mMarqueeOffset = (int)(Math::Scroll::loop(delay, scrollTime + returnTime, (float)mMarqueeTime, scrollLength + returnLength));

			if (mMarqueeOffset > (scrollLength - (limit - returnLength)))
				mMarqueeOffset2 = (int)(mMarqueeOffset - (scrollLength + returnLength));
		}
	}

	GuiComponent::update(deltaTime);
}

template <typename T>
void TextListComponent<T>::add(const std::string& name, const T& obj, unsigned int color)
{
	assert(color < COLOR_ID_COUNT);

	typename IList<TextListData, T>::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.colorId = color;
	static_cast<IList<TextListData, T>*>(this)->add(entry);
}

template <typename T>
void TextListComponent<T>::onCursorChanged(const CursorState& state)
{
	mMarqueeOffset = 0;
	mMarqueeOffset2 = 0;
	mMarqueeTime = 0;

	if (size() > 0)
	{
		if (isCarouselMode())
		{
			float startPos = mCarouselCamOffset;
			float posMax = (float)size();
			float target = (float)mCursor;
			float endPos = target;

			if (startPos < 0.0f || startPos >= posMax)
				startPos = target;

			if (mCarouselLoop && size() > 1)
			{
				float dist = std::fabs(endPos - startPos);

				if (std::fabs(target + posMax - startPos) < dist)
					endPos = target + posMax;

				if (std::fabs(target - posMax - startPos) < dist)
					endPos = target - posMax;
			}

			this->cancelAnimation(0);

			Animation* anim = new LambdaAnimation(
				[this, startPos, endPos, posMax](float t)
				{
					t = 1.0f - (1.0f - t) * (1.0f - t);

					float f = (endPos * t) + (startPos * (1.0f - t));

					if (posMax > 0.0f)
					{
						while (f < 0.0f)
							f += posMax;

						while (f >= posMax)
							f -= posMax;
					}

					mCarouselCamOffset = f;
				},
				400);

			this->setAnimation(anim, 0, nullptr, false, 0);
		}
		else
		{
			this->cancelAnimation(0);
			mCarouselCamOffset = (float)mCursor;
		}
	}

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);
}

template <typename T>
void TextListComponent<T>::applyTheme(const std::shared_ptr<ThemeData>& theme, const std::string& view, const std::string& element, unsigned int properties)
{
	if (Settings::getInstance()->getBool("UseFullscreenPaging"))
	{
		mViewportTop = REFRESH_LIST_CURSOR_POS;
	}

	GuiComponent::applyTheme(theme, view, element, properties);

	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "textlist");
	if (!elem)
		return;

	using namespace ThemeFlags;

	if (properties & COLOR)
	{
		if (elem->has("selectorColor"))
		{
			setSelectorColor(elem->get<unsigned int>("selectorColor"));
			setSelectorColorEnd(elem->get<unsigned int>("selectorColor"));
		}
		if (elem->has("selectorColorEnd"))
			setSelectorColorEnd(elem->get<unsigned int>("selectorColorEnd"));
		if (elem->has("selectorGradientType"))
			setSelectorColorGradientHorizontal(!(elem->get<std::string>("selectorGradientType").compare("horizontal")));
		if (elem->has("selectedColor"))
			setSelectedColor(elem->get<unsigned int>("selectedColor"));
		if (elem->has("primaryColor"))
			setColor(0, elem->get<unsigned int>("primaryColor"));
		if (elem->has("secondaryColor"))
			setColor(1, elem->get<unsigned int>("secondaryColor"));
	}

	setFont(Font::getFromTheme(elem, properties, mFont));
	const float selectorHeight = Math::max(mFont->getHeight(1.0f), (float)mFont->getSize()) * mLineSpacing;
	setSelectorHeight(selectorHeight);

	if (properties & SOUND)
	{
		if (elem->has("scrollSound"))
		{
			mScrollSound = elem->get<std::string>("scrollSound");
		}
		else
		{
			const ThemeData::ThemeElement* sndElem = theme->getElement("all", "scroll", "sound");

			if (sndElem && sndElem->has("path"))
				mScrollSound = sndElem->get<std::string>("path");
		}
	}

	if (properties & ALIGNMENT)
	{
		if (elem->has("alignment"))
		{
			const std::string& str = elem->get<std::string>("alignment");
			if (str == "left")
				setAlignment(ALIGN_LEFT);
			else if (str == "center")
				setAlignment(ALIGN_CENTER);
			else if (str == "right")
				setAlignment(ALIGN_RIGHT);
			else
				LOG(LogError) << "Unknown TextListComponent alignment \"" << str << "\"!";
		}

		if (elem->has("horizontalMargin"))
		{
			mHorizontalMargin = elem->get<float>("horizontalMargin") *
				(this->mParent ? this->mParent->getSize().x() : (float)Renderer::getScreenWidth());
		}
	}

	if (elem->has("orientation"))
	{
		const std::string& str = elem->get<std::string>("orientation");

		if (str == "horizontal")
			setOrientation(ORIENTATION_HORIZONTAL);
		else if (str == "vertical")
			setOrientation(ORIENTATION_VERTICAL);
		else
		{
			LOG(LogWarning) << "Unknown TextListComponent orientation \"" << str << "\"! Using vertical.";
			setOrientation(ORIENTATION_VERTICAL);
		}
	}
	else
	{
		setOrientation(ORIENTATION_VERTICAL);
	}

	mCarouselMode = false;
	if (elem->has("carouselMode"))
		mCarouselMode = elem->get<bool>("carouselMode");

	mCarouselImage = mCarouselMode;
	mCarouselImageType = "auto";
	mCarouselImageFit = "contain";
	mCarouselFallbackImage = "";
	mCarouselImagePadding = Vector2f(0.04f, 0.04f);

	mCarouselImageLockSize = false;
	mCarouselImageBoxSize = Vector2f::Zero();

	if (elem->has("carouselImage"))
		mCarouselImage = elem->get<bool>("carouselImage");

	if (elem->has("carouselImageType"))
	{
		mCarouselImageType = Utils::String::toLower(elem->get<std::string>("carouselImageType"));

		if (mCarouselImageType != "auto" &&
			mCarouselImageType != "image" &&
			mCarouselImageType != "thumbnail" &&
			mCarouselImageType != "marquee" &&
			mCarouselImageType != "cover" &&
			mCarouselImageType != "boxart" &&
			mCarouselImageType != "screenshot" &&
			mCarouselImageType != "wheel" &&
			mCarouselImageType != "texture" &&
			mCarouselImageType != "fanart" &&
			mCarouselImageType != "none")
		{
			LOG(LogWarning) << "Unknown carouselImageType \"" << mCarouselImageType << "\"! Using auto.";
			mCarouselImageType = "auto";
		}
	}

	if (mCarouselImageType == "none")
		mCarouselImage = false;

	if (elem->has("carouselImageFit"))
		mCarouselImageFit = Utils::String::toLower(elem->get<std::string>("carouselImageFit"));

	if (mCarouselImageFit != "contain" &&
		mCarouselImageFit != "cover" &&
		mCarouselImageFit != "stretch")
	{
		LOG(LogWarning) << "Unknown carouselImageFit \"" << mCarouselImageFit << "\"! Using contain.";
		mCarouselImageFit = "contain";
	}

	if (elem->has("carouselFallbackImage"))
		mCarouselFallbackImage = elem->get<std::string>("carouselFallbackImage");

	if (elem->has("carouselImagePadding"))
		mCarouselImagePadding = elem->get<Vector2f>("carouselImagePadding");

	if (elem->has("carouselImageLockSize"))
		mCarouselImageLockSize = elem->get<bool>("carouselImageLockSize");

	if (elem->has("carouselImageBoxSize"))
	{
		Vector2f boxSize = elem->get<Vector2f>("carouselImageBoxSize");

		mCarouselImageBoxSize = Vector2f(
			boxSize.x() * mSize.x(),
			boxSize.y() * mSize.y());
	}

	mCarouselImageBorder = "";
	mCarouselImageBorderSelected = "";
	mCarouselImageBorderScale = 1.0f;
	mCarouselImageBorderColor = 0xFFFFFFFF;

	mCarouselImageBorderComponent.setImage("");
	mCarouselImageBorderSelectedComponent.setImage("");

	if (elem->has("carouselImageBorder"))
	{
		mCarouselImageBorder = elem->get<std::string>("carouselImageBorder");

		if (!mCarouselImageBorder.empty())
			mCarouselImageBorderComponent.setImage(mCarouselImageBorder);
	}

	if (elem->has("carouselImageBorderSelected"))
	{
		mCarouselImageBorderSelected = elem->get<std::string>("carouselImageBorderSelected");

		if (!mCarouselImageBorderSelected.empty())
			mCarouselImageBorderSelectedComponent.setImage(mCarouselImageBorderSelected);
	}

	if (elem->has("carouselImageBorderScale"))
		mCarouselImageBorderScale = elem->get<float>("carouselImageBorderScale");

	if (mCarouselImageBorderScale <= 0.0f)
		mCarouselImageBorderScale = 1.0f;

	if (elem->has("carouselImageBorderColor"))
		mCarouselImageBorderColor = elem->get<unsigned int>("carouselImageBorderColor");

	mCarouselImageBorderComponent.setColorShift(mCarouselImageBorderColor);
	mCarouselImageBorderSelectedComponent.setColorShift(mCarouselImageBorderColor);

	mVirtualGap = false;
	mVirtualGapHeight = 0.0f;
	mVirtualGapOffset = 0;

	if (elem->has("virtualGap"))
		mVirtualGap = elem->get<bool>("virtualGap");

	if (elem->has("virtualGapHeight"))
	{
		float scale = this->mParent ? this->mParent->getSize().y() : (float)Renderer::getScreenHeight();
		mVirtualGapHeight = elem->get<float>("virtualGapHeight") * scale;
	}

	if (elem->has("virtualGapOffset"))
		mVirtualGapOffset = (int)std::round(elem->get<float>("virtualGapOffset"));

	mCarouselLoop = false;
	mCarouselMaxLogoCount = 5;
	mCarouselLogoScale = 1.20f;
	mCarouselMinLogoOpacity = 0.45f;
	mCarouselScaledLogoSpacing = 0.0f;
	mCarouselLogoAlignment = CAROUSEL_ALIGN_CENTER;
	mCarouselLogoSize = Vector2f::Zero();
	mCarouselLogoPosSet = false;
	mCarouselLogoPos = Vector2f::Zero();

	if (mCarouselMode)
	{
		mCarouselLoop = true;
		mCarouselMaxLogoCount = 9;
		mCarouselLogoScale = 1.30f;
		mCarouselMinLogoOpacity = 0.75f;
		mCarouselScaledLogoSpacing = 0.58f;
		mCarouselLogoAlignment = CAROUSEL_ALIGN_CENTER;

		if (mOrientation == ORIENTATION_VERTICAL)
			mCarouselLogoSize = Vector2f(0.733f * mSize.x(), 0.132f * mSize.y());
		else
			mCarouselLogoSize = Vector2f(0.132f * mSize.x(), 0.733f * mSize.y());
	}

	if (elem->has("carouselLoop"))
		mCarouselLoop = elem->get<bool>("carouselLoop");

	if (elem->has("maxLogoCount"))
		mCarouselMaxLogoCount = (int)std::round(elem->get<float>("maxLogoCount"));

	if (elem->has("logoScale"))
		mCarouselLogoScale = elem->get<float>("logoScale");

	if (elem->has("minLogoOpacity"))
		mCarouselMinLogoOpacity = elem->get<float>("minLogoOpacity");

	if (elem->has("scaledLogoSpacing"))
		mCarouselScaledLogoSpacing = elem->get<float>("scaledLogoSpacing");

	mCarouselLogoSpacingX = 0.0f;
	mCarouselLogoSpacingY = 0.0f;

	if (elem->has("logoSpacing"))
	{
		Vector2f v = elem->get<Vector2f>("logoSpacing");
		mCarouselLogoSpacingX = v.x() * mSize.x();
		mCarouselLogoSpacingY = v.y() * mSize.y();
	}

	if (elem->has("logoSpacingX"))
		mCarouselLogoSpacingX = elem->get<float>("logoSpacingX") * mSize.x();

	if (elem->has("logoSpacingY"))
		mCarouselLogoSpacingY = elem->get<float>("logoSpacingY") * mSize.y();

	if (elem->has("logoSize"))
	{
		Vector2f logoSize = elem->get<Vector2f>("logoSize");

		mCarouselLogoSize = Vector2f(
			logoSize.x() * mSize.x(),
			logoSize.y() * mSize.y());
	}

	if (elem->has("logoAlignment"))
	{
		const std::string& str = elem->get<std::string>("logoAlignment");

		if (str == "left")
			mCarouselLogoAlignment = CAROUSEL_ALIGN_LEFT;
		else if (str == "right")
			mCarouselLogoAlignment = CAROUSEL_ALIGN_RIGHT;
		else if (str == "top")
			mCarouselLogoAlignment = CAROUSEL_ALIGN_TOP;
		else if (str == "bottom")
			mCarouselLogoAlignment = CAROUSEL_ALIGN_BOTTOM;
		else if (str == "center")
			mCarouselLogoAlignment = CAROUSEL_ALIGN_CENTER;
		else
			mCarouselLogoAlignment = CAROUSEL_ALIGN_CENTER;
	}

	mCarouselLogoOffsetX = 0.0f;
	mCarouselLogoOffsetY = 0.0f;

	if (elem->has("carouselLogoPos") || elem->has("logoPos"))
	{
		Vector2f v = elem->has("carouselLogoPos") ?
			elem->get<Vector2f>("carouselLogoPos") :
			elem->get<Vector2f>("logoPos");

		mCarouselLogoPosSet = true;
		mCarouselLogoPos = Vector2f(
			v.x() * mSize.x(),
			v.y() * mSize.y());
	}

	if (elem->has("logoOffset"))
	{
		Vector2f v = elem->get<Vector2f>("logoOffset");

		mCarouselLogoOffsetX = v.x() * mSize.x();
		mCarouselLogoOffsetY = v.y() * mSize.y();
	}

	if (elem->has("logoOffsetX"))
		mCarouselLogoOffsetX = elem->get<float>("logoOffsetX") * mSize.x();

	if (elem->has("logoOffsetY"))
		mCarouselLogoOffsetY = elem->get<float>("logoOffsetY") * mSize.y();

	mCarouselSelectedLogoOffsetX = 0.0f;
	mCarouselSelectedLogoOffsetY = 0.0f;

	if (elem->has("selectedLogoOffsetX"))
		mCarouselSelectedLogoOffsetX = elem->get<float>("selectedLogoOffsetX") * mSize.x();

	if (elem->has("selectedLogoOffsetY"))
		mCarouselSelectedLogoOffsetY = elem->get<float>("selectedLogoOffsetY") * mSize.y();

	mCarouselItemColor = 0x00000088;

	if (elem->has("color"))
		mCarouselItemColor = elem->get<unsigned int>("color");

	if (elem->has("carouselItemColor"))
		mCarouselItemColor = elem->get<unsigned int>("carouselItemColor");

	mCarouselSelectedItemColor = 0x101020CC;
	if (elem->has("carouselSelectedItemColor"))
		mCarouselSelectedItemColor = elem->get<unsigned int>("carouselSelectedItemColor");

	mCarouselShowText = true;
	if (elem->has("carouselShowText"))
		mCarouselShowText = elem->get<bool>("carouselShowText");

	mCarouselTextMaxLines = 2;
	if (elem->has("carouselTextMaxLines"))
		mCarouselTextMaxLines = (int)std::round(elem->get<float>("carouselTextMaxLines"));

	if (mCarouselTextMaxLines < 1)
		mCarouselTextMaxLines = 1;
	if (mCarouselTextMaxLines > 3)
		mCarouselTextMaxLines = 3;

	if (properties & FORCE_UPPERCASE && elem->has("forceUppercase"))
		setUppercase(elem->get<bool>("forceUppercase"));

	if (elem->has("showRowNumbers"))
		setShowRowNumbers(elem->get<bool>("showRowNumbers"));

	if (properties & LINE_SPACING)
	{
		if (elem->has("lineSpacing"))
			setLineSpacing(elem->get<float>("lineSpacing"));

		if (elem->has("selectorHeight"))
			setSelectorHeight(elem->get<float>("selectorHeight") * Renderer::getScreenHeight());

		if (elem->has("selectorOffsetY"))
		{
			float scale = this->mParent ? this->mParent->getSize().y() : (float)Renderer::getScreenHeight();
			setSelectorOffsetY(elem->get<float>("selectorOffsetY") * scale);
		}
		else
		{
			setSelectorOffsetY(0.0f);
		}

		if (elem->has("selectorWidth"))
		{
			setSelectorWidth(elem->get<float>("selectorWidth") * Renderer::getScreenWidth());
		}
		else
		{
			setSelectorWidth(0.0f);
		}

		if (elem->has("selectorHorizontalOffset"))
		{
			float scale = this->mParent ? this->mParent->getSize().x() : (float)Renderer::getScreenWidth();
			setSelectorHorizontalOffset(elem->get<float>("selectorHorizontalOffset") * scale);
		}
		else
		{
			setSelectorHorizontalOffset(0.0f);
		}
	}

	if (elem->has("selectorImagePath"))
	{
		std::string path = elem->get<std::string>("selectorImagePath");
		bool tile = elem->has("selectorImageTile") && elem->get<bool>("selectorImageTile");
		mSelectorImage.setImage(path, tile);
		mSelectorImage.setSize((mSelectorWidth > 0.0f) ? mSelectorWidth : mSize.x(), mSelectorHeight);
		mSelectorImage.setColorShift(mSelectorColor);
		mSelectorImage.setColorShiftEnd(mSelectorColorEnd);
	}
	else
	{
		mSelectorImage.setImage("");
	}

	if (size() > 0)
		mCarouselCamOffset = (float)mCursor;
}

#endif // ES_APP_COMPONENTS_TEXT_LIST_COMPONENT_H
