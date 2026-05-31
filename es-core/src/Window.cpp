#include "Window.h"

#include "components/HelpComponent.h"
#include "components/ImageComponent.h"
#include "resources/Font.h"
#include "resources/TextureResource.h"
#include "ThemeData.h"
#include "Log.h"
#include "Scripting.h"

#include <algorithm>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <fstream>
#include <cstring>
#include <map>

#ifndef WIN32
#include <ifaddrs.h>
#include <net/if.h>
#include <arpa/inet.h>
#include <dirent.h>
#endif

#ifdef WIN32
#include <SDL_events.h>
#endif

#ifndef WIN32
static bool hasActiveNetworkConnection()
{
	struct ifaddrs* ifaddr = nullptr;
	if (getifaddrs(&ifaddr) == -1)
		return false;

	bool connected = false;

	for (struct ifaddrs* ifa = ifaddr; ifa != nullptr; ifa = ifa->ifa_next)
	{
		if (!ifa->ifa_addr)
			continue;

		// ignore loopback
		if (ifa->ifa_flags & IFF_LOOPBACK)
			continue;

		// interface must be up
		if (!(ifa->ifa_flags & IFF_UP))
			continue;

		const int family = ifa->ifa_addr->sa_family;
		if (family == AF_INET || family == AF_INET6)
		{
			connected = true;
			break;
		}
	}

	freeifaddrs(ifaddr);
	return connected;
}
#else
static bool hasActiveNetworkConnection()
{
	return false;
}
#endif

#ifndef WIN32
static bool hasActiveBluetoothConnection()
{
	// ES-X:
	// Detector liviano y seguro para overlay Bluetooth.
	// No ejecuta comandos externos, no usa bluetoothctl y no depende
	// directamente de BlueZ. Solo revisa sysfs:
	//
	// - /sys/class/bluetooth/hci*
	// - /sys/class/rfkill/rfkill*/type == bluetooth
	// - /sys/class/rfkill/rfkill*/state == 1
	//
	// Si existe hci* y no hay rfkill bluetooth, se asume activo.
	// Si existe rfkill bluetooth, solo se muestra si state == 1.

	bool hasHci = false;

	DIR* btDir = opendir("/sys/class/bluetooth");
	if (btDir)
	{
		struct dirent* entry = nullptr;

		while ((entry = readdir(btDir)) != nullptr)
		{
			if (std::strncmp(entry->d_name, "hci", 3) == 0)
			{
				hasHci = true;
				break;
			}
		}

		closedir(btDir);
	}

	if (!hasHci)
		return false;

	bool foundBluetoothRfkill = false;
	bool bluetoothUnblocked = false;

	DIR* rfkillDir = opendir("/sys/class/rfkill");
	if (rfkillDir)
	{
		struct dirent* entry = nullptr;

		while ((entry = readdir(rfkillDir)) != nullptr)
		{
			if (std::strncmp(entry->d_name, "rfkill", 6) != 0)
				continue;

			const std::string base = std::string("/sys/class/rfkill/") + entry->d_name;

			std::ifstream typeFile(base + "/type");
			std::string type;

			if (!typeFile.good())
				continue;

			std::getline(typeFile, type);

			if (type != "bluetooth")
				continue;

			foundBluetoothRfkill = true;

			std::ifstream stateFile(base + "/state");
			std::string state;

			if (!stateFile.good())
				continue;

			std::getline(stateFile, state);

			// state 1 = unblocked / active
			if (state == "1")
			{
				bluetoothUnblocked = true;
				break;
			}
		}

		closedir(rfkillDir);
	}

	if (foundBluetoothRfkill)
		return bluetoothUnblocked;

	return true;
}
#else
static bool hasActiveBluetoothConnection()
{
	return false;
}
#endif

Window::Window()
	: mNormalizeNextUpdate(false)
	, mFrameTimeElapsed(0)
	, mFrameCountElapsed(0)
	, mAverageDeltaTime(10)
	, mAllowSleep(true)
	, mSleeping(false)
	, mTimeSinceLastInput(0)
	, mScreenSaver(NULL)
	, mRenderScreenSaver(false)
	, mInfoPopup(NULL)
	, mRenderedHelpPrompts(false)
	// Clock
	, mClockTimeAccum(0)
	, mClockLastText("")
	, mClockTextCache(nullptr)
	, mClockOutlineCache(nullptr)
{
	mHelp = new HelpComponent(this);
	mBackgroundOverlay = new ImageComponent(this);
	mNetworkIcon.reset(new ImageComponent(this));
	mBluetoothIcon.reset(new ImageComponent(this));

	// Safe initial clock defaults
	mClockDefined = false;
	mClockPos = Vector2f(0.98f, 0.05f);
	mClockOrigin = Vector2f(1.0f, 0.0f);
	mClockColor = 0xFFFFFFFF;
	mClockOutlineColor = 0x000000FF;

	// Safe initial network defaults
	mNetworkDefined = false;
	mNetworkPos = Vector2f(0.90f, 0.05f);
	mNetworkOrigin = Vector2f(1.0f, 0.0f);
	mNetworkSize = Vector2f(0.03f, 0.03f);
	mNetworkPath = ":/icons/network.png";
	mNetworkConnected = false;
	mNetworkPollAccum = 0;

	// Safe initial bluetooth defaults
	mBluetoothDefined = false;
	mBluetoothPos = Vector2f(0.86f, 0.05f);
	mBluetoothOrigin = Vector2f(1.0f, 0.0f);
	mBluetoothSize = Vector2f(0.03f, 0.03f);
	mBluetoothPath = ":/icons/bluetooth.png";
	mBluetoothConnected = false;
	mBluetoothPollAccum = 0;
}

Window::~Window()
{
	delete mBackgroundOverlay;

	// delete all our GUIs
	while (peekGui())
		delete peekGui();

	delete mHelp;
}

void Window::pushGui(GuiComponent* gui)
{
	if (mGuiStack.size() > 0)
	{
		auto& top = mGuiStack.back();
		top->topWindow(false);
	}

	mGuiStack.push_back(gui);
	gui->updateHelpPrompts();
}

void Window::removeGui(GuiComponent* gui)
{
	for (auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
	{
		if (*i == gui)
		{
			i = mGuiStack.erase(i);

			if (i == mGuiStack.cend() && mGuiStack.size())
			{
				mGuiStack.back()->updateHelpPrompts();
				mGuiStack.back()->topWindow(true);
			}

			return;
		}
	}
}

GuiComponent* Window::peekGui()
{
	if (mGuiStack.size() == 0)
		return NULL;

	return mGuiStack.back();
}

bool Window::init()
{
	if (!Renderer::init())
	{
		LOG(LogError) << "Renderer failed to initialize!";
		return false;
	}

	ResourceManager::getInstance()->reloadAll();

	// keep a reference to the default fonts, so they don't keep getting destroyed/recreated
	if (mDefaultFonts.empty())
	{
		mDefaultFonts.push_back(Font::get(FONT_SIZE_SMALL));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_MEDIUM));
		mDefaultFonts.push_back(Font::get(FONT_SIZE_LARGE));
	}

	mBackgroundOverlay->setImage(":/scroll_gradient.png");
	mBackgroundOverlay->setResize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());

	// Fallback clock font
	if (!mDefaultFonts.empty() && !mClockFont)
		mClockFont = mDefaultFonts.at(1);

	if (mNetworkIcon)
	{
		mNetworkIcon->setImage(mNetworkPath);
		mNetworkIcon->setResize(
			Renderer::getScreenHeight() * mNetworkSize.x(),
			Renderer::getScreenHeight() * mNetworkSize.y());
	}

	if (mBluetoothIcon)
	{
		mBluetoothIcon->setImage(mBluetoothPath);
		mBluetoothIcon->setResize(
			Renderer::getScreenHeight() * mBluetoothSize.x(),
			Renderer::getScreenHeight() * mBluetoothSize.y());
	}

	mNetworkConnected = hasActiveNetworkConnection();
	mNetworkPollAccum = 0;

	mBluetoothConnected = hasActiveBluetoothConnection();
	mBluetoothPollAccum = 0;

	// update our help because font sizes probably changed
	if (peekGui())
		peekGui()->updateHelpPrompts();

	// Reset clock cache on init (safe)
	mClockTimeAccum = 0;
	mClockLastText.clear();
	mClockTextCache.reset();
	mClockOutlineCache.reset();

	return true;
}

void Window::applyClockTheme(const std::shared_ptr<ThemeData>& theme)
{
	// -------------------------------------------------
	// IMPORTANT:
	// Do NOT reset clock every time this is called.
	// If the theme reload path does not provide screen/clock
	// momentarily, we preserve the last valid clock styling.
	// -------------------------------------------------

	// Make sure there is always a usable fallback font
	if (!mClockFont && !mDefaultFonts.empty())
		mClockFont = mDefaultFonts.at(1);

	// =========================
	// Network defaults / fallback
	// =========================
	// Network can safely reset each call because it is purely visual
	// and does not affect cached text rendering like the clock does.

	mNetworkDefined = false;
	mNetworkPos = Vector2f(0.90f, 0.05f);
	mNetworkOrigin = Vector2f(1.0f, 0.0f);
	mNetworkSize = Vector2f(0.03f, 0.03f);
	mNetworkPath = ":/icons/network.png";

	// =========================
	// Bluetooth defaults / fallback
	// =========================

	mBluetoothDefined = false;
	mBluetoothPos = Vector2f(0.86f, 0.05f);
	mBluetoothOrigin = Vector2f(1.0f, 0.0f);
	mBluetoothSize = Vector2f(0.03f, 0.03f);
	mBluetoothPath = ":/icons/bluetooth.png";

	if (!theme)
	{
		if (mNetworkIcon)
		{
			mNetworkIcon->setImage(mNetworkPath);
			mNetworkIcon->setResize(
				Renderer::getScreenHeight() * mNetworkSize.x(),
				Renderer::getScreenHeight() * mNetworkSize.y());
		}

		if (mBluetoothIcon)
		{
			mBluetoothIcon->setImage(mBluetoothPath);
			mBluetoothIcon->setResize(
				Renderer::getScreenHeight() * mBluetoothSize.x(),
				Renderer::getScreenHeight() * mBluetoothSize.y());
		}

		// Force clock cache rebuild but preserve styling
		mClockTimeAccum = 0;
		mClockLastText.clear();
		mClockTextCache.reset();
		mClockOutlineCache.reset();

		return;
	}

	// =========================
	// Clock theme
	// =========================

	const ThemeData::ThemeElement* elem = theme->getElement("screen", "clock", "text");

	if (elem)
	{
		mClockDefined = true;

		if (elem->has("pos"))
			mClockPos = elem->get<Vector2f>("pos");

		if (elem->has("origin"))
			mClockOrigin = elem->get<Vector2f>("origin");

		if (elem->has("color"))
			mClockColor = elem->get<unsigned int>("color");

		if (elem->has("outlineColor"))
			mClockOutlineColor = elem->get<unsigned int>("outlineColor");

		// supports font and size from theme
		if (elem->has("fontPath") || elem->has("fontSize"))
		{
			mClockFont = Font::getFromTheme(
				elem,
				ThemeFlags::FONT_PATH | ThemeFlags::FONT_SIZE,
				mDefaultFonts.at(1));
		}
	}

	// =========================
	// Network theme
	// =========================

	const ThemeData::ThemeElement* netElem = theme->getElement("screen", "network", "image");

	if (netElem)
	{
		mNetworkDefined = true;

		if (netElem->has("pos"))
			mNetworkPos = netElem->get<Vector2f>("pos");

		if (netElem->has("origin"))
			mNetworkOrigin = netElem->get<Vector2f>("origin");

		if (netElem->has("size"))
			mNetworkSize = netElem->get<Vector2f>("size");

		if (netElem->has("path"))
			mNetworkPath = netElem->get<std::string>("path");
	}

	if (mNetworkIcon)
	{
		mNetworkIcon->setImage(mNetworkPath);
		mNetworkIcon->setResize(
			Renderer::getScreenHeight() * mNetworkSize.x(),
			Renderer::getScreenHeight() * mNetworkSize.y());
	}

	// =========================
	// Bluetooth theme
	// =========================

	const ThemeData::ThemeElement* btElem = theme->getElement("screen", "bluetooth", "image");

	if (btElem)
	{
		mBluetoothDefined = true;

		if (btElem->has("pos"))
			mBluetoothPos = btElem->get<Vector2f>("pos");

		if (btElem->has("origin"))
			mBluetoothOrigin = btElem->get<Vector2f>("origin");

		if (btElem->has("size"))
			mBluetoothSize = btElem->get<Vector2f>("size");

		if (btElem->has("path"))
			mBluetoothPath = btElem->get<std::string>("path");
	}

	if (mBluetoothIcon)
	{
		mBluetoothIcon->setImage(mBluetoothPath);
		mBluetoothIcon->setResize(
			Renderer::getScreenHeight() * mBluetoothSize.x(),
			Renderer::getScreenHeight() * mBluetoothSize.y());
	}

	// force clock cache rebuild using current styling
	mClockTimeAccum = 0;
	mClockLastText.clear();
	mClockTextCache.reset();
	mClockOutlineCache.reset();
}

void Window::deinit()
{
	// Hide all GUI elements on uninitialisation - this disable
	for (auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
	{
		(*i)->onHide();
	}

	ResourceManager::getInstance()->unloadAll();
	Renderer::deinit();
}

void Window::textInput(const char* text)
{
	if (peekGui())
		peekGui()->textInput(text);
}

void Window::input(InputConfig* config, Input input)
{
	if (mScreenSaver && mScreenSaver->isScreenSaverActive() && Settings::getInstance()->getBool("ScreenSaverControls")
		&& mScreenSaver->inputDuringScreensaver(config, input))
	{
		mTimeSinceLastInput = 0;
		return;
	}

	if (mSleeping)
	{
		mSleeping = false;
		mTimeSinceLastInput = 0;
		onWake();
		return;
	}

	mTimeSinceLastInput = 0;

	if (input.value != 0 && cancelScreenSaver())
		return;

	bool dbg_keyboard_key_press = Settings::getInstance()->getBool("Debug") && config->getDeviceId() == DEVICE_KEYBOARD && input.value;

	if (dbg_keyboard_key_press && input.id == SDLK_g && SDL_GetModState() & KMOD_LCTRL)
	{
		// toggle debug grid with Ctrl-G
		Settings::getInstance()->setBool("DebugGrid", !Settings::getInstance()->getBool("DebugGrid"));
	}
	else if (dbg_keyboard_key_press && input.id == SDLK_t && SDL_GetModState() & KMOD_LCTRL)
	{
		// toggle TextComponent debug view with Ctrl-T
		Settings::getInstance()->setBool("DebugText", !Settings::getInstance()->getBool("DebugText"));
	}
	else if (dbg_keyboard_key_press && input.id == SDLK_i && SDL_GetModState() & KMOD_LCTRL)
	{
		// toggle TextComponent debug view with Ctrl-I
		Settings::getInstance()->setBool("DebugImage", !Settings::getInstance()->getBool("DebugImage"));
	}
	else if (peekGui())
	{
		this->peekGui()->input(config, input);
	}
}

void Window::update(int deltaTime)
{
	if (mNormalizeNextUpdate)
	{
		mNormalizeNextUpdate = false;

		if (deltaTime > mAverageDeltaTime)
			deltaTime = mAverageDeltaTime;
	}

	mFrameTimeElapsed += deltaTime;
	mFrameCountElapsed++;

	if (mFrameTimeElapsed > 500)
	{
		mAverageDeltaTime = mFrameTimeElapsed / mFrameCountElapsed;

		if (Settings::getInstance()->getBool("DrawFramerate"))
		{
			std::stringstream ss;

			// fps
			ss << std::fixed << std::setprecision(1) << (1000.0f * (float)mFrameCountElapsed / (float)mFrameTimeElapsed) << "fps, ";
			ss << std::fixed << std::setprecision(2) << ((float)mFrameTimeElapsed / (float)mFrameCountElapsed) << "ms";

			// vram
			float textureVramUsageMb = TextureResource::getTotalMemUsage() / 1000.0f / 1000.0f;
			float textureTotalUsageMb = TextureResource::getTotalTextureSize() / 1000.0f / 1000.0f;
			float fontVramUsageMb = Font::getTotalMemUsage() / 1000.0f / 1000.0f;

			ss << "\nFont VRAM: " << fontVramUsageMb << " Tex VRAM: " << textureVramUsageMb <<
				" Tex Max: " << textureTotalUsageMb;

			mFrameDataText = std::unique_ptr<TextCache>(
				mDefaultFonts.at(1)->buildTextCache(ss.str(), 50.f, 50.f, 0xFF00FFFF));
		}

		mFrameTimeElapsed = 0;
		mFrameCountElapsed = 0;
	}

	mTimeSinceLastInput += deltaTime;

	if (peekGui())
		peekGui()->update(deltaTime);

	// Update the screensaver
	if (mScreenSaver)
		mScreenSaver->update(deltaTime);

	// Network polling
	mNetworkPollAccum += deltaTime;

	if (mNetworkPollAccum >= 3000)
	{
		mNetworkPollAccum = 0;
		mNetworkConnected = hasActiveNetworkConnection();
	}

	// Bluetooth polling
	mBluetoothPollAccum += deltaTime;

	if (mBluetoothPollAccum >= 3000)
	{
		mBluetoothPollAccum = 0;
		mBluetoothConnected = hasActiveBluetoothConnection();
	}

	if (!Settings::getInstance()->getBool("ShowClock"))
	{
		if (mClockTextCache)
		{
			mClockTimeAccum = 0;
			mClockLastText.clear();
			mClockTextCache.reset();
			mClockOutlineCache.reset();
		}
	}
	else
	{
		mClockTimeAccum += deltaTime;

		if (mClockTimeAccum >= 1000 || mClockTextCache == nullptr)
		{
			mClockTimeAccum = 0;

			std::time_t t = std::time(nullptr);
			std::tm tmNow;

#if defined(_WIN32)
			localtime_s(&tmNow, &t);
#else
			localtime_r(&t, &tmNow);
#endif

			const std::string clockFormat = Settings::getInstance()->getString("ClockFormat");

			std::ostringstream os;

			if (clockFormat == "12H")
				os << std::put_time(&tmNow, "%I:%M %p");
			else
				os << std::put_time(&tmNow, "%H:%M");

			std::string newText = os.str();

			// Para 12H, quitar cero inicial: "06:32 PM" -> "6:32 PM"
			if (clockFormat == "12H" && newText.size() > 0 && newText[0] == '0')
				newText.erase(0, 1);

			if (newText != mClockLastText)
			{
				mClockLastText = newText;

				std::shared_ptr<Font> font = mClockFont ? mClockFont : mDefaultFonts.at(1);

				mClockOutlineCache.reset(
					font->buildTextCache(
						mClockLastText,
						0.f,
						0.f,
						mClockOutlineColor));

				mClockTextCache.reset(
					font->buildTextCache(
						mClockLastText,
						0.f,
						0.f,
						mClockColor));
			}
		}
	}
}

void Window::render()
{
	Transform4x4f transform = Transform4x4f::Identity();

	mRenderedHelpPrompts = false;

	// draw only bottom and top of GuiStack (if they are different)
	if (mGuiStack.size())
	{
		auto& bottom = mGuiStack.front();
		auto& top = mGuiStack.back();

		bottom->render(transform);

		if (bottom != top)
		{
			mBackgroundOverlay->render(transform);
			top->render(transform);
		}
	}

	if (!mRenderedHelpPrompts)
		mHelp->render(transform);

	if (Settings::getInstance()->getBool("DrawFramerate") && mFrameDataText)
	{
		Renderer::setMatrix(Transform4x4f::Identity());
		mDefaultFonts.at(1)->renderTextCache(mFrameDataText.get());
	}

	unsigned int screensaverTime = (unsigned int)Settings::getInstance()->getInt("ScreenSaverTime");

	if (mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
		startScreenSaver();

	// Always call the screensaver render function regardless of whether the screensaver is active
	// or not because it may perform a fade on transition
	renderScreenSaver();

	const bool showClock = Settings::getInstance()->getBool("ShowClock");
	const bool hasOverlayGui = mGuiStack.size() > 1;

	// Render clock
	if (showClock && !hasOverlayGui && !mRenderScreenSaver && mClockTextCache && mClockOutlineCache)
	{
		float x = Renderer::getScreenWidth() * mClockPos.x();
		float y = Renderer::getScreenHeight() * mClockPos.y();

		float w = mClockTextCache->metrics.size.x();
		float h = mClockTextCache->metrics.size.y();

		x -= w * mClockOrigin.x();
		y -= h * mClockOrigin.y();

		std::shared_ptr<Font> font = mClockFont ? mClockFont : mDefaultFonts.at(1);

		// 1px outline (4 directions)
		static const int off[4][2] = { { -1, 0 }, { 1, 0 }, { 0, -1 }, { 0, 1 } };

		for (int i = 0; i < 4; i++)
		{
			Transform4x4f t = Transform4x4f::Identity();
			t = t.translate(Vector3f(x + (float)off[i][0], y + (float)off[i][1], 0.0f));
			Renderer::setMatrix(t);
			font->renderTextCache(mClockOutlineCache.get());
		}

		// Main text
		Transform4x4f t = Transform4x4f::Identity();
		t = t.translate(Vector3f(x, y, 0.0f));
		Renderer::setMatrix(t);
		font->renderTextCache(mClockTextCache.get());
	}

	// Render network icon
	if (Settings::getInstance()->getBool("ShowNetworkIcon") &&
		!hasOverlayGui &&
		mNetworkConnected &&
		!mRenderScreenSaver &&
		mNetworkIcon)
	{
		float x = 0.0f;
		float y = 0.0f;

		if (mNetworkDefined)
		{
			const float iconW = Renderer::getScreenHeight() * mNetworkSize.x();
			const float iconH = Renderer::getScreenHeight() * mNetworkSize.y();

			x = Renderer::getScreenWidth() * mNetworkPos.x();
			y = Renderer::getScreenHeight() * mNetworkPos.y();

			x -= iconW * mNetworkOrigin.x();
			y -= iconH * mNetworkOrigin.y();
		}
		else
		{
			// fallback: place it just before the clock
			const float iconW = Renderer::getScreenHeight() * mNetworkSize.x();
			const float margin = 10.0f;

			float clockX = Renderer::getScreenWidth() * mClockPos.x();
			float clockY = Renderer::getScreenHeight() * mClockPos.y();

			if (showClock && mClockTextCache)
			{
				float clockW = mClockTextCache->metrics.size.x();
				float clockH = mClockTextCache->metrics.size.y();

				clockX -= clockW * mClockOrigin.x();
				clockY -= clockH * mClockOrigin.y();

				x = clockX - iconW - margin;
				y = clockY;
			}
			else
			{
				x = Renderer::getScreenWidth() * 0.90f;
				y = Renderer::getScreenHeight() * 0.05f;
			}
		}

		mNetworkIcon->setPosition(x, y);
		mNetworkIcon->render(transform);
	}

		// Render bluetooth icon
	if (!hasOverlayGui &&
		mBluetoothConnected &&
		!mRenderScreenSaver &&
		mBluetoothIcon)
	{
		float x = 0.0f;
		float y = 0.0f;

		const float iconW = Renderer::getScreenHeight() * mBluetoothSize.x();
		const float iconH = Renderer::getScreenHeight() * mBluetoothSize.y();
		const float margin = 10.0f;

		if (mBluetoothDefined)
		{
			// Theme-defined position.
			x = Renderer::getScreenWidth() * mBluetoothPos.x();
			y = Renderer::getScreenHeight() * mBluetoothPos.y();

			x -= iconW * mBluetoothOrigin.x();
			y -= iconH * mBluetoothOrigin.y();
		}
		else if (Settings::getInstance()->getBool("ShowNetworkIcon") &&
				 mNetworkConnected &&
				 mNetworkIcon)
		{
			// Fallback: place Bluetooth just before the network icon.
			const float networkW = Renderer::getScreenHeight() * mNetworkSize.x();
			const float networkH = Renderer::getScreenHeight() * mNetworkSize.y();

			float networkX = Renderer::getScreenWidth() * mNetworkPos.x();
			float networkY = Renderer::getScreenHeight() * mNetworkPos.y();

			networkX -= networkW * mNetworkOrigin.x();
			networkY -= networkH * mNetworkOrigin.y();

			x = networkX - iconW - margin;
			y = networkY;
		}
		else if (showClock && mClockTextCache)
		{
			// Fallback: if there is no network icon, place Bluetooth before the clock.
			float clockX = Renderer::getScreenWidth() * mClockPos.x();
			float clockY = Renderer::getScreenHeight() * mClockPos.y();

			const float clockW = mClockTextCache->metrics.size.x();
			const float clockH = mClockTextCache->metrics.size.y();

			clockX -= clockW * mClockOrigin.x();
			clockY -= clockH * mClockOrigin.y();

			x = clockX - iconW - margin;
			y = clockY;
		}
		else
		{
			// Final safe fallback.
			x = Renderer::getScreenWidth() * 0.86f;
			y = Renderer::getScreenHeight() * 0.05f;
		}

		mBluetoothIcon->setPosition(x, y);
		mBluetoothIcon->render(transform);
	}

	// Info popup always on top
	if (mInfoPopup)
	{
		mInfoPopup->render(transform);
	}

	if (mTimeSinceLastInput >= screensaverTime && screensaverTime != 0)
	{
		unsigned int systemSleepTime = (unsigned int)Settings::getInstance()->getInt("SystemSleepTime");

		if (!isProcessing() && mAllowSleep && systemSleepTime != 0 && mTimeSinceLastInput >= systemSleepTime)
		{
			mSleeping = true;
			onSleep();
		}
	}
}

void Window::normalizeNextUpdate()
{
	mNormalizeNextUpdate = true;
}

bool Window::getAllowSleep()
{
	return mAllowSleep;
}

void Window::setAllowSleep(bool sleep)
{
	mAllowSleep = sleep;
}

void Window::renderLoadingScreen(std::string text, float percent, unsigned char opacity)
{
	Transform4x4f trans = Transform4x4f::Identity();

	Renderer::setMatrix(trans);
	Renderer::drawRect(0.0f, 0.0f, Renderer::getScreenWidth(), Renderer::getScreenHeight(), 0x000000FF, 0x000000FF);

	if (percent >= 0)
	{
		float baseHeight = 0.04f;

		float w = Renderer::getScreenWidth() / 2;
		float h = Renderer::getScreenHeight() * baseHeight;

		float x = Renderer::getScreenWidth() / 2 - w / 2;
		float y = Renderer::getScreenHeight() - (Renderer::getScreenHeight() * 3 * baseHeight);

		Renderer::drawRect(x, y, w, h, 0x25252500 | opacity, 0x25252500 | opacity);
		Renderer::drawRect(x, y, (w * percent), h, 0x006C9E00 | opacity, 0x006C9E00 | opacity);
	}

	ImageComponent splash(this, true);
	splash.setResize(Renderer::getScreenWidth() * 0.6f, 0.0f);
	splash.setImage(":/splash.svg");
	splash.setPosition((Renderer::getScreenWidth() - splash.getSize().x()) / 2, (Renderer::getScreenHeight() - splash.getSize().y()) / 2 * 0.6f);
	splash.render(trans);

	auto& font = mDefaultFonts.at(1);
	TextCache* cache = font->buildTextCache(text, 0, 0, 0x656565FF);

	float x = Math::round((Renderer::getScreenWidth() - cache->metrics.size.x()) / 2.0f);
	float y = Math::round(Renderer::getScreenHeight() * 0.78f);

	trans = Transform4x4f::Identity();
	trans = trans.translate(Vector3f(x, y, 0.0f));

	Renderer::setMatrix(trans);
	font->renderTextCache(cache);

	delete cache;

	Renderer::swapBuffers();

#ifdef WIN32
	// Avoid Window Freezing on Windows
	SDL_Event event;
	while (SDL_PollEvent(&event));
#endif
}

void Window::renderHelpPromptsEarly()
{
	mHelp->render(Transform4x4f::Identity());
	mRenderedHelpPrompts = true;
}

void Window::setHelpPrompts(const std::vector<HelpPrompt>& prompts, const HelpStyle& style)
{
	mHelp->clearPrompts();
	mHelp->setStyle(style);

	std::vector<HelpPrompt> addPrompts;

	std::map<std::string, bool> inputSeenMap;
	std::map<std::string, int> mappedToSeenMap;

	for (auto it = prompts.cbegin(); it != prompts.cend(); it++)
	{
		// only add it if the same icon hasn't already been added
		if (inputSeenMap.emplace(it->first, true).second)
		{
			// this symbol hasn't been seen yet, what about the action name?
			auto mappedTo = mappedToSeenMap.find(it->second);

			if (mappedTo != mappedToSeenMap.cend())
			{
				// yes, it has!

				// can we combine? (dpad only)
				if ((it->first == "up/down" && addPrompts.at(mappedTo->second).first != "left/right") ||
					(it->first == "left/right" && addPrompts.at(mappedTo->second).first != "up/down"))
				{
					// yes!
					addPrompts.at(mappedTo->second).first = "up/down/left/right";
				}
				else
				{
					// no, we can't combine!
					addPrompts.push_back(*it);
				}
			}
			else
			{
				// no, it hasn't!
				mappedToSeenMap.emplace(it->second, (int)addPrompts.size());
				addPrompts.push_back(*it);
			}
		}
	}

	// sort prompts so it goes [dpad_all] [dpad_u/d] [dpad_l/r] [a/b/x/y/l/r] [start/select]
	std::sort(addPrompts.begin(), addPrompts.end(), [](const HelpPrompt& a, const HelpPrompt& b) -> bool {

		static const char* map[] = {
			"up/down/left/right",
			"up/down",
			"left/right",
			"a", "b", "x", "y", "l", "r",
			"start", "select",
			NULL
		};

		int i = 0;
		int aVal = 0;
		int bVal = 0;

		while (map[i] != NULL)
		{
			if (a.first == map[i])
				aVal = i;

			if (b.first == map[i])
				bVal = i;

			i++;
		}

		return aVal > bVal;
	});

	mHelp->setPrompts(addPrompts);
}

void Window::onSleep()
{
	if (Settings::getInstance()->getBool("Windowed"))
	{
		LOG(LogInfo) << "running windowed. No further onSleep() processing.";
		return;
	}

	int gotErrors = Scripting::fireEvent("sleep");

	if (gotErrors == 0 && mScreenSaver && mRenderScreenSaver)
	{
		mScreenSaver->stopScreenSaver();
		mRenderScreenSaver = false;
	}
}

void Window::onWake()
{
	Scripting::fireEvent("wake");
}

bool Window::isProcessing()
{
	return count_if(mGuiStack.cbegin(), mGuiStack.cend(), [](GuiComponent* c) { return c->isProcessing(); }) > 0;
}

void Window::startScreenSaver(SystemData* system)
{
	if (mScreenSaver && !mRenderScreenSaver)
	{
		Scripting::fireEvent("screensaver-start");

		// Tell the GUI components the screensaver is starting
		for (auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
			(*i)->onScreenSaverActivate();

		mScreenSaver->startScreenSaver(system);
		mRenderScreenSaver = true;
	}
}

bool Window::cancelScreenSaver()
{
	if (mScreenSaver && mRenderScreenSaver)
	{
		mScreenSaver->stopScreenSaver();
		mRenderScreenSaver = false;

		Scripting::fireEvent("screensaver-stop");

		// Tell the GUI components the screensaver has stopped
		for (auto i = mGuiStack.cbegin(); i != mGuiStack.cend(); i++)
			(*i)->onScreenSaverDeactivate();

		return true;
	}

	return false;
}

void Window::renderScreenSaver()
{
	if (mScreenSaver)
		mScreenSaver->renderScreenSaver();
}
