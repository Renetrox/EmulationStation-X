#pragma once

#include <string>
#include <map>

class LocaleES
{
public:
	static LocaleES& getInstance();

	bool loadLanguageFile(const std::string& filePath);
	void loadFromSettings();
	std::string getLanguageName(const std::string& code) const;

	std::string translate(const std::string& key) const;
	static std::string get(const std::string& key);

private:
	LocaleES();

	std::map<std::string, std::string> mTranslations;
	std::map<std::string, std::string> mFallbackTranslations;
	std::string mCurrentLanguage;
};

// Función global para usar desde cualquier parte
std::string es_translate(const std::string& key);
