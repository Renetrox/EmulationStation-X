#pragma once

#include <memory>
#include <string>
#include <vector>

class GuiComponent;
class TextComponent;
class Window;

// Static theme splash. Owned only for the duration of a loading operation.
class ThemeSplash
{
public:
    static std::unique_ptr<ThemeSplash> create(Window* window);
    ~ThemeSplash();
    void render(const std::string& text, float percent, unsigned char opacity);

private:
    explicit ThemeSplash(Window* window);
    std::vector<std::unique_ptr<GuiComponent>> mElements;
    TextComponent* mLabel = nullptr;
    GuiComponent* mInactive = nullptr;
    GuiComponent* mActive = nullptr;
    unsigned int mBackgroundColor = 0x000000FF;
};
