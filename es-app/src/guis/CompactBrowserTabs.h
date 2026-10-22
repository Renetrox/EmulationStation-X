// Lightweight Tarave-inspired X-TRAS tabs.
// Presentation only: the browser retains ownership of filtering and input.
#pragma once

#include "GuiComponent.h"
#include "components/TextComponent.h"
#include "resources/Font.h"

#include <algorithm>
#include <string>

class CompactBrowserTabs : public GuiComponent
{
public:
    CompactBrowserTabs(Window* window,
                       const std::string& gamesLabel,
                       const std::string& portsLabel)
        : GuiComponent(window)
        , mGames(window, gamesLabel, Font::get(FONT_SIZE_SMALL), 0xD2D6DDFF, ALIGN_CENTER)
        , mPorts(window, portsLabel, Font::get(FONT_SIZE_SMALL), 0xD2D6DDFF, ALIGN_CENTER)
        , mGamesAvailable(false)
        , mPortsAvailable(false)
        , mActiveTab(0)
        , mTabWidth(0.0f)
        , mStartX(0.0f)
    {
    }

    void setAvailable(bool games, bool ports)
    {
        if (games == mGamesAvailable && ports == mPortsAvailable)
            return;
        mGamesAvailable = games;
        mPortsAvailable = ports;
        onSizeChanged();
    }

    void setActive(int tab)
    {
        mActiveTab = tab;
        mGames.setColor(tab == 0 ? 0xFFFFFFFF : 0x89939FFF);
        mPorts.setColor(tab == 1 ? 0xFFFFFFFF : 0x89939FFF);
    }

    void onSizeChanged() override
    {
        const int visibleCount = (mGamesAvailable ? 1 : 0) +
                                 (mPortsAvailable ? 1 : 0);
        if (!visibleCount)
            return;

        // Central, compact tabs instead of very wide menu buttons.
        mTabWidth = std::min(mSize.x() / visibleCount,
                             std::max(135.0f, Renderer::getScreenHeight() * 0.32f));
        mStartX = (mSize.x() - mTabWidth * visibleCount) * 0.5f;

        float x = mStartX;
        if (mGamesAvailable)
        {
            mGames.setPosition(x, 0.0f);
            mGames.setSize(mTabWidth, mSize.y() - 6.0f);
            x += mTabWidth;
        }
        if (mPortsAvailable)
        {
            mPorts.setPosition(x, 0.0f);
            mPorts.setSize(mTabWidth, mSize.y() - 6.0f);
        }
    }

    void render(const Transform4x4f& parentTrans) override
    {
        if (!mGamesAvailable && !mPortsAvailable)
            return;

        Transform4x4f trans = parentTrans * getTransform();
        Renderer::setMatrix(trans);

        // Tarave-like thin bottom rule and selected-tab underline.
        Renderer::drawRect(0.0f, mSize.y() - 2.0f, mSize.x(), 1.0f,
                           0x56616F88, 0x56616F88);

        const float activeX = mStartX +
            ((mActiveTab == 1 && mGamesAvailable) ? mTabWidth : 0.0f);
        Renderer::drawRect(activeX + 8.0f, mSize.y() - 4.0f,
                           std::max(0.0f, mTabWidth - 16.0f), 3.0f,
                           0x429BDFFF, 0x429BDFFF);

        if (mGamesAvailable)
            mGames.render(trans);
        if (mPortsAvailable)
            mPorts.render(trans);
    }

private:
    TextComponent mGames;
    TextComponent mPorts;
    bool mGamesAvailable;
    bool mPortsAvailable;
    int mActiveTab;
    float mTabWidth;
    float mStartX;
};
