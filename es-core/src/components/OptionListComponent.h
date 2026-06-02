#pragma once
#ifndef ES_CORE_COMPONENTS_OPTION_LIST_COMPONENT_H
#define ES_CORE_COMPONENTS_OPTION_LIST_COMPONENT_H

#include "GuiComponent.h"
#include "Log.h"
#include "Window.h"

// Used to display a list of options.
// Can select one or multiple options.

// if !multiSelect
// * <- curEntry ->

// always
// * press a -> open full list

#define OPTIONLIST_REPEAT_START_DELAY 650
#define OPTIONLIST_REPEAT_SPEED 250 // Lower is faster.

#define CHECKED_PATH ":/checkbox_checked.svg"
#define UNCHECKED_PATH ":/checkbox_unchecked.svg"

template<typename T>
class OptionListComponent : public GuiComponent
{
private:
	struct OptionListData
	{
		std::string name;
		T object;
		bool selected;
	};

	class OptionListPopup : public GuiComponent
	{
	private:
		MenuComponent mMenu;
		OptionListComponent<T>* mParent;

	public:
		OptionListPopup(Window* window, OptionListComponent<T>* parent, const std::string& title)
			: GuiComponent(window)
			, mMenu(window, title.c_str())
			, mParent(parent)
		{
			auto font = Font::get(FONT_SIZE_MEDIUM);
			ComponentListRow row;

			// for select all/none
			std::vector<ImageComponent*> checkboxes;

			for(auto it = mParent->mEntries.begin(); it != mParent->mEntries.end(); it++)
			{
				row.elements.clear();
				row.addElement(std::make_shared<TextComponent>(mWindow, Utils::String::toUpper(it->name), font, 0x777777FF), true);

				OptionListData& e = *it;

				if(mParent->mMultiSelect)
				{
					// add checkbox
					auto checkbox = std::make_shared<ImageComponent>(mWindow);
					checkbox->setImage(it->selected ? CHECKED_PATH : UNCHECKED_PATH);
					checkbox->setResize(0, font->getLetterHeight());
					row.addElement(checkbox, false);

					// input handler
					// update checkbox state & selected value
					row.makeAcceptInputHandler([this, &e, checkbox]
					{
						e.selected = !e.selected;
						checkbox->setImage(e.selected ? CHECKED_PATH : UNCHECKED_PATH);
						mParent->onSelectedChanged();
					});

					// for select all/none
					checkboxes.push_back(checkbox.get());
				}
				else
				{
					// input handler for non-multiselect
					// update selected value and close
					row.makeAcceptInputHandler([this, &e]
					{
						if(mParent->mEntries.empty())
						{
							delete this;
							return;
						}

						mParent->mEntries.at(mParent->getSelectedId()).selected = false;
						e.selected = true;
						mParent->onSelectedChanged();
						delete this;
					});
				}

				// also set cursor to this row if we're not multi-select and this row is selected
				mMenu.addRow(row, (!mParent->mMultiSelect && it->selected));
			}

			mMenu.addButton("BACK", "accept", [this] { delete this; });

			if(mParent->mMultiSelect)
			{
				mMenu.addButton("SELECT ALL", "select all", [this, checkboxes] {
					for(unsigned int i = 0; i < mParent->mEntries.size(); i++)
					{
						mParent->mEntries.at(i).selected = true;
						checkboxes.at(i)->setImage(CHECKED_PATH);
					}
					mParent->onSelectedChanged();
				});

				mMenu.addButton("SELECT NONE", "select none", [this, checkboxes] {
					for(unsigned int i = 0; i < mParent->mEntries.size(); i++)
					{
						mParent->mEntries.at(i).selected = false;
						checkboxes.at(i)->setImage(UNCHECKED_PATH);
					}
					mParent->onSelectedChanged();
				});
			}

			mMenu.setPosition((Renderer::getScreenWidth() - mMenu.getSize().x()) / 2, Renderer::getScreenHeight() * 0.15f);
			addChild(&mMenu);
		}

		bool input(InputConfig* config, Input input) override
		{
			if(config->isMappedTo("b", input) && input.value != 0)
			{
				delete this;
				return true;
			}

			return GuiComponent::input(config, input);
		}

		std::vector<HelpPrompt> getHelpPrompts() override
		{
			auto prompts = mMenu.getHelpPrompts();
			prompts.push_back(HelpPrompt("b", "back"));
			return prompts;
		}
	};

public:
	OptionListComponent(Window* window, const std::string& name, bool multiSelect = false)
		: GuiComponent(window)
		, mMultiSelect(multiSelect)
		, mKeyRepeat(false)
		, mKeyRepeatDir(0)
		, mKeyRepeatTimer(0)
		, mKeyRepeatStartDelay(OPTIONLIST_REPEAT_START_DELAY)
		, mKeyRepeatSpeed(OPTIONLIST_REPEAT_SPEED)
		, mName(name)
		, mText(window)
		, mLeftArrow(window)
		, mRightArrow(window)
	{
		auto font = Font::get(FONT_SIZE_MEDIUM, FONT_PATH_LIGHT);
		mText.setFont(font);
		mText.setColor(0x777777FF);
		mText.setHorizontalAlignment(ALIGN_CENTER);
		addChild(&mText);

		mLeftArrow.setResize(0, mText.getFont()->getLetterHeight());
		mRightArrow.setResize(0, mText.getFont()->getLetterHeight());

		if(mMultiSelect)
		{
			mRightArrow.setImage(":/arrow.svg");
			addChild(&mRightArrow);
		}
		else
		{
			mLeftArrow.setImage(":/option_arrow.svg");
			mLeftArrow.setFlipX(true);
			addChild(&mLeftArrow);

			mRightArrow.setImage(":/option_arrow.svg");
			addChild(&mRightArrow);
		}

		setSize(mLeftArrow.getSize().x() + mRightArrow.getSize().x(), font->getHeight());
	}

	// handles positioning/resizing of text and arrows
	void onSizeChanged() override
	{
		mLeftArrow.setResize(0, mText.getFont()->getLetterHeight());
		mRightArrow.setResize(0, mText.getFont()->getLetterHeight());

		if(mSize.x() < (mLeftArrow.getSize().x() + mRightArrow.getSize().x()))
			LOG(LogWarning) << "OptionListComponent too narrow!";

		mText.setSize(mSize.x() - mLeftArrow.getSize().x() - mRightArrow.getSize().x(), mText.getFont()->getHeight());

		// position
		mLeftArrow.setPosition(0, (mSize.y() - mLeftArrow.getSize().y()) / 2);
		mText.setPosition(mLeftArrow.getPosition().x() + mLeftArrow.getSize().x(), (mSize.y() - mText.getSize().y()) / 2);
		mRightArrow.setPosition(mText.getPosition().x() + mText.getSize().x(), (mSize.y() - mRightArrow.getSize().y()) / 2);
	}

	bool input(InputConfig* config, Input input) override
	{
		// ES-X / inspirado en ES-DE:
		// No ejecutar acciones en release. Los releases solo cortan repetición.
		if(input.value == 0)
		{
			if(config->isMappedLike("left", input) || config->isMappedLike("right", input))
				mKeyRepeatDir = 0;

			return GuiComponent::input(config, input);
		}

		if(config->isMappedTo("a", input))
		{
			mKeyRepeatDir = 0;
			open();
			return true;
		}

		if(!mMultiSelect)
		{
			if(mEntries.empty())
				return GuiComponent::input(config, input);

			if(config->isMappedLike("left", input))
			{
				if(mKeyRepeat)
				{
					mKeyRepeatDir = -1;
					mKeyRepeatTimer = -(mKeyRepeatStartDelay - mKeyRepeatSpeed);
				}

				moveSelection(-1);
				return true;
			}
			else if(config->isMappedLike("right", input))
			{
				if(mKeyRepeat)
				{
					mKeyRepeatDir = 1;
					mKeyRepeatTimer = -(mKeyRepeatStartDelay - mKeyRepeatSpeed);
				}

				moveSelection(1);
				return true;
			}
			else
			{
				mKeyRepeatDir = 0;
			}
		}

		return GuiComponent::input(config, input);
	}

	void update(int deltaTime) override
	{
		if(mKeyRepeat && mKeyRepeatDir != 0 && !mMultiSelect && !mEntries.empty())
		{
			mKeyRepeatTimer += deltaTime;

			while(mKeyRepeatTimer >= mKeyRepeatSpeed)
			{
				moveSelection(mKeyRepeatDir);
				mKeyRepeatTimer -= mKeyRepeatSpeed;
			}
		}

		GuiComponent::update(deltaTime);
	}

	void setKeyRepeat(bool state, int delay = OPTIONLIST_REPEAT_START_DELAY, int speed = OPTIONLIST_REPEAT_SPEED)
	{
		mKeyRepeat = state;
		mKeyRepeatStartDelay = delay;
		mKeyRepeatSpeed = speed;
	}

	int getNumEntries() const
	{
		return (int)mEntries.size();
	}

	std::vector<T> getSelectedObjects()
	{
		std::vector<T> ret;
		for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
		{
			if(it->selected)
				ret.push_back(it->object);
		}

		return ret;
	}

	T getSelected()
	{
		assert(mMultiSelect == false);
		auto selected = getSelectedObjects();
		assert(selected.size() == 1);
		return selected.at(0);
	}

	void add(const std::string& name, const T& obj, bool selected)
	{
		OptionListData e;
		e.name = name;
		e.object = obj;
		e.selected = selected;

		mEntries.push_back(e);
		onSelectedChanged();
	}

	void selectAll()
	{
		for(unsigned int i = 0; i < mEntries.size(); i++)
		{
			mEntries.at(i).selected = true;
		}
		onSelectedChanged();
	}

	void selectNone()
	{
		for(unsigned int i = 0; i < mEntries.size(); i++)
		{
			mEntries.at(i).selected = false;
		}
		onSelectedChanged();
	}

private:
	unsigned int getSelectedId()
	{
		assert(mMultiSelect == false);
		for(unsigned int i = 0; i < mEntries.size(); i++)
		{
			if(mEntries.at(i).selected)
				return i;
		}

		LOG(LogWarning) << "OptionListComponent::getSelectedId() - no selected element found, defaulting to 0";
		return 0;
	}

	void moveSelection(int direction)
	{
		if(mMultiSelect || mEntries.empty())
			return;

		unsigned int i = getSelectedId();
		int next = (int)i + direction;

		if(next < 0)
			next += (int)mEntries.size();
		else if(next >= (int)mEntries.size())
			next = 0;

		mEntries.at(i).selected = false;
		mEntries.at(next).selected = true;
		onSelectedChanged();
	}

	void open()
	{
		mWindow->pushGui(new OptionListPopup(mWindow, this, mName));
	}

	void onSelectedChanged()
	{
		if(mMultiSelect)
		{
			// display # selected
			std::stringstream ss;
			ss << getSelectedObjects().size() << " SELECTED";
			mText.setText(ss.str());
			mText.setSize(0, mText.getSize().y());
			setSize(mText.getSize().x() + mRightArrow.getSize().x() + 24, mText.getSize().y());
			if(mParent) // hack since theres no "on child size changed" callback atm...
				mParent->onSizeChanged();
		}
		else
		{
			// display currently selected + l/r cursors
			for(auto it = mEntries.cbegin(); it != mEntries.cend(); it++)
			{
				if(it->selected)
				{
					mText.setText(Utils::String::toUpper(it->name));
					mText.setSize(0, mText.getSize().y());
					setSize(mText.getSize().x() + mLeftArrow.getSize().x() + mRightArrow.getSize().x() + 24, mText.getSize().y());
					if(mParent) // hack since theres no "on child size changed" callback atm...
						mParent->onSizeChanged();
					break;
				}
			}
		}
	}

	std::vector<HelpPrompt> getHelpPrompts() override
	{
		std::vector<HelpPrompt> prompts;
		if(!mMultiSelect)
			prompts.push_back(HelpPrompt("left/right", "change"));

		prompts.push_back(HelpPrompt("a", "select"));
		return prompts;
	}

	bool mMultiSelect;

	bool mKeyRepeat;
	int mKeyRepeatDir;
	int mKeyRepeatTimer;
	int mKeyRepeatStartDelay;
	int mKeyRepeatSpeed;

	std::string mName;
	TextComponent mText;
	ImageComponent mLeftArrow;
	ImageComponent mRightArrow;

	std::vector<OptionListData> mEntries;
};

#endif // ES_CORE_COMPONENTS_OPTION_LIST_COMPONENT_H
