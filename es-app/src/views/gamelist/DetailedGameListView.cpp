#include "views/gamelist/DetailedGameListView.h"

#include "animations/LambdaAnimation.h"
#include "views/ViewController.h"
#include "LocaleES.h"

#include <sstream>

DetailedGameListView::DetailedGameListView(Window* window, FileData* root) :
	BasicGameListView(window, root),
	mDescContainer(window, DESCRIPTION_SCROLL_DELAY), mDescription(window),
	mBackground(window),
	mThumbnail(window),
	mMarquee(window),
	mImage(window),

	mLblRating(window), mLblReleaseDate(window), mLblDeveloper(window), mLblPublisher(window),
	mLblGenre(window), mLblPlayers(window), mLblLastPlayed(window), mLblPlayCount(window),

	mRating(window), mReleaseDate(window), mDeveloper(window), mPublisher(window),
	mGenre(window), mPlayers(window), mLastPlayed(window), mPlayCount(window),
	mName(window),
	mGameCounter(window),
	mInfoPanelDelay(0),
	mInfoPanelPending(false),
	mInfoPanelScheduledThisFrame(false)
{
	const float padding = 0.01f;

	mList.setPosition(mSize.x() * (0.50f + padding), mList.getPosition().y());
	mList.setSize(mSize.x() * (0.50f - padding), mList.getSize().y());
	mList.setAlignment(TextListComponent<FileData*>::ALIGN_LEFT);
	mList.setCursorChangedCallback([this](const CursorState& state)
	{
		scheduleInfoPanelUpdate(state);
	});

	// Fondo dinámico por juego
	mBackground.setOrigin(0.0f, 0.0f);
	mBackground.setPosition(0.0f, 0.0f);
	mBackground.setResize(mSize.x(), mSize.y());
	mBackground.setDefaultZIndex(5);
	mBackground.setVisible(false);
	addChild(&mBackground);

	// Imagen principal
	mImage.setOrigin(0.5f, 0.5f);
	mImage.setPosition(mSize.x() * 0.25f, mList.getPosition().y() + mSize.y() * 0.2125f);
	mImage.setMaxSize(mSize.x() * (0.50f - 2 * padding), mSize.y() * 0.4f);
	mImage.setDefaultZIndex(30);
	addChild(&mImage);

	// Thumbnail
	mThumbnail.setOrigin(0.5f, 0.5f);
	mThumbnail.setPosition(2.0f, 2.0f);
	mThumbnail.setMaxSize(mSize.x(), mSize.y());
	mThumbnail.setDefaultZIndex(35);
	mThumbnail.setVisible(false);
	addChild(&mThumbnail);

	// Marquee
	mMarquee.setOrigin(0.5f, 0.5f);
	mMarquee.setPosition(2.0f, 2.0f);
	mMarquee.setMaxSize(mSize.x(), mSize.y());
	mMarquee.setDefaultZIndex(35);
	mMarquee.setVisible(false);
	addChild(&mMarquee);

	// Localización
	LocaleES& loc = LocaleES::getInstance();
	loc.loadFromSettings();

	mLblRating.setText(loc.translate("RATING") + ": ");
	addChild(&mLblRating);
	addChild(&mRating);

	mLblReleaseDate.setText(loc.translate("RELEASE DATE") + ": ");
	addChild(&mLblReleaseDate);
	addChild(&mReleaseDate);

	mLblDeveloper.setText(loc.translate("DEVELOPER") + ": ");
	addChild(&mLblDeveloper);
	addChild(&mDeveloper);

	mLblPublisher.setText(loc.translate("PUBLISHER") + ": ");
	addChild(&mLblPublisher);
	addChild(&mPublisher);

	mLblGenre.setText(loc.translate("GENRE") + ": ");
	addChild(&mLblGenre);
	addChild(&mGenre);

	mLblPlayers.setText(loc.translate("PLAYERS") + ": ");
	addChild(&mLblPlayers);
	addChild(&mPlayers);

	mLblLastPlayed.setText(loc.translate("LAST PLAYED") + ": ");
	addChild(&mLblLastPlayed);
	mLastPlayed.setDisplayRelative(true);
	addChild(&mLastPlayed);

	mLblPlayCount.setText(loc.translate("PLAY COUNT") + ": ");
	addChild(&mLblPlayCount);
	addChild(&mPlayCount);

	mName.setPosition(mSize.x(), mSize.y());
	mName.setDefaultZIndex(40);
	mName.setColor(0xAAAAAAFF);
	mName.setFont(Font::get(FONT_SIZE_MEDIUM));
	mName.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mName);

	mGameCounter.setPosition(mSize.x(), mSize.y());
	mGameCounter.setDefaultZIndex(40);
	mGameCounter.setColor(0xAAAAAAFF);
	mGameCounter.setFont(Font::get(FONT_SIZE_SMALL));
	mGameCounter.setHorizontalAlignment(ALIGN_CENTER);
	addChild(&mGameCounter);

	mDescContainer.setPosition(mSize.x() * padding, mSize.y() * 0.65f);
	mDescContainer.setSize(mSize.x() * (0.50f - 2 * padding), mSize.y() - mDescContainer.getPosition().y());
	mDescContainer.setAutoScroll(true);
	mDescContainer.setDefaultZIndex(40);
	addChild(&mDescContainer);

	mDescription.setFont(Font::get(FONT_SIZE_SMALL));
	mDescription.setSize(mDescContainer.getSize().x(), 0);
	mDescContainer.addChild(&mDescription);

	initMDLabels();
	initMDValues();
	updateInfoPanel();
}

void DetailedGameListView::onThemeChanged(const std::shared_ptr<ThemeData>& theme)
{
	BasicGameListView::onThemeChanged(theme);

	using namespace ThemeFlags;
	mBackground.applyTheme(theme, getName(), "md_background", POSITION | ThemeFlags::SIZE | Z_INDEX | ROTATION | VISIBLE);
	mThumbnail.applyTheme(theme, getName(), "md_thumbnail", POSITION | ThemeFlags::SIZE | Z_INDEX | ROTATION | VISIBLE);
	mMarquee.applyTheme(theme, getName(), "md_marquee", POSITION | ThemeFlags::SIZE | Z_INDEX | ROTATION | VISIBLE);
	mImage.applyTheme(theme, getName(), "md_image", POSITION | ThemeFlags::SIZE | Z_INDEX | ROTATION | VISIBLE);
	mName.applyTheme(theme, getName(), "md_name", ALL);
	mGameCounter.applyTheme(theme, getName(), "md_gamecounter", ALL);

	initMDLabels();
	std::vector<TextComponent*> labels = getMDLabels();
	assert(labels.size() == 8);
	const char* lblElements[8] = {
		"md_lbl_rating", "md_lbl_releasedate", "md_lbl_developer", "md_lbl_publisher",
		"md_lbl_genre", "md_lbl_players", "md_lbl_lastplayed", "md_lbl_playcount"
	};

	for (unsigned int i = 0; i < labels.size(); i++)
		labels[i]->applyTheme(theme, getName(), lblElements[i], ALL);

	initMDValues();
	std::vector<GuiComponent*> values = getMDValues();
	assert(values.size() == 8);
	const char* valElements[8] = {
		"md_rating", "md_releasedate", "md_developer", "md_publisher",
		"md_genre", "md_players", "md_lastplayed", "md_playcount"
	};

	for (unsigned int i = 0; i < values.size(); i++)
		values[i]->applyTheme(theme, getName(), valElements[i], ALL ^ ThemeFlags::TEXT);

	mDescContainer.applyTheme(theme, getName(), "md_description", POSITION | ThemeFlags::SIZE | Z_INDEX | VISIBLE);
	mDescription.setSize(mDescContainer.getSize().x(), 0);
	mDescription.applyTheme(theme, getName(), "md_description", ALL ^ (POSITION | ThemeFlags::SIZE | ThemeFlags::ORIGIN | TEXT | ROTATION));

	sortChildren();
}

void DetailedGameListView::initMDLabels()
{
	std::vector<TextComponent*> components = getMDLabels();

	const unsigned int colCount = 2;
	const unsigned int rowCount = (int)(components.size() / 2);

	Vector3f start(mSize.x() * 0.01f, mSize.y() * 0.625f, 0.0f);

	const float colSize = (mSize.x() * 0.48f) / colCount;
	const float rowPadding = 0.01f * mSize.y();

	for (unsigned int i = 0; i < components.size(); i++)
	{
		const unsigned int row = i % rowCount;
		Vector3f pos(0.0f, 0.0f, 0.0f);
		if (row == 0)
			pos = start + Vector3f(colSize * (i / rowCount), 0, 0);
		else
		{
			GuiComponent* lc = components[i - 1];
			pos = lc->getPosition() + Vector3f(0, lc->getSize().y() + rowPadding, 0);
		}

		components[i]->setFont(Font::get(FONT_SIZE_SMALL));
		components[i]->setPosition(pos);
		components[i]->setDefaultZIndex(40);
	}
}

void DetailedGameListView::initMDValues()
{
	std::vector<TextComponent*> labels = getMDLabels();
	std::vector<GuiComponent*> values = getMDValues();

	std::shared_ptr<Font> defaultFont = Font::get(FONT_SIZE_SMALL);
	mRating.setSize(defaultFont->getHeight() * 5.0f, (float)defaultFont->getHeight());
	mReleaseDate.setFont(defaultFont);
	mDeveloper.setFont(defaultFont);
	mPublisher.setFont(defaultFont);
	mGenre.setFont(defaultFont);
	mPlayers.setFont(defaultFont);
	mLastPlayed.setFont(defaultFont);
	mPlayCount.setFont(defaultFont);

	float bottom = 0.0f;
	const float colSize = (mSize.x() * 0.48f) / 2;

	for (unsigned int i = 0; i < labels.size(); i++)
	{
		const float heightDiff = (labels[i]->getSize().y() - values[i]->getSize().y()) / 2;
		values[i]->setPosition(labels[i]->getPosition() + Vector3f(labels[i]->getSize().x(), heightDiff, 0));
		values[i]->setSize(colSize - labels[i]->getSize().x(), values[i]->getSize().y());
		values[i]->setDefaultZIndex(40);

		float testBot = values[i]->getPosition().y() + values[i]->getSize().y();
		if (testBot > bottom)
			bottom = testBot;
	}

	mDescContainer.setPosition(mDescContainer.getPosition().x(), bottom + mSize.y() * 0.01f);
	mDescContainer.setSize(mDescContainer.getSize().x(), mSize.y() - mDescContainer.getPosition().y());
}

int DetailedGameListView::getCurrentGameIndex(FileData* file) const
{
	if (file == nullptr || mRoot == nullptr)
		return 0;

	const std::vector<FileData*>& children = mRoot->getChildren();
	for (size_t i = 0; i < children.size(); i++)
	{
		if (children[i] == file)
			return (int)i + 1;
	}

	return 0;
}

void DetailedGameListView::scheduleInfoPanelUpdate(const CursorState& /*state*/)
{
	if (mList.size() == 0)
	{
		mInfoPanelPending = false;
		mInfoPanelDelay = 0;
		updateInfoPanel();
		return;
	}

	// Nombre y contador responden inmediatamente. Los elementos costosos se
	// aplican solo cuando la selección permanece estable durante unos frames.
	updateLightInfoPanel();
	mInfoPanelDelay = INFO_PANEL_DELAY;
	mInfoPanelPending = true;
	mInfoPanelScheduledThisFrame = true;
}

void DetailedGameListView::updateLightInfoPanel()
{
	if (mList.size() == 0)
	{
		mName.setValue("");
		mGameCounter.setValue("");
		return;
	}

	FileData* file = mList.getSelected();
	if (file == nullptr)
		return;

	mName.setValue(file->metadata.get("name"));

	const int total = (int)mList.size();
	int index = getCurrentGameIndex(file);
	if (index <= 0 && total > 0)
		index = 1;

	std::stringstream ss;
	ss << index << " / " << total;
	mGameCounter.setValue(ss.str());
}

void DetailedGameListView::update(int deltaTime)
{
	BasicGameListView::update(deltaTime);

	if (!mInfoPanelPending)
	{
		mInfoPanelScheduledThisFrame = false;
		return;
	}

	// El input se procesa antes de update(). No descontar el delta del mismo
	// frame en el que se programó la actualización, especialmente tras un tirón.
	if (mInfoPanelScheduledThisFrame)
	{
		mInfoPanelScheduledThisFrame = false;
		return;
	}

	mInfoPanelDelay -= deltaTime;
	if (mInfoPanelDelay > 0)
		return;

	mInfoPanelPending = false;
	mInfoPanelDelay = 0;
	updateInfoPanel();
}

void DetailedGameListView::updateInfoPanel()
{
	FileData* file = (mList.size() == 0) ? NULL : mList.getSelected();

	bool fadingOut;
	if (file == NULL)
	{
		fadingOut = true;
		mBackground.setImage("");
		mBackground.setVisible(false);
		mGameCounter.setValue("");
	}
	else
	{
		const std::string bgPath = file->getBackgroundPath();
		if (!bgPath.empty())
		{
			mBackground.setImage(bgPath);
			mBackground.setVisible(true);
		}
		else
		{
			mBackground.setImage("");
			mBackground.setVisible(false);
		}

		mThumbnail.setImage(file->getThumbnailPath());
		mMarquee.setImage(file->getMarqueePath());
		mImage.setImage(file->getImagePath());
		mDescription.setText(file->metadata.get("desc"));
		mDescContainer.reset();

		mRating.setValue(file->metadata.get("rating"));
		mReleaseDate.setValue(file->metadata.get("releasedate"));
		mDeveloper.setValue(file->metadata.get("developer"));
		mPublisher.setValue(file->metadata.get("publisher"));
		mGenre.setValue(file->metadata.get("genre"));
		mPlayers.setValue(file->metadata.get("players"));
		mName.setValue(file->metadata.get("name"));

		const int total = (int)mList.size();
		int index = getCurrentGameIndex(file);
		if (index <= 0 && total > 0)
			index = 1;

		std::stringstream ss;
		ss << index << " / " << total;
		mGameCounter.setValue(ss.str());

		if (file->getType() == GAME)
		{
			mLastPlayed.setValue(file->metadata.get("lastplayed"));
			mPlayCount.setValue(file->metadata.get("playcount"));
		}
		else
		{
			mLastPlayed.setValue("");
			mPlayCount.setValue("");
		}

		fadingOut = false;
	}

	std::vector<GuiComponent*> comps = getMDValues();
	comps.push_back(&mBackground);
	comps.push_back(&mThumbnail);
	comps.push_back(&mMarquee);
	comps.push_back(&mImage);
	comps.push_back(&mDescription);
	comps.push_back(&mName);
	comps.push_back(&mGameCounter);

	std::vector<TextComponent*> labels = getMDLabels();
	comps.insert(comps.cend(), labels.cbegin(), labels.cend());

	for (auto it = comps.cbegin(); it != comps.cend(); it++)
	{
		GuiComponent* comp = *it;
		if ((comp->isAnimationPlaying(0) && comp->isAnimationReversed(0) != fadingOut) ||
			(!comp->isAnimationPlaying(0) && comp->getOpacity() != (fadingOut ? 0 : 255)))
		{
			auto func = [comp](float t)
			{
				comp->setOpacity((unsigned char)(Math::lerp(0.0f, 1.0f, t) * 255));
			};
			comp->setAnimation(new LambdaAnimation(func, 150), 0, nullptr, fadingOut);
		}
	}
}

void DetailedGameListView::launch(FileData* game)
{
	Vector3f target(Renderer::getScreenWidth() / 2.0f, Renderer::getScreenHeight() / 2.0f, 0);
	if (mImage.hasImage())
		target = Vector3f(mImage.getCenter().x(), mImage.getCenter().y(), 0);

	ViewController::get()->launch(game, target);
}

std::vector<TextComponent*> DetailedGameListView::getMDLabels()
{
	std::vector<TextComponent*> ret;
	ret.push_back(&mLblRating);
	ret.push_back(&mLblReleaseDate);
	ret.push_back(&mLblDeveloper);
	ret.push_back(&mLblPublisher);
	ret.push_back(&mLblGenre);
	ret.push_back(&mLblPlayers);
	ret.push_back(&mLblLastPlayed);
	ret.push_back(&mLblPlayCount);
	return ret;
}

std::vector<GuiComponent*> DetailedGameListView::getMDValues()
{
	std::vector<GuiComponent*> ret;
	ret.push_back(&mRating);
	ret.push_back(&mReleaseDate);
	ret.push_back(&mDeveloper);
	ret.push_back(&mPublisher);
	ret.push_back(&mGenre);
	ret.push_back(&mPlayers);
	ret.push_back(&mLastPlayed);
	ret.push_back(&mPlayCount);
	return ret;
}

void DetailedGameListView::onFocusLost()
{
	mDescContainer.reset();
	mList.stopScrolling(true);

	mInfoPanelPending = false;
	mInfoPanelDelay = 0;
	mInfoPanelScheduledThisFrame = false;
}
