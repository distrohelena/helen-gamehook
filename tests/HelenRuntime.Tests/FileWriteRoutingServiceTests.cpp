#include <HelenHook/FileReadPolicy.h>
#include <HelenHook/FileWritePolicy.h>
#include <HelenHook/FileWriteRoute.h>
#include <HelenHook/FileWriteRoutingService.h>
#include <HelenHook/FileWriteRoutingTransaction.h>

#include <array>
#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace
{
    /**
     * @brief Stops the native test process when one behavioral assertion is false.
     * @param condition Behavioral assertion that should hold.
     * @param message Diagnostic explaining the failed expectation.
     */
    void Expect(bool condition, const char* message)
    {
        if (!condition)
        {
            throw std::runtime_error(message);
        }
    }

    /**
     * @brief Writes an exact byte sequence to one temporary test file.
     * @param path Destination file path.
     * @param bytes Bytes that should replace the file contents.
     */
    void WriteAllBytes(const std::filesystem::path& path, std::string_view bytes)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            throw std::runtime_error("Failed to create routing test file.");
        }

        stream.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
        if (!stream)
        {
            throw std::runtime_error("Failed to write routing test file.");
        }
    }

    /**
     * @brief Reads every byte from a native file handle and closes it.
     * @param handle Synchronous file handle to read.
     * @return Exact bytes returned by ReadFile.
     */
    std::string ReadHandle(HANDLE handle)
    {
        std::string result;
        std::array<char, 64> buffer{};
        for (;;)
        {
            DWORD bytes_read = 0;
            if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr))
            {
                CloseHandle(handle);
                throw std::runtime_error("ReadFile failed in routing test.");
            }

            result.append(buffer.data(), bytes_read);
            if (bytes_read != buffer.size())
            {
                break;
            }
        }

        Expect(CloseHandle(handle) != FALSE, "CloseHandle failed in routing test.");
        return result;
    }

    /**
     * @brief Opens one route through the service and returns its complete readable payload.
     * @param service Routing service under test.
     * @param path Path supplied by the caller.
     * @param access Desired access passed to CreateFileW.
     * @return Bytes exposed by the routed open.
     */
    std::string ReadThroughRouter(helen::FileWriteRoutingService& service, const std::filesystem::path& path, DWORD access = GENERIC_READ)
    {
        const HANDLE handle = service.Open(path, access, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(handle != INVALID_HANDLE_VALUE, "Routed read open failed.");
        std::array<char, 64> buffer{};
        std::string result;
        for (;;)
        {
            DWORD bytes_read = 0;
            if (!ReadFile(handle, buffer.data(), static_cast<DWORD>(buffer.size()), &bytes_read, nullptr))
            {
                service.Close(handle);
                throw std::runtime_error("Routed ReadFile failed in routing test.");
            }

            result.append(buffer.data(), bytes_read);
            if (bytes_read != buffer.size())
            {
                break;
            }
        }

        Expect(service.Close(handle) != FALSE, "Routed close failed in routing test.");
        return result;
    }

    /**
     * @brief Finds the single session-owned overlay file below a fresh routing session.
     * @param cache_directory Cache root supplied to the routing service.
     * @return Overlay path discovered beneath the service-owned session directory.
     */
    std::filesystem::path FindOverlay(const std::filesystem::path& cache_directory)
    {
        for (const std::filesystem::directory_entry& session_entry : std::filesystem::directory_iterator(cache_directory))
        {
            if (!session_entry.is_directory())
            {
                continue;
            }

            for (const std::filesystem::directory_entry& overlay_entry : std::filesystem::recursive_directory_iterator(session_entry.path()))
            {
                if (overlay_entry.is_regular_file())
                {
                    return overlay_entry.path();
                }
            }
        }

        return {};
    }
}

/**
 * @brief Verifies real-file session routing, exact source policies, handle tracking, and trusted synchronization.
 */
void RunFileWriteRoutingServiceTests()
{
    const std::filesystem::path root = std::filesystem::temp_directory_path() / "HelenRuntimeTests" / "FileWriteRouting";
    std::filesystem::remove_all(root);
    std::filesystem::create_directories(root / "documents");

    try
    {
        const std::filesystem::path cache_directory = root / "cache";
        const std::filesystem::path request_base = root / "documents";
        const std::filesystem::path redirect_path = request_base / "redirect.ini";
        const std::filesystem::path deny_path = request_base / "deny.ini";
        const std::filesystem::path original_read_path = request_base / "original.ini";
        const std::filesystem::path unrelated_path = request_base / "unrelated.ini";
        const std::filesystem::path cancellation_path = request_base / "cancellation.ini";
        WriteAllBytes(redirect_path, "A");
        WriteAllBytes(deny_path, "D");
        WriteAllBytes(original_read_path, "O");
        WriteAllBytes(unrelated_path, "U");
        WriteAllBytes(cancellation_path, "V");

        const std::vector<helen::FileWriteRoute> routes = {
            { "redirect", redirect_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected },
            { "deny", deny_path, helen::FileWritePolicy::Deny, helen::FileReadPolicy::Original },
            { "original", original_read_path, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Original },
        };

        helen::FileWriteRoutingService service(cache_directory, request_base);
        DWORD error = ERROR_SUCCESS;
        Expect(service.Initialize(routes, error), "Routing service initialization failed.");
        Expect(error == ERROR_SUCCESS, "Successful initialization changed the error code.");
        Expect(service.IsProtectedPath(redirect_path), "Declared route was not protected.");
        Expect(!service.IsProtectedPath(unrelated_path), "Unrelated file was reported as protected.");

        const std::filesystem::path extended_redirect_path = std::filesystem::path(L"\\\\?\\" + redirect_path.wstring());
        Expect(service.IsProtectedPath(extended_redirect_path), "Extended route spelling was not recognized.");
        Expect(ReadThroughRouter(service, extended_redirect_path) == "A", "Extended route spelling did not select the session copy.");

        Expect(ReadThroughRouter(service, redirect_path) == "A", "Redirected session did not start from original bytes.");
        const HANDLE redirected_write = service.Open(redirect_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(redirected_write != INVALID_HANDLE_VALUE, "Redirected CREATE_ALWAYS open failed.");
        Expect(service.IsTrackedHandle(redirected_write), "Routed handle was not tracked.");
        DWORD bytes_written = 0;
        Expect(WriteFile(redirected_write, "B", 1, &bytes_written, nullptr) != FALSE && bytes_written == 1, "Redirected write failed.");
        Expect(service.Close(redirected_write) != FALSE, "Routed close failed.");
        Expect(ReadThroughRouter(service, redirect_path) == "B", "Redirected read did not expose session bytes.");
        Expect(ReadThroughRouter(service, redirect_path, GENERIC_READ) == "B", "Redirected read policy changed unexpectedly.");
        Expect(ReadHandle(CreateFileW(redirect_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) == "A", "Original bytes changed through redirect.");

        const HANDLE truncate_handle = service.Open(redirect_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(truncate_handle != INVALID_HANDLE_VALUE, "Redirected truncate open failed.");
        Expect(service.Close(truncate_handle) != FALSE, "Redirected truncate close failed.");
        Expect(ReadThroughRouter(service, redirect_path).empty(), "CREATE_ALWAYS did not truncate the session file.");

        const HANDLE denied_handle = service.Open(deny_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(denied_handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_ACCESS_DENIED, "Deny route allowed a writable open.");
        Expect(ReadThroughRouter(service, deny_path) == "D", "Deny route did not read the original.");
        Expect(ReadThroughRouter(service, original_read_path) == "O", "Original-read route did not read the original.");

        const HANDLE original_policy_write = service.Open(original_read_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(original_policy_write != INVALID_HANDLE_VALUE, "Original-read redirect write open failed.");
        bytes_written = 0;
        Expect(WriteFile(original_policy_write, "X", 1, &bytes_written, nullptr) != FALSE && bytes_written == 1, "Original-read redirect write failed.");
        Expect(service.Close(original_policy_write) != FALSE, "Original-read redirect close failed.");
        Expect(ReadThroughRouter(service, original_read_path) == "O", "Original-read policy exposed the redirected write.");

        const std::filesystem::path missing_overlay_cache = root / "missing-overlay-cache";
        {
            helen::FileWriteRoutingService missing_overlay_service(missing_overlay_cache, request_base);
            Expect(missing_overlay_service.Initialize({ routes.front() }, error), "Missing-overlay setup initialization failed.");
            const std::filesystem::path missing_overlay = FindOverlay(missing_overlay_cache);
            Expect(!missing_overlay.empty() && DeleteFileW(missing_overlay.wstring().c_str()) != FALSE, "Missing-overlay setup did not remove its session file.");
            const HANDLE missing_overlay_read = missing_overlay_service.Open(redirect_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Expect(missing_overlay_read == INVALID_HANDLE_VALUE, "Missing overlay was recreated or fell back to the original.");
        }

        const HANDLE unrelated_handle = service.Open(unrelated_path, GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(unrelated_handle != INVALID_HANDLE_VALUE, "Unrelated native open did not pass through.");
        Expect(service.IsTrackedHandle(unrelated_handle) == false, "Unrelated native handle was tracked.");
        Expect(service.Close(unrelated_handle) == FALSE && GetLastError() == ERROR_INVALID_HANDLE, "Close accepted an unrelated handle.");

        const HANDLE busy_handle = service.Open(redirect_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(busy_handle != INVALID_HANDLE_VALUE, "Busy-handle setup open failed.");
        const std::unique_ptr<helen::FileWriteRoutingTransaction> busy_transaction = service.BeginTrustedWrite({ redirect_path }, error);
        Expect(busy_transaction == nullptr && error == ERROR_SHARING_VIOLATION, "Busy routed handle did not block trusted write.");
        Expect(service.Close(busy_handle) != FALSE, "Busy-handle setup close failed.");

        const std::unique_ptr<helen::FileWriteRoutingTransaction> transaction = service.BeginTrustedWrite({ redirect_path }, error);
        Expect(transaction != nullptr, "Trusted write transaction acquisition failed.");
        const HANDLE original_writer = CreateFileW(redirect_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(original_writer != INVALID_HANDLE_VALUE, "Trusted original writer open failed.");
        bytes_written = 0;
        Expect(WriteFile(original_writer, "C", 1, &bytes_written, nullptr) != FALSE && bytes_written == 1, "Trusted original write failed.");
        Expect(CloseHandle(original_writer) != FALSE, "Trusted original writer close failed.");
        Expect(transaction->Synchronize(error), "Trusted synchronization failed.");
        Expect(ReadThroughRouter(service, redirect_path) == "C", "Trusted synchronization did not update overlay.");
        Expect(ReadHandle(CreateFileW(redirect_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) == "C", "Trusted original bytes were not persisted.");

        const std::filesystem::path failure_cache = root / "failure-cache";
        {
            helen::FileWriteRoutingService failure_service(failure_cache, request_base);
            Expect(failure_service.Initialize(routes, error), "Synchronization failure setup initialization failed.");
            const std::unique_ptr<helen::FileWriteRoutingTransaction> failure_transaction = failure_service.BeginTrustedWrite({ redirect_path }, error);
            Expect(failure_transaction != nullptr, "Synchronization failure setup transaction acquisition failed.");
            const HANDLE original_lock = CreateFileW(redirect_path.c_str(), GENERIC_READ, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Expect(original_lock != INVALID_HANDLE_VALUE, "Synchronization failure lock setup failed.");
            Expect(!failure_transaction->Synchronize(error) && error == ERROR_SHARING_VIOLATION, "Locked original unexpectedly synchronized.");
            Expect(CloseHandle(original_lock) != FALSE, "Synchronization failure lock close failed.");
            const HANDLE failed_route_read = failure_service.Open(redirect_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
            Expect(failed_route_read == INVALID_HANDLE_VALUE, "Synchronization failure did not fail protected reads closed.");
        }

        std::unique_ptr<helen::FileWriteRoutingTransaction> abandoned = service.BeginTrustedWrite({ redirect_path }, error);
        Expect(abandoned != nullptr, "Abandonment setup transaction acquisition failed.");
        abandoned.reset();
        const HANDLE failed_read = service.Open(redirect_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(failed_read == INVALID_HANDLE_VALUE, "Abandoned transaction did not fail closed.");

        const std::filesystem::path overlay_path = FindOverlay(cache_directory);
        Expect(!overlay_path.empty(), "Session overlay was not materialized.");

        helen::FileWriteRoutingService second_service(cache_directory, request_base);
        Expect(second_service.Initialize(routes, error), "Second routing session initialization failed.");
        Expect(ReadThroughRouter(second_service, redirect_path) == "C", "New session did not start from the current original.");
        const std::unique_ptr<helen::FileWriteRoutingTransaction> no_write = second_service.BeginTrustedWrite({ cancellation_path }, error);
        Expect(no_write != nullptr, "Unprotected no-write transaction was rejected.");
        no_write->CancelWithoutWrite();
        const HANDLE unrelated_read = second_service.Open(cancellation_path, GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(unrelated_read != INVALID_HANDLE_VALUE, "No-write cancellation damaged unrelated access.");
        Expect(ReadHandle(unrelated_read) == "V", "No-write cancellation changed unrelated bytes.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
