#pragma once
#include <chrono>
#include <filesystem>
#include <system_error>
#include <thread>

namespace SeisWave {
// Replace only after the complete temporary file is closed. Never delete the old
// target to bypass sharing errors: readers must see either the old or new file.
inline bool replaceCompletedFile(const std::filesystem::path &pending,
                                 const std::filesystem::path &target, std::error_code &error) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        std::filesystem::rename(pending, target, error);
        if (!error) return true;
#ifdef _WIN32
        // A closed output can remain briefly held by a Windows file indexer or
        // scanner. Permanent access restrictions still fail after a bounded wait.
        if ((error.value() != 5 && error.value() != 32 && error.value() != 33) || attempt == 7)
            return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(25 * (attempt + 1)));
#else
        return false;
#endif
    }
    return false;
}
inline void replaceCompletedFile(const std::filesystem::path &pending, const std::filesystem::path &target) {
    std::error_code error;
    if (!replaceCompletedFile(pending, target, error))
        throw std::filesystem::filesystem_error("Could not replace completed output file", pending, target, error);
}
} // namespace SeisWave
