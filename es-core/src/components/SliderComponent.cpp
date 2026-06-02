#include "components/SliderComponent.h"

#include "resources/Font.h"

#define MOVE_REPEAT_DELAY 500
#define MOVE_REPEAT_RATE 40

SliderComponent::SliderComponent(Window* window, float min, float max, float increment, const std::string& suffix)
	: GuiComponent(window)
	, mMin(min)
	, mMax(max)
	, mSingleIncrement(increment)
	, mMoveRate(0)
	, mMoveAccumulator(0)
	, mKnob(window)
	, mSuffix(suffix)
{
	assert((min - max) != 0);

	// some sane default value
	mValue = (max + min) / 2;

	mKnob.setOrigin(0.5f, 0.5f);
	mKnob.setImage(":/slider_knob.svg");

	setSize(Renderer::getScreenWidth() * 0.15f, Font::get(FONT_SIZE_MEDIUM)->getLetterHeight());
}

bool SliderComponent::input(InputConfig* config, Input input)
{
	// ES-X:
	// Para mandos BT/wireless, evitamos autorepeat en sliders.
	// Algunos controles no envían un release limpio de left/right y el volumen
	// queda como si siguiera presionado.
	//
	// Cada pulsación cambia un paso. Al soltar, o ante cualquier release,
	// se corta cualquier repetición pendiente.
	if(input.value == 0)
	{
		mMoveRate = 0;
		mMoveAccumulator = 0;
		return GuiComponent::input(config, input);
	}

	if(config->isMappedLike("left", input))
	{
		setValue(mValue - mSingleIncrement);

		mMoveRate = 0;
		mMoveAccumulator = 0;
		return true;
	}

	if(config->isMappedLike("right", input))
	{
		setValue(mValue + mSingleIncrement);

		mMoveRate = 0;
		mMoveAccumulator = 0;
		return true;
	}

	// Si entra cualquier otro input mientras el slider tiene foco,
	// nos aseguramos de no dejar repetición pendiente.
	mMoveRate = 0;
	mMoveAccumulator = 0;

	return GuiComponent::input(config, input);
}

void SliderComponent::update(int deltaTime)
{
	if(mMoveRate != 0)
	{
		mMoveAccumulator += deltaTime;
		while(mMoveAccumulator >= MOVE_REPEAT_RATE)
		{
			setValue(mValue + mMoveRate);
			mMoveAccumulator -= MOVE_REPEAT_RATE;
		}
	}

	GuiComponent::update(deltaTime);
}

void SliderComponent::render(const Transform4x4f& parentTrans)
{
	Transform4x4f trans = parentTrans * getTransform();
	Renderer::setMatrix(trans);

	// render suffix
	if(mValueCache)
		mFont->renderTextCache(mValueCache.get());

	float width = mSize.x() - mKnob.getSize().x() - (mValueCache ? mValueCache->metrics.size.x() + 4 : 0);

	// render line
	const float lineWidth = 2;
	Renderer::drawRect(mKnob.getSize().x() / 2, mSize.y() / 2 - lineWidth / 2, width, lineWidth, 0x777777FF, 0x777777FF);

	// render knob
	mKnob.render(trans);

	GuiComponent::renderChildren(trans);
}

void SliderComponent::setValue(float value)
{
	mValue = value;
	if(mValue < mMin)
		mValue = mMin;
	else if(mValue > mMax)
		mValue = mMax;

	onValueChanged();
}

float SliderComponent::getValue()
{
	return mValue;
}

void SliderComponent::onSizeChanged()
{
	if(!mSuffix.empty())
		mFont = Font::get((int)(mSize.y()), FONT_PATH_LIGHT);

	onValueChanged();
}

void SliderComponent::onValueChanged()
{
	// update suffix textcache
	if(mFont)
	{
		std::stringstream ss;
		ss << std::fixed;
		ss.precision(0);
		ss << mValue;
		ss << mSuffix;
		const std::string val = ss.str();

		ss.str("");
		ss.clear();
		ss << std::fixed;
		ss.precision(0);
		ss << mMax;
		ss << mSuffix;
		const std::string max = ss.str();

		Vector2f textSize = mFont->sizeText(max);
		mValueCache = std::shared_ptr<TextCache>(mFont->buildTextCache(val, mSize.x() - textSize.x(), (mSize.y() - textSize.y()) / 2, 0x777777FF));
		mValueCache->metrics.size[0] = textSize.x(); // fudge the width
	}

	// update knob position/size
	mKnob.setResize(0, mSize.y() * 0.7f);

	float lineLength = mSize.x() - mKnob.getSize().x() - (mValueCache ? mValueCache->metrics.size.x() + 4 : 0);

	// ES-X:
	// Fórmula corregida siguiendo la lógica de ES-DE:
	// posición normalizada entre mMin y mMax.
	float range = mMax - mMin;
	float normalized = 0.0f;

	if(range != 0.0f)
		normalized = (mValue - mMin) / range;

	if(normalized < 0.0f)
		normalized = 0.0f;
	else if(normalized > 1.0f)
		normalized = 1.0f;

	mKnob.setPosition((normalized * lineLength) + mKnob.getSize().x() / 2, mSize.y() / 2);
}

std::vector<HelpPrompt> SliderComponent::getHelpPrompts()
{
	std::vector<HelpPrompt> prompts;
	prompts.push_back(HelpPrompt("left/right", "change"));
	return prompts;
}
