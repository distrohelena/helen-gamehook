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
        const std::string original_before_busy = ReadHandle(CreateFileW(redirect_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr));
        const std::unique_ptr<helen::FileWriteRoutingTransaction> busy_transaction = service.BeginTrustedWrite({ redirect_path }, error);
        Expect(busy_transaction == nullptr && error == ERROR_SHARING_VIOLATION, "Busy routed handle did not block trusted write.");
        Expect(ReadHandle(CreateFileW(redirect_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) == original_before_busy, "Busy transaction changed original bytes.");
        Expect(service.Close(busy_handle) != FALSE, "Busy-handle setup close failed.");

        const HANDLE locked_original = CreateFileW(redirect_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(locked_original != INVALID_HANDLE_VALUE, "Original-lock setup failed.");
        const HANDLE verify_locked_original = CreateFileW(redirect_path.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        const bool metadata_lock_verified = verify_locked_original == INVALID_HANDLE_VALUE;
        if (verify_locked_original != INVALID_HANDLE_VALUE)
        {
            CloseHandle(verify_locked_original);
        }
        Expect(CloseHandle(locked_original) != FALSE, "Original-lock fixture close failed.");
        Expect(metadata_lock_verified, "Original-lock fixture did not deny metadata sharing.");
        const HANDLE locked_original_again = CreateFileW(redirect_path.c_str(), GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(locked_original_again != INVALID_HANDLE_VALUE, "Original-lock setup retry failed.");
        const bool locked_path_rejected = service.ClassifyPath(redirect_path) == helen::FileWriteRoutingService::PathDisposition::Rejected;
        std::unique_ptr<helen::FileWriteRoutingTransaction> locked_transaction = service.BeginTrustedWrite({ redirect_path }, error);
        const bool locked_transaction_acquired = locked_transaction != nullptr;
        if (locked_transaction != nullptr)
        {
            locked_transaction->CancelWithoutWrite();
            locked_transaction.reset();
        }
        const DWORD locked_transaction_error = error;
        Expect(CloseHandle(locked_original_again) != FALSE, "Original-lock close failed.");
        Expect(locked_path_rejected, "Locked protected path was not explicitly classified as rejected.");
        Expect(!locked_transaction_acquired, "Trusted write accepted an unverifiable protected path.");
        Expect(locked_transaction_error == ERROR_SHARING_VIOLATION, "Trusted write rejected a locked path with the wrong error.");

        const std::filesystem::path replacement_original = request_base / "replacement.ini";
        const std::filesystem::path replacement_source = request_base / "replacement.new";
        WriteAllBytes(replacement_original, "Q");
        WriteAllBytes(replacement_source, "R");
        const helen::FileWriteRoute replacement_route = { "replacement", replacement_original, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected };
        helen::FileWriteRoutingService replacement_service(root / "replacement-cache", request_base);
        Expect(replacement_service.Initialize({ replacement_route }, error), "ReplaceFile setup initialization failed.");
        const std::unique_ptr<helen::FileWriteRoutingTransaction> replacement_transaction = replacement_service.BeginTrustedWrite({ replacement_original }, error);
        Expect(replacement_transaction != nullptr, "ReplaceFile transaction acquisition failed.");
        Expect(ReplaceFileW(replacement_original.c_str(), replacement_source.c_str(), nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) != FALSE, "ReplaceFileW setup failed.");
        Expect(replacement_transaction->Synchronize(error), "ReplaceFile synchronization failed.");
        Expect(ReadThroughRouter(replacement_service, replacement_original) == "R", "ReplaceFile synchronization kept stale overlay bytes.");
        Expect(ReadHandle(CreateFileW(replacement_original.c_str(), GENERIC_READ, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr)) == "R", "ReplaceFile did not update original bytes.");

        const std::filesystem::path alias_original = request_base / "alias-target.ini";
        WriteAllBytes(alias_original, "S");
        helen::FileWriteRoutingService alias_service(root / "alias-cache", request_base);
        const helen::FileWriteRoute alias_route = { "alias", alias_original, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected };
        Expect(alias_service.Initialize({ alias_route }, error), "Alias setup initialization failed.");
        std::array<wchar_t, MAX_PATH> short_path_buffer{};
        const DWORD short_path_length = GetShortPathNameW(alias_original.c_str(), short_path_buffer.data(), static_cast<DWORD>(short_path_buffer.size()));
        if (short_path_length > 0 && short_path_length < short_path_buffer.size())
        {
            const std::filesystem::path short_path(short_path_buffer.data());
            Expect(alias_service.ClassifyPath(short_path) == helen::FileWriteRoutingService::PathDisposition::Protected, "Short protected alias was not recognized.");
        }

        const std::filesystem::path hard_link_path = request_base / "alias-hardlink.ini";
        if (CreateHardLinkW(hard_link_path.c_str(), alias_original.c_str(), nullptr) != FALSE)
        {
            Expect(alias_service.ClassifyPath(hard_link_path) == helen::FileWriteRoutingService::PathDisposition::Rejected, "Hard-link protected alias was not rejected.");
            DeleteFileW(hard_link_path.c_str());
        }

        const std::filesystem::path reparse_path = request_base / "alias-reparse.ini";
        if (CreateSymbolicLinkW(reparse_path.c_str(), alias_original.c_str(), 0) != FALSE)
        {
            Expect(alias_service.ClassifyPath(reparse_path) == helen::FileWriteRoutingService::PathDisposition::Rejected, "Reparse protected alias was not rejected.");
            DeleteFileW(reparse_path.c_str());
        }

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

        const std::filesystem::path mutation_original = request_base / "mutation.ini";
        const std::filesystem::path mutation_source = request_base / "mutation.tmp";
        WriteAllBytes(mutation_original, "M");
        WriteAllBytes(mutation_source, "N");
        helen::FileWriteRoutingService mutation_service(root / "mutation-cache", request_base);
        const helen::FileWriteRoute mutation_route = { "mutation", mutation_original, helen::FileWritePolicy::Redirect, helen::FileReadPolicy::Redirected };
        Expect(mutation_service.Initialize({ mutation_route }, error), "Mutation routing initialization failed.");
        Expect(mutation_service.Copy(mutation_source, mutation_original, FALSE) != FALSE, "Protected copy destination was not redirected.");
        Expect(ReadThroughRouter(mutation_service, mutation_original) == "N", "Redirected copy did not update the session file.");
        WriteAllBytes(mutation_source, "O");
        Expect(mutation_service.Move(mutation_source, mutation_original, MOVEFILE_REPLACE_EXISTING) != FALSE, "Protected move destination was not redirected.");
        Expect(ReadThroughRouter(mutation_service, mutation_original) == "O", "Redirected move did not update the session file.");
        WriteAllBytes(mutation_source, "P");
        Expect(mutation_service.Replace(mutation_original, mutation_source, nullptr, REPLACEFILE_IGNORE_MERGE_ERRORS, nullptr, nullptr) != FALSE,
            "Protected replace destination was not redirected.");
        Expect(ReadThroughRouter(mutation_service, mutation_original) == "P", "Redirected replace did not update the session file.");
        Expect(mutation_service.SetAttributes(mutation_original, FILE_ATTRIBUTE_HIDDEN) != FALSE, "Protected attribute mutation was not redirected.");
        Expect((mutation_service.GetAttributes(mutation_original) & FILE_ATTRIBUTE_HIDDEN) != 0, "Redirected attributes were not visible.");
        Expect(mutation_service.Delete(mutation_original) != FALSE, "Protected delete did not remove the session file.");
        const HANDLE deleted_route = mutation_service.Open(mutation_original, GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        Expect(deleted_route == INVALID_HANDLE_VALUE, "Deleted overlay fell back to the original file.");

        WriteAllBytes(mutation_original, "M");
        helen::FileWriteRoutingService deny_mutation_service(root / "deny-mutation-cache", request_base);
        const helen::FileWriteRoute deny_mutation_route = { "deny-mutation", mutation_original, helen::FileWritePolicy::Deny, helen::FileReadPolicy::Original };
        Expect(deny_mutation_service.Initialize({ deny_mutation_route }, error), "Deny mutation initialization failed.");
        Expect(deny_mutation_service.Delete(mutation_original) == FALSE && GetLastError() == ERROR_ACCESS_DENIED,
            "Deny mutation unexpectedly deleted a protected original.");
        Expect(deny_mutation_service.Copy(mutation_source, mutation_original, FALSE) == FALSE && GetLastError() == ERROR_ACCESS_DENIED,
            "Deny mutation unexpectedly accepted a protected copy destination.");
    }
    catch (...)
    {
        std::filesystem::remove_all(root);
        throw;
    }

    std::filesystem::remove_all(root);
}
