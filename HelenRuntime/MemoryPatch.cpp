#include <HelenHook/MemoryPatch.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>

namespace helen {
[[noreturn]] void AbortUnsafePatch() noexcept {
    OutputDebugStringA("HelenHook: unresolved memory patch; stopping before ownership is discarded.\n");
    TerminateProcess(GetCurrentProcess(), ERROR_WRITE_FAULT);
    std::_Exit(ERROR_WRITE_FAULT);
}

MemoryPatch::~MemoryPatch() {
    if (HasOwnership() && !Restore().Completed) { AbortUnsafePatch(); }
}

bool MemoryPatch::HasOwnership() const noexcept { return Address != nullptr; }
const MemoryPatchResult& MemoryPatch::Result() const noexcept { return LastResult; }

MemoryPatchResult MemoryPatch::Apply(void* address, const void* data, std::size_t size) {
    if (HasOwnership()) {
        LastResult = {};
        LastResult.AccessError = ERROR_BUSY;
        return LastResult;
    }
    LastResult = {};
    const std::uintptr_t start = reinterpret_cast<std::uintptr_t>(address);
    if (address == nullptr || data == nullptr || size == 0 ||
        size > (std::numeric_limits<std::uintptr_t>::max)() - start) {
        LastResult.AccessError = ERROR_INVALID_PARAMETER;
        return LastResult;
    }
    Regions.clear();
    const std::uintptr_t end = start + size;
    for (std::uintptr_t cursor = start; cursor < end;) {
        MEMORY_BASIC_INFORMATION info{};
        if (VirtualQuery(reinterpret_cast<void*>(cursor), &info, sizeof(info)) == 0) {
            LastResult.AccessError = GetLastError();
            return LastResult;
        }
        const DWORD access = info.Protect & 0xFF;
        if (info.State != MEM_COMMIT || (info.Protect & PAGE_GUARD) != 0 ||
            (access != PAGE_READONLY && access != PAGE_READWRITE && access != PAGE_WRITECOPY &&
             access != PAGE_EXECUTE_READ && access != PAGE_EXECUTE_READWRITE && access != PAGE_EXECUTE_WRITECOPY)) {
            LastResult.AccessError = ERROR_NOACCESS;
            return LastResult;
        }
        const std::uintptr_t base = reinterpret_cast<std::uintptr_t>(info.BaseAddress);
        const std::size_t available = info.RegionSize - (cursor - base);
        const std::size_t count = (std::min)(available, end - cursor);
        Regions.push_back({reinterpret_cast<void*>(cursor), count, info.Protect, false});
        cursor += count;
    }
    OriginalBytes.resize(size);
    std::memcpy(OriginalBytes.data(), address, size);
    Address = address;
    LastResult = Write(data);
    Applied = LastResult.Completed;
    if (!LastResult.BytesWritten && LastResult.ProtectionError == ERROR_SUCCESS) { Clear(); }
    return LastResult;
}

MemoryPatchResult MemoryPatch::Write(const void* data) noexcept {
    MemoryPatchResult result;
    for (MemoryProtectionRegion& region : Regions) {
        DWORD previous = 0;
        if (!VirtualProtect(region.Address, region.Size, PAGE_EXECUTE_READWRITE, &previous)) {
            result.AccessError = GetLastError();
            RestoreProtections(result);
            return result;
        }
        region.Changed = true;
    }
    std::memmove(Address, data, OriginalBytes.size());
    result.BytesWritten = true;
    if (!FlushInstructionCache(GetCurrentProcess(), Address, OriginalBytes.size())) {
        result.FlushError = GetLastError();
    }
    RestoreProtections(result);
    result.Completed = result.FlushError == ERROR_SUCCESS && result.ProtectionError == ERROR_SUCCESS;
    return result;
}

void MemoryPatch::RestoreProtections(MemoryPatchResult& result) noexcept {
    for (MemoryProtectionRegion& region : Regions) {
        if (!region.Changed) { continue; }
        DWORD ignored = 0;
        if (VirtualProtect(region.Address, region.Size, region.Protection, &ignored)) {
            region.Changed = false;
        } else if (result.ProtectionError == ERROR_SUCCESS) {
            result.ProtectionError = GetLastError();
        }
    }
}

MemoryPatchResult MemoryPatch::Restore() noexcept {
    if (!HasOwnership()) {
        LastResult = {};
        LastResult.Completed = true;
        return LastResult;
    }
    Applied = false;
    LastResult = Write(OriginalBytes.data());
    if (LastResult.Completed) { Clear(); }
    return LastResult;
}

bool MemoryPatch::Commit() noexcept {
    if (!HasOwnership() || !Applied) { return false; }
    Clear();
    return true;
}

void MemoryPatch::Clear() noexcept {
    Address = nullptr;
    Applied = false;
    OriginalBytes.clear();
    Regions.clear();
}
}
