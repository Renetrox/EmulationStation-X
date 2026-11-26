#pragma once
#include "RetroPieDecorations.h"
#include <map>
#include <cstdlib>
namespace ESXDecorationCatalog {
inline const std::map<std::string,std::vector<std::string>>& catalog() {
    static const std::map<std::string, std::vector<std::string>> files = {
        {"ambiance_01", {"default.info", "default.png"}},
        {"ambiance_02", {"default.info", "default.png"}},
        {"ambiance_03", {"default.info", "default.png"}},
        {"ambiance_04", {"default.info", "default.png"}},
        {"ambiance_05", {"default.info", "default.png"}},
        {"ambiance_06", {"default.info", "default.png"}},
        {"ambiance_07", {"default.info", "default.png"}},
        {"ambiance_08", {"default.info", "default.png"}},
        {"ambiance_09", {"default.info", "default.png"}},
        {"ambiance_10", {"default.info", "default.png"}},
        {"ambiance_11", {"default.info", "default.png"}},
        {"ambiance_12", {"default.info", "default.png"}},
        {"ambiance_13", {"default.info", "default.png"}},
        {"ambiance_14", {"default.info", "default.png"}},
        {"ambiance_15", {"default.info", "default.png"}},
        {"ambiance_16", {"default.info", "default.png"}},
        {"ambiance_17", {"default.info", "default.png"}},
        {"ambiance_18", {"default.info", "default.png"}},
        {"ambiance_19", {"default.info", "default.png"}},
        {"ambiance_20", {"default.info", "default.png"}},
        {"arcade_01", {"default.info", "default.png"}},
        {"arcade_02", {"default.info", "default.png"}},
        {"arcade_vertical_01", {"default.info", "default.png"}},
        {"arcade_vertical_02", {"default.info", "default.png"}},
        {"arcade_vertical_default", {"default.info", "default.png"}},
        {"atomiswave_naomi_vertical", {"default.info", "default.png"}},
        {"default", {"systems/3do.info", "systems/3do.png", "systems/3ds.info", "systems/3ds.png", "systems/amiga.info", "systems/amiga.png", "systems/amigacd32.info", "systems/amigacd32.png", "systems/amstradcpc.info", "systems/amstradcpc.png", "systems/apple2.info", "systems/apple2.png", "systems/atari2600.info", "systems/atari2600.png", "systems/atari5200.info", "systems/atari5200.png", "systems/atari7800.info", "systems/atari7800.png", "systems/atari800.info", "systems/atari800.png", "systems/atarist.info", "systems/atarist.png", "systems/atomiswave.info", "systems/atomiswave.png", "systems/c64.info", "systems/c64.png", "systems/colecovision.info", "systems/colecovision.png", "systems/dos.info", "systems/dos.png", "systems/dreamcast.info", "systems/dreamcast.png", "systems/fba.info", "systems/fba.png", "systems/fba_libretro.info", "systems/fba_libretro.png", "systems/fds.info", "systems/fds.png", "systems/gamecube.info", "systems/gamecube.png", "systems/gamegear.info", "systems/gamegear.png", "systems/gb.info", "systems/gb.png", "systems/gba.info", "systems/gba.png", "systems/gbc.info", "systems/gbc.png", "systems/gx4000.info", "systems/gx4000.png", "systems/intellivision.info", "systems/intellivision.png", "systems/jaguar.info", "systems/jaguar.png", "systems/lightgun.info", "systems/lightgun.png", "systems/lynx.info", "systems/lynx.png", "systems/mame.info", "systems/mame.png", "systems/mastersystem.info", "systems/mastersystem.png", "systems/megadrive.info", "systems/megadrive.png", "systems/msx.info", "systems/msx.png", "systems/n64.info", "systems/n64.png", "systems/naomi.info", "systems/naomi.png", "systems/nds.info", "systems/nds.png", "systems/neogeo.info", "systems/neogeo.png", "systems/neogeocd.info", "systems/neogeocd.png", "systems/nes.info", "systems/nes.png", "systems/ngp.info", "systems/ngp.png", "systems/ngpc.info", "systems/ngpc.png", "systems/odyssey2.info", "systems/odyssey2.png", "systems/pc98.info", "systems/pc98.png", "systems/pcengine.info", "systems/pcengine.png", "systems/pcenginecd.info", "systems/pcenginecd.png", "systems/pcfx.info", "systems/pcfx.png", "systems/pokemini.info", "systems/pokemini.png", "systems/prboom.info", "systems/prboom.png", "systems/ps2.info", "systems/ps2.png", "systems/psp.info", "systems/psp.png", "systems/psx.info", "systems/psx.png", "systems/satellaview.info", "systems/satellaview.png", "systems/saturn.info", "systems/saturn.png", "systems/scummvm.info", "systems/scummvm.png", "systems/sega32x.info", "systems/sega32x.png", "systems/segacd.info", "systems/segacd.png", "systems/sg1000.info", "systems/sg1000.png", "systems/snes.info", "systems/snes.png", "systems/sufami.info", "systems/sufami.png", "systems/supergrafx.info", "systems/supergrafx.png", "systems/thomson.info", "systems/thomson.png", "systems/vectrex.info", "systems/vectrex.png", "systems/virtualboy.info", "systems/virtualboy.png", "systems/wii.info", "systems/wii.png", "systems/wswan.info", "systems/wswan.png", "systems/wswanc.info", "systems/wswanc.png", "systems/x68000.info", "systems/x68000.png", "systems/zx81.info", "systems/zx81.png", "systems/zxspectrum.info", "systems/zxspectrum.png"}},
        {"default_glazed", {"systems/3do.info", "systems/3do.png", "systems/3ds.info", "systems/3ds.png", "systems/amiga.info", "systems/amiga.png", "systems/amstradcpc.info", "systems/amstradcpc.png", "systems/amstradgx4000.info", "systems/amstradgx4000.png", "systems/apple2.info", "systems/apple2.png", "systems/atari2600.info", "systems/atari2600.png", "systems/atari5200.info", "systems/atari5200.png", "systems/atari7800.info", "systems/atari7800.png", "systems/atari800.info", "systems/atari800.png", "systems/atarist.info", "systems/atarist.png", "systems/atomiswave.info", "systems/atomiswave.png", "systems/c64.info", "systems/c64.png", "systems/colecovision.info", "systems/colecovision.png", "systems/dos.info", "systems/dos.png", "systems/dreamcast.info", "systems/dreamcast.png", "systems/fba.info", "systems/fba.png", "systems/fba_libretro.info", "systems/fba_libretro.png", "systems/fds.info", "systems/fds.png", "systems/gamecube.info", "systems/gamecube.png", "systems/gamegear.info", "systems/gamegear.png", "systems/gb.info", "systems/gb.png", "systems/gba.info", "systems/gba.png", "systems/gbc.info", "systems/gbc.png", "systems/intellivision.info", "systems/intellivision.png", "systems/jaguar.info", "systems/jaguar.png", "systems/lynx.info", "systems/lynx.png", "systems/mame.info", "systems/mame.png", "systems/mastersystem.info", "systems/mastersystem.png", "systems/megadrive.info", "systems/megadrive.png", "systems/msx.info", "systems/msx.png", "systems/n64.info", "systems/n64.png", "systems/naomi.info", "systems/naomi.png", "systems/nds.info", "systems/nds.png", "systems/neogeo.info", "systems/neogeo.png", "systems/neogeocd.info", "systems/neogeocd.png", "systems/nes.info", "systems/nes.png", "systems/ngp.info", "systems/ngp.png", "systems/ngpc.info", "systems/ngpc.png", "systems/odyssey2.info", "systems/odyssey2.png", "systems/pcengine.info", "systems/pcengine.png", "systems/pcenginecd.info", "systems/pcenginecd.png", "systems/pcfx.info", "systems/pcfx.png", "systems/pokemini.info", "systems/pokemini.png", "systems/prboom.info", "systems/prboom.png", "systems/ps2.info", "systems/ps2.png", "systems/psp.info", "systems/psp.png", "systems/psx.info", "systems/psx.png", "systems/satellaview.info", "systems/satellaview.png", "systems/saturn.info", "systems/saturn.png", "systems/scummvm.info", "systems/scummvm.png", "systems/sega32x.info", "systems/sega32x.png", "systems/segacd.info", "systems/segacd.png", "systems/sg1000.info", "systems/sg1000.png", "systems/snes.info", "systems/snes.png", "systems/sufami.info", "systems/sufami.png", "systems/supergrafx.info", "systems/supergrafx.png", "systems/thomson.info", "systems/thomson.png", "systems/vectrex.info", "systems/vectrex.png", "systems/virtualboy.info", "systems/virtualboy.png", "systems/wii.info", "systems/wii.png", "systems/wswan.info", "systems/wswan.png", "systems/wswanc.info", "systems/wswanc.png", "systems/x68000.info", "systems/x68000.png", "systems/zx81.info", "systems/zx81.png", "systems/zxspectrum.info", "systems/zxspectrum.png"}}
    };

    return files;
}
inline bool managed(const std::string& name) {
    return Utils::FileSystem::isRegularFile(RetroPieDecorations::root()+"/"+name+"/.tarave-managed");
}
inline bool change(const std::string& name, bool remove) {
    const auto it = catalog().find(name);
    if (it == catalog().end()) return false;
    const auto root = RetroPieDecorations::root();
    const auto dst = root + "/" + name;
    using RetroPieDecorations::shellQuote;
    std::string script = "set -eu; ";
    if (remove) {
        script += "test ! -L " + shellQuote(dst) + "; cd " + shellQuote(dst) +
          "; test ! -L .tarave-managed; test -f .tarave-managed; ";
        script += "test -z \"$(find . -type l -print -quit)\"; ";
        script += "test \"$(find . -type f | wc -l)\" -eq " + std::to_string(it->second.size()+1) + "; ";
        script += "test \"$(find . -mindepth 1 ! -type f ! -type d | wc -l)\" -eq 0; ";
        script += "test \"$(find . -mindepth 1 -type d | wc -l)\" -eq 1; test -d systems; ";
        script += "test \"$(wc -l < .tarave-managed)\" -eq " + std::to_string(it->second.size()) + "; ";
        for (const auto& file: it->second)
            script += "test -f " + shellQuote(file) + "; grep -Fq -- " + shellQuote("  "+file) + " .tarave-managed; ";
        script += "sha256sum -c --strict .tarave-managed; ";
        for (const auto& file: it->second) script += "rm -- " + shellQuote(file) + "; ";
        script += "rm -- .tarave-managed; rmdir systems; cd ..; rmdir -- " + shellQuote(name);
    } else {
        script += "mkdir -p " + shellQuote(root) + "; dst=" + shellQuote(dst) +
            "; test ! -e \"$dst\"; test ! -L \"$dst\"; tmp=$(mktemp -d " + shellQuote(root+"/.esx-download-XXXXXX") +
            "); trap 'rm -rf -- \"$tmp\"' EXIT; mkdir -p \"$tmp/systems\"; ";
        const auto base = "https://raw.githubusercontent.com/kaylh/RetroArch-Bezels/c230214c8d93f2b907c8f2eb808e8cfe60f35df0/" + name + "/";
        for (const auto& file: it->second)
            script += "curl --fail --location --proto '=https' --connect-timeout 15 --max-time 180 --silent --show-error -o \"$tmp/" + file + "\" " + shellQuote(base+file) + "; test -s \"$tmp/" + file + "\"; ";
        script += "(cd \"$tmp\"; sha256sum ";
        for (const auto& file: it->second) script += shellQuote(file)+" ";
        script += "> .tarave-managed); mv -T -n -- \"$tmp\" \"$dst\"; test ! -d \"$tmp\"";
    }
    return std::system(("bash -c "+shellQuote(script)).c_str()) == 0;
}
}
