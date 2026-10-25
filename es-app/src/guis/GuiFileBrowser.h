#pragma once

// ES-X native metadata file picker. Icons are rendered with the already
// bundled Font Awesome TTF; no new PNG/SVG artwork or file-manager dependency.
#include "GuiComponent.h"
#include "components/ComponentList.h"
#include "components/NinePatchComponent.h"
#include "components/TextComponent.h"

#include <functional>
#include <string>

class GuiFileBrowser : public GuiComponent
{
public:
    enum class MediaKind { IMAGE, VIDEO };

    GuiFileBrowser(Window* window,
                   const std::string& startDirectory,
                   const std::string& selectedPath,
                   MediaKind kind,
                   const std::function<void(const std::string&)>& selected,
                   const std::string& title);

    bool input(InputConfig* config, Input input) override;
    void update(int deltaTime) override;
    void render(const Transform4x4f& parentTrans) override;
    std::vector<HelpPrompt> getHelpPrompts() override;

private:
    void navigate(const std::string& directory);
    void addRow(const std::string& glyph, const std::string& name,
                const std::function<void()>& action, bool selected = false);
    void choose(const std::string& path);
    bool accepts(const std::string& path) const;

    NinePatchComponent mFrame;
    ComponentList mList;
    TextComponent mTitle;
    TextComponent mPath;
    TextComponent mFooter;

    MediaKind mKind;
    std::string mCurrentDirectory;
    std::string mSelectedPath;
    std::function<void(const std::string&)> mSelected;
    bool mDarkMenu;

    // Row callbacks execute inside ComponentList::input. Rebuilding the list
    // or deleting the picker from that callback invalidates the active row.
    // Apply requests only after ComponentList::input returns.
    std::string mPendingDirectory;
    std::string mPendingFile;
    bool mHasPendingChoice = false;
};
