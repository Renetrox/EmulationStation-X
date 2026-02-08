#include "LocaleES.h"

// Hook to expose es_translate for modules that depend on the core header.
#include "../es-core/src/LocaleESHook.h"

std::string es_translate(const std::string& key)
{
    return LocaleES::getInstance().translate(key);
}
