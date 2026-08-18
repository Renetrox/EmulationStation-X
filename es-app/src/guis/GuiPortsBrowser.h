// es-app/src/guis/GuiPortsBrowser.h
#pragma once

#include "GuiComponent.h"
#include "components/ComponentList.h"
#include "components/TextComponent.h"
#include "components/NinePatchComponent.h"

#include <string>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <functional>
#include <memory>

class BusyComponent;

class GuiPortsBrowser : public GuiComponent
{
public:
    GuiPortsBrowser(Window* window);
    ~GuiPortsBrowser() override;

    bool input(InputConfig* config, Input input) override;
    void update(int deltaTime) override;
    void render(const Transform4x4f& parentTrans) override;

    std::vector<HelpPrompt> getHelpPrompts() override;
    void updateHelpPrompts();

private:
    struct PortEntry
    {
        std::string id;
        std::string description;
        std::string section;
        std::string flags;
        bool installed = false;
    };

    void setBusyVisible(bool visible);
    void showPopup(const std::string& msg, int durationMs);
    void rebuildList();
    void updateFooter();

    bool findRetroPieSetup();
    bool loadPorts();
    bool readModuleMetadata(const std::string& path, PortEntry& entry) const;
    bool canUseSudo() const;

    bool installOrUpdatePort(const PortEntry& port);
    bool removePort(const PortEntry& port);

    std::string sectionName(const std::string& section) const;
    int sectionRank(const std::string& section) const;

    int runCmd(const std::string& cmd);

    void startJob(const std::string& busyMsg,
                  std::function<bool()> fn,
                  const std::string& okMsg,
                  const std::string& failMsg,
                  bool refreshPorts);

    void updateJobProgress();
    void finishJobIfDone();

private:
    NinePatchComponent mFrame;
    ComponentList mList;
    TextComponent mHeader;
    TextComponent mSubHeader;
    TextComponent mFooter;

    std::string mRetroPieSetupDir;
    std::string mPackagesScript;
    std::string mPortsModulesDir;
    std::string mLogPath;

    std::vector<PortEntry> mPorts;
    int mLastSelectedIndex;

    float mInnerX;
    float mInnerY;
    float mInnerW;
    float mInnerH;

    std::unique_ptr<BusyComponent> mBusy;
    bool mBusyVisible;

    std::thread mJobThread;
    std::mutex mJobMutex;
    std::atomic<bool> mJobRunning;
    std::atomic<bool> mJobDone;
    bool mJobOk;
    bool mJobRefreshPorts;
    int mProgressPollMs;
    long long mJobLogStart;
    std::string mJobInitialLabel;
    std::string mJobStageLabel;
    std::string mJobPortId;
    std::string mJobDoneMsg;
};
