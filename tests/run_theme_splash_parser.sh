#!/usr/bin/env bash
set -euo pipefail
repo_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
test_dir="$(mktemp -d)"
trap 'rm -rf "$test_dir"' EXIT
cd "$repo_dir"
read -r -a sdl_flags <<< "$(pkg-config --cflags sdl2 freetype2)"
"${CXX:-c++}" -std=c++11 -O0 -ffunction-sections -fdata-sections \
    -Ies-core/src -Ies-app/src -Iexternal/pugixml/src "${sdl_flags[@]}" \
    tests/theme_splash_parser.cpp es-core/src/ThemeData.cpp \
    es-core/src/resources/ResourceManager.cpp es-core/src/utils/FileSystemUtil.cpp \
    es-core/src/utils/StringUtil.cpp es-core/src/Settings.cpp es-core/src/Log.cpp \
    es-core/src/platform.cpp es-core/src/math/Vector2f.cpp es-core/src/math/Vector4f.cpp \
    external/pugixml/src/pugixml.cpp -Wl,--gc-sections -o "$test_dir/parser"
"$test_dir/parser" "$test_dir" "$@"
