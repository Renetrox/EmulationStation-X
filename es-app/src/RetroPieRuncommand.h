#pragma once
// ES-X optional RetroPie Runcommand backend. Header-only: no CMake edits.
// RetroPie owns emulator choices; ES-X should not duplicate core settings handled by RetroPie.
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>
#include <vector>

#ifndef WIN32
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace RetroPieRuncommand
{
struct Entry
{
    std::string key;
    std::string value;
};

inline std::string trim(const std::string& value)
{
    const std::string::size_type first = value.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return "";
    const std::string::size_type last = value.find_last_not_of(" \t\r\n");
    return value.substr(first, last - first + 1);
}

inline bool enabled()
{
#ifdef WIN32
    return false;
#else
    // The RetroPie-Extra installer intentionally stays unchanged. In this
    // non-Batocera build detect the installed RetroPie runtime automatically.
    // Setting the variable to 0 opts out; 1 explicitly opts in.
    const char* setting = std::getenv("ESX_USE_RETROPIE_RUNCOMMAND");
    if (setting != nullptr)
        return std::string(setting) == "1";
#if defined(BATOCERA)
    return false;
#else
    return std::ifstream("/opt/retropie/supplementary/runcommand/runcommand.sh").good();
#endif
#endif
}

inline bool safeId(const std::string& value)
{
    if (value.empty() || value == "." || value == "..")
        return false;
    for (unsigned char ch : value)
    {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
              (ch >= '0' && ch <= '9') || ch == '_' || ch == '-' || ch == '.'))
            return false;
    }
    return true;
}

inline bool exists(const std::string& path)
{
    std::ifstream file(path.c_str());
    return file.good();
}

inline std::string systemConfig(const std::string& system)
{
    return "/opt/retropie/configs/" + system + "/emulators.cfg";
}

inline std::string romConfig()
{
    return "/opt/retropie/configs/all/emulators.cfg";
}

inline bool handlesSystem(const std::string& system)
{
    // Keep custom application launcher commands as configured by RetroPie
    // or ES-X. Settings shortcuts, ports, Kodi and Wine launchers do not use
    // the ordinary game _SYS_ Runcommand contract.
    if (system == "retropie" || system == "ports" ||
        system == "kodi" || system == "wine")
        return false;
    return enabled() && safeId(system) &&
        exists("/opt/retropie/supplementary/runcommand/runcommand.sh") &&
        exists(systemConfig(system));
}

// Runcommand uses clean_name "${SYSTEM}_${ROM_BN}": retain only ASCII
// letters, digits, underscore and hyphen. The prefix path is not included.
inline std::string romKey(const std::string& system, const std::string& rom)
{
    if (!safeId(system))
        return "";
    std::string filename = rom.substr(rom.find_last_of('/') == std::string::npos ?
        0 : rom.find_last_of('/') + 1);
    const std::string::size_type extension = filename.find_last_of('.');
    if (extension != std::string::npos)
        filename.erase(extension);
    const std::string combined = system + "_" + filename;
    std::string key;
    for (unsigned char ch : combined)
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-')
            key += (char)ch;
    return key;
}

inline std::vector<Entry> readEntries(const std::string& path)
{
    std::vector<Entry> entries;
    std::ifstream input(path.c_str());
    std::string line;
    while (std::getline(input, line))
    {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        const std::string::size_type pos = line.find('=');
        if (pos == std::string::npos)
            continue;
        const std::string key = trim(line.substr(0, pos));
        std::string value = trim(line.substr(pos + 1));
        if (key.empty())
            continue;
        if (value.size() >= 2 && value[0] == '"' && value[value.size() - 1] == '"')
            value = value.substr(1, value.size() - 2);
        entries.push_back({key, value});
    }
    return entries;
}

inline std::string getValue(const std::string& path, const std::string& key)
{
    for (const Entry& entry : readEntries(path))
        if (entry.key == key)
            return entry.value;
    return "";
}

inline std::vector<std::string> emulators(const std::string& system)
{
    std::vector<std::string> result;
    if (!handlesSystem(system))
        return result;
    for (const Entry& entry : readEntries(systemConfig(system)))
        if (entry.key != "default" && safeId(entry.key) && !entry.value.empty() &&
            std::find(result.begin(), result.end(), entry.key) == result.end())
            result.push_back(entry.key);
    return result;
}

inline bool validChoice(const std::vector<std::string>& candidates, const std::string& value)
{
    return std::find(candidates.begin(), candidates.end(), value) != candidates.end();
}

// Preserve unrelated keys and comments. Write atomically in the same directory.
// Empty value removes a per-ROM override so Runcommand inherits the system choice.
inline bool setValue(const std::string& path, const std::string& key, const std::string& value)
{
#ifdef WIN32
    return false;
#else
    if (!safeId(key) || (!value.empty() && !safeId(value)))
        return false;

    struct stat st;
    const bool alreadyExists = (::stat(path.c_str(), &st) == 0);
    if (alreadyExists && !S_ISREG(st.st_mode))
        return false;

    std::vector<std::string> lines;
    if (alreadyExists)
    {
        std::ifstream input(path.c_str());
        if (!input)
            return false;
        std::string line;
        while (std::getline(input, line))
            lines.push_back(line);
        if (input.bad())
            return false;
    }

    std::vector<std::string> updated;
    bool found = false;
    for (const std::string& line : lines)
    {
        const std::string::size_type pos = line.find('=');
        if (pos != std::string::npos && trim(line.substr(0, pos)) == key)
        {
            if (!found && !value.empty())
                updated.push_back(key + " = \"" + value + "\"");
            found = true;
        }
        else
            updated.push_back(line);
    }
    if (!found && !value.empty())
        updated.push_back(key + " = \"" + value + "\"");

    const std::string temp = path + ".esx-" + std::to_string((long)::getpid()) + ".tmp";
    const int fd = ::open(temp.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
    if (fd < 0)
        return false;
    bool ok = (::fchmod(fd, alreadyExists ? (st.st_mode & 0777) : 0644) == 0);
    if (alreadyExists && ok)
        ok = (::fchown(fd, st.st_uid, st.st_gid) == 0 || errno == EPERM);
    for (const std::string& line : updated)
    {
        if (!ok)
            break;
        const std::string data = line + "\n";
        size_t offset = 0;
        while (offset < data.size())
        {
            const ssize_t written = ::write(fd, data.data() + offset, data.size() - offset);
            if (written < 0 && errno == EINTR)
                continue;
            if (written <= 0)
            {
                ok = false;
                break;
            }
            offset += (size_t)written;
        }
    }
    if (ok)
        ok = (::fsync(fd) == 0);
    if (::close(fd) != 0)
        ok = false;
    if (ok)
        ok = (::rename(temp.c_str(), path.c_str()) == 0);
    if (!ok)
        ::unlink(temp.c_str());
    return ok;
#endif
}

inline bool setSystemDefault(const std::string& system, const std::string& emulator)
{
    return handlesSystem(system) && validChoice(emulators(system), emulator) &&
        setValue(systemConfig(system), "default", emulator);
}

inline bool setGameDefault(const std::string& system, const std::string& rom,
                           const std::string& emulator)
{
    const std::string key = romKey(system, rom);
    return handlesSystem(system) && !key.empty() &&
        (emulator.empty() || validChoice(emulators(system), emulator)) &&
        setValue(romConfig(), key, emulator);
}
} // namespace RetroPieRuncommand
