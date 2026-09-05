#include "BatmanGraphicsExternalInterface.h"
#include "BatmanGraphicsPrimitiveCodec.h"
#include <HelenHook/Log.h>

#include <array>
#include <cstring>
#include <limits>

namespace {
    /** @brief Exact allowlist; version and spelling are part of the frontend/native contract. */
    constexpr std::array<const char*, 12> Names{
        "Helen_Graphics_OpenV1", "Helen_Graphics_GetV1", "Helen_Graphics_ModeCountV1",
        "Helen_Graphics_ModeWidthV1", "Helen_Graphics_ModeHeightV1", "Helen_Graphics_EndReadV1",
        "Helen_Graphics_BeginApplyV1", "Helen_Graphics_SetFieldV1", "Helen_Graphics_SetResolutionV1",
        "Helen_Graphics_CommitV1", "Helen_Graphics_CancelApplyV1", "Helen_Graphics_CloseV1"
    };
    /** @brief Required argument count for each exact allowlisted name, including explicit zero-argument Open. */
    constexpr std::array<unsigned, 12> Arity{0, 2, 2, 3, 3, 1, 1, 4, 4, 2, 2, 1};
}

namespace helen {
    BatmanGraphicsExternalInterface::BatmanGraphicsExternalInterface(BatmanGraphicsSessionService& sessions) : Sessions(sessions) {
    }

    bool BatmanGraphicsExternalInterface::Owns(const char* name) {
        if (name == nullptr) {
            return false;
        }
        for (const char* candidate : Names) {
            if (std::strcmp(name, candidate) == 0) {
                return true;
            }
        }
        return false;
    }

    bool BatmanGraphicsExternalInterface::TryHandle(const char* name, const void* arguments, unsigned count,
        BatmanGraphicsPrimitiveResult& result) {
        if (name == nullptr) {
            return false;
        }
        std::size_t operation = 0;
        while (operation < Names.size() && std::strcmp(name, Names[operation]) != 0) {
            ++operation;
        }
        if (operation == Names.size()) {
            return false;
        }
        if (count != Arity[operation]) {
            Logf(L"[graphics-direct] Rejected %hs: argc=%u expected=%u.", name, count, Arity[operation]);
            return true;
        }
        std::array<std::uint32_t, 4> values{};
        for (unsigned index = 0; index < count; ++index) {
            const std::optional<std::uint32_t> value = BatmanGraphicsPrimitiveCodec::ReadUnsigned(arguments, count, index);
            if (!value.has_value()) {
                Logf(L"[graphics-direct] Rejected %hs: argument %u is not a finite unsigned integer.", name, index);
                return true;
            }
            values[index] = *value;
        }
        try {
            std::optional<std::uint32_t> numeric;
            std::optional<int> scalar;
            bool is_boolean = false;
            bool succeeded = false;
            switch (operation) {
            case 0:
                numeric = Sessions.Open();
                break;
            case 1:
                scalar = Sessions.Get(values[0], static_cast<BatmanGraphicsField>(values[1]));
                break;
            case 2:
                scalar = Sessions.ModeCount(values[0], static_cast<BatmanDisplayModeCatalogKind>(values[1]));
                break;
            case 3:
                scalar = Sessions.ModeWidth(values[0], static_cast<BatmanDisplayModeCatalogKind>(values[1]), values[2]);
                break;
            case 4:
                scalar = Sessions.ModeHeight(values[0], static_cast<BatmanDisplayModeCatalogKind>(values[1]), values[2]);
                break;
            case 5:
                is_boolean = true;
                succeeded = Sessions.EndRead(values[0]);
                break;
            case 6:
                numeric = Sessions.BeginApply(values[0]);
                break;
            case 7:
                is_boolean = true;
                succeeded = values[3] <= static_cast<std::uint32_t>(std::numeric_limits<int>::max()) &&
                    Sessions.SetField(values[0], values[1], static_cast<BatmanGraphicsField>(values[2]), static_cast<int>(values[3]));
                break;
            case 8:
                is_boolean = true;
                succeeded = Sessions.SetResolution(values[0], values[1], static_cast<BatmanDisplayModeCatalogKind>(values[2]), values[3]);
                break;
            case 9: {
                const std::optional<BatmanGraphicsApplyResult> applied = Sessions.Commit(values[0], values[1]);
                if (applied.has_value()) {
                    numeric = static_cast<std::uint32_t>(applied->Outcome);
                }
                break;
            }
            case 10:
                is_boolean = true;
                succeeded = Sessions.CancelApply(values[0], values[1]);
                break;
            case 11:
                is_boolean = true;
                succeeded = Sessions.Close(values[0]);
                break;
            }
            if (scalar.has_value() && *scalar >= 0) {
                numeric = static_cast<std::uint32_t>(*scalar);
            }
            if (is_boolean && succeeded) {
                result.Payload[0] = 1;
                result.Type = 2;
            } else if (numeric.has_value()) {
                BatmanGraphicsPrimitiveCodec::WriteNumber(result, *numeric);
            }
            if (result.Type == 0) {
                Logf(L"[graphics-direct] %hs rejected or value unavailable.", name);
            } else if (operation == 0 || operation >= 5) {
                Logf(L"[graphics-direct] %hs completed type=%u value=%u.", name, static_cast<unsigned>(result.Type),
                    numeric.has_value() ? *numeric : static_cast<std::uint32_t>(succeeded));
            }
        } catch (const std::exception& error) {
            Logf(L"[graphics-direct] %hs failed: %hs.", name, error.what());
        } catch (...) {
            Logf(L"[graphics-direct] %hs failed with an unknown native exception.", name);
        }
        return true;
    }

    void BatmanGraphicsExternalInterface::ForwardStock(void* handler, void* movie, const char* name,
        const void* arguments, unsigned count) {
        /** @brief The original handler receives ECX plus movie, name, converted arguments and count on the stack. */
        using OriginalMethod = void (__thiscall*)(void*, void*, const char*, const void*, unsigned);
        const OriginalMethod* table = *static_cast<const OriginalMethod* const*>(handler);
        table[1](handler, movie, name, arguments, count);
    }
}
