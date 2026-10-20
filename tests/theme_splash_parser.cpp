// Native splash-view parser regression check.
#include "ThemeData.h"
#include <cassert>
#include <fstream>
#include <iostream>
#include <cstdio>

static void writeFile(const std::string& path, const std::string& text)
{
	std::ofstream file(path.c_str());
	file << text;
}

int main(int argc, char** argv)
{
	assert(argc == 2 || argc == 3);
	const std::string path = std::string(argv[1]) + "/splash-test.xml";

	writeFile(path,
		"<theme><formatVersion>7</formatVersion>"
		"<view name='splash'>"
		"<splash name='splash'><backgroundColor>030303</backgroundColor></splash>"
		"<image name='background'><path>./missing-is-allowed-in-parser.png</path></image>"
		"<image name='progressbar'><pos>0.05 0.86</pos><size>0.9 0.016</size>"
		"<color>6060607F</color></image>"
		"<image name='progressbar:active'><tile>true</tile><color>4a8d35</color></image>"
		"<text name='label' extra='true'><fontSize>0.023</fontSize>"
		"<color>000000</color><forceUppercase>true</forceUppercase></text>"
		"</view></theme>");

	ThemeData theme;
	theme.loadFile({}, path);
	assert(theme.hasView("splash"));

	auto splash = theme.getElement("splash", "splash", "splash");
	assert(splash && splash->get<unsigned int>("backgroundColor") == 0x030303FF);

	auto progress = theme.getElement("splash", "progressbar", "image");
	assert(progress);
	assert(progress->get<Vector2f>("pos") == Vector2f(0.05f, 0.86f));
	assert(progress->get<Vector2f>("size") == Vector2f(0.9f, 0.016f));

	auto active = theme.getElement("splash", "progressbar:active", "image");
	assert(active && active->get<bool>("tile"));

	auto label = theme.getElement("splash", "label", "text");
	assert(label && label->extra && label->get<bool>("forceUppercase"));

	if (argc == 3)
	{
		ThemeData external;
		external.loadFile({}, argv[2]);
		assert(external.hasView("splash"));
		std::cout << "External splash parsed successfully\n";
	}

	std::remove(path.c_str());
	std::cout << "Native splash ThemeData checks passed\n";
}
