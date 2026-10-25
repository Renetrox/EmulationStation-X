#include "guis/GuiFileBrowser.h"

#include "guis/CompactBrowserStyle.h"
#include "TaskGlyphs.h"
#include "LocaleES.h"
#include "Settings.h"
#include "Window.h"
#include "resources/Font.h"
#include "renderers/Renderer.h"
#include "utils/FileSystemUtil.h"
#include "utils/StringUtil.h"

#include <algorithm>
#include <vector>

namespace
{
    // Font Awesome 4 already ships in ES-X; no picture assets are introduced.
    const char* const FOLDER_ICON = u8"\uf07c";
    const char* const PARENT_ICON = u8"\uf062";
    const char* const IMAGE_ICON  = u8"\uf03e";
    const char* const VIDEO_ICON  = u8"\uf03d";
    const char* const FILE_ICON   = u8"\uf15b";

    std::string trFile(const std::string& key)
    {
        return LocaleES::get(key);
    }
}

GuiFileBrowser::GuiFileBrowser(Window* window,
                               const std::string& startDirectory,
                               const std::string& selectedPath,
                               MediaKind kind,
                               const std::function<void(const std::string&)>& selected,
                               const std::string& title)
    : GuiComponent(window)
    , mFrame(window)
    , mList(window)
    , mTitle(window, title, CompactBrowserStyle::titleFont(),
             Settings::getInstance()->getBool("MenuDark") ? 0xF3F3F3FF : 0x333333FF, ALIGN_CENTER)
    , mPath(window, "", CompactBrowserStyle::detailFont(),
            Settings::getInstance()->getBool("MenuDark") ? 0xB3BAC2FF : 0x606060FF, ALIGN_LEFT)
    , mFooter(window, "", CompactBrowserStyle::detailFont(),
              Settings::getInstance()->getBool("MenuDark") ? 0xB3BAC2FF : 0x606060FF, ALIGN_CENTER)
    , mKind(kind)
    , mSelectedPath(selectedPath.empty() ? "" : Utils::FileSystem::getCanonicalPath(selectedPath))
    , mSelected(selected)
    , mDarkMenu(Settings::getInstance()->getBool("MenuDark"))
{
    const float sw = static_cast<float>(Renderer::getScreenWidth());
    const float sh = static_cast<float>(Renderer::getScreenHeight());
    const float frameW = sw * 0.78f;
    const float frameH = sh * 0.83f;
    const float frameX = (sw - frameW) * 0.5f;
    const float frameY = (sh - frameH) * 0.5f;
    const float paddingX = sw * 0.019f;
    const float paddingY = sh * 0.025f;
    const float insideX = frameX + paddingX;
    const float insideY = frameY + paddingY;
    const float insideW = frameW - 2.0f * paddingX;
    const float insideH = frameH - 2.0f * paddingY;

    // Reuse ES-X's existing menu frame. All *file icons* are TTF glyphs.
    mFrame.setImagePath(mDarkMenu ? ":/frame_dark.png" : ":/frame.png");
    mFrame.setPosition(frameX, frameY);
    mFrame.setSize(frameW, frameH);

    mTitle.setPosition(insideX, insideY);
    mTitle.setSize(insideW, sh * 0.050f);

    mPath.setPosition(insideX, insideY + sh * 0.060f);
    mPath.setSize(insideW, sh * 0.038f);

    const float listTop = insideY + sh * 0.105f;
    const float footerHeight = sh * 0.040f;
    mList.setPosition(insideX, listTop);
    mList.setSize(insideW, std::max(1.0f, insideY + insideH - footerHeight - listTop));

    mFooter.setPosition(insideX, insideY + insideH - footerHeight);
    mFooter.setSize(insideW, footerHeight);
    mFooter.setText(trFile("SELECT") + " · " + trFile("BACK") +
                    " · " + trFile("PARENT FOLDER") + " · " + trFile("RESET"));

    const std::string initial = Utils::FileSystem::isDirectory(startDirectory) ?
        startDirectory : Utils::FileSystem::getHomePath();
    navigate(initial);
    mList.onFocusGained();
    mWindow->setHelpPrompts(getHelpPrompts(), HelpStyle());
}

bool GuiFileBrowser::accepts(const std::string& path) const
{
    const std::string ext = Utils::String::toLower(Utils::FileSystem::getExtension(path));
    if (mKind == MediaKind::VIDEO)
        return ext == ".mp4" || ext == ".mkv" || ext == ".avi" ||
               ext == ".webm" || ext == ".mov";

    return ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
           ext == ".gif" || ext == ".svg";
}

void GuiFileBrowser::addRow(const std::string& glyph, const std::string& name,
                            const std::function<void()>& action, bool selected)
{
    const float rowHeight = std::max(CompactBrowserStyle::titleFont()->getLetterHeight() + 12.0f,
                                     Renderer::getScreenHeight() * 0.052f);
    const unsigned int ink = mDarkMenu ? 0xEEEEEEFF : 0x404040FF;
    const unsigned int iconInk = mDarkMenu ? 0xC7D1DAFF : 0x526D88FF;

    ComponentListRow row;
    auto icon = std::make_shared<TextComponent>(
        mWindow, glyph, Font::get(CompactBrowserStyle::titleSize(), TaskGlyphs::FONT_PATH),
        iconInk, ALIGN_CENTER);
    icon->setSize(rowHeight, rowHeight);
    row.addElement(icon, false);

    auto label = std::make_shared<TextComponent>(
        mWindow, name, CompactBrowserStyle::titleFont(), ink, ALIGN_LEFT);
    label->setSize(0.0f, rowHeight);
    row.addElement(label, true);

    if (action)
        row.makeAcceptInputHandler(action);
    mList.addRow(row, selected);
}

void GuiFileBrowser::navigate(const std::string& directory)
{
    if (!Utils::FileSystem::isDirectory(directory))
        return;

    mCurrentDirectory = Utils::FileSystem::getCanonicalPath(directory);
    mPath.setText(mCurrentDirectory);
    mList.clear();

    if (mCurrentDirectory != "/" && !mCurrentDirectory.empty())
    {
        const std::string parent = Utils::FileSystem::getParent(mCurrentDirectory);
        if (parent != mCurrentDirectory && !parent.empty())
            addRow(PARENT_ICON, "..", [this, parent] { navigate(parent); });
    }

    const auto entries = Utils::FileSystem::getDirContent(mCurrentDirectory);
    std::vector<std::string> folders;
    std::vector<std::string> media;
    for (const auto& path : entries)
    {
        if (Utils::FileSystem::isHidden(path))
            continue;

        if (Utils::FileSystem::isDirectory(path))
            folders.push_back(path);
        else if (Utils::FileSystem::isRegularFile(path) && accepts(path))
            media.push_back(path);
    }

    const auto byName = [](const std::string& a, const std::string& b)
    {
        return Utils::String::toLower(Utils::FileSystem::getFileName(a)) <
               Utils::String::toLower(Utils::FileSystem::getFileName(b));
    };
    std::sort(folders.begin(), folders.end(), byName);
    std::sort(media.begin(), media.end(), byName);

    for (const auto& folder : folders)
    {
        addRow(FOLDER_ICON, Utils::FileSystem::getFileName(folder),
               [this, folder] { navigate(folder); });
    }

    const char* fileGlyph = mKind == MediaKind::VIDEO ? VIDEO_ICON : IMAGE_ICON;
    for (const auto& path : media)
    {
        addRow(fileGlyph, Utils::FileSystem::getFileName(path),
               [this, path] { choose(path); }, path == mSelectedPath);
    }

    if (folders.empty() && media.empty())
        addRow(FILE_ICON, trFile("NO COMPATIBLE FILES"), nullptr);
}

void GuiFileBrowser::choose(const std::string& path)
{
    if (!path.empty() && !Utils::FileSystem::isRegularFile(path))
        return;
    if (mSelected)
        mSelected(path);
    delete this;
}

bool GuiFileBrowser::input(InputConfig* config, Input input)
{
    if (input.value == 0)
        return mList.input(config, input);

    if (config->isMappedTo("b", input) || config->isMappedTo("back", input))
    {
        delete this;
        return true;
    }
    if (config->isMappedTo("y", input))
    {
        if (mCurrentDirectory != "/" && !mCurrentDirectory.empty())
            navigate(Utils::FileSystem::getParent(mCurrentDirectory));
        return true;
    }
    if (config->isMappedTo("x", input))
    {
        choose("");
        return true;
    }

    return mList.input(config, input);
}

void GuiFileBrowser::update(int deltaTime)
{
    GuiComponent::update(deltaTime);
    mFrame.update(deltaTime);
    mTitle.update(deltaTime);
    mPath.update(deltaTime);
    mList.update(deltaTime);
    mFooter.update(deltaTime);
}

void GuiFileBrowser::render(const Transform4x4f& parentTrans)
{
    const Transform4x4f trans = parentTrans * getTransform();
    Renderer::setMatrix(trans);
    mFrame.render(trans);
    mTitle.render(trans);
    mPath.render(trans);
    mList.render(trans);
    mFooter.render(trans);
    renderChildren(trans);
}

std::vector<HelpPrompt> GuiFileBrowser::getHelpPrompts()
{
    return {
        HelpPrompt("a", trFile("SELECT")),
        HelpPrompt("y", trFile("PARENT FOLDER")),
        HelpPrompt("x", trFile("RESET")),
        HelpPrompt("b", trFile("BACK"))
    };
}
