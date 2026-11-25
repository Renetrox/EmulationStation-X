#!/usr/bin/env python3
"""Experimental RetroPie onlaunch hook: only changes Runcommand's volatile config."""
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

def log(message):
    print("Tarave decorations: " + message, file=sys.stderr)

def setting(path, key):
    match = re.search(r'(?m)^\s*' + re.escape(key) + r'\s*=\s*"?([^"\r\n]*)', path.read_text())
    return match.group(1).strip() if match else ""

def resolution(path):
    x, y = (setting(path, "video_fullscreen_" + axis) for axis in ("x", "y"))
    if x.isdigit() and y.isdigit() and int(x) > 0 and int(y) > 0:
        return int(x), int(y)
    if os.environ.get("DISPLAY"):
        output = subprocess.check_output(["xrandr", "--current"], text=True, timeout=3)
        modes = re.findall(r'(?m)^\S+ connected(?: primary)? (\d+)x(\d+)[+-]\d+[+-]\d+', output)
        if len(modes) == 1:
            return tuple(map(int, modes[0]))
    return None

def geometry(info, width, height):
    w, h = (int(info[k]) for k in ("width", "height"))
    left, top, right, bottom = (int(info[k]) for k in ("left", "top", "right", "bottom"))
    if min(w, h) <= 0 or min(left, top, right, bottom) < 0 or left+right >= w or top+bottom >= h:
        raise ValueError("invalid margins")
    if abs(width/height - w/h) > 0.025:
        raise ValueError("screen aspect does not match decoration")
    x, y = round(left*width/w), round(top*height/h)
    return x, y, round((w-right)*width/w)-x, round((h-bottom)*height/h)-y

def decoration_pair(folder, system):
    """Prefer a system-specific pair; allow generic frames only for their intended class."""
    specific = (folder / "systems" / (system+".png"), folder / "systems" / (system+".info"))
    if all(p.is_file() for p in specific):
        return specific
    name = folder.name
    # Vertical artwork needs per-game orientation detection, not yet available.
    if "vertical" in name:
        return None
    if name.startswith("arcade_") and system not in ("arcade", "mame", "mame-libretro", "fba", "fbneo", "neogeo"):
        return None
    # Generic ambiance frames target conventional landscape 4:3 games.
    # Do not stretch handhelds, dual-screen systems or widescreen consoles.
    if system not in ("arcade", "mame", "mame-libretro", "fba", "fbneo", "neogeo", "nes", "snes", "n64", "psx", "mastersystem", "megadrive", "segacd", "sega32x", "3do", "saturn", "pcengine", "pcenginecd", "dreamcast"):
        return None
    generic = (folder / "default.png", folder / "default.info")
    return generic if all(p.is_file() for p in generic) else None

def apply(system, emulator, rom, command, path=Path("/dev/shm/retroarch.cfg")):
    pack = os.environ.get("TARAVE_DECORATION", "")
    root = os.environ.get("TARAVE_DECORATION_ROOT", "")
    if not pack or pack in ("auto", "none") or not root or not emulator.startswith("lr-"):
        return
    if not re.fullmatch(r"[A-Za-z0-9_-]+", system) or not re.fullmatch(r"[A-Za-z0-9_. -]+", pack) or pack in (".", ".."):
        return
    if "/dev/shm/retroarch.cfg" not in command and path == Path("/dev/shm/retroarch.cfg"):
        # xinit may rewrite the command; fail closed for this first test.
        log("temporary config not present in launch command")
        return
    if not path.is_file() or path.is_symlink():
        return
    pair = decoration_pair(Path(root) / pack, system)
    if pair is None:
        log("no compatible decoration for " + system)
        return
    png, info = pair
    # Explicit ROM configuration takes precedence.
    if Path(rom+".cfg").is_file():
        log("skipping ROM with custom configuration")
        return
    size = resolution(path)
    if not size:
        log("unknown output resolution; skipping")
        return
    data = json.loads(info.read_text())
    x,y,w,h = geometry(data, *size)
    opacity = float(data.get("opacity", 1))
    if not 0 <= opacity <= 1:
        raise ValueError("invalid opacity")
    # Generate a clean overlay descriptor instead of trusting absolute paths in the pack.
    image = str(png.resolve())
    if any(c in image for c in ('"', '\n', '\r', '\\')):
        raise ValueError("unsupported image path")
    overlay = path.with_name("tarave-decoration-overlay.cfg")
    overlay.write_text('overlays = 1\noverlay0_overlay = "'+image+'"\noverlay0_full_screen = true\noverlay0_descs = 0\n')
    values = {
        "input_overlay_enable": "true", "input_overlay": str(overlay),
        "input_overlay_opacity": str(opacity), "input_overlay_scale": "1.0",
        "input_overlay_auto_scale": "false", "input_overlay_aspect_adjust_landscape": "0.0",
        "input_overlay_x_offset_landscape": "0.0", "input_overlay_y_offset_landscape": "0.0",
        "video_scale_integer": "false", "video_force_aspect": "true",
        "aspect_ratio_index": "23", "custom_viewport_x": str(x), "custom_viewport_y": str(y),
        "custom_viewport_width": str(w), "custom_viewport_height": str(h),
    }
    old = path.read_text()
    lines = [line for line in old.splitlines() if line.split("=",1)[0].strip() not in values]
    lines += [k+' = "'+v+'"' for k,v in values.items()]
    with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as tmp:
        tmp.write("\n".join(lines)+"\n")
    os.replace(tmp.name, path)
    log(f"{pack}/{system}: {size[0]}x{size[1]}, viewport {x},{y} {w}x{h}")

if __name__ == "__main__":
    try:
        if sys.argv[1:] == ["--self-test"]:
            info = dict(width=1920,height=1080,left=242,top=2,right=245,bottom=3)
            assert geometry(info,1920,1080) == (242,2,1433,1075)
            assert geometry(info,1280,720) == (161,1,956,717)
            try:
                geometry(info,800,600)
            except ValueError:
                pass
            else:
                raise AssertionError("4:3 must be rejected")
            print("Geometry tests passed")
        elif len(sys.argv) == 5:
            apply(*sys.argv[1:])
    except Exception as exc:
        log(str(exc))
        if sys.argv[1:] == ["--self-test"]:
            sys.exit(1)
