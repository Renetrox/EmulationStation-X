#pragma once
#ifndef ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H
#define ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H

#include "components/DateTimeComponent.h"
#include "components/RatingComponent.h"
#include "components/ScrollableContainer.h"
#include "views/gamelist/BasicGameListView.h"

class DetailedGameListView : public BasicGameListView
{
public:
	DetailedGameListView(Window* window, FileData* root);

	virtual void onThemeChanged(const std::shared_ptr<ThemeData>& theme) override;
	virtual void update(int deltaTime) override;

	virtual const char* getName() const override { return "detailed"; }

	virtual void launch(FileData* game) override;

	void onFocusLost() override;

private:
	void updateInfoPanel();
	void updateLightInfoPanel();
	void scheduleInfoPanelUpdate(const CursorState& state);

	void initMDLabels();
	void initMDValues();

	int getCurrentGameIndex(FileData* file) const;

	ImageComponent mBackground;
	ImageComponent mThumbnail;
	ImageComponent mMarquee;
	ImageComponent mImage;

	TextComponent mLblRating, mLblReleaseDate, mLblDeveloper, mLblPublisher, mLblGenre, mLblPlayers, mLblLastPlayed, mLblPlayCount;

	RatingComponent mRating;
	DateTimeComponent mReleaseDate;
	TextComponent mDeveloper;
	TextComponent mPublisher;
	TextComponent mGenre;
	TextComponent mPlayers;
	DateTimeComponent mLastPlayed;
	TextComponent mPlayCount;
	TextComponent mName;
	TextComponent mGameCounter;

	std::vector<TextComponent*> getMDLabels();
	std::vector<GuiComponent*> getMDValues();

	ScrollableContainer mDescContainer;
	TextComponent mDescription;

	static constexpr int INFO_PANEL_DELAY = 120;
	int mInfoPanelDelay;
	bool mInfoPanelPending;
	bool mInfoPanelScheduledThisFrame;
};

#endif // ES_APP_VIEWS_GAME_LIST_DETAILED_GAME_LIST_VIEW_H
