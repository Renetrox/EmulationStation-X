#!/usr/bin/env bash
set -euo pipefail

ACTION="${1:-}"
SYSTEM_NAME="${2:-}"
LANG_CODE="${3:-en}"
VIDEOS_ENABLED="${5:-true}"
ONLY_MISSING="${6:-true}"

HOME_DIR="${HOME}"
ROMDIR="${HOME_DIR}/RetroPie/roms"
SKY_BIN="$(command -v Skyscraper || true)"
SKY_CFG_DIR="${HOME_DIR}/.skyscraper"
SKY_CFG_FILE="${SKY_CFG_DIR}/config.ini"

STATE_DIR="/tmp/esx-skyscraper"
PID_FILE="${STATE_DIR}/pid"
STATUS_FILE="${STATE_DIR}/status"
LOG_FILE="${STATE_DIR}/log"

RETROPIE_SKY_CFG_DIR="/opt/retropie/configs/all/skyscraper"

mkdir -p "${STATE_DIR}"

write_status() {
    printf '%s\n' "$1" > "${STATUS_FILE}"
}

fail() {
    write_status "error:$1"
    echo "ERROR: $1" >> "${LOG_FILE}"
    exit 1
}

normalize_lang() {
    local raw="${1:-en}"
    raw="${raw,,}"

    case "$raw" in
        es|es_*|es-*) echo "es" ;;
        en|en_*|en-*) echo "en" ;;
        fr|fr_*|fr-*) echo "fr" ;;
        de|de_*|de-*) echo "de" ;;
        pt|pt_*|pt-*) echo "pt" ;;
        it|it_*|it-*) echo "it" ;;
        nl|nl_*|nl-*) echo "nl" ;;
        ja|ja_*|ja-*) echo "ja" ;;
        ru|ru_*|ru-*) echo "ru" ;;
        *) echo "en" ;;
    esac
}

normalize_videos() {
    local raw="${1:-true}"
    raw="${raw,,}"

    case "$raw" in
        true|1|yes|on) echo "true" ;;
        false|0|no|off) echo "false" ;;
        *) echo "true" ;;
    esac
}

normalize_only_missing() {
    local raw="${1:-true}"
    raw="${raw,,}"

    case "$raw" in
        true|1|yes|on) echo "true" ;;
        false|0|no|off) echo "false" ;;
        *) echo "true" ;;
    esac
}

build_lang_prios() {
    local lang="$1"
    if [[ "$lang" == "en" ]]; then
        echo "en"
    else
        echo "${lang},en"
    fi
}

ensure_bin() {
    [[ -n "${SKY_BIN}" ]] || fail "Skyscraper not found in PATH"
}

ensure_config() {
    mkdir -p "${SKY_CFG_DIR}"
    touch "${SKY_CFG_FILE}"
}

ensure_system() {
    [[ -n "${SYSTEM_NAME}" ]] || fail "Missing system name"
    [[ -d "${ROMDIR}/${SYSTEM_NAME}" ]] || fail "System folder not found: ${ROMDIR}/${SYSTEM_NAME}"
}

is_running() {
    if [[ -f "${PID_FILE}" ]]; then
        local pid
        pid="$(cat "${PID_FILE}" 2>/dev/null || true)"
        if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
            return 0
        fi
    fi

    return 1
}

ensure_not_running() {
    if is_running; then
        fail "Skyscraper is already running"
    fi

    rm -f "${PID_FILE}"
}

path_size() {
    local path="$1"

    if [[ ! -e "${path}" ]]; then
        echo "0"
        return
    fi

    local resolved="${path}"
    resolved="$(readlink -f "${path}" 2>/dev/null || printf '%s' "${path}")"

    du -sh "${resolved}" 2>/dev/null | awk '{print $1}' || echo "0"
}

file_lines() {
    local path="$1"

    if [[ ! -f "${path}" ]]; then
        echo "0"
        return
    fi

    wc -l < "${path}" 2>/dev/null | tr -d ' ' || echo "0"
}

replace_or_add_main_key() {
    local key="$1"
    local value="$2"

    python3 - "$SKY_CFG_FILE" "$key" "$value" <<'PY'
import sys
from pathlib import Path

cfg = Path(sys.argv[1])
key = sys.argv[2]
value = sys.argv[3]

text = cfg.read_text(encoding="utf-8", errors="ignore") if cfg.exists() else ""
if "[main]" not in text:
    text = "[main]\n" + text

lines = text.splitlines()
out = []
in_main = False
done = False

for line in lines:
    stripped = line.strip()

    if stripped.startswith("[") and stripped.endswith("]"):
        if in_main and not done:
            out.append(f'{key}="{value}"')
            done = True
        in_main = (stripped == "[main]")
        out.append(line)
        continue

    if in_main and (stripped.startswith(f"{key}=") or stripped.startswith(f";{key}=")):
        if not done:
            out.append(f'{key}="{value}"')
            done = True
        continue

    out.append(line)

if in_main and not done:
    out.append(f'{key}="{value}"')

cfg.write_text("\n".join(out) + "\n", encoding="utf-8")
PY
}

update_config_lang() {
    local lang="$1"
    local prios="$2"
    replace_or_add_main_key "lang" "${lang}"
    replace_or_add_main_key "langPrios" "${prios}"
}

update_config_videos() {
    local videos="$1"
    replace_or_add_main_key "videos" "${videos}"
}

build_gather_flags() {
    local flags="unattend,skipped"

    if [[ "${ONLY_MISSING}" == "true" ]]; then
        flags+=",onlymissing"
    fi

    echo "${flags}"
}

build_generate_flags() {
    local flags="unattend,skipped,relative"

    if [[ "${ONLY_MISSING}" == "true" ]]; then
        flags+=",onlymissing"
    fi

    echo "${flags}"
}


media_missing_count() {
    # Outputs: "<total_launchable_roms> <missing_media_count>"
    # This is an ES-X guard for ONLY_MISSING=true:
    # Skyscraper's own "onlymissing" works against its resource cache, while users
    # usually expect "only missing" to mean "do not go online again if final media
    # already exists". This function checks final generated media/gamelist first.
    python3 - "${SYSTEM_NAME}" "${ROMDIR}/${SYSTEM_NAME}" "${HOME_DIR}" <<'PY'
import os
import sys
import xml.etree.ElementTree as ET
from pathlib import Path

system = sys.argv[1]
rom_dir = Path(sys.argv[2]).expanduser()
home = Path(sys.argv[3]).expanduser()

# Common launchable extensions. PSX is special because .bin/.wav/.ape tracks
# should not be treated as independent games.
psx_exts = {
    ".cue", ".chd", ".pbp", ".m3u", ".iso", ".img", ".ccd", ".zip", ".7z"
}
skip_exts = {
    ".bin", ".wav", ".ape", ".flac", ".mp3", ".ogg", ".sub", ".toc",
    ".sbi", ".txt", ".nfo", ".xml", ".png", ".jpg", ".jpeg", ".webp"
}
generic_skip_dirs = {"media", "images", "downloaded_media", "manuals", "videos"}

def norm_name(value: str) -> str:
    return "".join(ch.lower() for ch in value if ch.isalnum())

def is_launchable(path: Path) -> bool:
    if not path.is_file():
        return False
    try:
        rel_parts = path.relative_to(rom_dir).parts
        if any(part in generic_skip_dirs for part in rel_parts[:-1]):
            return False
    except Exception:
        pass

    ext = path.suffix.lower()

    if system.lower() in {"psx", "ps1", "playstation"}:
        return ext in psx_exts

    if ext in skip_exts:
        return False

    # Conservative generic behavior: ignore obvious sidecar/media files, allow
    # normal ROM/archive files.
    return True

def path_variants(path: Path):
    variants = set()
    try:
        rel = path.relative_to(rom_dir)
        variants.add("./" + rel.as_posix())
        variants.add(rel.as_posix())
    except Exception:
        pass
    variants.add(str(path))
    variants.add(path.name)
    return variants

def existing_media_paths_from_gamelist():
    covered = set()
    gamelist_candidates = [
        rom_dir / "gamelist.xml",
        home / ".emulationstation" / "gamelists" / system / "gamelist.xml",
    ]

    for gamelist in gamelist_candidates:
        if not gamelist.is_file():
            continue

        try:
            root = ET.parse(gamelist).getroot()
        except Exception:
            continue

        for game in root.findall(".//game"):
            path_el = game.find("path")
            if path_el is None or not path_el.text:
                continue

            rom_ref = path_el.text.strip()
            media_found = False

            for tag in ("image", "thumbnail", "marquee", "video"):
                el = game.find(tag)
                if el is None or not el.text:
                    continue

                media_ref = el.text.strip()
                media_path = Path(media_ref)
                if not media_path.is_absolute():
                    media_path = rom_dir / media_ref

                if media_path.exists():
                    media_found = True
                    break

            if media_found:
                covered.add(rom_ref)
                covered.add(Path(rom_ref).name)

    return covered

def media_stem_index():
    media_dir = rom_dir / "media"
    stems = set()

    if not media_dir.is_dir():
        return stems

    media_exts = {".png", ".jpg", ".jpeg", ".webp", ".mp4", ".mkv", ".avi"}
    for item in media_dir.rglob("*"):
        if not item.is_file() or item.suffix.lower() not in media_exts:
            continue
        stems.add(norm_name(item.stem))

    return stems

if not rom_dir.is_dir():
    print("0 0")
    raise SystemExit(0)

roms = sorted([p for p in rom_dir.rglob("*") if is_launchable(p)])
covered_refs = existing_media_paths_from_gamelist()
media_stems = media_stem_index()

missing = 0

for rom in roms:
    variants = path_variants(rom)
    stem = norm_name(rom.stem)

    has_media = False

    if any(v in covered_refs for v in variants):
        has_media = True
    elif rom.name in covered_refs:
        has_media = True
    elif stem and stem in media_stems:
        has_media = True

    if not has_media:
        missing += 1

print(f"{len(roms)} {missing}")
PY
}

should_skip_gather_for_existing_media() {
    [[ "${ONLY_MISSING}" == "true" ]] || return 1

    local total="0"
    local missing="0"

    if read -r total missing < <(media_missing_count 2>> "${LOG_FILE}"); then
        {
            echo "MEDIA_AWARE_ONLY_MISSING=true"
            echo "LAUNCHABLE_ROMS=${total}"
            echo "MISSING_MEDIA=${missing}"
            echo "-----"
        } >> "${LOG_FILE}"

        if [[ "${total}" =~ ^[0-9]+$ && "${missing}" =~ ^[0-9]+$ ]]; then
            if (( total > 0 && missing == 0 )); then
                {
                    echo "No missing generated media detected."
                    echo "Skipping online resource gathering to avoid downloading again."
                    echo "Use 'Generate gamelist' to rebuild the gamelist from cache."
                    echo "-----"
                } >> "${LOG_FILE}"
                return 0
            fi
        fi
    else
        {
            echo "MEDIA_AWARE_ONLY_MISSING=failed"
            echo "Falling back to Skyscraper native onlymissing."
            echo "-----"
        } >> "${LOG_FILE}"
    fi

    return 1
}


run_gather() {
    local flags="$1"

    {
        echo "GATHER_FLAGS=${flags}"
        echo "ONLY_MISSING=${ONLY_MISSING}"
        echo "VIDEOS_ENABLED=${VIDEOS_ENABLED}"
        echo "COMMAND=${SKY_BIN} -p ${SYSTEM_NAME} -g ${ROMDIR}/${SYSTEM_NAME} -o ${ROMDIR}/${SYSTEM_NAME}/media -s screenscraper --lang ${LANG_CODE} --flags ${flags}"
        echo "-----"
    } >> "${LOG_FILE}"

    if should_skip_gather_for_existing_media; then
        return 0
    fi

    "${SKY_BIN}" \
        -p "${SYSTEM_NAME}" \
        -g "${ROMDIR}/${SYSTEM_NAME}" \
        -o "${ROMDIR}/${SYSTEM_NAME}/media" \
        -s screenscraper \
        --lang "${LANG_CODE}" \
        --flags "${flags}"
}

run_generate() {
    local flags="$1"
    "${SKY_BIN}" \
        -p "${SYSTEM_NAME}" \
        -g "${ROMDIR}/${SYSTEM_NAME}" \
        -o "${ROMDIR}/${SYSTEM_NAME}/media" \
        --lang "${LANG_CODE}" \
        --flags "${flags}"
}

maintenance_size() {
    local sky_cache="${SKY_CFG_DIR}/cache"
    local sky_dbs="${SKY_CFG_DIR}/dbs"
    local sys_cache="${SKY_CFG_DIR}/cache/${SYSTEM_NAME}"
    local sys_dbs="${SKY_CFG_DIR}/dbs/${SYSTEM_NAME}"
    local rom_media="${ROMDIR}/${SYSTEM_NAME}/media"
    local rom_gamelist="${ROMDIR}/${SYSTEM_NAME}/gamelist.xml"
    local es_media="${HOME_DIR}/.emulationstation/downloaded_media/${SYSTEM_NAME}"
    local es_gamelist="${HOME_DIR}/.emulationstation/gamelists/${SYSTEM_NAME}/gamelist.xml"
    local skipped_home="${SKY_CFG_DIR}/skipped-${SYSTEM_NAME}-cache.txt"
    local skipped_retropie="${RETROPIE_SKY_CFG_DIR}/skipped-${SYSTEM_NAME}-cache.txt"

    : > "${LOG_FILE}"

    {
        echo "SKYSCRAPER MAINTENANCE"
        echo "System: ${SYSTEM_NAME}"
        echo "-----"
        echo ".skyscraper: $(path_size "${SKY_CFG_DIR}")"
        echo ".skyscraper/cache: $(path_size "${sky_cache}")"
        echo ".skyscraper/dbs: $(path_size "${sky_dbs}")"
        echo "Current system cache: $(path_size "${sys_cache}")"
        echo "Current system dbs: $(path_size "${sys_dbs}")"
        echo "ROM media: $(path_size "${rom_media}")"
        echo "ES media: $(path_size "${es_media}")"
        echo "ROM gamelist: $(path_size "${rom_gamelist}")"
        echo "ES gamelist: $(path_size "${es_gamelist}")"
        echo "Skipped file: $(path_size "${skipped_home}") / $(file_lines "${skipped_home}") lines"
        echo "Skipped file (RetroPie cfg): $(path_size "${skipped_retropie}") / $(file_lines "${skipped_retropie}") lines"
    } | tee -a "${LOG_FILE}"

    write_status "done:size:${SYSTEM_NAME}"
}

maintenance_clean_skipped() {
    local skipped_home="${SKY_CFG_DIR}/skipped-${SYSTEM_NAME}-cache.txt"
    local skipped_retropie="${RETROPIE_SKY_CFG_DIR}/skipped-${SYSTEM_NAME}-cache.txt"
    local removed=0

    : > "${LOG_FILE}"
    write_status "running:clean-skipped:${SYSTEM_NAME}"

    for f in "${skipped_home}" "${skipped_retropie}"; do
        if [[ -f "${f}" ]]; then
            rm -f "${f}"
            removed=$((removed + 1))
            echo "Removed: ${f}" >> "${LOG_FILE}"
        fi
    done

    echo "Skipped files removed: ${removed}" | tee -a "${LOG_FILE}"
    write_status "done:clean-skipped:${SYSTEM_NAME}"
}

maintenance_clean_temp() {
    write_status "running:clean-temp:${SYSTEM_NAME}"

    rm -f "${PID_FILE}"
    rm -f "${LOG_FILE}"

    write_status "done:clean-temp:${SYSTEM_NAME}"
    echo "Temporary files cleaned"
}

maintenance_vacuum() {
    trap cleanup EXIT INT TERM
    echo $$ > "${PID_FILE}"
    : > "${LOG_FILE}"
    write_status "running:vacuum:${SYSTEM_NAME}"

    {
        echo "Running Skyscraper cache vacuum for ${SYSTEM_NAME}"
        echo "-----"
    } >> "${LOG_FILE}"

    if "${SKY_BIN}" --flags unattend -p "${SYSTEM_NAME}" --cache vacuum >> "${LOG_FILE}" 2>&1; then
        write_status "done:vacuum:${SYSTEM_NAME}"
    else
        write_status "error:vacuum:${SYSTEM_NAME}"
        exit 1
    fi
}

maintenance_purge() {
    trap cleanup EXIT INT TERM
    echo $$ > "${PID_FILE}"
    : > "${LOG_FILE}"
    write_status "running:purge:${SYSTEM_NAME}"

    {
        echo "Running Skyscraper cache purge for ${SYSTEM_NAME}"
        echo "-----"
    } >> "${LOG_FILE}"

    if "${SKY_BIN}" --flags unattend -p "${SYSTEM_NAME}" --cache purge:all >> "${LOG_FILE}" 2>&1; then
        write_status "done:purge:${SYSTEM_NAME}"
    else
        write_status "error:purge:${SYSTEM_NAME}"
        exit 1
    fi
}

cleanup() {
    rm -f "${PID_FILE}"
}

run_foreground() {
    trap cleanup EXIT INT TERM

    : > "${LOG_FILE}"
    {
        echo "ACTION=${ACTION}"
        echo "SYSTEM_NAME=${SYSTEM_NAME}"
        echo "LANG_CODE=${LANG_CODE}"
        echo "VIDEOS_ENABLED=${VIDEOS_ENABLED}"
        echo "ONLY_MISSING=${ONLY_MISSING}"
        echo "SKY_BIN=${SKY_BIN}"
        echo "ROMDIR=${ROMDIR}/${SYSTEM_NAME}"
        echo "-----"
    } >> "${LOG_FILE}"

    echo $$ > "${PID_FILE}"

    case "${ACTION}" in
        gather)
            write_status "running:gather:${SYSTEM_NAME}"
            if run_gather "$(build_gather_flags)" >> "${LOG_FILE}" 2>&1; then
                write_status "done:gather:${SYSTEM_NAME}"
            else
                write_status "error:gather:${SYSTEM_NAME}"
                exit 1
            fi
            ;;
        generate)
            write_status "running:generate:${SYSTEM_NAME}"
            if run_generate "$(build_generate_flags)" >> "${LOG_FILE}" 2>&1; then
                write_status "done:generate:${SYSTEM_NAME}"
            else
                write_status "error:generate:${SYSTEM_NAME}"
                exit 1
            fi
            ;;
        *)
            fail "Unknown action: ${ACTION}"
            ;;
    esac
}

status_only() {
    if [[ -f "${STATUS_FILE}" ]]; then
        cat "${STATUS_FILE}"
    else
        echo "idle"
    fi
}

stop_job() {
    if [[ -f "${PID_FILE}" ]]; then
        local pid
        pid="$(cat "${PID_FILE}")"
        if kill -0 "${pid}" 2>/dev/null; then
            kill "${pid}" 2>/dev/null || true
            write_status "stopped"
        fi
        rm -f "${PID_FILE}"
    else
        write_status "idle"
    fi
}

start_background() {
    local real_action="$1"

    if [[ -f "${PID_FILE}" ]]; then
        local pid
        pid="$(cat "${PID_FILE}" 2>/dev/null || true)"
        if [[ -n "${pid}" ]] && kill -0 "${pid}" 2>/dev/null; then
            write_status "running:${real_action}:${SYSTEM_NAME}"
            exit 0
        else
            rm -f "${PID_FILE}"
        fi
    fi

    write_status "starting:${real_action}:${SYSTEM_NAME}"

    nohup "$0" "${real_action}" "${SYSTEM_NAME}" "${LANG_CODE}" "" "${VIDEOS_ENABLED}" "${ONLY_MISSING}" >> "${LOG_FILE}" 2>&1 &
    exit 0
}

main() {
    case "${ACTION}" in
        status)
            status_only
            exit 0
            ;;
        stop)
            stop_job
            exit 0
            ;;
        maintenance-size)
            ensure_system
            maintenance_size
            exit 0
            ;;
        maintenance-clean-skipped)
            ensure_system
            ensure_not_running
            maintenance_clean_skipped
            exit 0
            ;;
        maintenance-clean-temp)
            ensure_system
            ensure_not_running
            maintenance_clean_temp
            exit 0
            ;;
        maintenance-vacuum)
            ensure_bin
            ensure_config
            ensure_system
            ensure_not_running
            maintenance_vacuum
            exit 0
            ;;
        maintenance-purge)
            ensure_bin
            ensure_config
            ensure_system
            ensure_not_running
            maintenance_purge
            exit 0
            ;;
        start-gather)
            ensure_bin
            ensure_config
            ensure_system
            LANG_CODE="$(normalize_lang "${LANG_CODE}")"
            VIDEOS_ENABLED="$(normalize_videos "${VIDEOS_ENABLED}")"
            ONLY_MISSING="$(normalize_only_missing "${ONLY_MISSING}")"
            update_config_lang "${LANG_CODE}" "$(build_lang_prios "${LANG_CODE}")"
            update_config_videos "${VIDEOS_ENABLED}"
            start_background "gather"
            ;;
        start-generate)
            ensure_bin
            ensure_config
            ensure_system
            LANG_CODE="$(normalize_lang "${LANG_CODE}")"
            VIDEOS_ENABLED="$(normalize_videos "${VIDEOS_ENABLED}")"
            ONLY_MISSING="$(normalize_only_missing "${ONLY_MISSING}")"
            update_config_lang "${LANG_CODE}" "$(build_lang_prios "${LANG_CODE}")"
            update_config_videos "${VIDEOS_ENABLED}"
            start_background "generate"
            ;;
        gather|generate)
            ;;
        *)
            fail "Unknown action: ${ACTION}"
            ;;
    esac

    ensure_bin
    ensure_config
    ensure_system

    LANG_CODE="$(normalize_lang "${LANG_CODE}")"
    VIDEOS_ENABLED="$(normalize_videos "${VIDEOS_ENABLED}")"
    ONLY_MISSING="$(normalize_only_missing "${ONLY_MISSING}")"

    update_config_lang "${LANG_CODE}" "$(build_lang_prios "${LANG_CODE}")"
    update_config_videos "${VIDEOS_ENABLED}"

    run_foreground
}

main "$@"
