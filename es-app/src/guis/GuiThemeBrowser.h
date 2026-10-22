// es-app/src/guis/GuiThemeBrowser.h
#pragma once

#include "GuiComponent.h"

#include "components/ComponentList.h"
#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "components/NinePatchComponent.h"

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>
#include <unordered_map>

class BusyComponent;

class GuiThemeBrowser : public GuiComponent
{
public:
	GuiThemeBrowser(Window* window);
	~GuiThemeBrowser() override;

	bool input(InputConfig* config, Input input) override;
	void update(int deltaTime) override;
	void render(const Transform4x4f& parentTrans) override;

	// Help prompts
	std::vector<HelpPrompt> getHelpPrompts() override;
	void updateHelpPrompts();

private:
	// -----------------------------
	// ThemeEntry
	// -----------------------------
	struct ThemeEntry
	{
		std::string id;
		std::string title;
		std::string repo;
		std::string previewHint;
		std::string folder;
		std::string author;
	};

	// -----------------------------
	// Update state
	// -----------------------------
	enum class UpdateState
	{
		UNKNOWN = 0,
		UP_TO_DATE,
		UPDATE_AVAILABLE,
		ERROR
	};

private:
	// UI helpers
	void setBusyVisible(bool v);
	void showPopup(const std::string& msg, int durationMs);

	// Disk / catalog
	void loadThemes();
	void rebuildList();
	void updatePreview();
	void updateFooter();

	std::string getPreviewPath(const ThemeEntry& e) const;
	bool isInstalled(const ThemeEntry& e) const;

	// Jobs
	void startJob(const std::string& busyMsg, std::function<bool()> fn,
	              const std::string& okMsg, const std::string& failMsg,
	              bool refreshCatalogAfter);

	void finishJobIfDone();

	// Command helpers
	int  runCmd(const std::string& cmd);
	bool hasCommand(const std::string& cmd) const;

	// New: capture output
	bool runCmdCapture(const std::string& cmd, std::string& out) const;

	// Catalog ZIP
	bool updateCatalogZipSilent();

	// Install / uninstall
	bool downloadOrUpdateSilent(const ThemeEntry& e);
	bool uninstallThemeSilent(const ThemeEntry& e);

	// -----------------------------
	// Update check (NEW)
	// -----------------------------
	void startUpdateCheck();      // start/refresh check thread (non-blocking)
	void stopUpdateCheck();       // stop thread cleanly
	void updateCheckWorker();     // thread worker

	UpdateState getUpdateStateFor(const ThemeEntry& e) const;
	void        setUpdateStateFor(const ThemeEntry& e, UpdateState st);

	std::string getThemeLocalCommitPath(const ThemeEntry& e) const;
	std::string getThemeLocalRepoPath(const ThemeEntry& e) const;

	bool readTextFile(const std::string& path, std::string& out) const;
	bool writeTextFile(const std::string& path, const std::string& text) const;

	bool getInstalledCommit(const ThemeEntry& e, std::string& outCommit) const;
	bool saveInstalledInfo(const ThemeEntry& e, const std::string& repo, const std::string& commit) const;

	bool getRemoteHeadCommit(const std::string& repoUrl, std::string& outCommit) const;

private:
	// UI
	NinePatchComponent mFrame;
	ComponentList      mList;
	ImageComponent     mPreview;

	TextComponent mRepoLabel;
	TextComponent mHeader;
	TextComponent mSubtitle;
	TextComponent mFooter;

	// Paths
	std::string mCatalogDir;
	std::string mPreviewDir;
	std::string mThemesDir;

	// Data
	std::vector<ThemeEntry> mThemes;
	int mLastSelectedIndex;

	// Layout
	float mInnerX;
	float mInnerY;
	float mInnerW;
	float mInnerH;

	// Busy overlay
	std::unique_ptr<BusyComponent> mBusy;
	bool mBusyVisible;

	// Background job
	std::thread mJobThread;
	std::mutex  mJobMutex;
	std::atomic<bool> mJobRunning;
	std::atomic<bool> mJobDone;
	bool mJobOk;
	bool mJobRefreshCatalogAfter;
	std::string mJobDoneMsg;

	// Update check thread (NEW)
	std::thread mUpdateThread;
	std::atomic<bool> mUpdateStop;
	std::atomic<bool> mUpdateDirty; // when true -> rebuildList()
	mutable std::mutex mUpdateMutex;
	std::unordered_map<std::string, UpdateState> mUpdateStates; // key = folder
};
