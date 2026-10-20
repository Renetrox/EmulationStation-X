// Standalone parser regression checks; no window or OpenGL context required.
#include "ThemeData.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdio>

static void write(const std::string& path, const std::string& text)
{
    std::ofstream file(path); file << text;
}

int main(int argc, char** argv)
{
    assert(argc == 2 || argc == 3);
    const std::string path = std::string(argv[1]) + "/splash-test.xml";
    const std::string header = "<theme><formatVersion>7</formatVersion>";
    ThemeData theme;
    write(path, header +
        "<view name='system'><unsupported name='ignored'/></view>"
        "<view name='splash'><splash name='splash'><backgroundColor>030303</backgroundColor></splash>"
        "<image name='progressbar, progressbar:active'><size>0.4 1</size><color>b616e4</color></image>"
        "<image name='overlay' extra='static'><maxSize>0.35</maxSize><minSize>1 1</minSize>"
        "<linearSmooth>true</linearSmooth><pulse>true</pulse><frameAnimation>true</frameAnimation></image>"
        "<text name='label'><color>FFFFFF</color><color ifSubset='x:y'>${missing}</color>"
        "<glowColor>222222</glowColor><glowOffset>1</glowOffset>"
        "<verticalAlignment>center</verticalAlignment></text></view></theme>");
    theme.loadFile({}, path, true);
    assert(theme.hasView("splash") && !theme.hasView("system"));
    assert(theme.getElement("splash", "splash", "splash")->get<unsigned int>("backgroundColor") == 0x030303FF);
    auto overlay = theme.getElement("splash", "overlay", "image");
    assert(overlay && overlay->extra && overlay->has("minSize"));
    assert(overlay->get<Vector2f>("maxSize") == Vector2f(0.35f, 0.35f));
    assert(!overlay->has("pulse") && !overlay->has("frameAnimation"));
    assert(theme.getElement("splash", "progressbar:active", "image")->get<Vector2f>("size") == Vector2f(0.4f, 1));
    assert(!theme.getElement("splash", "label", "text")->has("glowColor"));
    bool rejected = false;
    try { theme.loadFile({}, path); } catch (const ThemeException&) { rejected = true; }
    assert(rejected); // Normal theme parsing remains strict.
    write(path, header + "<view name='splash'><text name='label'><text>${missing}</text></text></view></theme>");
    rejected = false;
    try { theme.loadFile({}, path, true); } catch (const ThemeException&) { rejected = true; }
    assert(rejected);
    write(path, "<theme>");
    rejected = false;
    try { theme.loadFile({}, path, true); } catch (const ThemeException&) { rejected = true; }
    assert(rejected);
    write(path, header + "<include>./splash-test.xml</include></theme>");
    rejected = false;
    try { theme.loadFile({}, path, true); } catch (const ThemeException&) { rejected = true; }
    assert(rejected); // Cycles cannot overflow the stack.
    write(path, header + "<view name='splash'><text name='label'><text>OK</text></text></view></theme>");
    theme.loadFile({}, path, true);
    assert(theme.getElement("splash", "label", "text")->get<std::string>("text") == "OK");
    if (argc == 3)
    {
        theme.loadFile({}, argv[2], true);
        auto mask = theme.getElement("splash", "background_gamecc", "image");
        assert(mask && mask->extra && mask->get<float>("zIndex") == 10);
        assert(theme.getElement("splash", "progressbar:active", "image")->get<Vector2f>("size") == Vector2f(0.4f, 1));
        assert(theme.getElement("splash", "label", "text")->get<std::string>("verticalAlignment") == "center");
        std::cout << "Original Riescade splash parsed successfully\n";
    }
    std::remove(path.c_str());
    std::cout << "Static splash parser checks passed\n";
}
