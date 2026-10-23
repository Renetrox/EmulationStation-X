#include "guis/GuiScraperMulti.h"

#include "components/ButtonComponent.h"
#include "components/MenuComponent.h"
#include "components/ScraperSearchComponent.h"
#include "components/TextComponent.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiInfoPopup.h"
#include "views/ViewController.h"
#include "Gamelist.h"
#include "PowerSaver.h"
#include "SystemData.h"
#include "Window.h"
#include "LocaleESHook.h"
#include "Settings.h"
#include <algorithm>

namespace
{
	inline bool isMenuDark()
	{
		return Settings::getInstance()->getBool("MenuDark");
	}

	inline const char* getFramePath()
	{
		return isMenuDark() ? ":/frame_dark.png" : ":/frame.png";
	}

	inline unsigned int getTitleColor()
	{
		return isMenuDark() ? 0xFFFFFFFF : 0x555555FF;
	}

	inline unsigned int getPrimaryTextColor()
	{
		return isMenuDark() ? 0xE0E0E0FF : 0x777777FF;
	}

	inline unsigned int getSecondaryTextColor()
	{
		return isMenuDark() ? 0xBBBBBBFF : 0x888888FF;
	}
}

GuiScraperMulti::GuiScraperMulti(Window* window, const std::queue<ScraperSearchParams>& searches, bool approveResults) :
	GuiComponent(window),
	mBackground(window, getFramePath()),
	mGrid(window, Vector2i(1, 5)),
	mSearchQueue(searches),
	mBackgroundAllowed(!approveResults),
	mInBackground(false)
{
	assert(mSearchQueue.size());

	addChild(&mBackground);
	addChild(&mGrid);

	// ✅ Evitar que Window entre en sleep/idle mientras scrapea
	mWindow->setAllowSleep(false);

	PowerSaver::pause();
	mIsProcessing = true;

	mTotalGames = (int)mSearchQueue.size();
	mCurrentGame = 0;
	mTotalSuccessful = 0;
	mTotalSkipped = 0;

	// título
	mTitle = std::make_shared<TextComponent>(
		mWindow,
		es_translate("SCRAPING IN PROGRESS"),
		Font::get(FONT_SIZE_LARGE),
		getTitleColor(),
		ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 0), false, true);

	// sistema
	mSystem = std::make_shared<TextComponent>(
		mWindow,
		es_translate("SYSTEM"),
		Font::get(FONT_SIZE_MEDIUM),
		getPrimaryTextColor(),
		ALIGN_CENTER);
	mGrid.setEntry(mSystem, Vector2i(0, 1), false, true);

	// subtítulo
	mSubtitle = std::make_shared<TextComponent>(
		mWindow,
		"",
		Font::get(FONT_SIZE_SMALL),
		getSecondaryTextColor(),
		ALIGN_CENTER);
	mGrid.setEntry(mSubtitle, Vector2i(0, 2), false, true);

	// componente de búsqueda
	mSearchComp = std::make_shared<ScraperSearchComponent>(
		mWindow,
		approveResults ? ScraperSearchComponent::ALWAYS_ACCEPT_MATCHING_CRC
		               : ScraperSearchComponent::ALWAYS_ACCEPT_FIRST_RESULT);

	mSearchComp->setAcceptCallback(std::bind(&GuiScraperMulti::acceptResult, this, std::placeholders::_1));
	mSearchComp->setSkipCallback(std::bind(&GuiScraperMulti::skip, this));
	mSearchComp->setCancelCallback(std::bind(&GuiScraperMulti::finish, this));
	mSearchComp->setBackgroundErrorCallback(
		std::bind(&GuiScraperMulti::backgroundError, this, std::placeholders::_1));

	mGrid.setEntry(
		mSearchComp,
		Vector2i(0, 3),
		mSearchComp->getSearchType() != ScraperSearchComponent::ALWAYS_ACCEPT_FIRST_RESULT,
		true);

	std::vector<std::shared_ptr<ButtonComponent>> buttons;

	if (approveResults)
	{
		// INPUT
		buttons.push_back(std::make_shared<ButtonComponent>(
			mWindow,
			es_translate("INPUT"),
			es_translate("SEARCH"),
			[this] {
				mSearchComp->openInputScreen(mSearchQueue.front());
				mGrid.resetCursor();
			}));

		// SKIP
		buttons.push_back(std::make_shared<ButtonComponent>(
			mWindow,
			es_translate("SKIP"),
			es_translate("SKIP"),
			[this] {
				skip();
				mGrid.resetCursor();
			}));
	}

	// Only unattended automatic results can safely continue away from this
	// screen. Manual confirmation is never silently bypassed.
	if (mBackgroundAllowed)
	{
		buttons.push_back(std::make_shared<ButtonComponent>(
			mWindow, es_translate("BACKGROUND"), es_translate("BACKGROUND"),
			std::bind(&GuiScraperMulti::goBackground, this)));
	}

	// STOP
	buttons.push_back(std::make_shared<ButtonComponent>(
		mWindow,
		es_translate("STOP"),
		es_translate("STOP (PROGRESS SAVED)"),
		std::bind(&GuiScraperMulti::finish, this)));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 4), true, false);

	setSize(Renderer::getScreenWidth() * 0.95f, Renderer::getScreenHeight() * 0.849f);
	setPosition(
		(Renderer::getScreenWidth() - mSize.x()) / 2,
		(Renderer::getScreenHeight() - mSize.y()) / 2);

	doNextSearch();
}

GuiScraperMulti::~GuiScraperMulti()
{
	if (mWindow)
		mWindow->setAllowSleep(true);

	if (mIsProcessing)
		PowerSaver::resume();

	for (auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
		ViewController::get()->reloadGameListView(*it, false);
}

void GuiScraperMulti::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setRowHeightPerc(0, mTitle->getFont()->getLetterHeight() * 1.9725f / mSize.y(), false);
	mGrid.setRowHeightPerc(1, (mSystem->getFont()->getLetterHeight() + 2) / mSize.y(), false);
	mGrid.setRowHeightPerc(2, mSubtitle->getFont()->getHeight() * 1.75f / mSize.y(), false);
	mGrid.setRowHeightPerc(4, mButtonGrid->getSize().y() / mSize.y(), false);
	mGrid.setSize(mSize);
}

void GuiScraperMulti::update(int deltaTime)
{
	// Detached search: keep processing async HTTP and metadata callbacks,
	// but do not update the unrendered scraper window's decorative UI.
	if (mInBackground)
	{
		if (mSearchComp)
			mSearchComp->update(deltaTime);
		return;
	}
	GuiComponent::update(deltaTime);
}

bool GuiScraperMulti::input(InputConfig* config, Input input)
{
	if (mIsProcessing && input.value && config->isMappedTo("b", input))
	{
		if (mBackgroundAllowed)
			goBackground();
		else
			mWindow->setInfoPopup(new GuiInfoPopup(
				mWindow, es_translate("BACKGROUND REQUIRES AUTOMATIC RESULTS"), 4500));
		return true;
	}
	return GuiComponent::input(config, input);
}

void GuiScraperMulti::publishBackgroundProgress()
{
	if (!mInBackground)
		return;

	Window::BackgroundTaskStatus task;
	task.title = es_translate("SCRAPING IN PROGRESS");
	task.iconPath = ":/icons/scraper.png";
	task.current = static_cast<int>(mCurrentGame);
	task.total = static_cast<int>(mTotalGames);
	if (!mSearchQueue.empty())
	{
		const ScraperSearchParams& item = mSearchQueue.front();
		task.detail = item.system->getFullName();
		task.footer = Utils::FileSystem::getFileName(item.game->getPath());
	}
	mWindow->setBackgroundTask("native-scraper", task);
}

void GuiScraperMulti::goBackground()
{
	if (!mBackgroundAllowed || !mIsProcessing || mInBackground)
		return;

	if (!mWindow->parkGuiInBackground(this))
		return;

	mInBackground = true;
	mSearchComp->setBackgroundMode(true);
	publishBackgroundProgress();
}

void GuiScraperMulti::resumeForeground()
{
	if (!mInBackground)
		return;
	mInBackground = false;
	mSearchComp->setBackgroundMode(false);
	mWindow->clearBackgroundTask("native-scraper");
}

void GuiScraperMulti::backgroundError(const std::string& reason)
{
	// Never open a modal from a detached GUI; end safely with a persistent
	// error notification while keeping all previously saved game metadata.
	mBackgroundError = reason;
	finish();
}

// Draw a small native progress indicator beneath the game counter.
// This counter is exact: a game advances only after accepting or skipping.
void GuiScraperMulti::render(const Transform4x4f& parentTrans)
{
	GuiComponent::render(parentTrans);

	if (mTotalGames == 0)
		return;

	const float width = mSize.x() * 0.70f;
	const float x = (mSize.x() - width) * 0.5f;
	const float rowHeight = mGrid.getRowHeight(2);
	const float height = std::min(4.0f, rowHeight * 0.15f);
	const float y = mGrid.getRowHeight(0) + mGrid.getRowHeight(1) + rowHeight - height;
	const float portion = std::min(1.0f,
		static_cast<float>(mCurrentGame) / static_cast<float>(mTotalGames));

	Renderer::setMatrix(parentTrans * getTransform());
	Renderer::drawRect(x, y, width, height, 0x77777766, 0x77777766);
	Renderer::drawRect(x, y, width * portion, height, 0x4A90E2FF, 0x4A90E2FF);
}

void GuiScraperMulti::doNextSearch()
{
	if (mSearchQueue.empty())
	{
		finish();
		return;
	}

	mSystem->setText(Utils::String::toUpper(mSearchQueue.front().system->getFullName()));

	std::stringstream ss;
	const std::string gameLabel = es_translate("GAME");
	const std::string ofLabel   = es_translate("OF");

	ss << gameLabel << " " << (mCurrentGame + 1)
	   << " " << ofLabel << " " << mTotalGames
	   << " - "
	   << Utils::String::toUpper(Utils::FileSystem::getFileName(mSearchQueue.front().game->getPath()));

	mSubtitle->setText(ss.str());

	publishBackgroundProgress();
	mSearchComp->search(mSearchQueue.front());
}

void GuiScraperMulti::acceptResult(const ScraperSearchResult& result)
{
	ScraperSearchParams& search = mSearchQueue.front();

	importScrapedMetadata(search.game->metadata, result);
	updateGamelist(search.system);

	mSearchQueue.pop();
	mCurrentGame++;
	mTotalSuccessful++;
	doNextSearch();
}

void GuiScraperMulti::skip()
{
	mSearchQueue.pop();
	mCurrentGame++;
	mTotalSkipped++;
	doNextSearch();
}

void GuiScraperMulti::finish()
{
	if (!mIsProcessing)
		return;

	std::stringstream ss;

	if (mTotalSuccessful == 0)
	{
		ss << es_translate("NO GAMES WERE SCRAPED.");
	}
	else
	{
		if (mTotalSuccessful == 1)
			ss << mTotalSuccessful << " " << es_translate("GAME SCRAPED");
		else
			ss << mTotalSuccessful << " " << es_translate("GAMES SCRAPED");

		if (mTotalSkipped > 0)
		{
			ss << "\n";
			if (mTotalSkipped == 1)
				ss << mTotalSkipped << " " << es_translate("GAME SKIPPED");
			else
				ss << mTotalSkipped << " " << es_translate("GAMES SKIPPED");
		}
	}

	// STOP must also cancel pending HTTP/metadata requests, not only hide the UI.
	if (mSearchComp)
		mSearchComp->stop();

	mIsProcessing = false;
	PowerSaver::resume();

	if (mWindow)
		mWindow->setAllowSleep(true);

	if (mInBackground)
	{
		Window::BackgroundTaskStatus task;
		task.iconPath = ":/icons/scraper.png";
		task.title = es_translate("NATIVE SCRAPER FINISHED");
		task.current = static_cast<int>(mCurrentGame);
		task.total = static_cast<int>(mTotalGames);
		task.dismissOnInput = true;
		if (!mBackgroundError.empty())
		{
			task.state = Window::BackgroundTaskStatus::FAILED;
			task.title = es_translate("NATIVE SCRAPER FAILED");
			task.detail = mBackgroundError;
		}
		else if (!mSearchQueue.empty())
		{
			task.state = Window::BackgroundTaskStatus::CANCELLED;
			task.title = es_translate("NATIVE SCRAPER STOPPED");
			task.detail = ss.str();
		}
		else
		{
			task.state = Window::BackgroundTaskStatus::SUCCESS;
			task.detail = std::to_string(mTotalSuccessful) + " " +
			(mTotalSuccessful == 1 ? es_translate("GAME SCRAPED") : es_translate("GAMES SCRAPED"));
			if (mTotalSkipped)
				task.footer = std::to_string(mTotalSkipped) + " " +
					(mTotalSkipped == 1 ? es_translate("GAME SKIPPED") : es_translate("GAMES SKIPPED"));
		}
		if (task.footer.empty())
			task.footer = es_translate("PRESS ANY BUTTON TO DISMISS");
		mWindow->setBackgroundTask("native-scraper", task);
		mWindow->releaseBackgroundGuiAfterUpdate(this);
		return;
	}

	mWindow->pushGui(new GuiMsgBox(
		mWindow,
		ss.str(),
		es_translate("OK"),
		[this] { delete this; }));
}

std::vector<HelpPrompt> GuiScraperMulti::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts = mGrid.getHelpPrompts();
	if (mBackgroundAllowed && mIsProcessing)
		prompts.push_back(HelpPrompt("b", es_translate("BACKGROUND")));
	return prompts;
}
