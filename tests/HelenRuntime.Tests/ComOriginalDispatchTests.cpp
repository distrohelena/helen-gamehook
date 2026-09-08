#include <HelenHook/ComOriginalDispatch.h>
#include <stdexcept>

namespace {
    /** Reports a dispatch contract violation through the console runner. */
    void Expect(bool condition, const char* message) {
        if (!condition) { throw std::runtime_error(message); }
    }
}

/** Exercises shared-table forwarding without requiring tracked objects or a GPU. */
void RunComOriginalDispatchTests() {
    helen::ComOriginalDispatch dispatch;
    int first = 1, second = 2, detour = 3, refreshed = 4;
    void* table[] = { &first, &second };
    void** object = table;
    void** otherObject = table;
    dispatch.Capture(table, 2);
    table[1] = &detour;
    Expect(dispatch.Resolve(&object, 1) == &second, "Dispatch returned our detour instead of the saved original.");
    Expect(dispatch.Resolve(&otherObject, 1) == &second, "Untracked shared object lost original dispatch.");
    Expect(dispatch.Capture(table, 2)[1] == &second, "Repeated capture saved a detour as the original.");
    dispatch.Replace(table, { &first, &refreshed });
    Expect(dispatch.Resolve(&object, 1) == &refreshed, "Reset did not publish refreshed original.");
    void* separate[] = { &detour };
    void** unpatched = separate;
    Expect(dispatch.Resolve(&unpatched, 0) == &detour, "Unpatched table must forward its live method.");
    bool rejected = false;
    try { dispatch.Resolve(&object, 2); } catch (const std::exception&) { rejected = true; }
    Expect(rejected, "Captured interface must reject out-of-range dispatch.");
    rejected = false;
    try { dispatch.Replace(table, { &first }); } catch (const std::exception&) { rejected = true; }
    Expect(rejected, "Refresh must reject a different interface size.");
}
