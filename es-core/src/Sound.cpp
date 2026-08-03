#include "Sound.h"

#include "AudioManager.h"
#include "Log.h"
#include "Settings.h"
#include "ThemeData.h"

std::map<std::string, std::shared_ptr<Sound>> Sound::sMap;

std::shared_ptr<Sound> Sound::get(const std::string& path)
{
	auto it = sMap.find(path);
	if (it != sMap.cend())
		return it->second;

	std::shared_ptr<Sound> sound = std::shared_ptr<Sound>(new Sound(path));

	// Mantener compatibilidad con tu flujo actual
	// (aunque AudioManager ahora sea pasivo, no molesta)
	AudioManager::getInstance()->registerSound(sound);

	sMap[path] = sound;
	return sound;
}

std::shared_ptr<Sound> Sound::getFromTheme(const std::shared_ptr<ThemeData>& theme,
                                           const std::string& view,
                                           const std::string& element)
{
	LOG(LogInfo) << " req sound [" << view << "." << element << "]";

	const ThemeData::ThemeElement* elem = theme ? theme->getElement(view, element, "sound") : nullptr;
	if (!elem || !elem->has("path"))
	{
		LOG(LogInfo) << "   (missing)";
		return get(""); // dummy
	}

	return get(elem->get<std::string>("path"));
}

void Sound::cleanup()
{
	sMap.clear();
}

Sound::Sound(const std::string& path)
	: mPath(path),
	  mChunk(nullptr),
	  mChannel(-1)
{
	loadFile(path);
}

Sound::~Sound()
{
	deinit();
}

bool Sound::isMixerAvailable()
{
	int freq = 0;
	Uint16 format = 0;
	int channels = 0;
	return (Mix_QuerySpec(&freq, &format, &channels) != 0);
}

void Sound::loadFile(const std::string& path)
{
	mPath = path;
	init();
}

void Sound::ensureMixerOpen()
{
	// Si el mixer ya está abierto, no hacer nada.
	if (isMixerAvailable())
		return;

	// Mixer todavía no está abierto. Abrimos con algo estándar.
	if (Mix_OpenAudio(44100, AUDIO_S16SYS, 2, 2048) < 0)
	{
		LOG(LogError) << "Sound: Mix_OpenAudio failed: " << Mix_GetError();
	}
}

void Sound::init()
{
	deinit();

	if (mPath.empty())
		return;

	if (!Settings::getInstance()->getBool("EnableSounds"))
		return;

	ensureMixerOpen();

	if (!isMixerAvailable())
		return;

	mChunk = Mix_LoadWAV(mPath.c_str());
	if (mChunk == nullptr)
	{
		LOG(LogError) << "Error loading sound \"" << mPath << "\"!\n"
		              << "	" << Mix_GetError();
		return;
	}

	Mix_VolumeChunk(mChunk, MIX_MAX_VOLUME);
}

void Sound::deinit()
{
	stop();

	if (mChunk != nullptr)
	{
		Mix_FreeChunk(mChunk);
		mChunk = nullptr;
	}
}

void Sound::play()
{
	if (!Settings::getInstance()->getBool("EnableSounds"))
		return;

	if (mChunk == nullptr)
		return;

	ensureMixerOpen();

	if (!isMixerAvailable())
		return;

	// No apilar varias reproducciones del mismo sonido. Esto evita que el
	// sonido de desplazamiento ocupe varios canales durante la navegación.
	if (isPlaying())
		stop();

	// Play once. (no loop)
	// Devuelve canal asignado o -1 si falló.
	mChannel = Mix_PlayChannel(-1, mChunk, 0);
	if (mChannel < 0)
	{
		LOG(LogError) << "Sound: Mix_PlayChannel failed: " << Mix_GetError();
		return;
	}
}

bool Sound::isPlaying() const
{
	if (mChannel < 0)
		return false;

	int freq = 0;
	Uint16 format = 0;
	int channels = 0;

	if (Mix_QuerySpec(&freq, &format, &channels) == 0)
		return false;

	return (Mix_Playing(mChannel) != 0);
}

void Sound::stop()
{
	if (mChannel < 0)
		return;

	int freq = 0;
	Uint16 format = 0;
	int channels = 0;

	if (Mix_QuerySpec(&freq, &format, &channels) != 0)
	{
		Mix_HaltChannel(mChannel);
	}

	mChannel = -1;
}
