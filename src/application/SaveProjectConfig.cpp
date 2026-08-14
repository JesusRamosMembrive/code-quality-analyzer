#include "codeqa/application/SaveProjectConfig.h"

#include "codeqa/config/ConfigJson.h"

#include <atomic>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <cerrno>
#include <cstdio>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace codeqa::application
{

namespace
{

struct ConfigPathResolution
{
    std::optional<std::filesystem::path> path;
    bool exists{false};
    std::optional<std::string> error;
};

std::atomic<std::uint64_t> temporaryFileSequence{0};

std::string unavailablePathError(const std::filesystem::path& path)
{
    return "Error [CONFIG011]: configuration path '" + path.string()
        + "' is unavailable or resolves outside the project root.";
}

#ifdef _WIN32
std::optional<bool> isUnsafeWindowsLink(const std::filesystem::path& path)
{
    const DWORD attributes = GetFileAttributesW(path.c_str());
    if (attributes == INVALID_FILE_ATTRIBUTES)
    {
        return std::nullopt;
    }
    if ((attributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0)
    {
        return true;
    }

    const HANDLE handle = CreateFileW(
        path.c_str(),
        FILE_READ_ATTRIBUTES,
        FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
        nullptr,
        OPEN_EXISTING,
        FILE_FLAG_OPEN_REPARSE_POINT,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return std::nullopt;
    }

    BY_HANDLE_FILE_INFORMATION information{};
    const bool inspected = GetFileInformationByHandle(handle, &information) != 0;
    const bool closed = CloseHandle(handle) != 0;
    if (!inspected || !closed)
    {
        return std::nullopt;
    }
    return information.nNumberOfLinks > 1;
}

std::uint64_t processId()
{
    return static_cast<std::uint64_t>(GetCurrentProcessId());
}
#else
std::optional<bool> isUnsafePosixLink(const std::filesystem::path& path)
{
    struct stat information{};
    if (lstat(path.c_str(), &information) != 0)
    {
        return std::nullopt;
    }
    return S_ISLNK(information.st_mode) || information.st_nlink > 1;
}

std::uint64_t processId()
{
    return static_cast<std::uint64_t>(getpid());
}
#endif

ConfigPathResolution resolveConfigPath(const std::filesystem::path& projectRoot)
{
    const std::filesystem::path requestedPath = projectRoot / "codeqa.json";
    std::error_code rootError;
    const std::filesystem::path canonicalRoot = std::filesystem::canonical(projectRoot, rootError);
    if (rootError)
    {
        return {std::nullopt, false, unavailablePathError(requestedPath)};
    }

    std::error_code directoryError;
    if (!std::filesystem::is_directory(canonicalRoot, directoryError) || directoryError)
    {
        return {std::nullopt, false, unavailablePathError(requestedPath)};
    }

    const std::filesystem::path target = canonicalRoot / "codeqa.json";
    std::error_code statusError;
    const std::filesystem::file_status status = std::filesystem::symlink_status(target, statusError);
    if (statusError && status.type() != std::filesystem::file_type::not_found)
    {
        return {std::nullopt, false, unavailablePathError(target)};
    }
    if (status.type() == std::filesystem::file_type::not_found)
    {
        return {target, false, std::nullopt};
    }
    if (status.type() == std::filesystem::file_type::symlink
        || status.type() != std::filesystem::file_type::regular)
    {
        return {std::nullopt, true, unavailablePathError(target)};
    }

#ifdef _WIN32
    const std::optional<bool> unsafeLink = isUnsafeWindowsLink(target);
#else
    const std::optional<bool> unsafeLink = isUnsafePosixLink(target);
#endif
    if (!unsafeLink.has_value() || *unsafeLink)
    {
        return {std::nullopt, true, unavailablePathError(target)};
    }

    std::error_code targetError;
    const std::filesystem::path canonicalTarget = std::filesystem::canonical(target, targetError);
    std::error_code equivalentError;
    const bool hasExpectedParent = !targetError && std::filesystem::equivalent(
        canonicalTarget.parent_path(), canonicalRoot, equivalentError);
    if (targetError || equivalentError || !hasExpectedParent)
    {
        return {std::nullopt, true, unavailablePathError(target)};
    }

    return {target, true, std::nullopt};
}

std::filesystem::path temporaryPathFor(const std::filesystem::path& target)
{
    const std::uint64_t sequence = temporaryFileSequence.fetch_add(1, std::memory_order_relaxed);
    const std::string filename = target.filename().string() + ".tmp."
        + std::to_string(processId()) + "." + std::to_string(sequence);
    return target.parent_path() / filename;
}

void removeTemporaryFile(const std::filesystem::path& path)
{
    std::error_code ignored;
    std::filesystem::remove(path, ignored);
}

#ifdef _WIN32
std::optional<std::filesystem::path> writeTemporaryConfig(
    const std::filesystem::path& target, const std::string& contents, std::string& error)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path temporary = temporaryPathFor(target);
        const HANDLE handle = CreateFileW(
            temporary.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            const DWORD lastError = GetLastError();
            if (lastError == ERROR_FILE_EXISTS || lastError == ERROR_ALREADY_EXISTS)
            {
                continue;
            }
            error = "Error [CONFIG011]: could not create a temporary configuration file beside '"
                + target.string() + "'.";
            return std::nullopt;
        }

        std::size_t written = 0;
        bool succeeded = true;
        while (written < contents.size())
        {
            const std::size_t remaining = contents.size() - written;
            const std::size_t maximumChunk = static_cast<std::size_t>(std::numeric_limits<DWORD>::max());
            const DWORD chunk = static_cast<DWORD>(remaining < maximumChunk ? remaining : maximumChunk);
            DWORD chunkWritten = 0;
            if (WriteFile(handle, contents.data() + written, chunk, &chunkWritten, nullptr) == 0
                || chunkWritten == 0)
            {
                succeeded = false;
                break;
            }
            written += static_cast<std::size_t>(chunkWritten);
        }

        if (succeeded && FlushFileBuffers(handle) == 0)
        {
            succeeded = false;
        }
        if (CloseHandle(handle) == 0)
        {
            succeeded = false;
        }
        if (!succeeded)
        {
            removeTemporaryFile(temporary);
            error = "Error [CONFIG011]: could not write configuration file '"
                + target.string() + "'.";
            return std::nullopt;
        }
        return temporary;
    }

    error = "Error [CONFIG011]: could not create a unique temporary configuration file beside '"
        + target.string() + "'.";
    return std::nullopt;
}

bool replaceConfigFile(
    const std::filesystem::path& target,
    const std::filesystem::path& temporary,
    const bool targetExists)
{
    if (targetExists)
    {
        return ReplaceFileW(
            target.c_str(), temporary.c_str(), nullptr, REPLACEFILE_WRITE_THROUGH, nullptr, nullptr) != 0;
    }
    return MoveFileExW(temporary.c_str(), target.c_str(), MOVEFILE_WRITE_THROUGH) != 0;
}
#else
std::optional<std::filesystem::path> writeTemporaryConfig(
    const std::filesystem::path& target, const std::string& contents, std::string& error)
{
    for (int attempt = 0; attempt < 100; ++attempt)
    {
        const std::filesystem::path temporary = temporaryPathFor(target);
        const int descriptor = open(temporary.c_str(), O_WRONLY | O_CREAT | O_EXCL, 0600);
        if (descriptor < 0)
        {
            if (errno == EEXIST)
            {
                continue;
            }
            error = "Error [CONFIG011]: could not create a temporary configuration file beside '"
                + target.string() + "'.";
            return std::nullopt;
        }

        std::size_t written = 0;
        bool succeeded = true;
        while (written < contents.size())
        {
            const ssize_t result = write(
                descriptor, contents.data() + written, contents.size() - written);
            if (result <= 0)
            {
                succeeded = false;
                break;
            }
            written += static_cast<std::size_t>(result);
        }
        if (succeeded && fsync(descriptor) != 0)
        {
            succeeded = false;
        }
        if (close(descriptor) != 0)
        {
            succeeded = false;
        }
        if (!succeeded)
        {
            removeTemporaryFile(temporary);
            error = "Error [CONFIG011]: could not write configuration file '"
                + target.string() + "'.";
            return std::nullopt;
        }
        return temporary;
    }

    error = "Error [CONFIG011]: could not create a unique temporary configuration file beside '"
        + target.string() + "'.";
    return std::nullopt;
}

bool replaceConfigFile(
    const std::filesystem::path& target,
    const std::filesystem::path& temporary,
    const bool)
{
    return std::rename(temporary.c_str(), target.c_str()) == 0;
}
#endif

} // namespace

std::optional<std::string> SaveProjectConfig::execute(
    const std::filesystem::path& projectRoot, const codeqa::config::ProjectConfig& config) const
{
    const nlohmann::json document = codeqa::config::toJson(config);
    const codeqa::config::ParseConfigResult validation = codeqa::config::parseConfig(document.dump());

    if (!validation.config.has_value())
    {
        return validation.error.value_or("Error [CONFIG010]: configuration validation failed.");
    }

    const ConfigPathResolution initialResolution = resolveConfigPath(projectRoot);
    if (!initialResolution.path.has_value())
    {
        return initialResolution.error;
    }
    const std::filesystem::path& target = *initialResolution.path;

    std::string writeError;
    const std::optional<std::filesystem::path> temporary = writeTemporaryConfig(
        target, document.dump(2) + '\n', writeError);
    if (!temporary.has_value())
    {
        return writeError;
    }

    const ConfigPathResolution finalResolution = resolveConfigPath(projectRoot);
    if (!finalResolution.path.has_value() || *finalResolution.path != target)
    {
        removeTemporaryFile(*temporary);
        return finalResolution.error.value_or(unavailablePathError(target));
    }

    if (!replaceConfigFile(target, *temporary, finalResolution.exists))
    {
        removeTemporaryFile(*temporary);
        return "Error [CONFIG011]: could not replace configuration file '"
            + target.string() + "'; the existing file was preserved.";
    }

    return std::nullopt;
}

} // namespace codeqa::application
