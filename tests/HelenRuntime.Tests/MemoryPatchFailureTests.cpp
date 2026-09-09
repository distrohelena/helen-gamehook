#include <HelenHook/Hook.h>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>

#undef VirtualProtect
#undef FlushInstructionCache
#undef VirtualFree

namespace {
/** @brief Number of upcoming cache operations refused by the Win32 boundary. */
unsigned FailFlush = 0;
/** @brief Optional target filter so trampoline writes do not consume the injected failure. */
const void* FlushTarget = nullptr;
/** @brief Target whose next write-access request is refused. */
void* RefuseWrite = nullptr;
/** @brief Target whose original-protection restoration is refused. */
void* RefuseRestore = nullptr;
/** @brief Allocation whose release is refused without actually freeing it. */
void* RefuseFree = nullptr;
/** @brief Throws an observable test failure without invoking a GUI assertion. */
void Expect(bool condition, const char* message) {
    if (!condition) { throw std::runtime_error(message); }
}

/** @brief Returns actual Windows page protection, independent of production snapshots. */
DWORD Protection(void* address) {
    MEMORY_BASIC_INFORMATION info{};
    Expect(VirtualQuery(address, &info, sizeof(info)) != 0, "query failed");
    return info.Protect;
}

/** @brief Exercises write-stage errors and recovery on real differently protected pages. */
void OwnedCases(unsigned char* bytes) {
    DWORD ignored = 0;
    Expect(VirtualProtect(bytes, 4096, PAGE_READONLY, &ignored) != FALSE, "first protection failed");
    Expect(VirtualProtect(bytes + 4096, 4096, PAGE_EXECUTE_READ, &ignored) != FALSE, "second protection failed");
    const unsigned char replacement[2] = {0x11, 0x22};
    helen::MemoryPatch patch;
    Expect(patch.Apply(bytes + 4095, replacement, 2).Completed, "cross-region apply failed");
    Expect(bytes[4095] == 0x11 && bytes[4096] == 0x22, "cross-region bytes wrong");
    Expect(Protection(bytes) == PAGE_READONLY && Protection(bytes + 4096) == PAGE_EXECUTE_READ, "mixed protections were flattened");
    Expect(patch.Restore().Completed, "cross-region restore failed");
    Expect(bytes[4095] == 0x90 && bytes[4096] == 0x90 && !patch.HasOwnership(), "cross-region originals lost");

    RefuseWrite = bytes + 4096;
    const helen::MemoryPatchResult refused = patch.Apply(bytes + 4095, replacement, 2);
    RefuseWrite = nullptr;
    Expect(!refused.Completed && !refused.BytesWritten && refused.AccessError == ERROR_ACCESS_DENIED, "later access failure not reported");
    Expect(!patch.HasOwnership() && Protection(bytes) == PAGE_READONLY, "pre-write failure left changed protection");

    RefuseWrite = bytes + 4096;
    RefuseRestore = bytes + 4095;
    const helen::MemoryPatchResult accessCleanup = patch.Apply(bytes + 4095, replacement, 2);
    RefuseWrite = nullptr;
    RefuseRestore = nullptr;
    Expect(!accessCleanup.BytesWritten && accessCleanup.AccessError == ERROR_ACCESS_DENIED && accessCleanup.ProtectionError == ERROR_ACCESS_DENIED, "combined pre-write errors lost");
    Expect(patch.HasOwnership() && !patch.Commit(), "protection-only failure lost ownership or permitted commit");
    Expect(patch.Restore().Completed && Protection(bytes) == PAGE_READONLY, "protection-only recovery failed");

    FlushTarget = bytes;
    FailFlush = 1;
    RefuseRestore = bytes;
    const helen::MemoryPatchResult combined = patch.Apply(bytes, replacement, 2);
    RefuseRestore = nullptr;
    Expect(!combined.Completed && combined.BytesWritten && combined.FlushError == ERROR_WRITE_FAULT && combined.ProtectionError == ERROR_ACCESS_DENIED, "post-write errors not independently reported");
    Expect(patch.HasOwnership() && bytes[0] == 0x11 && !patch.Commit(), "failed apply was silently rolled back or committed");
    RefuseWrite = bytes;
    const helen::MemoryPatchResult retry = patch.Restore();
    RefuseWrite = nullptr;
    Expect(!retry.Completed && patch.HasOwnership(), "failed retry discarded owner");
    Expect(patch.Restore().Completed && bytes[0] == 0x90 && Protection(bytes) == PAGE_READONLY, "retry lost original protection or bytes");

    Expect(patch.Apply(bytes, replacement, 2).Completed, "healthy reapply failed");
    Expect(!patch.Apply(bytes + 1, replacement, 1).Completed && patch.HasOwnership(), "busy patch accepted another target");
    Expect(patch.Result().AccessError == ERROR_BUSY, "latest result hid rejected apply");
    FailFlush = 1;
    const helen::MemoryPatchResult restoreFlush = patch.Restore();
    Expect(!restoreFlush.Completed && restoreFlush.BytesWritten && patch.HasOwnership() && bytes[0] == 0x90, "restore cache failure relinquished ownership");
    Expect(patch.Restore().Completed, "restore cache retry failed");
    {
        helen::MemoryPatch scoped;
        Expect(scoped.Apply(bytes, replacement, 2).Completed, "scoped apply failed");
    }
    Expect(bytes[0] == 0x90 && Protection(bytes) == PAGE_READONLY, "healthy destructor did not restore");
    RefuseWrite = bytes;
    const bool legacyRefused = helen::WriteMemory(bytes, replacement, 2);
    RefuseWrite = nullptr;
    Expect(!legacyRefused && bytes[0] == 0x90, "legacy pre-write refusal changed target");
    FailFlush = 1;
    Expect(!helen::FillMemoryBytes(bytes, 0x55, 2) && bytes[0] == 0x90 && bytes[1] == 0x90, "failed fill did not roll back");
    Expect(patch.Apply(bytes, replacement, 2).Completed && patch.Commit(), "permanent commit failed");
    Expect(!patch.HasOwnership() && patch.Restore().Completed && bytes[0] == 0x11, "commit was undone");
    Expect(!patch.Apply(nullptr, replacement, 2).Completed && !patch.HasOwnership(), "null destination accepted");
    Expect(patch.Restore().Completed && patch.Result().Completed, "latest result hid empty cleanup");
    Expect(!patch.Apply(bytes, replacement, 0).Completed && !patch.HasOwnership(), "empty write accepted");
    Expect(!patch.Apply(reinterpret_cast<void*>(0xFFFFFFFEu), replacement, 4).Completed, "overflow accepted");
    Expect(VirtualProtect(bytes + 4096, 4096, PAGE_NOACCESS, &ignored) != FALSE, "noaccess setup failed");
    Expect(!patch.Apply(bytes + 4096, replacement, 2).Completed && !patch.HasOwnership(), "inaccessible target accepted");
    FlushTarget = nullptr;
}

/** @brief Exercises target-specific partial installation and retained executable allocation. */
void InlineCases(unsigned char* bytes) {
    helen::InlineHook hook;
    FlushTarget = bytes;
    FailFlush = 1;
    Expect(!hook.TryInstall(bytes, bytes + 32, 5), "partial target install reported success");
    Expect(hook.IsInstalled() && bytes[0] == 0xE9 && hook.Original<void*>() != nullptr, "partial install lost target/trampoline ownership");
    RefuseWrite = bytes;
    const bool refused = hook.TryRemove();
    RefuseWrite = nullptr;
    Expect(!refused && hook.IsInstalled(), "partial install cleanup discarded owner");
    MEMORY_BASIC_INFORMATION allocation{};
    Expect(VirtualQuery(hook.Original<void*>(), &allocation, sizeof(allocation)) != 0 && allocation.State == MEM_COMMIT, "failed restoration freed the trampoline");
    hook.Original<void(__cdecl*)()>()();
    Expect(hook.TryRemove() && !hook.IsInstalled() && bytes[0] == 0x90, "partial installation recovery failed");
    Expect(hook.Install(bytes, bytes + 32, 5), "healthy hook failed");
    Expect(!hook.Install(bytes, bytes + 32, 5) && hook.IsInstalled() && bytes[0] == 0xE9, "duplicate install removed existing hook");
    hook.Original<void(__cdecl*)()>()();
    reinterpret_cast<void(__cdecl*)()>(bytes)();
    RefuseFree = hook.Original<void*>();
    const bool freeRefused = hook.TryRemove();
    RefuseFree = nullptr;
    Expect(!freeRefused && hook.IsInstalled() && bytes[0] == 0x90, "failed release discarded allocation");
    Expect(hook.TryRemove() && !hook.IsInstalled(), "allocation release retry failed");
    FailFlush = 1;
    Expect(!hook.Install(bytes, bytes + 32, 5) && !hook.IsInstalled() && bytes[0] == 0x90, "legacy install returned without rollback");
    FlushTarget = nullptr;
}

/** @brief Replacement function used to prove actual PE import slot mutation. */
DWORD WINAPI ReplacementThreadId() { return 123456789; }

/** @brief Uses this executable's real import slot for ownership and callable recovery tests. */
void IatCases() {
    const DWORD originalId = GetCurrentThreadId();
    const auto module = helen::QueryMainModule();
    Expect(module.has_value(), "main module missing");
    void** slot = helen::FindImportAddress(*module, "KERNEL32.dll", "GetCurrentThreadId");
    Expect(slot != nullptr, "fixture import missing");
    void* original = *slot;
    FlushTarget = slot;
    FailFlush = 1;
    helen::IatHook hook;
    Expect(!hook.TryInstall(*module, "KERNEL32.dll", "GetCurrentThreadId", reinterpret_cast<void*>(&ReplacementThreadId)), "partial IAT install reported success");
    Expect(hook.IsInstalled() && hook.Original<void*>() == original && *slot == reinterpret_cast<void*>(&ReplacementThreadId), "partial IAT owner lost");
    RefuseWrite = slot;
    const bool refused = hook.TryRemove();
    RefuseWrite = nullptr;
    Expect(!refused && hook.IsInstalled() && hook.Original<void*>() == original, "failed IAT restore discarded original");
    Expect(hook.TryRemove() && !hook.IsInstalled() && *slot == original, "IAT retry failed");
    Expect(hook.Install(*module, "KERNEL32.dll", "GetCurrentThreadId", reinterpret_cast<void*>(&ReplacementThreadId)), "healthy IAT install failed");
    Expect(GetCurrentThreadId() == 123456789, "IAT replacement not callable");
    hook.Remove();
    Expect(GetCurrentThreadId() == originalId, "IAT original not restored");
    FlushTarget = nullptr;
}

/** @brief Runs an owned console child and requires the production no-dialog stop code. */
void FatalCase(const char* executable, const char* scenario) {
    std::string command = std::string("\"") + executable + "\" " + scenario;
    STARTUPINFOA startup{};
    startup.cb = sizeof(startup);
    startup.dwFlags = STARTF_USESHOWWINDOW;
    startup.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION process{};
    Expect(CreateProcessA(executable, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &startup, &process) != FALSE, "fatal child could not start");
    const DWORD wait = WaitForSingleObject(process.hProcess, 10000);
    DWORD exitCode = 0;
    const BOOL readExit = GetExitCodeProcess(process.hProcess, &exitCode);
    if (wait != WAIT_OBJECT_0) { TerminateProcess(process.hProcess, ERROR_TIMEOUT); }
    CloseHandle(process.hThread);
    CloseHandle(process.hProcess);
    Expect(wait == WAIT_OBJECT_0 && readExit != FALSE && exitCode == ERROR_WRITE_FAULT, "unsafe child did not stop with patch failure code");
    std::printf("PATCH_FATAL_CHILD_PASS: %s\n", scenario);
}
}

BOOL WINAPI PatchTestVirtualProtect(LPVOID address, SIZE_T size, DWORD protection, PDWORD previous) {
    if (address == RefuseWrite && protection == PAGE_EXECUTE_READWRITE) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    if (address == RefuseRestore && protection != PAGE_EXECUTE_READWRITE) {
        SetLastError(ERROR_ACCESS_DENIED);
        return FALSE;
    }
    return VirtualProtect(address, size, protection, previous);
}

BOOL WINAPI PatchTestFlushInstructionCache(HANDLE process, LPCVOID address, SIZE_T size) {
    if (FailFlush != 0 && (FlushTarget == nullptr || FlushTarget == address)) {
        --FailFlush;
        SetLastError(ERROR_WRITE_FAULT);
        return FALSE;
    }
    return FlushInstructionCache(process, address, size);
}

BOOL WINAPI PatchTestVirtualFree(LPVOID address, SIZE_T size, DWORD type) {
    if (address == RefuseFree) { SetLastError(ERROR_ACCESS_DENIED); return FALSE; }
    return VirtualFree(address, size, type);
}

/** @brief Runs real-memory regression cases selected by the console runner. */
int main(int argc, char** argv) {
    std::puts("MEMORY_PATCH_FIXTURE_ENTERED");
    std::fflush(stdout);
    SetErrorMode(0x8003);
    try {
        auto* bytes = static_cast<unsigned char*>(VirtualAlloc(nullptr, 8192, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
        Expect(bytes != nullptr, "allocation failed");
        std::memset(bytes, 0x90, 8192);
        bytes[5] = 0xC3;
        bytes[32] = 0xC3;
        if (argc > 1 && std::strcmp(argv[1], "fatal-checked") == 0) {
            FatalCase(argv[0], "fatal-write");
            FatalCase(argv[0], "fatal-remove");
            FatalCase(argv[0], "fatal-destructor");
            FatalCase(argv[0], "fatal-install");
            FatalCase(argv[0], "fatal-iat");
        } else if (argc > 1 && std::strcmp(argv[1], "owned") == 0) {
            OwnedCases(bytes);
        } else if (argc > 1 && std::strcmp(argv[1], "inline") == 0) {
            InlineCases(bytes);
        } else if (argc > 1 && std::strcmp(argv[1], "iat") == 0) {
            IatCases();
        } else if (argc > 1 && std::strcmp(argv[1], "fatal-write") == 0) {
            FailFlush = 2;
            const unsigned char replacement = 0xCC;
            helen::WriteMemory(bytes, &replacement, 1);
            throw std::runtime_error("unsafe legacy write returned");
        } else if (argc > 1 && std::strcmp(argv[1], "fatal-remove") == 0) {
            helen::InlineHook hook;
            Expect(hook.Install(bytes, bytes + 32, 5), "fatal fixture install failed");
            RefuseWrite = bytes;
            hook.Remove();
            throw std::runtime_error("unsafe legacy removal returned");
        } else if (argc > 1 && std::strcmp(argv[1], "fatal-destructor") == 0) {
            {
                helen::MemoryPatch patch;
                const unsigned char replacement = 0xCC;
                Expect(patch.Apply(bytes, &replacement, 1).Completed, "fatal owned setup failed");
                RefuseWrite = bytes;
            }
            throw std::runtime_error("unsafe destructor returned");
        } else if (argc > 1 && std::strcmp(argv[1], "fatal-install") == 0) {
            helen::InlineHook hook;
            FlushTarget = bytes;
            FailFlush = 2;
            hook.Install(bytes, bytes + 32, 5);
            throw std::runtime_error("unsafe failed install returned");
        } else if (argc > 1 && std::strcmp(argv[1], "fatal-iat") == 0) {
            const auto module = helen::QueryMainModule();
            Expect(module.has_value(), "fatal module missing");
            helen::IatHook hook;
            Expect(hook.Install(*module, "KERNEL32.dll", "GetCurrentThreadId", reinterpret_cast<void*>(&ReplacementThreadId)), "fatal IAT setup failed");
            RefuseWrite = helen::FindImportAddress(*module, "KERNEL32.dll", "GetCurrentThreadId");
            hook.Remove();
            throw std::runtime_error("unsafe IAT removal returned");
        } else if (argc > 1 && std::strcmp(argv[1], "remove") == 0) {
            helen::InlineHook hook;
            Expect(hook.Install(bytes, bytes + 32, 5), "healthy install failed");
            RefuseWrite = bytes;
            Expect(!hook.TryRemove(), "refused removal reported success");
            Expect(hook.IsInstalled(), "failed removal discarded ownership");
            RefuseWrite = nullptr;
            hook.Remove();
        } else {
            const unsigned char replacement = 0xCC;
            FailFlush = 1;
            Expect(!helen::WriteMemory(bytes, &replacement, 1), "flush failure reported success");
            Expect(bytes[0] == 0x90, "failed legacy write did not restore original bytes");
        }
        VirtualFree(bytes, 0, MEM_RELEASE);
        std::puts("MEMORY_PATCH_FAILURE_PASS");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
