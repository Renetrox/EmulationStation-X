// InputManager.cpp
#include "InputManager.h"

#include "utils/FileSystemUtil.h"
#include "CECInput.h"
#include "Log.h"
#include "platform.h"
#include "Scripting.h"
#include "Window.h"
#include "guis/GuiInfoPopup.h"
#include "utils/StringUtil.h"
#include "Settings.h"
#include "InputConfig.h"
#include "LocaleES.h"

#include <pugixml.hpp>
#include <SDL.h>
#include <iostream>
#include <assert.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#define KEYBOARD_GUID_STRING "-1"
#define CEC_GUID_STRING      "-2"

// hack for cec support
int SDL_USER_CECBUTTONDOWN = -1;
int SDL_USER_CECBUTTONUP   = -1;

InputManager* InputManager::mInstance = NULL;

namespace
{
	inline std::string tr(const std::string& key)
	{
		return es_translate(key);
	}

	inline std::string buildControllerPopupMessage(bool connected, const std::string& joyName)
	{
		return std::string(connected ? "🎮 ✓ " : "🎮 ✕ ")
			+ tr(connected ? "CONTROLLER_CONNECTED" : "CONTROLLER_DISCONNECTED")
			+ ": " + joyName;
	}

#if SDL_VERSION_ATLEAST(2,0,9)
	struct SDLPhysicalBinding
	{
		InputType type;
		int id;
		int value;
		int axisMin;
		int axisMax;
		bool valid;

		SDLPhysicalBinding()
			: type(TYPE_COUNT), id(-1), value(0), axisMin(0), axisMax(0), valid(false)
		{
		}
	};

	inline void stripSpaces(std::string& value)
	{
		value.erase(std::remove(value.begin(), value.end(), ' '), value.end());
	}

	bool retropieSwapABEnabled()
	{
		const std::string path = "/opt/retropie/configs/all/autoconf.cfg";
		if(!Utils::FileSystem::exists(path))
			return false;

		std::ifstream file(path.c_str());
		if(!file)
			return false;

		std::string line;
		while(std::getline(file, line))
		{
			line.erase(std::remove_if(line.begin(), line.end(), [](unsigned char c) {
				return c == ' ' || c == '\t' || c == '\r';
			}), line.end());

			if(line.empty() || line[0] == '#')
				continue;

			if(line == "es_swap_a_b=\"1\"" || line == "es_swap_a_b=1")
				return true;
		}

		return false;
	}

	bool parseNonNegativeInt(const std::string& text, int& value)
	{
		if(text.empty())
			return false;

		for(size_t i = 0; i < text.size(); ++i)
		{
			if(text[i] < '0' || text[i] > '9')
				return false;
		}

		value = std::atoi(text.c_str());
		return true;
	}

	SDLPhysicalBinding parseSDLPhysicalBinding(std::string binding)
	{
		SDLPhysicalBinding result;
		stripSpaces(binding);

		if(binding.empty())
			return result;

		char halfAxis = 0;
		if(binding[0] == '+' || binding[0] == '-')
		{
			halfAxis = binding[0];
			binding.erase(0, 1);
		}

		if(binding.empty())
			return result;

		bool inverted = false;
		if(binding[binding.size() - 1] == '~')
		{
			inverted = true;
			binding.erase(binding.size() - 1);
		}

		if(binding.size() < 2)
			return result;

		if(binding[0] == 'b')
		{
			int id = -1;
			if(!parseNonNegativeInt(binding.substr(1), id))
				return result;

			result.type = TYPE_BUTTON;
			result.id = id;
			result.value = 1;
			result.valid = true;
			return result;
		}

		if(binding[0] == 'h')
		{
			size_t dot = binding.find('.');
			if(dot == std::string::npos || dot <= 1 || dot + 1 >= binding.size())
				return result;

			int hat = -1;
			int mask = 0;
			if(!parseNonNegativeInt(binding.substr(1, dot - 1), hat) ||
			   !parseNonNegativeInt(binding.substr(dot + 1), mask))
				return result;

			result.type = TYPE_HAT;
			result.id = hat;
			result.value = mask;
			result.valid = true;
			return result;
		}

		if(binding[0] == 'a')
		{
			int axis = -1;
			if(!parseNonNegativeInt(binding.substr(1), axis))
				return result;

			result.type = TYPE_AXIS;
			result.id = axis;

			// Mirror SDL's GameController parser. The axis endpoints are enough
			// to translate the normalized mapping back to raw SDL_Joystick input.
			if(halfAxis == '+')
			{
				result.axisMin = 0;
				result.axisMax = 1;
			}
			else if(halfAxis == '-')
			{
				result.axisMin = 0;
				result.axisMax = -1;
			}
			else
			{
				result.axisMin = -1;
				result.axisMax = 1;
			}

			if(inverted)
				std::swap(result.axisMin, result.axisMax);

			result.valid = true;
			return result;
		}

		return result;
	}

	bool mapBindingEndpoint(InputConfig* config, SDL_JoystickID deviceId,
		const std::string& name, const SDLPhysicalBinding& binding, bool useMinEndpoint)
	{
		if(!binding.valid)
			return false;

		if(binding.type == TYPE_AXIS)
		{
			int value = useMinEndpoint ? binding.axisMin : binding.axisMax;
			if(value == 0)
				return false;

			config->mapInput(name, Input(deviceId, TYPE_AXIS, binding.id, value, true));
			return true;
		}

		// Buttons and hats only have a meaningful pressed endpoint.
		if(useMinEndpoint)
			return false;

		if(binding.type == TYPE_BUTTON)
		{
			config->mapInput(name, Input(deviceId, TYPE_BUTTON, binding.id, 1, true));
			return true;
		}

		if(binding.type == TYPE_HAT)
		{
			config->mapInput(name, Input(deviceId, TYPE_HAT, binding.id, binding.value, true));
			return true;
		}

		return false;
	}

	void mapSDLLogicalAxis(InputConfig* config, SDL_JoystickID deviceId,
		const std::string& negativeName, const std::string& positiveName,
		char halfOutput, const SDLPhysicalBinding& binding)
	{
		if(halfOutput == '+')
		{
			mapBindingEndpoint(config, deviceId, positiveName, binding, false);
		}
		else if(halfOutput == '-')
		{
			mapBindingEndpoint(config, deviceId, negativeName, binding, false);
		}
		else if(binding.type == TYPE_AXIS)
		{
			mapBindingEndpoint(config, deviceId, negativeName, binding, true);
			mapBindingEndpoint(config, deviceId, positiveName, binding, false);
		}
	}

	bool autoConfigureFromSDLGameController(int deviceIndex, SDL_JoystickID deviceId, InputConfig* config)
	{
		if(!SDL_IsGameController(deviceIndex))
			return false;

		char* mappingC = SDL_GameControllerMappingForDeviceIndex(deviceIndex);
		if(mappingC == NULL)
			return false;

		std::string mapping(mappingC);
		SDL_free(mappingC);

		// Mapping format: GUID,name,logical:physical,logical:physical,...
		size_t firstComma = mapping.find(',');
		if(firstComma == std::string::npos)
			return false;

		size_t secondComma = mapping.find(',', firstComma + 1);
		if(secondComma == std::string::npos)
			return false;

		std::stringstream stream(mapping.substr(secondComma + 1));
		std::string token;
		const bool swapAB = retropieSwapABEnabled();

		while(std::getline(stream, token, ','))
		{
			size_t colon = token.find(':');
			if(colon == std::string::npos)
				continue;

			std::string logical = token.substr(0, colon);
			std::string physical = token.substr(colon + 1);
			stripSpaces(logical);
			stripSpaces(physical);

			if(logical.empty() || physical.empty())
				continue;

			char halfOutput = 0;
			if(logical[0] == '+' || logical[0] == '-')
			{
				halfOutput = logical[0];
				logical.erase(0, 1);
			}

			SDLPhysicalBinding binding = parseSDLPhysicalBinding(physical);
			if(!binding.valid)
				continue;

			if(logical == "dpup")
				mapBindingEndpoint(config, deviceId, "Up", binding, false);
			else if(logical == "dpdown")
				mapBindingEndpoint(config, deviceId, "Down", binding, false);
			else if(logical == "dpleft")
				mapBindingEndpoint(config, deviceId, "Left", binding, false);
			else if(logical == "dpright")
				mapBindingEndpoint(config, deviceId, "Right", binding, false);
			else if(logical == "start")
				mapBindingEndpoint(config, deviceId, "Start", binding, false);
			else if(logical == "back")
				mapBindingEndpoint(config, deviceId, "Select", binding, false);
			// SDL GameController uses positional Xbox-style names. ES-X/RetroPie
			// labels the face buttons by position: A=east, B=south, X=north, Y=west.
			// RetroPie's es_swap_a_b option swaps only these two ES actions.
			else if(logical == "a")
				mapBindingEndpoint(config, deviceId, swapAB ? "A" : "B", binding, false);
			else if(logical == "b")
				mapBindingEndpoint(config, deviceId, swapAB ? "B" : "A", binding, false);
			else if(logical == "x")
				mapBindingEndpoint(config, deviceId, "Y", binding, false);
			else if(logical == "y")
				mapBindingEndpoint(config, deviceId, "X", binding, false);
			else if(logical == "leftshoulder")
				mapBindingEndpoint(config, deviceId, "LeftShoulder", binding, false);
			else if(logical == "rightshoulder")
				mapBindingEndpoint(config, deviceId, "RightShoulder", binding, false);
			else if(logical == "lefttrigger")
				mapBindingEndpoint(config, deviceId, "LeftTrigger", binding, false);
			else if(logical == "righttrigger")
				mapBindingEndpoint(config, deviceId, "RightTrigger", binding, false);
			else if(logical == "leftstick")
				mapBindingEndpoint(config, deviceId, "LeftThumb", binding, false);
			else if(logical == "rightstick")
				mapBindingEndpoint(config, deviceId, "RightThumb", binding, false);
			else if(logical == "leftx")
				mapSDLLogicalAxis(config, deviceId, "LeftAnalogLeft", "LeftAnalogRight", halfOutput, binding);
			else if(logical == "lefty")
				mapSDLLogicalAxis(config, deviceId, "LeftAnalogUp", "LeftAnalogDown", halfOutput, binding);
			else if(logical == "rightx")
				mapSDLLogicalAxis(config, deviceId, "RightAnalogLeft", "RightAnalogRight", halfOutput, binding);
			else if(logical == "righty")
				mapSDLLogicalAxis(config, deviceId, "RightAnalogUp", "RightAnalogDown", halfOutput, binding);
		}

		// Match the minimum controls required by GuiInputConfig. Partial SDL
		// mappings should fall back to the existing manual configurator.
		Input check;
		if(!config->getInputByName("Up", &check) ||
		   !config->getInputByName("Down", &check) ||
		   !config->getInputByName("Left", &check) ||
		   !config->getInputByName("Right", &check) ||
		   !config->getInputByName("A", &check))
		{
			config->clear();
			return false;
		}

		// RetroPie traditionally uses Select as the default hotkey enable.
		Input selectInput;
		if(config->getInputByName("Select", &selectInput))
			config->mapInput("HotKeyEnable", selectInput);

		if(swapAB)
			LOG(LogInfo) << "SDL auto-mapping: honoring RetroPie es_swap_a_b for '"
			             << config->getDeviceName() << "'.";

		return true;
	}
#endif
}

InputManager::InputManager()
	: mKeyboardInputConfig(NULL)
	, mCECInputConfig(NULL)
	, mSuppressHotplugPopups(false)
{
}

InputManager::~InputManager()
{
	deinit();
}

InputManager* InputManager::getInstance()
{
	if(!mInstance)
		mInstance = new InputManager();

	return mInstance;
}

void InputManager::init()
{
	if(initialized())
		deinit();

	// ✅ No mostrar popups durante el scan inicial (joysticks ya presentes)
	mSuppressHotplugPopups = true;

	SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS,
		Settings::getInstance()->getBool("BackgroundJoystickInput") ? "1" : "0");

	// Don't enable the HIDAPI drivers by default, it will break the existing configurations
	// for a few controller types, since the names and the input mappings are different.
#if !defined(_WIN32)
#if SDL_VERSION_ATLEAST(2,0,9)
	SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "0");
#endif
#endif

	// Initialize the GameController subsystem so SDL's mapping database is
	// available, while keeping ES-X on raw SDL_Joystick events for compatibility
	// with existing es_input.cfg files and RetroPie physical button/axis IDs.
	SDL_InitSubSystem(SDL_INIT_GAMECONTROLLER);
	SDL_GameControllerEventState(SDL_IGNORE);
	SDL_JoystickEventState(SDL_ENABLE);

#if SDL_VERSION_ATLEAST(2,0,9)
	// Optional user mappings can extend or override the mappings compiled into SDL.
	std::string controllerMappings = Utils::FileSystem::getHomePath();
	controllerMappings += "/.emulationstation/es_controller_mappings.cfg";
	if(Utils::FileSystem::exists(controllerMappings))
	{
		int loadedMappings = SDL_GameControllerAddMappingsFromFile(controllerMappings.c_str());
		if(loadedMappings < 0)
		{
			LOG(LogWarning) << "Could not load SDL controller mappings from '"
			                << controllerMappings << "': " << SDL_GetError();
		}
		else if(loadedMappings > 0)
		{
			LOG(LogInfo) << "Loaded " << loadedMappings
			             << " custom SDL controller mapping(s) from '"
			             << controllerMappings << "'.";
		}
	}
#endif

	// first, open all currently present joysticks
	int numJoysticks = SDL_NumJoysticks();
	for(int i = 0; i < numJoysticks; i++)
	{
		addJoystickByDeviceIndex(i);
	}

	// ✅ A partir de aquí, sí: notificar hotplug real
	mSuppressHotplugPopups = false;

	mKeyboardInputConfig = new InputConfig(DEVICE_KEYBOARD, "Keyboard", KEYBOARD_GUID_STRING);
	loadInputConfig(mKeyboardInputConfig);

	SDL_USER_CECBUTTONDOWN = SDL_RegisterEvents(2);
	SDL_USER_CECBUTTONUP   = SDL_USER_CECBUTTONDOWN + 1;
	CECInput::init();
	mCECInputConfig = new InputConfig(DEVICE_CEC, "CEC", CEC_GUID_STRING);
	loadInputConfig(mCECInputConfig);
}

void InputManager::addJoystickByDeviceIndex(int id)
{
	assert(id > -1);
	assert(id < SDL_NumJoysticks());
	// open joystick & add to our list
	SDL_Joystick* joy = SDL_JoystickOpen(id);
	assert(joy);

	// add it to our list so we can close it again later
	SDL_JoystickID joyId = SDL_JoystickInstanceID(joy);

	// SDL_INIT_GAMECONTROLLER can leave an SDL_JOYDEVICEADDED event queued for
	// a device already opened during the initial scan. Do not replace/leak the
	// existing joystick/config objects if the same instance is reported again.
	if(mJoysticks.find(joyId) != mJoysticks.end())
	{
		LOG(LogDebug) << "Ignoring duplicate joystick add for instance ID " << joyId << ".";
		SDL_JoystickClose(joy); // balance the extra SDL_JoystickOpen reference
		return;
	}

	mJoysticks[joyId] = joy;

	// ✅ Cachear nombre por instance id (para REMOVED seguro)
	const char* nameC = SDL_JoystickName(joy);
	mJoystickNameCache[joyId] = (nameC && nameC[0]) ? std::string(nameC) : std::string("Controller");

	char guid[65];
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy), guid, 65);

	// create the InputConfig
	mInputConfigs[joyId] = new InputConfig(joyId, SDL_JoystickName(joy), guid);

	// add Vendor and Product IDs
	mInputConfigs[joyId]->setVendorId(SDL_JoystickGetVendor(joy));
	mInputConfigs[joyId]->setProductId(SDL_JoystickGetProduct(joy));

	if(!loadInputConfig(mInputConfigs[joyId]))
	{
		bool autoConfigured = false;
#if SDL_VERSION_ATLEAST(2,0,9)
		autoConfigured = autoConfigureFromSDLGameController(id, joyId, mInputConfigs[joyId]);
#endif

		if(autoConfigured)
		{
			LOG(LogInfo) << "Added SDL auto-configured joystick '" << SDL_JoystickName(joy)
			             << "' (GUID: " << guid << ", instance ID: " << joyId
			             << ", device index: " << id << ").";
		}
		else
		{
			LOG(LogInfo) << "Added unconfigured joystick '" << SDL_JoystickName(joy)
			             << "' (GUID: " << guid << ", instance ID: " << joyId
			             << ", device index: " << id << ").";
		}
	}
	else
	{
		LOG(LogInfo) << "Added known joystick '" << SDL_JoystickName(joy)
		             << "' (instance ID: " << joyId << ", device index: " << id << ")";
	}

	// set up the prevAxisValues
	int numAxes = SDL_JoystickNumAxes(joy);
	mPrevAxisValues[joyId] = new int[numAxes];
	std::fill(mPrevAxisValues[joyId], mPrevAxisValues[joyId] + numAxes, 0); // initialize array to 0
}

void InputManager::removeJoystickByJoystickID(SDL_JoystickID joyId)
{
	assert(joyId != -1);

	// delete old prevAxisValues
	auto axisIt = mPrevAxisValues.find(joyId);
	if(axisIt != mPrevAxisValues.end())
	{
		delete[] axisIt->second;
		mPrevAxisValues.erase(axisIt);
	}

	// delete old InputConfig
	auto it = mInputConfigs.find(joyId);
	if(it != mInputConfigs.end())
	{
		delete it->second;
		mInputConfigs.erase(it);
	}

	// close the joystick
	auto joyIt = mJoysticks.find(joyId);
	if(joyIt != mJoysticks.end())
	{
		LOG(LogInfo) << "Removed joystick '" << SDL_JoystickName(joyIt->second)
		             << "' (instance ID: " << joyId << ")";
		SDL_JoystickClose(joyIt->second);
		mJoysticks.erase(joyIt);
	}

	// ✅ limpiar cache al final
	mJoystickNameCache.erase(joyId);
}

void InputManager::deinit()
{
	if(!initialized())
		return;

	for(auto iter = mJoysticks.cbegin(); iter != mJoysticks.cend(); iter++)
	{
		SDL_JoystickClose(iter->second);
	}
	mJoysticks.clear();

	for(auto iter = mInputConfigs.cbegin(); iter != mInputConfigs.cend(); iter++)
	{
		delete iter->second;
	}
	mInputConfigs.clear();

	for(auto iter = mPrevAxisValues.cbegin(); iter != mPrevAxisValues.cend(); iter++)
	{
		delete[] iter->second;
	}
	mPrevAxisValues.clear();

	mJoystickNameCache.clear();
	mSuppressHotplugPopups = false;

	if(mKeyboardInputConfig != NULL)
	{
		delete mKeyboardInputConfig;
		mKeyboardInputConfig = NULL;
	}

	if(mCECInputConfig != NULL)
	{
		delete mCECInputConfig;
		mCECInputConfig = NULL;
	}

	CECInput::deinit();

	SDL_JoystickEventState(SDL_DISABLE);
	SDL_GameControllerEventState(SDL_DISABLE);
	SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
}

int InputManager::getNumJoysticks()
{
	return (int)mJoysticks.size();
}

int InputManager::getAxisCountByDevice(SDL_JoystickID id)
{
	return SDL_JoystickNumAxes(mJoysticks[id]);
}

int InputManager::getButtonCountByDevice(SDL_JoystickID id)
{
	if(id == DEVICE_KEYBOARD)
		return 120; // it's a lot, okay.
	else if(id == DEVICE_CEC)
#ifdef HAVE_CECLIB
		return CEC::CEC_USER_CONTROL_CODE_MAX;
#else
		return 0;
#endif
	else
		return SDL_JoystickNumButtons(mJoysticks[id]);
}

InputConfig* InputManager::getInputConfigByDevice(int device)
{
	if(device == DEVICE_KEYBOARD)
		return mKeyboardInputConfig;
	else if(device == DEVICE_CEC)
		return mCECInputConfig;
	else
		return mInputConfigs[device];
}

bool InputManager::parseEvent(const SDL_Event& ev, Window* window)
{
	bool causedEvent = false;

	switch(ev.type)
	{
	case SDL_JOYAXISMOTION:
		// if it switched boundaries
		if((abs(ev.jaxis.value) > DEADZONE) != (abs(mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis]) > DEADZONE))
		{
			int normValue;
			if(abs(ev.jaxis.value) <= DEADZONE)
				normValue = 0;
			else if(ev.jaxis.value > 0)
				normValue = 1;
			else
				normValue = -1;

			window->input(getInputConfigByDevice(ev.jaxis.which),
				Input(ev.jaxis.which, TYPE_AXIS, ev.jaxis.axis, normValue, false));
			causedEvent = true;
		}

		mPrevAxisValues[ev.jaxis.which][ev.jaxis.axis] = ev.jaxis.value;
		return causedEvent;

	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		window->input(getInputConfigByDevice(ev.jbutton.which),
			Input(ev.jbutton.which, TYPE_BUTTON, ev.jbutton.button, ev.jbutton.state == SDL_PRESSED, false));
		return true;

	case SDL_JOYHATMOTION:
		window->input(getInputConfigByDevice(ev.jhat.which),
			Input(ev.jhat.which, TYPE_HAT, ev.jhat.hat, ev.jhat.value, false));
		return true;

	case SDL_KEYDOWN:
		if(ev.key.repeat)
			return false;

		if(ev.key.keysym.sym == SDLK_F4)
		{
			SDL_Event* quit = new SDL_Event();
			quit->type = SDL_QUIT;
			SDL_PushEvent(quit);
			return false;
		}

		window->input(
			getInputConfigByDevice(DEVICE_KEYBOARD),
			Input(DEVICE_KEYBOARD, TYPE_KEY, ev.key.keysym.sym, 1, false)
		);
		return true;

	case SDL_KEYUP:
		window->input(getInputConfigByDevice(DEVICE_KEYBOARD),
			Input(DEVICE_KEYBOARD, TYPE_KEY, ev.key.keysym.sym, 0, false));
		return true;

	case SDL_TEXTINPUT:
		window->textInput(ev.text.text);
		break;

	case SDL_JOYDEVICEADDED:
	{
		// ev.jdevice.which is a device index
		int deviceIndex = ev.jdevice.which;

#if SDL_VERSION_ATLEAST(2,0,4)
		SDL_JoystickID instanceId = SDL_JoystickGetDeviceInstanceID(deviceIndex);
		if(instanceId != -1 && mJoysticks.find(instanceId) != mJoysticks.end())
		{
			LOG(LogDebug) << "Ignoring duplicate SDL_JOYDEVICEADDED for instance ID "
			              << instanceId << ".";
			return true;
		}
#endif

		addJoystickByDeviceIndex(deviceIndex);

		// ✅ Notificar SOLO hotplug real (no durante init/scan)
		if(!mSuppressHotplugPopups && window != nullptr &&
		   Settings::getInstance()->getBool("ShowControllerNotifications"))
		{
			std::string joyName = "Controller";

#if SDL_VERSION_ATLEAST(2,0,4)
			SDL_JoystickID popupInstanceId = SDL_JoystickGetDeviceInstanceID(deviceIndex);
			auto it = mJoystickNameCache.find(popupInstanceId);
			if(it != mJoystickNameCache.end() && !it->second.empty())
				joyName = it->second;
			else
#endif
			{
				const char* joyNameC = SDL_JoystickNameForIndex(deviceIndex);
				if(joyNameC && joyNameC[0])
					joyName = joyNameC;
			}

			window->setInfoPopup(new GuiInfoPopup(
				window,
				buildControllerPopupMessage(true, joyName),
				2500
			));
		}
		return true;
	}

	case SDL_JOYDEVICEREMOVED:
	{
		// ev.jdevice.which is an SDL_JoystickID (instance ID)
		SDL_JoystickID instanceId = ev.jdevice.which;

		// ✅ Popup ANTES de remover/cerrar
		if(!mSuppressHotplugPopups && window != nullptr &&
		   Settings::getInstance()->getBool("ShowControllerNotifications"))
		{
			std::string joyName = "Controller";
			auto it = mJoystickNameCache.find(instanceId);
			if(it != mJoystickNameCache.end() && !it->second.empty())
				joyName = it->second;

			window->setInfoPopup(new GuiInfoPopup(
				window,
				buildControllerPopupMessage(false, joyName),
				2500
			));
		}

		removeJoystickByJoystickID(instanceId);
		return true;
	}
	}

	if((ev.type == (unsigned int)SDL_USER_CECBUTTONDOWN) || (ev.type == (unsigned int)SDL_USER_CECBUTTONUP))
	{
		window->input(getInputConfigByDevice(DEVICE_CEC),
			Input(DEVICE_CEC, TYPE_CEC_BUTTON, ev.user.code,
			ev.type == (unsigned int)SDL_USER_CECBUTTONDOWN, false));
		return true;
	}

	return false;
}

bool InputManager::loadInputConfig(InputConfig* config)
{
	std::string path = getConfigPath();
	if(!Utils::FileSystem::exists(path))
		return false;

	pugi::xml_document doc;
	pugi::xml_parse_result res = doc.load_file(path.c_str());

	if(!res)
	{
		LOG(LogError) << "Error parsing input config: " << res.description();
		return false;
	}

	pugi::xml_node root = doc.child("inputList");
	if(!root)
		return false;

	pugi::xml_node configNode = root.find_child_by_attribute("inputConfig", "deviceGUID", config->getDeviceGUIDString().c_str());
	if(!configNode)
		configNode = root.find_child_by_attribute("inputConfig", "deviceName", config->getDeviceName().c_str());
	if(!configNode)
		return false;

	config->loadFromXML(configNode);
	return true;
}

void InputManager::loadDefaultKBConfig()
{
	InputConfig* cfg = getInputConfigByDevice(DEVICE_KEYBOARD);

	cfg->clear();
	cfg->mapInput("up", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_UP, 1, true));
	cfg->mapInput("down", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_DOWN, 1, true));
	cfg->mapInput("left", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFT, 1, true));
	cfg->mapInput("right", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHT, 1, true));

	cfg->mapInput("a", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RETURN, 1, true));
	cfg->mapInput("b", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_ESCAPE, 1, true));
	cfg->mapInput("start", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F1, 1, true));
	cfg->mapInput("select", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_F2, 1, true));

	cfg->mapInput("leftshoulder", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_LEFTBRACKET, 1, true));
	cfg->mapInput("rightshoulder", Input(DEVICE_KEYBOARD, TYPE_KEY, SDLK_RIGHTBRACKET, 1, true));
}

void InputManager::writeDeviceConfig(InputConfig* config)
{
	assert(initialized());

	std::string path = getConfigPath();

	pugi::xml_document doc;

	if(Utils::FileSystem::exists(path))
	{
		// merge files
		pugi::xml_parse_result result = doc.load_file(path.c_str());
		if(!result)
		{
			LOG(LogError) << "Error parsing input config: " << result.description();
		}
		else
		{
			// successfully loaded, delete the old entry if it exists
			pugi::xml_node root = doc.child("inputList");
			if(root)
			{
				// if inputAction @type=onfinish is set, let onfinish command take care for creating input configuration.
				// we just put the input configuration into a temporary input config file.
				pugi::xml_node actionnode = root.find_child_by_attribute("inputAction", "type", "onfinish");
				if(actionnode)
				{
					path = getTemporaryConfigPath();
					doc.reset();
					root = doc.append_child("inputList");
				}
				else
				{
					pugi::xml_node oldEntry = root.find_child_by_attribute("inputConfig", "deviceGUID",
											  config->getDeviceGUIDString().c_str());
					if(oldEntry)
					{
						root.remove_child(oldEntry);
					}
					oldEntry = root.find_child_by_attribute("inputConfig", "deviceName",
															config->getDeviceName().c_str());
					if(oldEntry)
					{
						root.remove_child(oldEntry);
					}
				}
			}
		}
	}

	pugi::xml_node root = doc.child("inputList");
	if(!root)
		root = doc.append_child("inputList");

	config->writeToXML(root);
	doc.save_file(path.c_str());

	Scripting::fireEvent("config-changed");
	Scripting::fireEvent("controls-changed");

	// execute any onFinish commands and re-load the config for changes
	doOnFinish();
	loadInputConfig(config);
}

void InputManager::doOnFinish()
{
	assert(initialized());
	std::string path = getConfigPath();
	pugi::xml_document doc;

	if(Utils::FileSystem::exists(path))
	{
		pugi::xml_parse_result result = doc.load_file(path.c_str());
		if(!result)
		{
			LOG(LogError) << "Error parsing input config: " << result.description();
		}
		else
		{
			pugi::xml_node root = doc.child("inputList");
			if(root)
			{
				root = root.find_child_by_attribute("inputAction", "type", "onfinish");
				if(root)
				{
					for(pugi::xml_node command = root.child("command"); command;
						command = command.next_sibling("command"))
					{
						std::string tocall = command.text().get();

						LOG(LogInfo) << "	" << tocall;
						std::cout << "==============================================\ninput config finish command:\n";
						int exitCode = runSystemCommand(tocall);
						std::cout << "==============================================\n";

						if(exitCode != 0)
						{
							LOG(LogWarning) << "...launch terminated with nonzero exit code " << exitCode << "!";
						}
					}
				}
			}
		}
	}
}

std::string InputManager::getConfigPath()
{
	std::string path = Utils::FileSystem::getHomePath();
	path += "/.emulationstation/es_input.cfg";
	return path;
}

std::string InputManager::getTemporaryConfigPath()
{
	std::string path = Utils::FileSystem::getHomePath();
	path += "/.emulationstation/es_temporaryinput.cfg";
	return path;
}

bool InputManager::initialized() const
{
	return mKeyboardInputConfig != NULL;
}

int InputManager::getNumConfiguredDevices()
{
	int num = 0;
	for(auto it = mInputConfigs.cbegin(); it != mInputConfigs.cend(); it++)
	{
		if(it->second->isConfigured())
			num++;
	}

	if(mKeyboardInputConfig->isConfigured())
		num++;

	if(mCECInputConfig->isConfigured())
		num++;

	return num;
}

std::string InputManager::getDeviceGUIDString(int deviceId)
{
	if(deviceId == DEVICE_KEYBOARD)
		return KEYBOARD_GUID_STRING;

	if(deviceId == DEVICE_CEC)
		return CEC_GUID_STRING;

	auto it = mJoysticks.find(deviceId);
	if(it == mJoysticks.cend())
	{
		LOG(LogError) << "getDeviceGUIDString - deviceId " << deviceId << " not found!";
		return "something went horribly wrong";
	}

	char guid[65];
	SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(it->second), guid, 65);
	return std::string(guid);
}
