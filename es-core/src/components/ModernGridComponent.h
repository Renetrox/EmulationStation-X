#pragma once
#ifndef ES_CORE_COMPONENTS_MODERN_GRID_COMPONENT_H
#define ES_CORE_COMPONENTS_MODERN_GRID_COMPONENT_H

#include "Log.h"
#include "components/GridTileComponent.h"
#include "components/IList.h"
#include "resources/ResourceManager.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <string>
#include <vector>

enum ScrollDirection
{
	SCROLL_VERTICALLY,
	SCROLL_HORIZONTALLY
};

enum ImageSource
{
	THUMBNAIL,
	IMAGE,
	MARQUEE
};

struct ModernGridData
{
	std::string texturePath;
};

// ES-X modern grid.
//
// Goals:
//  * deterministic 2D navigation
//  * a real viewport instead of camera/EXTRAITEMS bookkeeping
//  * a small visible tile pool with explicit rebinding
//  * no "hide every tile" fast-scroll shortcut
//  * keep legacy imagegrid/gridtile theming compatible
//
// The first implementation intentionally uses instant row/column viewport changes.
// Once behavior is stable, a visual-only scroll offset can be layered on top without
// changing cursor or tile identity.
template<typename T>
class ModernGridComponent : public IList<ModernGridData, T>
{
protected:
	using List = IList<ModernGridData, T>;
	using List::mEntries;
	using List::mCursor;
	using List::mScrollTier;
	using List::mSize;
	using List::listInput;
	using List::listUpdate;
	using List::listRenderTitleOverlay;
	using List::getTransform;

public:
	using List::getSelected;
	using List::isScrolling;
	using List::setCursor;
	using List::size;
	using List::stopScrolling;

	ModernGridComponent(Window* window);

	void add(const std::string& name, const std::string& imagePath, const T& obj);
	void clear();
	bool remove(const T& obj);

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;
	void applyTheme(const std::shared_ptr<ThemeData>& theme,
	                const std::string& view,
	                const std::string& element,
	                unsigned int properties) override;
	void onSizeChanged() override;

	inline void setCursorChangedCallback(const std::function<void(CursorState state)>& func)
	{
		mCursorChangedCallback = func;
	}

	ImageSource getImageSource() const { return mImageSource; }

protected:
	void onCursorChanged(const CursorState& state) override;

private:
	bool isVertical() const { return mScrollDirection == SCROLL_VERTICALLY; }
	void calculateLayout();
	void rebuildTiles();
	void updateViewport();
	void bindVisibleTiles();
	int logicalIndexForCell(int localRow, int localCol) const;
	bool moveCursor(int deltaRow, int deltaCol);
	int visiblePrimaryCount() const;
	int visibleSecondaryCount() const;
	int totalPrimaryCount() const;
	bool canLoop() const;

	std::shared_ptr<ThemeData> mTheme;
	std::string mThemeView;

	std::vector<std::shared_ptr<GridTileComponent>> mTiles;
	Vector2f mTileSize;
	Vector2f mMargin;
	Vector4f mPadding;
	Vector2f mAutoLayout;
	float mAutoLayoutZoom;
	Vector2i mGridDimension;

	int mFirstVisiblePrimary;
	bool mLayoutValid;
	bool mEntriesDirty;

	std::string mDefaultGameTexture;
	std::string mDefaultFolderTexture;
	ScrollDirection mScrollDirection;
	ImageSource mImageSource;
	bool mCenterSelection;
	bool mScrollLoop;
	bool mAnimate;

	std::function<void(CursorState state)> mCursorChangedCallback;
};

template<typename T>
ModernGridComponent<T>::ModernGridComponent(Window* window)
	: IList<ModernGridData, T>(window, LIST_SCROLL_STYLE_QUICK, LIST_ALWAYS_LOOP)
{
	Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	mTileSize = GridTileComponent::getDefaultTileSize();
	mMargin = screen * 0.07f;
	mPadding = Vector4f::Zero();
	mAutoLayout = Vector2f::Zero();
	mAutoLayoutZoom = 1.0f;
	mGridDimension = Vector2i(1, 1);

	mFirstVisiblePrimary = 0;
	mLayoutValid = false;
	mEntriesDirty = true;

	mDefaultGameTexture = ":/cartridge.svg";
	mDefaultFolderTexture = ":/folder.svg";
	mScrollDirection = SCROLL_VERTICALLY;
	mImageSource = THUMBNAIL;
	mCenterSelection = false;
	mScrollLoop = false;
	mAnimate = false;

	mSize = screen * 0.80f;
}

template<typename T>
void ModernGridComponent<T>::add(const std::string& name,
                                 const std::string& imagePath,
                                 const T& obj)
{
	typename List::Entry entry;
	entry.name = name;
	entry.object = obj;
	entry.data.texturePath = imagePath;
	List::add(entry);
	mEntriesDirty = true;
}

template<typename T>
void ModernGridComponent<T>::clear()
{
	List::clear();
	mFirstVisiblePrimary = 0;
	mEntriesDirty = true;
	bindVisibleTiles();
}

template<typename T>
bool ModernGridComponent<T>::remove(const T& obj)
{
	bool removed = List::remove(obj);
	if (removed)
	{
		mEntriesDirty = true;
		updateViewport();
		bindVisibleTiles();
	}
	return removed;
}

template<typename T>
bool ModernGridComponent<T>::input(InputConfig* config, Input input)
{
	if (!mLayoutValid)
		calculateLayout();

	if (input.value != 0)
	{
		if (config->isMappedLike("up", input))
			return moveCursor(-1, 0);
		if (config->isMappedLike("down", input))
			return moveCursor(1, 0);
		if (config->isMappedLike("left", input))
			return moveCursor(0, -1);
		if (config->isMappedLike("right", input))
			return moveCursor(0, 1);
	}
	else if (config->isMappedLike("up", input) ||
	         config->isMappedLike("down", input) ||
	         config->isMappedLike("left", input) ||
	         config->isMappedLike("right", input))
	{
		stopScrolling();
		return true;
	}

	return GuiComponent::input(config, input);
}

template<typename T>
bool ModernGridComponent<T>::moveCursor(int deltaRow, int deltaCol)
{
	if (mEntries.empty())
		return false;

	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());
	int velocity = 0;

	if (isVertical())
	{
		if (deltaCol != 0)
		{
			const int currentCol = mCursor % columns;
			const int targetCol = currentCol + deltaCol;
			if (!canLoop() && (targetCol < 0 || targetCol >= columns))
				return true;
			velocity = deltaCol;
		}
		else if (deltaRow != 0)
		{
			const int target = mCursor + (deltaRow * columns);
			if (!canLoop())
			{
				if (target < 0 || target >= size())
					return true;
			}
			velocity = deltaRow * columns;
		}
	}
	else
	{
		if (deltaRow != 0)
		{
			const int currentRow = mCursor % rows;
			const int targetRow = currentRow + deltaRow;
			if (!canLoop() && (targetRow < 0 || targetRow >= rows))
				return true;
			velocity = deltaRow;
		}
		else if (deltaCol != 0)
		{
			const int target = mCursor + (deltaCol * rows);
			if (!canLoop())
			{
				if (target < 0 || target >= size())
					return true;
			}
			velocity = deltaCol * rows;
		}
	}

	if (velocity == 0)
		return true;

	listInput(velocity);
	return true;
}

template<typename T>
void ModernGridComponent<T>::update(int deltaTime)
{
	GuiComponent::update(deltaTime);
	listUpdate(deltaTime);

	if (mEntriesDirty)
	{
		updateViewport();
		bindVisibleTiles();
		mEntriesDirty = false;
	}

	for (auto& tile : mTiles)
		tile->update(deltaTime);
}

template<typename T>
void ModernGridComponent<T>::render(const Transform4x4f& parentTrans)
{
	if (!mLayoutValid)
		calculateLayout();

	if (mEntriesDirty)
	{
		updateViewport();
		bindVisibleTiles();
		mEntriesDirty = false;
	}

	Transform4x4f trans = getTransform() * parentTrans;

	float scaleX = trans.r0().x();
	float scaleY = trans.r1().y();
	Vector2i pos((int)Math::round(trans.translation()[0]), (int)Math::round(trans.translation()[1]));
	Vector2i sizePx((int)Math::round(mSize.x() * scaleX), (int)Math::round(mSize.y() * scaleY));

	Renderer::pushClipRect(pos, sizePx);

	std::shared_ptr<GridTileComponent> selectedTile;
	for (auto& tile : mTiles)
	{
		if (!tile->isVisible())
			continue;
		if (tile->isSelected())
			selectedTile = tile;
		else
			tile->render(trans);
	}

	Renderer::popClipRect();

	// Keep legacy behavior: selected zoom/frame may extend outside the clipping area.
	if (selectedTile)
		selectedTile->render(trans);

	listRenderTitleOverlay(trans);
	GuiComponent::renderChildren(trans);
}

template<typename T>
void ModernGridComponent<T>::applyTheme(const std::shared_ptr<ThemeData>& theme,
                                        const std::string& view,
                                        const std::string& element,
                                        unsigned int properties)
{
	GuiComponent::applyTheme(theme, view, element, properties ^ ThemeFlags::SIZE);

	mTheme = theme;
	mThemeView = view;

	Vector2f screen((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	const ThemeData::ThemeElement* elem = theme->getElement(view, element, "imagegrid");

	if (elem)
	{
		if (elem->has("margin"))
			mMargin = elem->get<Vector2f>("margin") * screen;
		if (elem->has("padding"))
			mPadding = elem->get<Vector4f>("padding") * Vector4f(screen.x(), screen.y(), screen.x(), screen.y());
		if (elem->has("autoLayout"))
			mAutoLayout = elem->get<Vector2f>("autoLayout");
		if (elem->has("autoLayoutSelectedZoom"))
			mAutoLayoutZoom = elem->get<float>("autoLayoutSelectedZoom");

		if (elem->has("imageSource"))
		{
			const std::string source = elem->get<std::string>("imageSource");
			if (source == "image")
				mImageSource = IMAGE;
			else if (source == "marquee")
				mImageSource = MARQUEE;
			else
				mImageSource = THUMBNAIL;
		}
		else
		{
			mImageSource = THUMBNAIL;
		}

		if (elem->has("scrollDirection"))
			mScrollDirection = (elem->get<std::string>("scrollDirection") == "horizontal") ?
				SCROLL_HORIZONTALLY : SCROLL_VERTICALLY;
		if (elem->has("centerSelection"))
			mCenterSelection = elem->get<bool>("centerSelection");
		if (elem->has("scrollLoop"))
			mScrollLoop = elem->get<bool>("scrollLoop");
		if (elem->has("animate"))
			mAnimate = elem->get<bool>("animate");

		if (elem->has("gameImage"))
			mDefaultGameTexture = elem->get<std::string>("gameImage");
		if (elem->has("folderImage"))
			mDefaultFolderTexture = elem->get<std::string>("folderImage");
	}

	const ThemeData::ThemeElement* tileElem = theme->getElement(view, "default", "gridtile");
	mTileSize = (tileElem && tileElem->has("size")) ?
		tileElem->get<Vector2f>("size") * screen : GridTileComponent::getDefaultTileSize();

	GuiComponent::applyTheme(theme, view, element, ThemeFlags::SIZE);

	mLayoutValid = false;
	calculateLayout();
	updateViewport();
	bindVisibleTiles();
	mEntriesDirty = false;
}

template<typename T>
void ModernGridComponent<T>::onSizeChanged()
{
	mLayoutValid = false;
	calculateLayout();
	updateViewport();
	bindVisibleTiles();
}

template<typename T>
void ModernGridComponent<T>::onCursorChanged(const CursorState& state)
{
	updateViewport();
	bindVisibleTiles();

	if (mCursorChangedCallback)
		mCursorChangedCallback(state);
}

template<typename T>
void ModernGridComponent<T>::calculateLayout()
{
	Vector2f available(
		std::max(1.0f, mSize.x() - mPadding.x() - mPadding.z()),
		std::max(1.0f, mSize.y() - mPadding.y() - mPadding.w()));

	int columns = 1;
	int rows = 1;

	if (mAutoLayout.x() > 0.0f && mAutoLayout.y() > 0.0f)
	{
		columns = std::max(1, (int)Math::round(mAutoLayout.x()));
		rows = std::max(1, (int)Math::round(mAutoLayout.y()));

		const float tileW = (available.x() - (mMargin.x() * (columns - 1))) / columns;
		const float tileH = (available.y() - (mMargin.y() * (rows - 1))) / rows;
		mTileSize = Vector2f(std::max(1.0f, tileW), std::max(1.0f, tileH));
	}
	else
	{
		const float stepX = std::max(1.0f, mTileSize.x() + mMargin.x());
		const float stepY = std::max(1.0f, mTileSize.y() + mMargin.y());
		const float fitX = (available.x() + mMargin.x()) / stepX;
		const float fitY = (available.y() + mMargin.y()) / stepY;

		columns = std::max(1, (int)std::floor(fitX));
		rows = std::max(1, (int)(isVertical() ? std::ceil(fitY) : std::floor(fitY)));

		if (!isVertical())
			columns = std::max(1, (int)std::ceil(fitX));
	}

	mGridDimension = Vector2i(columns, rows);
	mLayoutValid = true;
	rebuildTiles();
}

template<typename T>
void ModernGridComponent<T>::rebuildTiles()
{
	mTiles.clear();

	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());
	const Vector2f start(
		mPadding.x() + (mTileSize.x() * 0.5f),
		mPadding.y() + (mTileSize.y() * 0.5f));
	const Vector2f step = mTileSize + mMargin;

	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < columns; ++col)
		{
			auto tile = std::make_shared<GridTileComponent>(this->mWindow);
			tile->setPosition(start.x() + (col * step.x()), start.y() + (row * step.y()));
			tile->setOrigin(0.5f, 0.5f);
			tile->setImage("");

			if (mTheme)
				tile->applyTheme(mTheme, mThemeView, "gridtile", ThemeFlags::ALL);

			if (mAutoLayout.x() > 0.0f && mAutoLayout.y() > 0.0f)
				tile->forceSize(mTileSize, mAutoLayoutZoom);

			mTiles.push_back(tile);
		}
	}
}

template<typename T>
void ModernGridComponent<T>::updateViewport()
{
	if (!mLayoutValid)
		calculateLayout();

	if (mEntries.empty())
	{
		mFirstVisiblePrimary = 0;
		return;
	}

	const int secondary = visibleSecondaryCount();
	const int primaryVisible = visiblePrimaryCount();
	const int totalPrimary = totalPrimaryCount();
	const int selectedPrimary = mCursor / std::max(1, secondary);
	const int maxFirst = std::max(0, totalPrimary - primaryVisible);

	if (mCenterSelection)
	{
		mFirstVisiblePrimary = selectedPrimary - (primaryVisible / 2);
	}
	else
	{
		if (selectedPrimary < mFirstVisiblePrimary)
			mFirstVisiblePrimary = selectedPrimary;
		else if (selectedPrimary >= mFirstVisiblePrimary + primaryVisible)
			mFirstVisiblePrimary = selectedPrimary - primaryVisible + 1;
	}

	mFirstVisiblePrimary = std::max(0, std::min(maxFirst, mFirstVisiblePrimary));
}

template<typename T>
void ModernGridComponent<T>::bindVisibleTiles()
{
	if (!mLayoutValid || mTiles.empty())
		return;

	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());

	for (int row = 0; row < rows; ++row)
	{
		for (int col = 0; col < columns; ++col)
		{
			const int tilePos = row * columns + col;
			if (tilePos < 0 || tilePos >= (int)mTiles.size())
				continue;

			auto& tile = mTiles.at(tilePos);
			const int index = logicalIndexForCell(row, col);

			// A recycled tile must never carry animation/selection/visibility from
			// the entry it represented previously.
			tile->cancelAllAnimations();
			tile->setSelected(false, false, nullptr, true);
			tile->setVisible(false);
			tile->reset();

			if (index < 0 || index >= size())
				continue;

			std::string imagePath = mEntries.at(index).data.texturePath;
			if (!imagePath.empty() && ResourceManager::getInstance()->fileExists(imagePath))
				tile->setImage(imagePath);
			else if (mEntries.at(index).object->getType() == 2)
				tile->setImage(mDefaultFolderTexture);
			else
				tile->setImage(mDefaultGameTexture);

			tile->setSelected(index == mCursor, false, nullptr, true);
			tile->setVisible(true);
		}
	}
}

template<typename T>
int ModernGridComponent<T>::logicalIndexForCell(int localRow, int localCol) const
{
	const int columns = std::max(1, mGridDimension.x());
	const int rows = std::max(1, mGridDimension.y());

	if (isVertical())
	{
		const int absoluteRow = mFirstVisiblePrimary + localRow;
		return absoluteRow * columns + localCol;
	}

	const int absoluteCol = mFirstVisiblePrimary + localCol;
	return absoluteCol * rows + localRow;
}

template<typename T>
int ModernGridComponent<T>::visiblePrimaryCount() const
{
	return isVertical() ? std::max(1, mGridDimension.y()) : std::max(1, mGridDimension.x());
}

template<typename T>
int ModernGridComponent<T>::visibleSecondaryCount() const
{
	return isVertical() ? std::max(1, mGridDimension.x()) : std::max(1, mGridDimension.y());
}

template<typename T>
int ModernGridComponent<T>::totalPrimaryCount() const
{
	if (mEntries.empty())
		return 0;
	const int secondary = visibleSecondaryCount();
	return (size() + secondary - 1) / secondary;
}

template<typename T>
bool ModernGridComponent<T>::canLoop() const
{
	if (!mScrollLoop)
		return false;
	return size() >= (visiblePrimaryCount() * visibleSecondaryCount());
}

#endif // ES_CORE_COMPONENTS_MODERN_GRID_COMPONENT_H
