#include "guis/GuiSkyscraperMenu.h"

#include "components/TextComponent.h"
#include "components/SwitchComponent.h"
#include "components/ImageComponent.h"
#include "guis/GuiInfoPopup.h"
#include "guis/GuiMsgBox.h"
#include "views/ViewController.h"
#include "SystemData.h"
#include "Settings.h"
#include "Log.h"
#include "resources/Font.h"
#include "renderers/Renderer.h"
#include "LocaleES.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <cctype>

#define SKY_SCRIPT "$HOME/.emulationstation/scripts/skyscraper-esx.sh"
#define SKY_STATUS_FILE "/tmp/esx-skyscraper/status"
#define SKY_PID_FILE "/tmp/esx-skyscraper/pid"
#define SKY_LOG_FILE "/tmp/esx-skyscraper/log"

namespace
{
	inline unsigned int getMenuTextColor()
	{
		return Settings::getInstance()->getBool("MenuDark") ? 0xFFFFFFFF : 0x777777FF;
	}

	inline unsigned int getPanelColor()
	{
		return Settings::getInstance()->getBool("MenuDark") ? 0x101010EE : 0xF0F0F0EE;
	}

	inline unsigned int getPanelBorderColor()
	{
		return 0x4A90E2FF;
	}

	inline std::string quote(const std::string& s)
	{
		return std::string("\"") + s + "\"";
	}

	inline std::string tr(const std::string& key)
	{
		return es_translate(key);
	}

	inline bool fileExists(const std::string& path)
	{
		struct stat st;
		return stat(path.c_str(), &st) == 0;
	}

	inline std::string readFirstLine(const std::string& path)
	{
		std::ifstream file(path.c_str());
		if (!file.is_open())
			return "";

		std::string line;
		std::getline(file, line);
		return line;
	}

	inline bool startsWith(const std::string& value, const std::string& prefix)
	{
		return value.size() >= prefix.size() &&
			   value.compare(0, prefix.size(), prefix) == 0;
	}

	inline int getCurrentSystemGameCountSafe()
	{
		auto vc = ViewController::get();
		if (!vc)
			return 0;

		auto state = vc->getState();
		SystemData* sys = state.getSystem();
		if (!sys)
			return 0;

		const int count = sys->getGameCount();
		return count > 0 ? count : 0;
	}

	inline bool parseProgressFromLine(const std::string& line, int& current, int& total)
	{
		size_t hashPos = line.find('#');
		if (hashPos == std::string::npos)
			return false;

		size_t i = hashPos + 1;

		if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i])))
			return false;

		int a = 0;
		while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])))
		{
			a = (a * 10) + (line[i] - '0');
			++i;
		}

		if (i >= line.size() || line[i] != '/')
			return false;

		++i;

		if (i >= line.size() || !std::isdigit(static_cast<unsigned char>(line[i])))
			return false;

		int b = 0;
		while (i < line.size() && std::isdigit(static_cast<unsigned char>(line[i])))
		{
			b = (b * 10) + (line[i] - '0');
			++i;
		}

		if (a <= 0 || b <= 0)
			return false;

		current = a;
		total = b;
		return true;
	}

	inline bool readProgressFromLog(const std::string& path, int& current, int& total)
	{
		std::ifstream file(path.c_str());
		if (!file.is_open())
			return false;

		std::string line;
		bool found = false;
		int lastCurrent = 0;
		int lastTotal = 0;

		while (std::getline(file, line))
		{
			int c = 0;
			int t = 0;
			if (parseProgressFromLine(line, c, t))
			{
				lastCurrent = c;
				lastTotal = t;
				found = true;
			}
		}

		if (found)
		{
			current = lastCurrent;
			total = lastTotal;
		}

		return found;
	}

	inline std::string normalizeInterfaceLanguage(const std::string& rawLang)
	{
		if (rawLang.empty())
			return "en";

		std::string lang = rawLang;
		for (size_t i = 0; i < lang.size(); ++i)
			lang[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(lang[i])));

		if (lang.size() >= 2)
			lang = lang.substr(0, 2);

		if (lang == "es" || lang == "en" || lang == "fr" || lang == "de" ||
			lang == "pt" || lang == "it" || lang == "nl" || lang == "ja" || lang == "ru")
			return lang;

		return "en";
	}

	inline std::string normalizeBoolString(const std::string& rawValue, bool defaultValue)
	{
		if (rawValue.empty())
			return defaultValue ? "true" : "false";

		std::string value = rawValue;
		for (size_t i = 0; i < value.size(); ++i)
			value[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(value[i])));

		if (value == "1" || value == "true" || value == "yes" || value == "on")
			return "true";

		if (value == "0" || value == "false" || value == "no" || value == "off")
			return "false";

		return defaultValue ? "true" : "false";
	}

	inline std::string getConfiguredSkyscraperLanguage()
	{
		Settings* settings = Settings::getInstance();

		std::string lang = settings->getString("SkyscraperLanguage");
		if (lang.empty())
			lang = settings->getString("Language");

		return normalizeInterfaceLanguage(lang);
	}

	inline std::string getConfiguredSkyscraperVideos()
	{
		Settings* settings = Settings::getInstance();
		std::string videos = settings->getString("SkyscraperVideos");
		return normalizeBoolString(videos, true);
	}

	inline std::string getConfiguredSkyscraperOnlyMissing()
	{
		Settings* settings = Settings::getInstance();
		std::string onlyMissing = settings->getString("SkyscraperOnlyMissing");
		return normalizeBoolString(onlyMissing, true);
	}

	inline std::string getSkyscraperStatus()
	{
		return readFirstLine(SKY_STATUS_FILE);
	}

	inline bool isSkyscraperJobActive(const std::string& status)
	{
		if (status.empty())
			return false;

		if (!fileExists(SKY_PID_FILE) &&
			!startsWith(status, "done:") &&
			!startsWith(status, "error:") &&
			status != "stopped")
		{
			return false;
		}

		return startsWith(status, "starting:") ||
			   startsWith(status, "running:");
	}

	class GuiSkyscraperProgress : public GuiComponent
	{
	public:
		GuiSkyscraperProgress(Window* window, const std::string& action, const std::string& system, int total)
			: GuiComponent(window)
			, mAction(action)
			, mSystem(system)
			, mLine1(window, "", Font::get(FONT_SIZE_SMALL), getMenuTextColor())
			, mLine2(window, "", Font::get(FONT_SIZE_SMALL), getMenuTextColor())
			, mLine3(window, "", Font::get(FONT_SIZE_SMALL), getMenuTextColor())
			, mIcon(window)
			, mFakeCurrent(0)
			, mFakeTotal(total > 0 ? total : 0)
			, mDots(0)
			, mTickAccumulator(0)
			, mStartupGraceMs(0)
			, mNoPidMs(0)
			, mFinishDelayMs(0)
			, mFinished(false)
		{
			setSize(Renderer::getScreenWidth() * 0.28f, Renderer::getScreenHeight() * 0.13f);
			setPosition(
				Renderer::getScreenWidth() * 0.69f,
				Renderer::getScreenHeight() * 0.67f
			);

			mLine1.setPosition(18, 6);
			mLine2.setPosition(18, 34);
			mLine3.setPosition(18, 60);

			const float iconSize = Renderer::getScreenHeight() * 0.052f;
			mIcon.setImage(":/icons/scraper.png");
			mIcon.setResize(iconSize, iconSize);
			mIcon.setPosition(mSize.x() - iconSize - 12.0f, 10.0f);

			addChild(&mLine1);
			addChild(&mLine2);
			addChild(&mLine3);
			addChild(&mIcon);

			refreshText("starting");
		}

		bool input(InputConfig* config, Input input) override
		{
			if (GuiComponent::input(config, input))
				return true;

			if (config->isMappedTo("b", input) && input.value && !mFinished)
			{
				mWindow->pushGui(new GuiMsgBox(
					mWindow,
					tr("SCRAPING IS RUNNING. DO YOU WANT TO STOP IT?"),
					tr("YES"),
					[this]
					{
						stopJob();
						mWindow->setInfoPopup(new GuiInfoPopup(
							mWindow,
							tr("STOPPING SKYSCRAPER..."),
							3000
						));
					},
					tr("NO"),
					nullptr
				));
				return true;
			}

			return false;
		}

		void update(int deltaTime) override
		{
			GuiComponent::update(deltaTime);

			if (mFinished)
			{
				mFinishDelayMs += deltaTime;
				if (mFinishDelayMs >= 2200)
					delete this;
				return;
			}

			mTickAccumulator += deltaTime;
			mStartupGraceMs += deltaTime;

			if (mTickAccumulator < 900)
				return;

			mTickAccumulator = 0;
			pollState(900);
		}

		void render(const Transform4x4f& parentTrans) override
		{
			Transform4x4f trans = parentTrans * getTransform();
			Renderer::setMatrix(trans);

			const unsigned int panelColor = getPanelColor();
			const unsigned int borderColor = getPanelBorderColor();

			Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), panelColor, panelColor);
			Renderer::drawRect(0.0f, 0.0f, mSize.x(), 2.0f, borderColor, borderColor);
			Renderer::drawRect(0.0f, mSize.y() - 2.0f, mSize.x(), 2.0f, borderColor, borderColor);
			Renderer::drawRect(0.0f, 0.0f, 2.0f, mSize.y(), borderColor, borderColor);
			Renderer::drawRect(mSize.x() - 2.0f, 0.0f, 2.0f, mSize.y(), borderColor, borderColor);

			GuiComponent::render(parentTrans);
		}

		std::vector<HelpPrompt> getHelpPrompts() override
		{
			if (mFinished)
				return {};

			return { { "b", tr("STOP") } };
		}

	private:
		void pollState(int stepMs)
		{
			const std::string status = readFirstLine(SKY_STATUS_FILE);
			const bool pidExists = fileExists(SKY_PID_FILE);

			int logCurrent = 0;
			int logTotal = 0;
			if (readProgressFromLog(SKY_LOG_FILE, logCurrent, logTotal))
			{
				mFakeCurrent = logCurrent;
				mFakeTotal = logTotal;
			}

			mDots = (mDots + 1) % 4;

			if (startsWith(status, "done:"))
			{
				if (mFakeCurrent < mFakeTotal)
					mFakeCurrent = mFakeTotal;

				mFinished = true;
				refreshText("done");
				mWindow->setInfoPopup(new GuiInfoPopup(
					mWindow,
					tr("SKYSCRAPER FINISHED: ") + mSystem,
					5000
				));
				return;
			}

			if (startsWith(status, "error:"))
			{
				mFinished = true;
				refreshText("error");
				mWindow->setInfoPopup(new GuiInfoPopup(
					mWindow,
					tr("SKYSCRAPER FAILED: ") + mSystem,
					5000
				));
				return;
			}

			if (status == "stopped")
			{
				mFinished = true;
				refreshText("stopped");
				mWindow->setInfoPopup(new GuiInfoPopup(
					mWindow,
					tr("SKYSCRAPER STOPPED"),
					4000
				));
				return;
			}

			if (startsWith(status, "running:"))
			{
				mNoPidMs = 0;
				refreshText(status);
				return;
			}

			if (startsWith(status, "starting:"))
			{
				mNoPidMs = 0;
				refreshText("starting");
				return;
			}

			if (!pidExists)
				mNoPidMs += stepMs;
			else
				mNoPidMs = 0;

			if (mStartupGraceMs < 12000)
			{
				refreshText("starting");
				return;
			}

			if (mNoPidMs > 15000)
			{
				mFinished = true;
				refreshText("error");
				mWindow->setInfoPopup(new GuiInfoPopup(
					mWindow,
					tr("SKYSCRAPER FAILED: ") + mSystem,
					5000
				));
				return;
			}

			refreshText("starting");
		}

		void refreshText(const std::string& status)
		{
			std::string dots(mDots, '.');
			if (dots.empty())
				dots = ".";

			std::ostringstream line1;
			line1 << mFakeCurrent << "/" << mFakeTotal << " " << tr("SCRAPING") << dots;

			std::string line2 = tr("SYSTEM") + ": " + mSystem;
			std::string line3;

			if (status == "done")
				line3 = tr("COMPLETED");
			else if (status == "error")
				line3 = tr("ERROR");
			else if (status == "stopped")
				line3 = tr("STOPPED");
			else if (startsWith(status, "running:gather:") || mAction == "gather")
				line3 = tr("GATHERING RESOURCES") + dots;
			else if (startsWith(status, "running:generate:") || mAction == "generate")
				line3 = tr("GENERATING GAMELIST") + dots;
			else
				line3 = tr("PLEASE WAIT") + dots;

			mLine1.setText(line1.str());
			mLine2.setText(line2);
			mLine3.setText(line3);
		}

		void stopJob()
		{
			const std::string cmd = std::string(SKY_SCRIPT) + " stop";
			LOG(LogInfo) << "Stopping Skyscraper: " << cmd;
			std::system(cmd.c_str());
		}

	private:
		std::string mAction;
		std::string mSystem;

		TextComponent mLine1;
		TextComponent mLine2;
		TextComponent mLine3;
		ImageComponent mIcon;

		int mFakeCurrent;
		int mFakeTotal;
		int mDots;
		int mTickAccumulator;
		int mStartupGraceMs;
		int mNoPidMs;
		int mFinishDelayMs;
		bool mFinished;
	};
}

GuiSkyscraperMenu::GuiSkyscraperMenu(Window* window)
	: GuiComponent(window)
	, mMenu(window, es_translate("SKYSCRAPER").c_str())
{
	std::shared_ptr<Font> font = Font::get(FONT_SIZE_MEDIUM);
	const unsigned int textColor = getMenuTextColor();

	{
		ComponentListRow row;
		auto txt = std::make_shared<TextComponent>(window, tr("GATHER RESOURCES"), font, textColor);
		row.addElement(txt, true);
		row.makeAcceptInputHandler([this] { runGather(); });
		mMenu.addRow(row);
	}

	{
		ComponentListRow row;
		auto txt = std::make_shared<TextComponent>(window, tr("GENERATE GAMELIST"), font, textColor);
		row.addElement(txt, true);
		row.makeAcceptInputHandler([this] { runGenerate(); });
		mMenu.addRow(row);
	}

	{
		auto onlyMissing = std::make_shared<SwitchComponent>(window);
		onlyMissing->setState(getConfiguredSkyscraperOnlyMissing() == "true");

		ComponentListRow row;
		row.addElement(std::make_shared<TextComponent>(window, tr("ONLY MISSING"), font, textColor), true);
		row.addElement(onlyMissing, false);
		row.makeAcceptInputHandler([onlyMissing]
		{
			onlyMissing->setState(!onlyMissing->getState());
			Settings::getInstance()->setString("SkyscraperOnlyMissing", onlyMissing->getState() ? "true" : "false");
			Settings::getInstance()->saveFile();
		});
		mMenu.addRow(row);
	}

	{
		auto videos = std::make_shared<SwitchComponent>(window);
		videos->setState(getConfiguredSkyscraperVideos() == "true");

		ComponentListRow row;
		row.addElement(std::make_shared<TextComponent>(window, tr("VIDEOS"), font, textColor), true);
		row.addElement(videos, false);
		row.makeAcceptInputHandler([videos]
		{
			videos->setState(!videos->getState());
			Settings::getInstance()->setString("SkyscraperVideos", videos->getState() ? "true" : "false");
			Settings::getInstance()->saveFile();
		});
		mMenu.addRow(row);
	}

	{
		ComponentListRow row;
		auto txt = std::make_shared<TextComponent>(window, tr("OPEN LOG"), font, textColor);
		row.addElement(txt, true);
		row.makeAcceptInputHandler([this] { openLog(); });
		mMenu.addRow(row);
	}

	addChild(&mMenu);

	setSize(mMenu.getSize());
	setPosition(
		(Renderer::getScreenWidth() - mSize.x()) / 2,
		(Renderer::getScreenHeight() - mSize.y()) / 2
	);
}

bool GuiSkyscraperMenu::input(InputConfig* config, Input input)
{
	if (GuiComponent::input(config, input))
		return true;

	if (config->isMappedTo("b", input) && input.value)
	{
		delete this;
		return true;
	}

	return false;
}

std::string GuiSkyscraperMenu::getCurrentSystem()
{
	auto vc = ViewController::get();
	if (!vc)
		return "";

	auto state = vc->getState();
	SystemData* sys = state.getSystem();
	if (!sys)
		return "";

	return sys->getName();
}

std::string GuiSkyscraperMenu::getCurrentLanguage()
{
	return getConfiguredSkyscraperLanguage();
}

int GuiSkyscraperMenu::launchScript(const std::string& action)
{
	const std::string system = getCurrentSystem();
	const std::string lang = getCurrentLanguage();
	const std::string videos = getConfiguredSkyscraperVideos();
	const std::string onlyMissing = getConfiguredSkyscraperOnlyMissing();

	if (system.empty())
	{
		LOG(LogError) << "Skyscraper launch aborted: current system is empty";
		return 1;
	}

	const std::string cmd =
		std::string(SKY_SCRIPT) +
		" " + quote(action) +
		" " + quote(system) +
		" " + quote(lang) +
		" \"\" " + quote(videos) +
		" " + quote(onlyMissing);

	LOG(LogInfo) << "Launching Skyscraper: " << cmd;

	return std::system(cmd.c_str());
}

void GuiSkyscraperMenu::runGather()
{
	const std::string system = getCurrentSystem();
	const int totalGames = getCurrentSystemGameCountSafe();
	const std::string status = getSkyscraperStatus();

	if (system.empty())
	{
		mWindow->setInfoPopup(new GuiInfoPopup(
			mWindow,
			tr("SKYSCRAPER GATHER FAILED: CURRENT SYSTEM NOT FOUND"),
			5000
		));
		delete this;
		return;
	}

	if (isSkyscraperJobActive(status))
	{
		mWindow->setInfoPopup(new GuiInfoPopup(
			mWindow,
			tr("SKYSCRAPER IS ALREADY RUNNING"),
			4000
		));
		mWindow->pushGui(new GuiSkyscraperProgress(mWindow, "gather", system, totalGames));
		delete this;
		return;
	}

	mWindow->pushGui(new GuiMsgBox(
		mWindow,
		tr("SKYSCRAPER_CONFIRM"),
		tr("CONTINUE"),
		[this, system, totalGames]
		{
			const int ret = launchScript("start-gather");

			if (ret == 0)
			{
				mWindow->pushGui(new GuiSkyscraperProgress(mWindow, "gather", system, totalGames));
				delete this;
			}
			else
			{
				mWindow->setInfoPopup(new GuiInfoPopup(
					mWindow,
					tr("SKYSCRAPER GATHER FAILED: ") + system,
					5000
				));
			}
		},
		tr("CANCEL"),
		nullptr
	));
}

void GuiSkyscraperMenu::runGenerate()
{
	const std::string system = getCurrentSystem();
	const int totalGames = getCurrentSystemGameCountSafe();
	const std::string status = getSkyscraperStatus();

	if (system.empty())
	{
		mWindow->setInfoPopup(new GuiInfoPopup(
			mWindow,
			tr("SKYSCRAPER GENERATE FAILED: CURRENT SYSTEM NOT FOUND"),
			5000
		));
		delete this;
		return;
	}

	if (isSkyscraperJobActive(status))
	{
		mWindow->setInfoPopup(new GuiInfoPopup(
			mWindow,
			tr("SKYSCRAPER IS ALREADY RUNNING"),
			4000
		));
		mWindow->pushGui(new GuiSkyscraperProgress(mWindow, "generate", system, totalGames));
		delete this;
		return;
	}

	const int ret = launchScript("start-generate");

	if (ret == 0)
	{
		mWindow->pushGui(new GuiSkyscraperProgress(mWindow, "generate", system, totalGames));
		delete this;
	}
	else
	{
		mWindow->setInfoPopup(new GuiInfoPopup(
			mWindow,
			tr("SKYSCRAPER GENERATE FAILED: ") + system,
			5000
		));
	}
}

void GuiSkyscraperMenu::openLog()
{
	std::string msg = tr("LOG: /TMP/ESX-SKYSCRAPER/LOG");
	const std::string status = getSkyscraperStatus();

	if (!status.empty())
		msg += " | " + tr("STATUS") + ": " + status;

	mWindow->setInfoPopup(new GuiInfoPopup(
		mWindow,
		msg,
		4000
	));
}

HelpStyle GuiSkyscraperMenu::getHelpStyle()
{
	HelpStyle style;

	auto vc = ViewController::get();
	if (vc && vc->getState().getSystem())
		style.applyTheme(vc->getState().getSystem()->getTheme(), "system");

	return style;
}

std::vector<HelpPrompt> GuiSkyscraperMenu::getHelpPrompts()
{
	return {
		{ "up/down", tr("CHOOSE") },
		{ "a", tr("SELECT") },
		{ "b", tr("BACK") }
	};
}
