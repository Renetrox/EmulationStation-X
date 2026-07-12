#include "views/SystemView.h"

#include "animations/LambdaAnimation.h"
#include "guis/GuiMsgBox.h"
#include "guis/GuiInfoTextViewer.h"
#include "views/UIModeController.h"
#include "views/ViewController.h"
#include "Log.h"
#include "Scripting.h"
#include "Settings.h"
#include "SystemData.h"
#include "Window.h"
#include "LocaleES.h"
#include "Sound.h"
#include "NavigationSounds.h"

#include "components/ImageComponent.h"
#include "components/TextComponent.h"
#include "renderers/Renderer.h"
#include "resources/ResourceManager.h"
#include "utils/StringUtil.h"
#include "math/Misc.h"

#include <algorithm>
#include <sstream>
#include <cmath>
#include <unordered_map>
#include <limits>
#include <memory>

// buffer values for scrolling velocity (left, stopped, right)
const int logoBuffersLeft[]  = { -5, -2, -1 };
const int logoBuffersRight[] = {  1,  2,  5 };

// ─────────────────────────────────────────────
// Helpers: descripción resuelta desde ThemeData (system-description)
// ─────────────────────────────────────────────
static inline bool looksLikeUnresolvedPlaceholder(const std::string& s)
{
	return (s.find("${") != std::string::npos);
}

static inline std::string getResolvedThemeText(const std::shared_ptr<ThemeData>& theme,
                                               const std::string& view,
                                               const std::string& key)
{
	if (!theme) return "";

	const ThemeData::ThemeElement* elem = theme->getElement(view, key, "text");
	if (!elem) return "";

	if (!elem->has("text")) return "";

	std::string out = elem->get<std::string>("text");
	out = Utils::String::trim(out);

	if (out.empty()) return "";
	if (looksLikeUnresolvedPlaceholder(out)) return "";

	return out;
}

// Crea un componente desde el tema para un key (image/text).
static inline GuiComponent* makeThemedGuiComponent(Window* window,
                                                   const std::shared_ptr<ThemeData>& theme,
                                                   const std::string& view,
                                                   const std::string& key)
{
	if (!theme) return nullptr;

	const ThemeData::ThemeElement* elem = theme->getElement(view, key, "");
	if (!elem) return nullptr;

	GuiComponent* comp = nullptr;

	if (elem->type == "image")
		comp = new ImageComponent(window);
	else if (elem->type == "text")
		comp = new TextComponent(window);
	else
		return nullptr;

	comp->setDefaultZIndex(10);
	comp->applyTheme(theme, view, key, ThemeFlags::ALL);
	return comp;
}

// ─────────────────────────────────────────────
// Extras locales para el carrusel, sin tocar header
// ─────────────────────────────────────────────
namespace
{
	struct SystemCarouselExtras
	{
		float logoOffsetX = 0.0f;
		float logoOffsetY = 0.0f;

		float selectedLogoOffsetX = 0.0f;
		float selectedLogoOffsetY = 0.0f;

		float logoSpacingX = std::numeric_limits<float>::quiet_NaN();
		float logoSpacingY = std::numeric_limits<float>::quiet_NaN();
	};

	static std::unordered_map<const SystemView*, SystemCarouselExtras> sCarouselExtras;

	static inline SystemCarouselExtras& getCarouselExtras(const SystemView* view)
	{
		return sCarouselExtras[view];
	}

	static inline bool validSpacing(float v)
	{
		return std::isfinite(v) && v > 0.0f;
	}

	// ─────────────────────────────────────────
	// Extras locales para info de sistema
	// ─────────────────────────────────────────
	struct SystemInfoExtras
	{
		std::unique_ptr<TextComponent> gamesLabel;
		std::unique_ptr<TextComponent> gameCount;

		bool gamesLabelEnabled = false;
		bool gameCountEnabled  = false;
		bool fragmentedEnabled = false;
	};

	static std::unordered_map<const SystemView*, SystemInfoExtras> sInfoExtras;

	static inline SystemInfoExtras& getInfoExtras(const SystemView* view)
	{
		return sInfoExtras[view];
	}

	static inline void setComponentOpacity(TextComponent* comp, float value01)
	{
		if (!comp) return;
		const float clamped = std::max(0.0f, std::min(1.0f, value01));
		comp->setOpacity((unsigned char)(clamped * 255.0f));
	}

	static inline float getComponentOpacity01(TextComponent* comp)
	{
		if (!comp) return 0.0f;
		return comp->getOpacity() / 255.0f;
	}
}

	static inline void resetFrameAnimationsInExtras(const std::vector<GuiComponent*>& extras)
	{
		for (GuiComponent* extra : extras)
		{
			if (!extra)
				continue;

			ImageComponent* image =
				dynamic_cast<ImageComponent*>(extra);

			if (image)
				image->resetFrameAnimation();
		}
	}

SystemView::SystemView(Window* window) :
	IList<SystemViewData, SystemData*>(window, LIST_SCROLL_STYLE_SLOW, LIST_ALWAYS_LOOP),
	mViewNeedsReload(true),
	mSystemInfo(window, "SYSTEM INFO", Font::get(FONT_SIZE_SMALL), 0x33333300, ALIGN_CENTER),
	mInfoFocus(false)
{
	auto& loc = LocaleES::getInstance();
	loc.loadFromSettings();

	mSystemInfo.setText(loc.translate("SYSTEM INFO"));

	mCamOffset = 0;
	mExtrasCamOffset = 0;
	mExtrasFadeOpacity = 0.0f;
	mScrollSnd.reset();

	// asegurar entradas locales
	getCarouselExtras(this);
	SystemInfoExtras& infoExtras = getInfoExtras(this);
	infoExtras.gamesLabel.reset(new TextComponent(window, "", Font::get(FONT_SIZE_SMALL), 0xFFFFFFFF, ALIGN_LEFT));
	infoExtras.gameCount.reset(new TextComponent(window, "", Font::get(FONT_SIZE_SMALL), 0xFFFFFFFF, ALIGN_RIGHT));

	setSize((float)Renderer::getScreenWidth(), (float)Renderer::getScreenHeight());
	populate();
}

SystemView::~SystemView()
{
	// Limpiar extras estáticos antes del teardown global de Window/GUI.
	auto itInfo = sInfoExtras.find(this);
	if (itInfo != sInfoExtras.end())
	{
		itInfo->second.gamesLabel.reset();
		itInfo->second.gameCount.reset();
		sInfoExtras.erase(itInfo);
	}

	auto itCarousel = sCarouselExtras.find(this);
	if (itCarousel != sCarouselExtras.end())
	{
		sCarouselExtras.erase(itCarousel);
	}
}

void SystemView::populate()
{
	mEntries.clear();

	for(auto it = SystemData::sSystemVector.cbegin(); it != SystemData::sSystemVector.cend(); it++)
	{
		const std::shared_ptr<ThemeData>& theme = (*it)->getTheme();

		if(mViewNeedsReload)
			getViewElements(theme);

		if((*it)->isVisible())
		{
			Entry e;
			e.name   = (*it)->getName();
			e.object = *it;

			// init data
			e.data.infoButton = nullptr;
			e.data.infoButtonSelected = nullptr;
			e.data.hasDescription = false;
			e.data.descriptionText.clear();

			// make logo
			const ThemeData::ThemeElement* logoElem = theme->getElement("system", "logo", "image");
			if(logoElem)
			{
				std::string path        = logoElem->get<std::string>("path");
				std::string defaultPath = logoElem->has("default") ? logoElem->get<std::string>("default") : "";

				if((!path.empty() && ResourceManager::getInstance()->fileExists(path)) ||
				   (!defaultPath.empty() && ResourceManager::getInstance()->fileExists(defaultPath)))
				{
					ImageComponent* logo = new ImageComponent(mWindow, false, false);
					logo->setMaxSize(mCarousel.logoSize * mCarousel.logoScale);
					logo->applyTheme(theme, "system", "logo", ThemeFlags::PATH | ThemeFlags::COLOR);
					logo->setRotateByTargetSize(true);
					e.data.logo = std::shared_ptr<GuiComponent>(logo);
				}
			}

			if(!e.data.logo)
			{
				TextComponent* text = new TextComponent(
					mWindow,
					(*it)->getName(),
					Font::get(FONT_SIZE_LARGE),
					0x000000FF,
					ALIGN_CENTER);

				text->setSize(mCarousel.logoSize * mCarousel.logoScale);
				text->applyTheme(
					(*it)->getTheme(),
					"system",
					"logoText",
					ThemeFlags::FONT_PATH
						| ThemeFlags::FONT_SIZE
						| ThemeFlags::COLOR
						| ThemeFlags::FORCE_UPPERCASE
						| ThemeFlags::LINE_SPACING
						| ThemeFlags::TEXT);

				e.data.logo = std::shared_ptr<GuiComponent>(text);

				if(mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
				{
					text->setHorizontalAlignment(mCarousel.logoAlignment);
					text->setVerticalAlignment(ALIGN_CENTER);
				}
				else
				{
					text->setHorizontalAlignment(ALIGN_CENTER);
					text->setVerticalAlignment(mCarousel.logoAlignment);
				}
			}

			if(mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL)
			{
				if(mCarousel.logoAlignment == ALIGN_LEFT)
					e.data.logo->setOrigin(0, 0.5);
				else if(mCarousel.logoAlignment == ALIGN_RIGHT)
					e.data.logo->setOrigin(1.0, 0.5);
				else
					e.data.logo->setOrigin(0.5, 0.5);
			}
			else
			{
				if(mCarousel.logoAlignment == ALIGN_TOP)
					e.data.logo->setOrigin(0.5, 0);
				else if(mCarousel.logoAlignment == ALIGN_BOTTOM)
					e.data.logo->setOrigin(0.5, 1);
				else
					e.data.logo->setOrigin(0.5, 0.5);
			}

			Vector2f denormalized = mCarousel.logoSize * e.data.logo->getOrigin();
			e.data.logo->setPosition(denormalized.x(), denormalized.y(), 0.0);

			// delete any existing extras
			for(auto extra : e.data.backgroundExtras)
				delete extra;
			e.data.backgroundExtras.clear();

			// make background extras
			e.data.backgroundExtras = ThemeData::makeExtras((*it)->getTheme(), "system", mWindow);

			// ─────────────────────────────────────────────
			// Descripción: SOLO como dato desde theme key "system-description"
			// ─────────────────────────────────────────────
			e.data.descriptionText = getResolvedThemeText(theme, "system", "system-description");
			e.data.hasDescription  = !e.data.descriptionText.empty();

			// ─────────────────────────────────────────────
			// infoButton (idle/focused) temable
			// ─────────────────────────────────────────────
			if (e.data.hasDescription)
			{
				GuiComponent* idle = makeThemedGuiComponent(mWindow, theme, "system", "infoButton");
				GuiComponent* sel  = makeThemedGuiComponent(mWindow, theme, "system", "infoButtonSelected");

				if (idle)
				{
					e.data.infoButton = idle;
					e.data.backgroundExtras.push_back(idle);
				}
				if (sel)
				{
					e.data.infoButtonSelected = sel;
					e.data.infoButtonSelected->setVisible(false);
					e.data.backgroundExtras.push_back(sel);
				}
			}

			// sort the extras by z-index
			std::stable_sort(
				e.data.backgroundExtras.begin(),
				e.data.backgroundExtras.end(),
				[](GuiComponent* a, GuiComponent* b)
				{
					return b->getZIndex() > a->getZIndex();
				});

			this->add(e);
		}
	}

	if(mEntries.size() == 0)
	{
		if(!UIModeController::getInstance()->isUIModeFull())
		{
			Settings::getInstance()->setString("UIMode", "Full");
			mWindow->pushGui(new GuiMsgBox(
				mWindow,
				"The selected UI mode has nothing to show,\n returning to UI mode: FULL",
				"OK",
				nullptr));
		}
	}

	// Reset foco al reconstruir
	mInfoFocus = false;

	// ─────────────────────────────────────────────
	// FIX (wheel stability): si repoblamos, dejar offsets/animaciones en estado estable
	// ─────────────────────────────────────────────
	cancelAnimation(0);
	cancelAnimation(1);
	cancelAnimation(2);

	mCamOffset = (float)mCursor;
	mExtrasCamOffset = (float)mCursor;
	mExtrasFadeOpacity = 0.0f;

	setInfoFocus(false);
}

void SystemView::onShow()
{
	mShowing = true;

	// ES-X: al volver desde una gamelist o un juego, dejar la vista
	// de sistemas en un estado completamente estable.
	finishAnimation(0);
	cancelAnimation(1);
	cancelAnimation(2);

	mCamOffset = static_cast<float>(mCursor);
	mExtrasCamOffset = static_cast<float>(mCursor);
	mExtrasFadeOpacity = 0.0f;

	setInfoFocus(false);

	// Una salida durante el cambio de sistema puede dejar la información
	// con una opacidad intermedia. Restaurarla al reaparecer la vista.
	SystemInfoExtras& infoExtras = getInfoExtras(this);
	mSystemInfo.setOpacity(255);

	if (infoExtras.gamesLabelEnabled)
		setComponentOpacity(infoExtras.gamesLabel.get(), 1.0f);

	if (infoExtras.gameCountEnabled)
		setComponentOpacity(infoExtras.gameCount.get(), 1.0f);

	// Reiniciar únicamente la animación del sistema visible.
	if (!mEntries.empty() &&
	    mCursor >= 0 &&
	    mCursor < static_cast<int>(mEntries.size()))
	{
		resetFrameAnimationsInExtras(
			mEntries.at(mCursor).data.backgroundExtras);
	}
}

void SystemView::onHide()
{
	mShowing = false;
}

void SystemView::goToSystem(SystemData* system, bool animate)
{
	setCursor(system);

	if(!animate)
		finishAnimation(0);
}

// helper local para reproducir scroll de sistema
inline void playSystemScrollSound(SystemData* sys,
                                  const std::shared_ptr<Sound>& scrollFromCarousel)
{
	if (scrollFromCarousel)
	{
		scrollFromCarousel->play();
		return;
	}

	if (!sys)
		return;

	const std::shared_ptr<ThemeData>& theme = sys->getTheme();
	if (!theme)
		return;

	auto snd = NavigationSounds::getFromTheme(theme, "systembrowse");
	if (!snd)
		snd = NavigationSounds::getFromTheme(theme, "scroll");

	if (snd)
		snd->play();
}

// ─────────────────────────────────────────────
// InfoButton focus helpers
// ─────────────────────────────────────────────
bool SystemView::selectedHasInfo() const
{
	if (mEntries.empty()) return false;
	const SystemViewData& d = mEntries.at(mCursor).data;

	if (!d.hasDescription) return false;
	return (d.infoButton != nullptr || d.infoButtonSelected != nullptr);
}

void SystemView::setInfoFocus(bool focus)
{
	mInfoFocus = focus;

	if (mEntries.empty()) return;
	SystemViewData& d = mEntries.at(mCursor).data;

	if (!d.infoButton && !d.infoButtonSelected)
		return;

	if (d.infoButton)
		d.infoButton->setVisible(!mInfoFocus);

	if (d.infoButtonSelected)
		d.infoButtonSelected->setVisible(mInfoFocus);
}

bool SystemView::input(InputConfig* config, Input input)
{
	if(input.value != 0)
	{
		if(config->getDeviceId() == DEVICE_KEYBOARD && input.value &&
		   input.id == SDLK_r && SDL_GetModState() & KMOD_LCTRL &&
		   Settings::getInstance()->getBool("Debug"))
		{
			LOG(LogInfo) << " Reloading all";
			ViewController::get()->reloadAll();
			return true;
		}

		const bool carouselIsVertical = (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL);

		if (!mInfoFocus)
		{
			if (selectedHasInfo())
			{
				if (!carouselIsVertical && config->isMappedLike("down", input))
				{
					setInfoFocus(true);
					return true;
				}
				if (carouselIsVertical && config->isMappedLike("right", input))
				{
					setInfoFocus(true);
					return true;
				}
			}
		}
		else
		{
			if (config->isMappedTo("b", input) || config->isMappedTo("back", input))
			{
				setInfoFocus(false);
				return true;
			}

			if (!carouselIsVertical && config->isMappedLike("up", input))
			{
				setInfoFocus(false);
				return true;
			}
			if (carouselIsVertical && config->isMappedLike("left", input))
			{
				setInfoFocus(false);
				return true;
			}

			if (config->isMappedTo("a", input))
			{
				if (!mEntries.empty())
				{
					const SystemViewData& d = mEntries.at(mCursor).data;
					if (d.hasDescription && !d.descriptionText.empty())
					{
						std::string title = getSelected() ? getSelected()->getName() : "INFO";
						mWindow->pushGui(new GuiInfoTextViewer(mWindow, title, d.descriptionText));
					}
				}
				return true;
			}

			if (config->isMappedLike("left", input) ||
				config->isMappedLike("right", input) ||
				config->isMappedLike("up", input) ||
				config->isMappedLike("down", input))
			{
				return true;
			}

			if (config->isMappedTo("x", input))
				return true;

			return true;
		}

		bool moved = false;

		switch(mCarousel.type)
		{
		case VERTICAL:
		case VERTICAL_WHEEL:
			if(config->isMappedLike("up", input))
			{
				moved = listInput(-1);
				if (moved)
					playSystemScrollSound(getSelected(), mScrollSnd);
				return true;
			}
			if(config->isMappedLike("down", input))
			{
				moved = listInput(1);
				if (moved)
					playSystemScrollSound(getSelected(), mScrollSnd);
				return true;
			}
			break;

		case HORIZONTAL:
		case HORIZONTAL_WHEEL:
		default:
			if(config->isMappedLike("left", input))
			{
				moved = listInput(-1);
				if (moved)
					playSystemScrollSound(getSelected(), mScrollSnd);
				return true;
			}
			if(config->isMappedLike("right", input))
			{
				moved = listInput(1);
				if (moved)
					playSystemScrollSound(getSelected(), mScrollSnd);
				return true;
			}
			break;
		}

		if(config->isMappedTo("a", input))
		{
			if (mInfoFocus)
				return true;

			std::shared_ptr<Sound> selectSnd;

			SystemData* sys = getSelected();
			if(sys != nullptr)
			{
				const std::shared_ptr<ThemeData>& theme = sys->getTheme();
				if (theme)
				{
					selectSnd = NavigationSounds::getFromTheme(theme, "select");

					if (!selectSnd && theme->hasView("system"))
					{
						const ThemeData::ThemeElement* selectElem =
							theme->getElement("system", "selectSound", "sound");

						if(selectElem && selectElem->has("path"))
						{
							std::string path = selectElem->get<std::string>("path");
							if(!path.empty())
								selectSnd = Sound::get(path);
						}
					}
				}
			}

			if(selectSnd)
				selectSnd->play();

			stopScrolling();
			setInfoFocus(false);
			ViewController::get()->goToGameList(getSelected());
			return true;
		}

		if(config->isMappedTo("x", input))
		{
			setInfoFocus(false);
			setCursor(SystemData::getRandomSystem());
			return true;
		}
	}
	else
	{
		if(config->isMappedLike("left", input) ||
		   config->isMappedLike("right", input) ||
		   config->isMappedLike("up", input) ||
		   config->isMappedLike("down", input))
			listInput(0);

		Scripting::fireEvent("system-select", this->IList::getSelected()->getName(), "input");

		if(!UIModeController::getInstance()->isUIModeKid() &&
		   config->isMappedTo("select", input) &&
		   Settings::getInstance()->getBool("ScreenSaverControls"))
		{
			mWindow->startScreenSaver();
			mWindow->renderScreenSaver();
			return true;
		}
	}

	return GuiComponent::input(config, input);
}

void SystemView::update(int deltaTime)
{
	listUpdate(deltaTime);
	GuiComponent::update(deltaTime);

	// ES-X: actualizar los extras del sistema actualmente visible.
	// Esto permite que ImageComponent::update() ejecute animaciones
	// como pulseOpacity en imágenes extra="true".
	if (mShowing && !mEntries.empty())
	{
		const int index = mCursor;

		if (index >= 0 && index < static_cast<int>(mEntries.size()))
		{
			SystemViewData& data = mEntries.at(index).data;

			for (GuiComponent* extra : data.backgroundExtras)
			{
				if (extra)
					extra->update(deltaTime);
			}
		}
	}
}

void SystemView::onCursorChanged(const CursorState& /*state*/)
{
	updateHelpPrompts();

	// Al cambiar sistema, volver el foco al carrusel y refrescar visibilidad del botón
	setInfoFocus(false);

	// ES-X: al cambiar de sistema, reiniciar únicamente los fondos vivos
	// del sistema que acaba de quedar seleccionado.
	if (!mEntries.empty() &&
	    mCursor >= 0 &&
	    mCursor < static_cast<int>(mEntries.size()))
	{
		resetFrameAnimationsInExtras(
			mEntries.at(mCursor).data.backgroundExtras);
	}

	float startPos = mCamOffset;

	float posMax = (float)mEntries.size();
	float target = (float)mCursor;

	float endPos = target;
	float dist   = abs(endPos - startPos);

	if(abs(target + posMax - startPos) < dist)
		endPos = target + posMax;
	if(abs(target - posMax - startPos) < dist)
		endPos = target - posMax;

	cancelAnimation(1);
	cancelAnimation(2);

	std::string transition_style = Settings::getInstance()->getString("TransitionStyle");
	bool goFast                  = transition_style == "instant";

	SystemInfoExtras& infoExtras = getInfoExtras(this);

	const bool fragmented = infoExtras.fragmentedEnabled;
	const float infoStartOpacity       = mSystemInfo.getOpacity() / 255.f;
	const float gamesLabelStartOpacity = getComponentOpacity01(infoExtras.gamesLabel.get());
	const float gameCountStartOpacity  = getComponentOpacity01(infoExtras.gameCount.get());

	Animation* infoFadeOut = new LambdaAnimation(
		[infoStartOpacity, gamesLabelStartOpacity, gameCountStartOpacity, fragmented, this](float t)
		{
			if (!fragmented)
			{
				const float alpha = Math::lerp(infoStartOpacity, 0.f, t);
				mSystemInfo.setOpacity((unsigned char)(alpha * 255));
			}

			SystemInfoExtras& extras = getInfoExtras(this);
			setComponentOpacity(extras.gamesLabel.get(), Math::lerp(gamesLabelStartOpacity, 0.f, t));
			setComponentOpacity(extras.gameCount.get(), Math::lerp(gameCountStartOpacity, 0.f, t));
		},
		(int)(std::max(infoStartOpacity, 0.01f) * (goFast ? 10 : 150)));

	unsigned int gameCount = getSelected()->getDisplayedGameCount();
	LocaleES& loc = LocaleES::getInstance();

	setAnimation(
		infoFadeOut,
		0,
		[this, gameCount, &loc]()
		{
			std::stringstream ss;
			SystemInfoExtras& extras = getInfoExtras(this);

			if(!getSelected()->isGameSystem())
			{
				ss << loc.translate("CONFIGURATION");

				if (extras.gamesLabel)
					extras.gamesLabel->setText(loc.translate("CONFIGURATION"));
				if (extras.gameCount)
					extras.gameCount->setText("");
			}
			else
			{
				ss << gameCount << " "
				   << loc.translate(gameCount == 1 ? "GAME" : "GAMES")
				   << " "
				   << loc.translate("AVAILABLE");

				if (extras.gamesLabel)
					extras.gamesLabel->setText(loc.translate(gameCount == 1 ? "GAME" : "GAMES"));

				if (extras.gameCount)
				{
					std::stringstream gc;
					gc << gameCount;
					extras.gameCount->setText(gc.str());
				}
			}

			mSystemInfo.setText(ss.str());
		},
		false,
		1);

	Animation* infoFadeIn = new LambdaAnimation(
		[fragmented, this](float t)
		{
			if (!fragmented)
				mSystemInfo.setOpacity((unsigned char)(Math::lerp(0.f, 1.f, t) * 255));

			SystemInfoExtras& extras = getInfoExtras(this);
			setComponentOpacity(extras.gamesLabel.get(), t);
			setComponentOpacity(extras.gameCount.get(), t);
		},
		goFast ? 10 : 300);

	setAnimation(infoFadeIn, goFast ? 0 : 2000, nullptr, false, 2);

	if(endPos == mCamOffset && endPos == mExtrasCamOffset)
		return;

	Animation* anim;
	bool move_carousel = Settings::getInstance()->getBool("MoveCarousel");

	if(transition_style == "fade")
	{
		float startExtrasFade = mExtrasFadeOpacity;
		anim = new LambdaAnimation(
			[this, startExtrasFade, startPos, endPos, posMax, move_carousel](float t)
			{
				t -= 1;
				float f = Math::lerp(startPos, endPos, t * t * t + 1);
				if(f < 0)
					f += posMax;
				if(f >= posMax)
					f -= posMax;

				this->mCamOffset = move_carousel ? f : endPos;

				t += 1;
				if(t < 0.3f)
					this->mExtrasFadeOpacity = Math::lerp(0.0f, 1.0f, t / 0.3f + startExtrasFade);
				else if(t < 0.7f)
					this->mExtrasFadeOpacity = 1.0f;
				else
					this->mExtrasFadeOpacity = Math::lerp(1.0f, 0.0f, (t - 0.7f) / 0.3f);

				if(t > 0.5f)
					this->mExtrasCamOffset = endPos;
			},
			500);
	}
	else if(transition_style == "slide")
	{
		anim = new LambdaAnimation(
			[this, startPos, endPos, posMax, move_carousel](float t)
			{
				t -= 1;
				float f = Math::lerp(startPos, endPos, t * t * t + 1);
				if(f < 0)
					f += posMax;
				if(f >= posMax)
					f -= posMax;

				this->mCamOffset       = move_carousel ? f : endPos;
				this->mExtrasCamOffset = f;
			},
			500);
	}
	else
	{
		anim = new LambdaAnimation(
			[this, startPos, endPos, posMax, move_carousel](float t)
			{
				t -= 1;
				float f = Math::lerp(startPos, endPos, t * t * t + 1);
				if(f < 0)
					f += posMax;
				if(f >= posMax)
					f -= posMax;

				this->mCamOffset       = move_carousel ? f : endPos;
				this->mExtrasCamOffset = endPos;
			},
			move_carousel ? 500 : 1);
	}

	setAnimation(anim, 0, nullptr, false, 0);
}

void SystemView::render(const Transform4x4f& parentTrans)
{
	if(size() == 0)
		return;

	Transform4x4f trans = getTransform() * parentTrans;

	SystemInfoExtras& infoExtras = getInfoExtras(this);

	float infoMinZ = mSystemInfo.getZIndex();
	float infoMaxZ = mSystemInfo.getZIndex();

	if (infoExtras.gamesLabelEnabled && infoExtras.gamesLabel)
	{
		infoMinZ = std::min(infoMinZ, infoExtras.gamesLabel->getZIndex());
		infoMaxZ = std::max(infoMaxZ, infoExtras.gamesLabel->getZIndex());
	}

	if (infoExtras.gameCountEnabled && infoExtras.gameCount)
	{
		infoMinZ = std::min(infoMinZ, infoExtras.gameCount->getZIndex());
		infoMaxZ = std::max(infoMaxZ, infoExtras.gameCount->getZIndex());
	}

	const float lowerZ = std::min(mCarousel.zIndex, infoMinZ);
	const float upperZ = std::max(mCarousel.zIndex, infoMaxZ);

	renderExtras(trans, INT16_MIN, lowerZ);
	renderFade(trans);

	if(mCarousel.zIndex > infoMinZ)
		renderInfoBar(trans);
	else
		renderCarousel(trans);

	renderExtras(trans, lowerZ, upperZ);

	if(mCarousel.zIndex > infoMinZ)
		renderCarousel(trans);
	else
		renderInfoBar(trans);

	renderExtras(trans, upperZ, INT16_MAX);
}

std::vector<HelpPrompt> SystemView::getHelpPrompts()
{
	LocaleES& loc = LocaleES::getInstance();

	std::vector<HelpPrompt> prompts;

	const bool carouselIsVertical = (mCarousel.type == VERTICAL || mCarousel.type == VERTICAL_WHEEL);

	if (!mInfoFocus)
	{
		if (carouselIsVertical)
			prompts.push_back(HelpPrompt("up/down", loc.translate("CHOOSE")));
		else
			prompts.push_back(HelpPrompt("left/right", loc.translate("CHOOSE")));

		prompts.push_back(HelpPrompt("a", loc.translate("SELECT")));
		prompts.push_back(HelpPrompt("x", loc.translate("RANDOM")));

		if (selectedHasInfo())
		{
			if (carouselIsVertical)
				prompts.push_back(HelpPrompt("right", "INFO"));
			else
				prompts.push_back(HelpPrompt("down", "INFO"));
		}
	}
	else
	{
		prompts.push_back(HelpPrompt("a", "INFO"));
		prompts.push_back(HelpPrompt("b", loc.translate("BACK")));

		if (carouselIsVertical)
			prompts.push_back(HelpPrompt("left", loc.translate("BACK")));
		else
			prompts.push_back(HelpPrompt("up", loc.translate("BACK")));
	}

	if(!UIModeController::getInstance()->isUIModeKid() &&
	   Settings::getInstance()->getBool("ScreenSaverControls"))
	{
		prompts.push_back(HelpPrompt("select", loc.translate("LAUNCH SCREENSAVER")));
	}

	return prompts;
}

HelpStyle SystemView::getHelpStyle()
{
	HelpStyle style;
	style.applyTheme(mEntries.at(mCursor).object->getTheme(), "system");
	return style;
}

void SystemView::onThemeChanged(const std::shared_ptr<ThemeData>& /*theme*/)
{
	LOG(LogDebug) << "SystemView::onThemeChanged()";
	mViewNeedsReload = true;
	populate();
}

void SystemView::getViewElements(const std::shared_ptr<ThemeData>& theme)
{
	LOG(LogDebug) << "SystemView::getViewElements()";

	getDefaultElements();

	SystemInfoExtras& infoExtras = getInfoExtras(this);
	infoExtras.gamesLabelEnabled = false;
	infoExtras.gameCountEnabled  = false;
	infoExtras.fragmentedEnabled = false;

	if(!theme->hasView("system"))
		return;

	const ThemeData::ThemeElement* carouselElem = theme->getElement("system", "systemcarousel", "carousel");
	if(carouselElem)
		getCarouselFromTheme(carouselElem);

	const ThemeData::ThemeElement* sysInfoElem = theme->getElement("system", "systemInfo", "text");
	if(sysInfoElem)
		mSystemInfo.applyTheme(theme, "system", "systemInfo", ThemeFlags::ALL);

	const ThemeData::ThemeElement* gamesLabelElem = theme->getElement("system", "systemGames", "text");
	if (gamesLabelElem && infoExtras.gamesLabel)
	{
		infoExtras.gamesLabel->applyTheme(theme, "system", "systemGames", ThemeFlags::ALL);
		infoExtras.gamesLabelEnabled = true;
	}

	const ThemeData::ThemeElement* gameCountElem = theme->getElement("system", "systemGameCount", "text");
	if (gameCountElem && infoExtras.gameCount)
	{
		infoExtras.gameCount->applyTheme(theme, "system", "systemGameCount", ThemeFlags::ALL);
		infoExtras.gameCountEnabled = true;
	}

	infoExtras.fragmentedEnabled = (infoExtras.gamesLabelEnabled || infoExtras.gameCountEnabled);

	mViewNeedsReload = false;
}

void SystemView::renderCarousel(const Transform4x4f& trans)
{
	if(mEntries.empty())
		return;

	const SystemCarouselExtras& extraCfg = getCarouselExtras(this);

	Transform4x4f carouselTrans = trans;
	carouselTrans.translate(Vector3f(mCarousel.pos.x(), mCarousel.pos.y(), 0.0f));
	carouselTrans.translate(Vector3f(
		mCarousel.origin.x() * mCarousel.size.x() * -1,
		mCarousel.origin.y() * mCarousel.size.y() * -1,
		0.0f));

	Vector2f clipPos(carouselTrans.translation().x(), carouselTrans.translation().y());
	Renderer::pushClipRect(
		Vector2i((int)clipPos.x(), (int)clipPos.y()),
		Vector2i((int)mCarousel.size.x(), (int)mCarousel.size.y()));

	Renderer::setMatrix(carouselTrans);

	Renderer::drawRect(
		0.0f,
		0.0f,
		mCarousel.size.x(),
		mCarousel.size.y(),
		mCarousel.color,
		mCarousel.colorEnd,
		mCarousel.colorGradientHorizontal);

	Vector2f logoSpacing(0.0f, 0.0f);
	float xOff = 0.0f;
	float yOff = 0.0f;

	switch(mCarousel.type)
	{
	case VERTICAL_WHEEL:
		if(validSpacing(extraCfg.logoSpacingY))
			logoSpacing[1] = extraCfg.logoSpacingY;

		yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f - (mCamOffset * logoSpacing[1]);

		if(mCarousel.logoAlignment == ALIGN_LEFT)
			xOff = mCarousel.logoSize.x() / 10.f;
		else if(mCarousel.logoAlignment == ALIGN_RIGHT)
			xOff = mCarousel.size.x() - (mCarousel.logoSize.x() * 1.1f);
		else
			xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f;
		break;

	case VERTICAL:
		logoSpacing[1] = ((mCarousel.size.y() -
		                   (mCarousel.logoSize.y() * mCarousel.maxLogoCount)) /
		                  (mCarousel.maxLogoCount))
		               + mCarousel.logoSize.y();

		if(validSpacing(extraCfg.logoSpacingY))
			logoSpacing[1] = extraCfg.logoSpacingY;

		yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f
		     - (mCamOffset * logoSpacing[1]);

		if(mCarousel.logoAlignment == ALIGN_LEFT)
			xOff = mCarousel.logoSize.x() / 10.f;
		else if(mCarousel.logoAlignment == ALIGN_RIGHT)
			xOff = mCarousel.size.x() - (mCarousel.logoSize.x() * 1.1f);
		else
			xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f;
		break;

	case HORIZONTAL_WHEEL:
		if(validSpacing(extraCfg.logoSpacingX))
			logoSpacing[0] = extraCfg.logoSpacingX;

		xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f - (mCamOffset * logoSpacing[0]);

		if(mCarousel.logoAlignment == ALIGN_TOP)
			yOff = mCarousel.logoSize.y() / 10.f;
		else if(mCarousel.logoAlignment == ALIGN_BOTTOM)
			yOff = mCarousel.size.y() - (mCarousel.logoSize.y() * 1.1f);
		else
			yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f;
		break;

	case HORIZONTAL:
	default:
		logoSpacing[0] = ((mCarousel.size.x() -
		                   (mCarousel.logoSize.x() * mCarousel.maxLogoCount)) /
		                  (mCarousel.maxLogoCount))
		               + mCarousel.logoSize.x();

		if(validSpacing(extraCfg.logoSpacingX))
			logoSpacing[0] = extraCfg.logoSpacingX;

		xOff = (mCarousel.size.x() - mCarousel.logoSize.x()) / 2.f
		     - (mCamOffset * logoSpacing[0]);

		if(mCarousel.logoAlignment == ALIGN_TOP)
			yOff = mCarousel.logoSize.y() / 10.f;
		else if(mCarousel.logoAlignment == ALIGN_BOTTOM)
			yOff = mCarousel.size.y() - (mCarousel.logoSize.y() * 1.1f);
		else
			yOff = (mCarousel.size.y() - mCarousel.logoSize.y()) / 2.f;
		break;
	}

	// offset fino del bloque completo
	xOff += extraCfg.logoOffsetX;
	yOff += extraCfg.logoOffsetY;

	int center    = (int)(mCamOffset);
	int logoCount = Math::min(mCarousel.maxLogoCount, (int)mEntries.size());

	int bufferIndex = getScrollingVelocity() + 1;
	if(bufferIndex < 0) bufferIndex = 0;
	if(bufferIndex > 2) bufferIndex = 2;

	int bufferLeft  = logoBuffersLeft[bufferIndex];
	int bufferRight = logoBuffersRight[bufferIndex];

	if(logoCount == 1)
	{
		bufferLeft  = 0;
		bufferRight = 0;
	}

	auto renderLogo = [this, &carouselTrans, &logoSpacing, xOff, yOff, &extraCfg](int i)
	{
		if(mEntries.empty())
			return;

		int index = i;
		while(index < 0)
			index += (int)mEntries.size();
		while(index >= (int)mEntries.size())
			index -= (int)mEntries.size();

		Transform4x4f logoTrans = carouselTrans;

		float baseX = i * logoSpacing[0] + xOff;
		float baseY = i * logoSpacing[1] + yOff;

		const bool selected = (index == mCursor);

		if((mCarousel.type == HORIZONTAL || mCarousel.type == VERTICAL) &&
		   mCarousel.logoScale != 1.0f &&
		   mCarousel.scaledLogoSpacing != 0.0f)
		{
			if(mCarousel.type == HORIZONTAL)
			{
				float logoDiffX = ((logoSpacing[0] * mCarousel.logoScale) - logoSpacing[0])
					/ 2.0f * mCarousel.scaledLogoSpacing;

				if(selected)
				{
					// sin ajuste lateral extra
				}
				else if(i < mCursor || (mCursor == 0 && index > mCarousel.maxLogoCount))
				{
					baseX -= logoDiffX;
				}
				else
				{
					baseX += logoDiffX;
				}
			}
			else // VERTICAL
			{
				float logoDiffY = ((logoSpacing[1] * mCarousel.logoScale) - logoSpacing[1])
					/ 2.0f * mCarousel.scaledLogoSpacing;

				if(selected)
				{
					// sin ajuste vertical extra
				}
				else if(i < mCursor || (mCursor == 0 && index > mCarousel.maxLogoCount))
				{
					baseY -= logoDiffY;
				}
				else
				{
					baseY += logoDiffY;
				}
			}
		}

		if(selected)
		{
			baseX += extraCfg.selectedLogoOffsetX;
			baseY += extraCfg.selectedLogoOffsetY;
		}

		logoTrans.translate(Vector3f(baseX, baseY, 0.0f));

		float distance = i - mCamOffset;

		float scale = 1.0f + ((mCarousel.logoScale - 1.0f) * (1.0f - fabs(distance)));
		scale       = Math::min(mCarousel.logoScale, Math::max(1.0f, scale));
		scale      /= mCarousel.logoScale;

		float minOpacity = mCarousel.minLogoOpacity;
		if(minOpacity < 0.0f) minOpacity = 0.0f;
		if(minOpacity > 1.0f) minOpacity = 1.0f;

		int opref   = (int)Math::round(minOpacity * 255.0f);
		int opacity = (int)Math::round(opref + ((0xFF - opref) * (1.0f - fabs(distance))));
		if(opacity < opref)
			opacity = opref;

		const std::shared_ptr<GuiComponent>& comp = mEntries.at(index).data.logo;
		if(!comp)
			return;

		if(mCarousel.type == VERTICAL_WHEEL || mCarousel.type == HORIZONTAL_WHEEL)
		{
			comp->setRotationDegrees(mCarousel.logoRotation * distance);
			comp->setRotationOrigin(mCarousel.logoRotationOrigin);
		}

		comp->setScale(scale);
		comp->setOpacity((unsigned char)opacity);
		comp->render(logoTrans);
	};

	std::vector<int> activePositions;
	for(int i = center - logoCount / 2 + bufferLeft;
	    i <= center + logoCount / 2 + bufferRight;
	    i++)
	{
		int index = i;
		while(index < 0)
			index += (int)mEntries.size();
		while(index >= (int)mEntries.size())
			index -= (int)mEntries.size();

		if(index == mCursor)
			activePositions.push_back(i);
		else
			renderLogo(i);
	}

	for(auto activePos : activePositions)
		renderLogo(activePos);

	Renderer::popClipRect();
}

void SystemView::renderInfoBar(const Transform4x4f& trans)
{
	Renderer::setMatrix(trans);

	SystemInfoExtras& infoExtras = getInfoExtras(this);

	std::vector<TextComponent*> components;

	if (!infoExtras.fragmentedEnabled)
		components.push_back(&mSystemInfo);

	if (infoExtras.gamesLabelEnabled &&
	    infoExtras.gamesLabel &&
	    !infoExtras.gamesLabel->getValue().empty())
	{
		components.push_back(infoExtras.gamesLabel.get());
	}

	if (infoExtras.gameCountEnabled &&
	    infoExtras.gameCount &&
	    !infoExtras.gameCount->getValue().empty())
	{
		components.push_back(infoExtras.gameCount.get());
	}

	std::stable_sort(
		components.begin(),
		components.end(),
		[](TextComponent* a, TextComponent* b)
		{
			return a->getZIndex() < b->getZIndex();
		});

	for (TextComponent* component : components)
		component->render(trans);
}

void SystemView::renderExtras(const Transform4x4f& trans, float lower, float upper)
{
	int extrasCenter = (int)mExtrasCamOffset;

	int bufferIndex = getScrollingVelocity() + 1;
	if(bufferIndex < 0) bufferIndex = 0;
	if(bufferIndex > 2) bufferIndex = 2;

	Renderer::pushClipRect(Vector2i::Zero(), Vector2i((int)mSize.x(), (int)mSize.y()));

	for(int i = extrasCenter + logoBuffersLeft[bufferIndex];
	    i <= extrasCenter + logoBuffersRight[bufferIndex];
	    i++)
	{
		int index = i;
		while(index < 0)
			index += (int)mEntries.size();
		while(index >= (int)mEntries.size())
			index -= (int)mEntries.size();

		if(mShowing || index == mCursor)
		{
			Transform4x4f extrasTrans = trans;
			if(mCarousel.type == HORIZONTAL || mCarousel.type == HORIZONTAL_WHEEL)
				extrasTrans.translate(Vector3f((i - mExtrasCamOffset) * mSize.x(), 0, 0));
			else
				extrasTrans.translate(Vector3f(0, (i - mExtrasCamOffset) * mSize.y(), 0));

			Renderer::pushClipRect(
				Vector2i((int)extrasTrans.translation()[0], (int)extrasTrans.translation()[1]),
				Vector2i((int)mSize.x(), (int)mSize.y()));

			// ─────────────────────────────────────────────
			// FIX: NO copiar SystemViewData (evita estados raros con infoButton/wheel)
			// ─────────────────────────────────────────────
			const SystemViewData& data = mEntries.at(index).data;

			for(unsigned int j = 0; j < data.backgroundExtras.size(); j++)
			{
				GuiComponent* extra = data.backgroundExtras[j];
				if(extra->getZIndex() >= lower && extra->getZIndex() < upper)
					extra->render(extrasTrans);
			}

			Renderer::popClipRect();
		}
	}

	Renderer::popClipRect();
}

void SystemView::renderFade(const Transform4x4f& trans)
{
	if(mExtrasFadeOpacity)
	{
		unsigned int fadeColor = 0x00000000 | (unsigned char)(mExtrasFadeOpacity * 255);
		Renderer::setMatrix(trans);
		Renderer::drawRect(0.0f, 0.0f, mSize.x(), mSize.y(), fadeColor, fadeColor);
	}
}

void SystemView::getDefaultElements(void)
{
	mCarousel.type          = HORIZONTAL;
	mCarousel.logoAlignment = ALIGN_CENTER;
	mCarousel.size.x()      = mSize.x();
	mCarousel.size.y()      = 0.2325f * mSize.y();
	mCarousel.pos.x()       = 0.0f;
	mCarousel.pos.y()       = 0.5f * (mSize.y() - mCarousel.size.y());
	mCarousel.origin.x()    = 0.0f;
	mCarousel.origin.y()    = 0.0f;
	mCarousel.color         = 0xFFFFFFD8;
	mCarousel.colorEnd      = 0xFFFFFFD8;
	mCarousel.colorGradientHorizontal = true;
	mCarousel.logoScale     = 1.2f;
	mCarousel.logoRotation  = 7.5f;
	mCarousel.logoRotationOrigin.x() = -5.0f;
	mCarousel.logoRotationOrigin.y() = 0.5f;
	mCarousel.logoSize.x()  = 0.25f * mSize.x();
	mCarousel.logoSize.y()  = 0.155f * mSize.y();
	mCarousel.maxLogoCount  = 3;
	mCarousel.zIndex        = 40;

	mCarousel.minLogoOpacity    = 0.5f;
	mCarousel.scaledLogoSpacing = 0.0f;

	mScrollSnd.reset();

	// extras locales
	SystemCarouselExtras& extraCfg = getCarouselExtras(this);
	extraCfg.logoOffsetX = 0.0f;
	extraCfg.logoOffsetY = 0.0f;
	extraCfg.selectedLogoOffsetX = 0.0f;
	extraCfg.selectedLogoOffsetY = 0.0f;
	extraCfg.logoSpacingX = std::numeric_limits<float>::quiet_NaN();
	extraCfg.logoSpacingY = std::numeric_limits<float>::quiet_NaN();

	mSystemInfo.setSize(mSize.x(), mSystemInfo.getFont()->getLetterHeight() * 2.2f);
	mSystemInfo.setPosition(0, (mCarousel.pos.y() + mCarousel.size.y() - 0.2f));
	mSystemInfo.setBackgroundColor(0xDDDDDDD8);
	mSystemInfo.setRenderBackground(true);
	mSystemInfo.setFont(Font::get((int)(0.035f * mSize.y()), Font::getDefaultPath()));
	mSystemInfo.setColor(0x000000FF);
	mSystemInfo.setZIndex(50);
	mSystemInfo.setDefaultZIndex(50);

	SystemInfoExtras& infoExtras = getInfoExtras(this);

	if (infoExtras.gamesLabel)
	{
		infoExtras.gamesLabel->setSize(0.18f * mSize.x(), Font::get((int)(0.025f * mSize.y()), Font::getDefaultPath())->getLetterHeight() * 1.4f);
		infoExtras.gamesLabel->setPosition(0.74f * mSize.x(), 0.74f * mSize.y());
		infoExtras.gamesLabel->setFont(Font::get((int)(0.025f * mSize.y()), Font::getDefaultPath()));
		infoExtras.gamesLabel->setColor(0xFFFFFFFF);
		infoExtras.gamesLabel->setHorizontalAlignment(ALIGN_LEFT);
		infoExtras.gamesLabel->setZIndex(55);
		infoExtras.gamesLabel->setDefaultZIndex(55);
		infoExtras.gamesLabel->setOpacity(0);
	}

	if (infoExtras.gameCount)
	{
		infoExtras.gameCount->setSize(0.08f * mSize.x(), Font::get((int)(0.04f * mSize.y()), Font::getDefaultPath())->getLetterHeight() * 1.4f);
		infoExtras.gameCount->setPosition(0.90f * mSize.x(), 0.735f * mSize.y());
		infoExtras.gameCount->setFont(Font::get((int)(0.04f * mSize.y()), Font::getDefaultPath()));
		infoExtras.gameCount->setColor(0xFFFFFFFF);
		infoExtras.gameCount->setHorizontalAlignment(ALIGN_RIGHT);
		infoExtras.gameCount->setZIndex(56);
		infoExtras.gameCount->setDefaultZIndex(56);
		infoExtras.gameCount->setOpacity(0);
	}
}

void SystemView::getCarouselFromTheme(const ThemeData::ThemeElement* elem)
{
	SystemCarouselExtras& extraCfg = getCarouselExtras(this);

	if(elem->has("type"))
	{
		if(!(elem->get<std::string>("type").compare("vertical")))
			mCarousel.type = VERTICAL;
		else if(!(elem->get<std::string>("type").compare("vertical_wheel")))
			mCarousel.type = VERTICAL_WHEEL;
		else if(!(elem->get<std::string>("type").compare("horizontal_wheel")))
			mCarousel.type = HORIZONTAL_WHEEL;
		else
			mCarousel.type = HORIZONTAL;
	}

	if(elem->has("size"))
		mCarousel.size = elem->get<Vector2f>("size") * mSize;

	if(elem->has("pos"))
		mCarousel.pos = elem->get<Vector2f>("pos") * mSize;

	if(elem->has("origin"))
		mCarousel.origin = elem->get<Vector2f>("origin");

	if(elem->has("color"))
	{
		mCarousel.color    = elem->get<unsigned int>("color");
		mCarousel.colorEnd = mCarousel.color;
	}

	if(elem->has("colorEnd"))
		mCarousel.colorEnd = elem->get<unsigned int>("colorEnd");

	if(elem->has("gradientType"))
		mCarousel.colorGradientHorizontal = !(elem->get<std::string>("gradientType").compare("horizontal"));

	if(elem->has("logoScale"))
		mCarousel.logoScale = elem->get<float>("logoScale");

	if(elem->has("logoSize"))
		mCarousel.logoSize = elem->get<Vector2f>("logoSize") * mSize;

	if(elem->has("maxLogoCount"))
		mCarousel.maxLogoCount = (int)Math::round(elem->get<float>("maxLogoCount"));

	if(elem->has("zIndex"))
		mCarousel.zIndex = elem->get<float>("zIndex");

	if(elem->has("logoRotation"))
		mCarousel.logoRotation = elem->get<float>("logoRotation");

	if(elem->has("logoRotationOrigin"))
		mCarousel.logoRotationOrigin = elem->get<Vector2f>("logoRotationOrigin");

	if(elem->has("logoAlignment"))
	{
		if(!(elem->get<std::string>("logoAlignment").compare("left")))
			mCarousel.logoAlignment = ALIGN_LEFT;
		else if(!(elem->get<std::string>("logoAlignment").compare("right")))
			mCarousel.logoAlignment = ALIGN_RIGHT;
		else if(!(elem->get<std::string>("logoAlignment").compare("top")))
			mCarousel.logoAlignment = ALIGN_TOP;
		else if(!(elem->get<std::string>("logoAlignment").compare("bottom")))
			mCarousel.logoAlignment = ALIGN_BOTTOM;
		else
			mCarousel.logoAlignment = ALIGN_CENTER;
	}

	if(elem->has("minLogoOpacity"))
		mCarousel.minLogoOpacity = elem->get<float>("minLogoOpacity");

	if(elem->has("scaledLogoSpacing"))
		mCarousel.scaledLogoSpacing = elem->get<float>("scaledLogoSpacing");

	if(elem->has("scrollSound"))
	{
		std::string path = elem->get<std::string>("scrollSound");
		if(!path.empty())
			mScrollSnd = Sound::get(path);
	}

	// ─────────────────────────────────────────────
	// Nuevas propiedades opcionales
	// ─────────────────────────────────────────────

	if(elem->has("logoOffset"))
	{
		Vector2f v = elem->get<Vector2f>("logoOffset") * mSize;
		extraCfg.logoOffsetX = v.x();
		extraCfg.logoOffsetY = v.y();
	}

	if(elem->has("logoOffsetX"))
		extraCfg.logoOffsetX = elem->get<float>("logoOffsetX") * mSize.x();

	if(elem->has("logoOffsetY"))
		extraCfg.logoOffsetY = elem->get<float>("logoOffsetY") * mSize.y();

	if(elem->has("selectedLogoOffsetX"))
		extraCfg.selectedLogoOffsetX = elem->get<float>("selectedLogoOffsetX") * mSize.x();

	if(elem->has("selectedLogoOffsetY"))
		extraCfg.selectedLogoOffsetY = elem->get<float>("selectedLogoOffsetY") * mSize.y();

	if(elem->has("logoSpacing"))
	{
		Vector2f v = elem->get<Vector2f>("logoSpacing") * mSize;
		extraCfg.logoSpacingX = v.x();
		extraCfg.logoSpacingY = v.y();
	}

	if(elem->has("logoSpacingX"))
		extraCfg.logoSpacingX = elem->get<float>("logoSpacingX") * mSize.x();

	if(elem->has("logoSpacingY"))
		extraCfg.logoSpacingY = elem->get<float>("logoSpacingY") * mSize.y();
}
