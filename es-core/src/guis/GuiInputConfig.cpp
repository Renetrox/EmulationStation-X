#include "guis/GuiInputConfig.h"

#include "components/ButtonComponent.h"
#include "components/MenuComponent.h"
#include "guis/GuiMsgBox.h"
#include "InputManager.h"
#include "Log.h"
#include "Window.h"
#include "Settings.h"

// ✅ ES-X locale
#include "LocaleES.h"

#include <sstream>
#include <cstdio>

static inline std::string tr(const char* key)
{
	return es_translate(std::string(key));
}

namespace
{
	inline bool isMenuDark()
	{
		return Settings::getInstance()->getBool("MenuDark");
	}

	inline const char* getFramePath()
	{
		return isMenuDark() ? ":/frame_dark.png" : ":/frame.png";
	}

	inline unsigned int getTitleColor()
	{
		return isMenuDark() ? 0xFFFFFFFF : 0x555555FF;
	}

	inline unsigned int getSubtitleColor()
	{
		return isMenuDark() ? 0xD0D0D0FF : 0x555555FF;
	}

	inline unsigned int getHintColor()
	{
		return isMenuDark() ? 0xA8A8A8FF : 0x999999FF;
	}

	inline unsigned int getLabelColor()
	{
		return isMenuDark() ? 0xE0E0E0FF : 0x777777FF;
	}

	inline unsigned int getValueColor()
	{
		return isMenuDark() ? 0xBEBEBEFF : 0x999999FF;
	}

	inline unsigned int getActiveColor()
	{
		return isMenuDark() ? 0xFFFFFFFF : 0x656565FF;
	}

	inline unsigned int getErrorColor()
	{
		return isMenuDark() ? 0xFF9090FF : 0x656565FF;
	}

	inline unsigned int getIconColor()
	{
		return isMenuDark() ? 0xE0E0E0FF : 0x777777FF;
	}
}

struct InputConfigStructure
{
	const char* name;
	const bool  skippable;
	const char* dispName;
	const char* icon;
};

static const int inputCount = 25;
static const InputConfigStructure GUI_INPUT_CONFIG_LIST[inputCount] =
{
	{ "Up",               false, "D-PAD UP",           ":/help/dpad_up.svg" },
	{ "Down",             false, "D-PAD DOWN",         ":/help/dpad_down.svg" },
	{ "Left",             false, "D-PAD LEFT",         ":/help/dpad_left.svg" },
	{ "Right",            false, "D-PAD RIGHT",        ":/help/dpad_right.svg" },
	{ "Start",            true,  "START",              ":/help/button_start.svg" },
	{ "Select",           true,  "SELECT",             ":/help/button_select.svg" },
	{ "A",                false, "BUTTON A / EAST",    ":/help/buttons_east.svg" },
	{ "B",                true,  "BUTTON B / SOUTH",   ":/help/buttons_south.svg" },
	{ "X",                true,  "BUTTON X / NORTH",   ":/help/buttons_north.svg" },
	{ "Y",                true,  "BUTTON Y / WEST",    ":/help/buttons_west.svg" },
	{ "LeftShoulder",     true,  "LEFT SHOULDER",      ":/help/button_l.svg" },
	{ "RightShoulder",    true,  "RIGHT SHOULDER",     ":/help/button_r.svg" },
	{ "LeftTrigger",      true,  "LEFT TRIGGER",       ":/help/button_lt.svg" },
	{ "RightTrigger",     true,  "RIGHT TRIGGER",      ":/help/button_rt.svg" },
	{ "LeftThumb",        true,  "LEFT THUMB",         ":/help/analog_thumb.svg" },
	{ "RightThumb",       true,  "RIGHT THUMB",        ":/help/analog_thumb.svg" },
	{ "LeftAnalogUp",     true,  "LEFT ANALOG UP",     ":/help/analog_up.svg" },
	{ "LeftAnalogDown",   true,  "LEFT ANALOG DOWN",   ":/help/analog_down.svg" },
	{ "LeftAnalogLeft",   true,  "LEFT ANALOG LEFT",   ":/help/analog_left.svg" },
	{ "LeftAnalogRight",  true,  "LEFT ANALOG RIGHT",  ":/help/analog_right.svg" },
	{ "RightAnalogUp",    true,  "RIGHT ANALOG UP",    ":/help/analog_up.svg" },
	{ "RightAnalogDown",  true,  "RIGHT ANALOG DOWN",  ":/help/analog_down.svg" },
	{ "RightAnalogLeft",  true,  "RIGHT ANALOG LEFT",  ":/help/analog_left.svg" },
	{ "RightAnalogRight", true,  "RIGHT ANALOG RIGHT", ":/help/analog_right.svg" },
	{ "HotKeyEnable",     true,  "HOTKEY ENABLE",      ":/help/button_hotkey.svg" }
};

#define HOLD_TO_SKIP_MS 1000

GuiInputConfig::GuiInputConfig(Window* window, InputConfig* target, bool reconfigureAll, const std::function<void()>& okCallback) : GuiComponent(window),
	mBackground(window, getFramePath()), mGrid(window, Vector2i(1, 7)),
	mTargetConfig(target), mHoldingInput(false), mSkipAxis(false), mBusyAnim(window)
{
	LOG(LogInfo) << "Configuring device " << target->getDeviceId() << " (" << target->getDeviceName() << ").";

	if(reconfigureAll)
		target->clear();

	mConfiguringAll = reconfigureAll;
	mConfiguringRow = mConfiguringAll;

	addChild(&mBackground);
	addChild(&mGrid);

	// 0 is a spacer row
	mGrid.setEntry(std::make_shared<GuiComponent>(mWindow), Vector2i(0, 0), false);

	mTitle = std::make_shared<TextComponent>(mWindow, tr("CONFIGURING"), Font::get(FONT_SIZE_LARGE), getTitleColor(), ALIGN_CENTER);
	mGrid.setEntry(mTitle, Vector2i(0, 1), false, true);

	std::stringstream ss;
	if(target->getDeviceId() == DEVICE_KEYBOARD)
		ss << tr("KEYBOARD");
	else if(target->getDeviceId() == DEVICE_CEC)
		ss << tr("CEC");
	else
	{
		char buf[64];
		std::snprintf(buf, sizeof(buf), "%s %d", tr("GAMEPAD").c_str(), (target->getDeviceId() + 1));
		ss << buf;
	}

	mSubtitle1 = std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(ss.str()), Font::get(FONT_SIZE_MEDIUM), getSubtitleColor(), ALIGN_CENTER);
	mGrid.setEntry(mSubtitle1, Vector2i(0, 2), false, true);

	mSubtitle2 = std::make_shared<TextComponent>(mWindow, tr("HOLD ANY BUTTON TO SKIP"), Font::get(FONT_SIZE_SMALL), getHintColor(), ALIGN_CENTER);
	mSubtitle2->setOpacity(GUI_INPUT_CONFIG_LIST[0].skippable * 255);
	mGrid.setEntry(mSubtitle2, Vector2i(0, 3), false, true);

	// 4 is a spacer row

	mList = std::make_shared<ComponentList>(mWindow);
	mGrid.setEntry(mList, Vector2i(0, 5), true, true);
	for(int i = 0; i < inputCount; i++)
	{
		ComponentListRow row;

		auto icon = std::make_shared<ImageComponent>(mWindow);
		icon->setImage(GUI_INPUT_CONFIG_LIST[i].icon);
		icon->setColorShift(getIconColor());
		icon->setResize(0, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight() * 1.25f);
		row.addElement(icon, false);

		auto spacer = std::make_shared<GuiComponent>(mWindow);
		spacer->setSize(16, 0);
		row.addElement(spacer, false);

		auto text = std::make_shared<TextComponent>(mWindow, tr(GUI_INPUT_CONFIG_LIST[i].dispName), Font::get(FONT_SIZE_MEDIUM), getLabelColor());
		row.addElement(text, true);

		auto mapping = std::make_shared<TextComponent>(mWindow, tr("-NOT DEFINED-"), Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT), getValueColor(), ALIGN_RIGHT);
		setNotDefined(mapping);
		row.addElement(mapping, true);
		mMappings.push_back(mapping);

		row.input_handler = [this, i, mapping](InputConfig* config, Input input) -> bool
		{
			if(config != mTargetConfig)
				return false;

			if(!mConfiguringRow)
			{
				if(config->isMappedTo("a", input) && input.value)
				{
					mList->stopScrolling();
					mConfiguringRow = true;
					setPress(mapping);
					return true;
				}

				return false;
			}

			if(filterTrigger(input, config, i))
				return true;

			if(input.value != 0)
			{
				if(mHoldingInput)
					return true;

				mHoldingInput = true;
				mHeldInput = input;
				mHeldTime = 0;
				mHeldInputId = i;

				return true;
			}else{
				if(!mHoldingInput || mHeldInput.device != input.device || mHeldInput.id != input.id || mHeldInput.type != input.type)
					return true;

				mHoldingInput = false;

				if(assign(mHeldInput, i))
					rowDone();

				return true;
			}
		};

		mList->addRow(row);
	}

	mList->setCursorChangedCallback([this](CursorState /*state*/) {
		bool skippable = GUI_INPUT_CONFIG_LIST[mList->getCursorId()].skippable;
		mSubtitle2->setOpacity(skippable * 255);
	});

	if(mConfiguringAll)
		setPress(mMappings.front());

	std::vector< std::shared_ptr<ButtonComponent> > buttons;
	std::function<void()> okFunction = [this, okCallback] {
		InputManager::getInstance()->writeDeviceConfig(mTargetConfig);
		if(okCallback)
			okCallback();
		delete this;
	};

	buttons.push_back(std::make_shared<ButtonComponent>(mWindow, "OK", "ok", [this, okFunction] {
		Input input;
		if (!mTargetConfig->getInputByName("HotKeyEnable", &input)) {
			mWindow->pushGui(new GuiMsgBox(mWindow,
				tr("YOU DIDN'T CHOOSE A HOTKEY ENABLE BUTTON. THIS IS REQUIRED FOR EXITING GAMES WITH A CONTROLLER. DO YOU WANT TO USE THE SELECT BUTTON DEFAULT ? PLEASE ANSWER YES TO USE SELECT OR NO TO NOT SET A HOTKEY ENABLE BUTTON."),
				tr("YES"), [this, okFunction] {
					Input input;
					mTargetConfig->getInputByName("Select", &input);
					mTargetConfig->mapInput("HotKeyEnable", input);
					okFunction();
					},
				tr("NO"), [this, okFunction] {
					mTargetConfig->mapInput("HotKeyEnable", Input(DEVICE_KEYBOARD, TYPE_KEY, 0, 1, true));
					okFunction();
				}
			));
		} else {
			okFunction();
		}
	}));

	mButtonGrid = makeButtonGrid(mWindow, buttons);
	mGrid.setEntry(mButtonGrid, Vector2i(0, 6), true, false);

	setSize(Renderer::getScreenWidth() * 0.6f, Renderer::getScreenHeight() * 0.75f);
	setPosition((Renderer::getScreenWidth() - mSize.x()) / 2, (Renderer::getScreenHeight() - mSize.y()) / 2);
}

void GuiInputConfig::onSizeChanged()
{
	mBackground.fitTo(mSize, Vector3f::Zero(), Vector2f(-32, -32));

	mGrid.setSize(mSize);

	mGrid.setRowHeightPerc(1, mTitle->getFont()->getHeight()*0.75f / mSize.y());
	mGrid.setRowHeightPerc(2, mSubtitle1->getFont()->getHeight() / mSize.y());
	mGrid.setRowHeightPerc(3, mSubtitle2->getFont()->getHeight() / mSize.y());
	mGrid.setRowHeightPerc(5, (mList->getRowHeight(0) * 5 + 2) / mSize.y());
	mGrid.setRowHeightPerc(6, mButtonGrid->getSize().y() / mSize.y());

	mBusyAnim.setSize(mSize);
}

void GuiInputConfig::update(int deltaTime)
{
	if(mConfiguringRow && mHoldingInput && GUI_INPUT_CONFIG_LIST[mHeldInputId].skippable)
	{
		int prevSec = mHeldTime / 1000;
		mHeldTime += deltaTime;
		int curSec = mHeldTime / 1000;

		if(mHeldTime >= HOLD_TO_SKIP_MS)
		{
			setNotDefined(mMappings.at(mHeldInputId));
			clearAssignment(mHeldInputId);
			mHoldingInput = false;
			rowDone();
		}else{
			if(prevSec != curSec)
			{
				const auto& text = mMappings.at(mHeldInputId);

				char buf[128];
				int remaining = (HOLD_TO_SKIP_MS/1000) - curSec;
				std::snprintf(buf, sizeof(buf), tr("HOLD FOR %dS TO SKIP").c_str(), remaining);

				text->setText(buf);
				text->setColor(getLabelColor());
			}
		}
	}
}

void GuiInputConfig::rowDone()
{
	if(mConfiguringAll)
	{
		if(!mList->moveCursor(1))
		{
			mConfiguringAll = false;
			mConfiguringRow = false;
			mGrid.moveCursor(Vector2i(0, 1));
		}else{
			setPress(mMappings.at(mList->getCursorId()));
		}
	}else{
		mConfiguringRow = false;
	}
}

void GuiInputConfig::setPress(const std::shared_ptr<TextComponent>& text)
{
	text->setText(tr("PRESS ANYTHING"));
	text->setColor(getActiveColor());
}

void GuiInputConfig::setNotDefined(const std::shared_ptr<TextComponent>& text)
{
	text->setText(tr("-NOT DEFINED-"));
	text->setColor(getValueColor());
}

void GuiInputConfig::setAssignedTo(const std::shared_ptr<TextComponent>& text, Input input)
{
	text->setText(Utils::String::toUpper(input.string()));
	text->setColor(getLabelColor());
}

void GuiInputConfig::error(const std::shared_ptr<TextComponent>& text, const std::string& /*msg*/)
{
	text->setText(tr("ALREADY TAKEN"));
	text->setColor(getErrorColor());
}

bool GuiInputConfig::assign(Input input, int inputId)
{
	if(mTargetConfig->getMappedTo(input).size() > 0 && !mTargetConfig->isMappedTo(GUI_INPUT_CONFIG_LIST[inputId].name, input) && strcmp(GUI_INPUT_CONFIG_LIST[inputId].name, "HotKeyEnable") != 0)
	{
		error(mMappings.at(inputId), "Already mapped!");
		return false;
	}

	setAssignedTo(mMappings.at(inputId), input);

	input.configured = true;
	mTargetConfig->mapInput(GUI_INPUT_CONFIG_LIST[inputId].name, input);

	LOG(LogInfo) << "  Mapping [" << input.string() << "] -> " << GUI_INPUT_CONFIG_LIST[inputId].name;

	return true;
}

void GuiInputConfig::clearAssignment(int inputId)
{
	mTargetConfig->unmapInput(GUI_INPUT_CONFIG_LIST[inputId].name);
}

bool GuiInputConfig::filterTrigger(Input input, InputConfig* config, int inputId)
{
#if defined(__linux__)
	bool isPlaystation = (
		strstr(config->getDeviceName().c_str(), "PLAYSTATION") != NULL
		|| strstr(config->getDeviceName().c_str(), "Sony Interactive") != NULL
		|| strstr(config->getDeviceName().c_str(), "PS3 Ga") != NULL
		|| strstr(config->getDeviceName().c_str(), "PS(R) Ga") != NULL
		|| strcmp(config->getDeviceGUIDString().c_str(), "030000006b1400000209000011010000") == 0
	);
	bool isAnbernic = (
		strcmp(config->getDeviceGUIDString().c_str(), "03004ab1020500000913000010010000") == 0
	);

	if((isPlaystation || isAnbernic)
		&& InputManager::getInstance()->getAxisCountByDevice(config->getDeviceId()) == 6)
	{
		if((
			(isPlaystation && (input.id == 6 || input.id == 7))
			|| (isAnbernic && (input.id == 8 || input.id == 9))
			) && input.type == TYPE_BUTTON)
		{
			mHoldingInput = false;
			return true;
		}
	}

	bool genericTrigger = !isAnbernic && (input.id == 2 || input.id == 5);
	bool anbernicTrigger = isAnbernic && (input.id == 4 || input.id == 5);

	if(input.type == TYPE_AXIS && (genericTrigger || anbernicTrigger))
	{
		const bool configuringTrigger = strstr(GUI_INPUT_CONFIG_LIST[inputId].name, "Trigger") != NULL;

		if(configuringTrigger)
		{
			// Negative-rest trigger axes (DS3-style and some clones) cross -1 before +1.
			// Arm the one-shot filter only after that negative pole is actually observed.
			if(input.value == -1)
			{
				mSkipAxis = true;
				return true;
			}
		}
		else if(mSkipAxis)
		{
			// Consume only the expected negative return-to-rest event. If the first
			// later event is anything else, it is real input and stale state is cleared.
			mSkipAxis = false;
			if(input.value == -1)
				return true;
		}
	}
#else
	(void)input;
	(void)config;
	(void)inputId;
#endif

	return false;
}
