#pragma once
#ifndef ES_APP_THEGAMESDB_RATE_LIMIT_H
#define ES_APP_THEGAMESDB_RATE_LIMIT_H

// Session-local cooldown for TheGamesDB API requests only. No waiting/sleeping
// and no impact on ScreenScraper, artwork downloads or unrelated HTTP hosts.
#include <chrono>
#include <mutex>
#include <string>

namespace TheGamesDBRateLimit
{
inline std::mutex& mutex()
{
    static std::mutex value;
    return value;
}

inline std::chrono::steady_clock::time_point& blockedUntil()
{
    static std::chrono::steady_clock::time_point value;
    return value;
}

inline bool isApiUrl(const std::string& url)
{
    const std::string prefix = "https://api.thegamesdb.net/";
    return url.compare(0, prefix.size(), prefix) == 0;
}

inline long remainingSeconds()
{
    std::lock_guard<std::mutex> guard(mutex());
    const auto now = std::chrono::steady_clock::now();
    if (blockedUntil() <= now)
        return 0;
    // Round up, so a fractional remaining second never appears as 0.
    return std::chrono::duration_cast<std::chrono::seconds>(
        blockedUntil() - now).count() + 1;
}

inline void noteHttp429()
{
    // A conservative local pause, not a promise that the server quota resets.
    const auto candidate = std::chrono::steady_clock::now() +
        std::chrono::seconds(120);
    std::lock_guard<std::mutex> guard(mutex());
    if (candidate > blockedUntil())
        blockedUntil() = candidate;
}
} // namespace TheGamesDBRateLimit

#endif
