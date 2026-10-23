#pragma once
#ifndef ES_CORE_WINDOW_H
#define ES_CORE_WINDOW_H

#include "HelpPrompt.h"
#include "InputConfig.h"
#include "Settings.h"
#include "math/Vector2f.h"

#include <memory>
#include <functional>
#include <map>
#include <string>
#include <vector>

class ThemeSplash;
class SystemData;
class FileData;
class Font;
class GuiComponent;
class HelpComponent;
class ImageComponent;
class InputConfig;
class TextCache;
class Transform4x4f;
struct HelpStyle;

class Window
{
public:
	class ScreenSaver {
	public:
		virtual void startScreenSaver(SystemData* system = NULL) = 0;
		virtual void stopScreenSaver(bool toResume = false) = 0;
		virtual void renderScreenSaver() = 0;
		virtual bool allowSleep() = 0;
		virtual void update(int deltaTime) = 0;
		virtual bool isScreenSaverActive() = 0;
		virtual FileData* getCurrentGame() = 0;
		virtual void selectGame(bool launch) = 0;
		virtual bool inputDuringScreensaver(InputConfig* config, Input input) = 0;
	};

	class InfoPopup {
	public:
		virtual void render(const Transform4x4f& parentTrans) = 0;
		virtual void stop() = 0;
		virtual ~InfoPopup() {};
	};

	Window();
	~Window();

	void pushGui(GuiComponent* gui);
	void removeGui(GuiComponent* gui);
	GuiComponent* peekGui();
	inline int getGuiStackSize() { return (int)mGuiStack.size(); }

	void textInput(const char* text);
	void input(InputConfig* config, Input input);
	void update(int deltaTime);
	void render();

	bool init();
	void deinit();

	void normalizeNextUpdate();

	inline bool isSleeping() const { return mSleeping; }
	bool getAllowSleep();
	void setAllowSleep(bool sleep);

	void renderLoadingScreen(std::string text, float percent = -1, unsigned char opacity = 255);

	void closeLoadingScreen();

	// Small reusable status overlay for long-running frontend tasks. All calls
	// and monitors run on the UI thread; worker threads must marshal updates.
	struct BackgroundTaskStatus
	{
		enum State { RUNNING, SUCCESS, FAILED, CANCELLED };
		std::string title;
		std::string detail;
		std::string footer; // optional third line (stage, status)
		std::string iconPath; // legacy image icon fallback (still supported)
		std::string iconGlyph; // Font Awesome UTF-8 glyph; terminal state selects its own icon
		int current = -1; // <0: progress cannot be measured
		int total = 0;
		State state = RUNNING;
		// Keep terminal feedback visible until the next configured button press.
		// It is dismissed without consuming that input or changing screens.
		bool dismissOnInput = false;
	};

	void observeBackgroundTask(const std::string& id,
		const std::function<BackgroundTaskStatus()>& monitor);
	void setBackgroundTask(const std::string& id, const BackgroundTaskStatus& status);
	void clearBackgroundTask(const std::string& id);
	// A GUI doing async work can leave the stack while the Window keeps
	// updating it on the main thread; this avoids threads touching game data.
	bool parkGuiInBackground(GuiComponent* gui);
	bool hasBackgroundGui() const { return mBackgroundGui != nullptr; }
	GuiComponent* restoreBackgroundGui();
	void releaseBackgroundGuiAfterUpdate(GuiComponent* gui);
	// Theme Browser downloads own a GUI-scoped worker thread. Keep that GUI
	// alive independently of the native scraper while its job completes.
	bool parkThemeBrowserGui(GuiComponent* gui);
	bool hasParkedThemeBrowser() const { return mThemeBrowserBackgroundGui != nullptr; }
	GuiComponent* restoreThemeBrowserGui();
	void releaseThemeBrowserGuiAfterUpdate(GuiComponent* gui);
	void setBackgroundTaskOverlaySuppressed(bool suppress)
	{
		mBackgroundTaskOverlaySuppressed = suppress;
	}

	void renderHelpPromptsEarly();
	void setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style);

	void setScreenSaver(ScreenSaver* screenSaver) { mScreenSaver = screenSaver; }
	void setInfoPopup(InfoPopup* infoPopup) { delete mInfoPopup; mInfoPopup = infoPopup; }
	inline void stopInfoPopup() { if (mInfoPopup) mInfoPopup->stop(); };

	void startScreenSaver(SystemData* system = NULL);
	bool cancelScreenSaver();
	void renderScreenSaver();

	// Clock + network + bluetooth theme integration
	void applyClockTheme(const std::shared_ptr<class ThemeData>& theme);

private:
	void onSleep();
	void onWake();
	void updateBackgroundTasks(int deltaTime);
	void renderBackgroundTasks();

	bool isProcessing();

	std::unique_ptr<ThemeSplash> mThemeSplash;
	bool mSplashAttempted = false;
	HelpComponent* mHelp;
	ImageComponent* mBackgroundOverlay;
	ScreenSaver* mScreenSaver;
	InfoPopup* mInfoPopup;
	bool mRenderScreenSaver;

	std::vector<GuiComponent*> mGuiStack;

	std::vector<std::shared_ptr<Font>> mDefaultFonts;

	int mFrameTimeElapsed;
	int mFrameCountElapsed;
	int mAverageDeltaTime;

	std::unique_ptr<TextCache> mFrameDataText;

	bool mNormalizeNextUpdate;

	bool mAllowSleep;
	bool mSleeping;
	unsigned int mTimeSinceLastInput;

	bool mRenderedHelpPrompts;

	struct BackgroundTaskEntry
	{
		BackgroundTaskStatus status;
		std::function<BackgroundTaskStatus()> monitor;
		std::unique_ptr<TextCache> titleCache;
		std::unique_ptr<TextCache> detailCache;
		std::unique_ptr<TextCache> footerCache;
		std::unique_ptr<TextCache> glyphCache;
		std::shared_ptr<Font> glyphFont;
		std::string cachedGlyph;
		int glyphFontSize = 0;
		unsigned int glyphColor = 0;
		bool themeCached = false;
		bool cachedDark = false;
		int finishedMs = 0;
	};
	std::map<std::string, BackgroundTaskEntry> mBackgroundTasks;
	int mBackgroundTaskPollMs = 0;
	int mBackgroundTaskAnimMs = 0;
	int mBackgroundTaskShowMs = 0;
	bool mBackgroundTaskOverlaySuppressed = false;
	std::unique_ptr<GuiComponent> mBackgroundGui;
	bool mBackgroundGuiReleasePending = false;
	std::unique_ptr<GuiComponent> mThemeBrowserBackgroundGui;
	bool mThemeBrowserReleasePending = false;
	std::unique_ptr<ImageComponent> mBackgroundTaskIcon;
	std::string mBackgroundTaskIconPath;
	float mBackgroundTaskIconSize = 0.0f;

	// =========================
	// Clock overlay (theme-driven)
	// =========================

	bool mClockDefined = false;

	int mClockTimeAccum = 0;
	std::string mClockLastText;

	std::unique_ptr<TextCache> mClockTextCache;
	std::unique_ptr<TextCache> mClockOutlineCache;

	Vector2f mClockPos = { 0.98f, 0.05f };
	Vector2f mClockOrigin = { 1.0f, 0.0f };

	unsigned int mClockColor = 0xFFFFFFFF;
	unsigned int mClockOutlineColor = 0x000000FF;

	std::shared_ptr<Font> mClockFont;

	// =========================
	// Network overlay
	// =========================

	bool mNetworkDefined = false;
	bool mNetworkConnected = false;
	int mNetworkPollAccum = 0;

	Vector2f mNetworkPos = { 0.90f, 0.05f };
	Vector2f mNetworkOrigin = { 1.0f, 0.0f };
	Vector2f mNetworkSize = { 0.03f, 0.03f };

	std::string mNetworkPath = ":/icons/network.png";
	std::unique_ptr<ImageComponent> mNetworkIcon;

	// =========================
	// Bluetooth overlay
	// =========================

	bool mBluetoothDefined = false;
	bool mBluetoothConnected = false;
	int mBluetoothPollAccum = 0;

	Vector2f mBluetoothPos = { 0.86f, 0.05f };
	Vector2f mBluetoothOrigin = { 1.0f, 0.0f };
	Vector2f mBluetoothSize = { 0.03f, 0.03f };

	std::string mBluetoothPath = ":/icons/bluetooth.png";
	std::unique_ptr<ImageComponent> mBluetoothIcon;
};

#endif
