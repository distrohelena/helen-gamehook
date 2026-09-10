#include <HelenHook/BatmanGraphicsConfigService.h>
#include <HelenHook/BatmanDisplayModeService.h>
#include <HelenHook/BatmanGraphicsFileOperations.h>
#include <HelenHook/FileWriteRoutingService.h>
#include <HelenHook/FileWriteRoutingTransaction.h>

#include <HelenHook/Log.h>
#include <HelenHook/CommandDispatcher.h>

#include <windows.h>

#include <array>
#include <atomic>
#include <charconv>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <mutex>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
    /**
     * @brief Serializes every Batman graphics INI transaction in this process from snapshot through cleanup.
     *
     * The service can be instantiated more than once, so the lock is process-wide rather than a member lock.
     * Holding it across preparation, publication, reconciliation, and cleanup prevents concurrent applies from
     * interleaving snapshots or touching one another's sibling transaction files.
     */
    std::mutex BatmanGraphicsApplyMutex;

    /** @brief Sticky process-wide lockout after unverified publication; no session recreation can clear it. */
    std::atomic<bool> BatmanGraphicsIntegrityUncertain{false};

    /**
     * @brief Supplies a monotonic per-process identity component for sibling transaction paths.
     *
     * The sequence remains collision-safe even when several callers request paths during the same tick count.
     */
    std::atomic<std::uint64_t> BatmanGraphicsTransactionSequence{ 0 };

    /**
     * @brief Identifies the on-disk text encoding that must be preserved for one Batman INI document.
     */
    enum class IniTextEncoding
    {
        /** @brief Single-byte text used by Batman's generated `BmEngine.ini`. */
        SingleByte,
        /** @brief BOM-prefixed UTF-16 little-endian text used by the retail launcher `UserEngine.ini`. */
        Utf16LittleEndian
    };

    /**
     * @brief Stores decoded INI lines together with the original on-disk encoding required for writes.
     */
    struct IniTextDocument
    {
        /** @brief Decoded UTF-8 lines without trailing carriage-return characters. */
        std::vector<std::string> Lines;
        /** @brief Original file encoding that must be retained when the document is persisted. */
        IniTextEncoding Encoding{ IniTextEncoding::SingleByte };
        /** @brief Exact original bytes retained so failed dual-file publication can stage this document for reconciliation. */
        std::string RawBytes;
    };

    /**
     * @brief Stores the normalized Batman graphics draft values used by the ActionScript graphics menu.
     */
    struct LegacyBatmanGraphicsDraftState
    {
        /** @brief Normalized fullscreen state where `0` means windowed and `1` means fullscreen. */
        int Fullscreen{};
        /** @brief Horizontal resolution currently exposed to the graphics menu. */
        int ResolutionWidth{};
        /** @brief Vertical resolution currently exposed to the graphics menu. */
        int ResolutionHeight{};
        /** @brief Normalized VSync state where `0` means disabled and `1` means enabled. */
        int Vsync{};
        /** @brief Normalized MSAA menu state where `0` is disabled and higher values map to larger sample counts. */
        int Msaa{};
        /** @brief Normalized Batman detail preset state where `0-3` are presets and `4` is custom. */
        int DetailLevel{};
        /** @brief Normalized Bloom state where `0` means disabled and `1` means enabled. */
        int Bloom{};
        /** @brief Normalized Dynamic Shadows state where `0` means disabled and `1` means enabled. */
        int DynamicShadows{};
        /** @brief Normalized Motion Blur state where `0` means disabled and `1` means enabled. */
        int MotionBlur{};
        /** @brief Normalized Distortion state where `0` means disabled and `1` means enabled. */
        int Distortion{};
        /** @brief Normalized Fog Volumes state where `0` means disabled and `1` means enabled. */
        int FogVolumes{};
        /** @brief Normalized spherical-harmonic-lighting state where `0` means disabled and `1` means enabled. */
        int SphericalHarmonicLighting{};
        /** @brief Normalized ambient-occlusion state where `0` means disabled and `1` means enabled. */
        int AmbientOcclusion{};
        /** @brief Normalized PhysX quality state where `0` is off, `1` is normal, and `2` is high. */
        int Physx{};
        /** @brief Normalized stereo-rendering state where `0` means disabled and `1` means enabled. */
        int Stereo{};
    };

    /**
     * @brief Describes one Batman graphics detail preset and the encoded UE3 detail-mode it should persist.
     */
    struct BatmanGraphicsPresetDefinition
    {
        /** @brief Menu-visible Batman detail-level state for this preset. */
        int DetailLevel{};
        /** @brief Raw `DetailMode` value that UE3 expects for this preset. */
        int DetailMode{};
        /** @brief Bloom toggle encoded by this preset. */
        int Bloom{};
        /** @brief Dynamic-shadows toggle encoded by this preset. */
        int DynamicShadows{};
        /** @brief Motion-blur toggle encoded by this preset. */
        int MotionBlur{};
        /** @brief Distortion toggle encoded by this preset. */
        int Distortion{};
        /** @brief Fog-volumes toggle encoded by this preset. */
        int FogVolumes{};
        /** @brief Spherical-harmonic-lighting toggle encoded by this preset. */
        int SphericalHarmonicLighting{};
        /** @brief Ambient-occlusion toggle encoded by this preset. */
        int AmbientOcclusion{};
    };

    /** @brief Canonical Batman graphics presets derived from the retail launcher detail settings. */
    constexpr std::array<BatmanGraphicsPresetDefinition, 4> BatmanGraphicsPresets = {
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 0,
            .DetailMode = 0,
            .Bloom = 0,
            .DynamicShadows = 0,
            .MotionBlur = 0,
            .Distortion = 0,
            .FogVolumes = 0,
            .SphericalHarmonicLighting = 0,
            .AmbientOcclusion = 0
        },
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 1,
            .DetailMode = 1,
            .Bloom = 1,
            .DynamicShadows = 1,
            .MotionBlur = 0,
            .Distortion = 0,
            .FogVolumes = 0,
            .SphericalHarmonicLighting = 0,
            .AmbientOcclusion = 0
        },
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 2,
            .DetailMode = 1,
            .Bloom = 1,
            .DynamicShadows = 1,
            .MotionBlur = 1,
            .Distortion = 1,
            .FogVolumes = 1,
            .SphericalHarmonicLighting = 1,
            .AmbientOcclusion = 0
        },
        BatmanGraphicsPresetDefinition{
            .DetailLevel = 3,
            .DetailMode = 2,
            .Bloom = 1,
            .DynamicShadows = 1,
            .MotionBlur = 1,
            .Distortion = 1,
            .FogVolumes = 1,
            .SphericalHarmonicLighting = 1,
            .AmbientOcclusion = 1
        }
    };

    /**
     * @brief Removes ASCII leading and trailing whitespace from one string view.
     * @param text Text view that should be trimmed.
     * @return Trimmed text as an owning string.
     */
    std::string TrimAscii(std::string_view text)
    {
        std::size_t start = 0;
        while (start < text.size())
        {
            const char character = text[start];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
            {
                break;
            }

            ++start;
        }

        std::size_t end = text.size();
        while (end > start)
        {
            const char character = text[end - 1];
            if (character != ' ' && character != '\t' && character != '\r' && character != '\n')
            {
                break;
            }

            --end;
        }

        return std::string(text.substr(start, end - start));
    }

    /**
     * @brief Returns true when two ASCII strings match ignoring character case.
     * @param left First ASCII string that should be compared.
     * @param right Second ASCII string that should be compared.
     * @return True when the strings match ignoring ASCII case; otherwise false.
     */
    bool EqualsIgnoreCaseAscii(std::string_view left, std::string_view right)
    {
        if (left.size() != right.size())
        {
            return false;
        }

        for (std::size_t index = 0; index < left.size(); ++index)
        {
            char left_character = left[index];
            if (left_character >= 'A' && left_character <= 'Z')
            {
                left_character = static_cast<char>(left_character - 'A' + 'a');
            }

            char right_character = right[index];
            if (right_character >= 'A' && right_character <= 'Z')
            {
                right_character = static_cast<char>(right_character - 'A' + 'a');
            }

            if (left_character != right_character)
            {
                return false;
            }
        }

        return true;
    }

    /**
     * @brief Splits one INI assignment line into key and value components.
     * @param line Raw INI line that may contain one `key=value` assignment.
     * @param key Receives the trimmed assignment key on success.
     * @param value Receives the trimmed assignment value on success.
     * @return True when the line contains one assignment; otherwise false.
     */
    bool TrySplitIniAssignment(std::string_view line, std::string& key, std::string& value)
    {
        const std::size_t separator_index = line.find('=');
        if (separator_index == std::string_view::npos)
        {
            return false;
        }

        key = TrimAscii(line.substr(0, separator_index));
        value = TrimAscii(line.substr(separator_index + 1));
        return !key.empty();
    }

    /**
     * @brief Returns true when one INI line declares the supplied section.
     * @param line Raw INI line that may contain one section declaration.
     * @param section_name Exact section name that should be matched.
     * @return True when the line declares the supplied section; otherwise false.
     */
    bool IsIniSectionDeclaration(std::string_view line, std::string_view section_name)
    {
        const std::string trimmed = TrimAscii(line);
        if (trimmed.size() < 3 || trimmed.front() != '[' || trimmed.back() != ']')
        {
            return false;
        }

        return trimmed.substr(1, trimmed.size() - 2) == section_name;
    }

    /**
     * @brief Reads exact bytes from one file while permitting concurrent readers but not requiring write or delete sharing.
     * @param path File path whose bytes should be loaded.
     * @return Exact file bytes when the file can be read completely; otherwise no value.
     */
    std::optional<std::string> TryReadFileBytes(const std::filesystem::path& path)
    {
        const HANDLE handle = CreateFileW(
            path.c_str(),
            GENERIC_READ,
            FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
            nullptr,
            OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        LARGE_INTEGER file_size{};
        if (GetFileSizeEx(handle, &file_size) == FALSE ||
            file_size.QuadPart < 0 ||
            static_cast<unsigned long long>(file_size.QuadPart) > std::numeric_limits<std::size_t>::max())
        {
            CloseHandle(handle);
            return std::nullopt;
        }

        std::string bytes(static_cast<std::size_t>(file_size.QuadPart), '\0');
        std::size_t offset = 0;
        bool read_succeeded = true;
        while (offset < bytes.size())
        {
            const std::size_t bytes_remaining = bytes.size() - offset;
            const DWORD request_size = bytes_remaining > static_cast<std::size_t>(MAXDWORD)
                ? MAXDWORD
                : static_cast<DWORD>(bytes_remaining);
            DWORD bytes_read = 0;
            if (ReadFile(handle, bytes.data() + offset, request_size, &bytes_read, nullptr) == FALSE || bytes_read == 0)
            {
                read_succeeded = false;
                break;
            }

            offset += static_cast<std::size_t>(bytes_read);
        }

        const BOOL close_succeeded = CloseHandle(handle);
        if (!read_succeeded || close_succeeded == FALSE)
        {
            return std::nullopt;
        }

        return bytes;
    }

    /**
     * @brief Splits decoded UTF-8 INI text into normalized lines before line-oriented helpers consume it.
     * @param text Decoded text whose CRLF or LF delimiters should be consumed.
     * @return Lines without trailing carriage-return characters.
     */
    std::vector<std::string> SplitIniTextIntoLines(const std::string& text);

    /**
     * @brief Reads every line from one text file while normalizing trailing carriage returns away.
     * @param path Text file path that should be loaded.
     * @return File lines without trailing carriage returns when the file opens successfully; otherwise no value.
     */
    std::optional<std::vector<std::string>> TryReadAllLines(const std::filesystem::path& path)
    {
        const std::optional<std::string> file_bytes = TryReadFileBytes(path);
        if (!file_bytes.has_value())
        {
            return std::nullopt;
        }

        return SplitIniTextIntoLines(*file_bytes);
    }

    /**
     * @brief Writes text lines back to disk using CRLF line endings.
     * @param path File path that should receive the supplied lines.
     * @param lines Normalized text lines that should be written to the file.
     * @return True when the file is written successfully; otherwise false.
     */
    bool WriteAllLines(const std::filesystem::path& path, const std::vector<std::string>& lines)
    {
        std::ofstream stream(path, std::ios::binary | std::ios::trunc);
        if (!stream)
        {
            return false;
        }

        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            stream << lines[index];
            if (index + 1 < lines.size())
            {
                stream << "\r\n";
            }
        }

        return static_cast<bool>(stream);
    }

    /**
     * @brief Splits decoded UTF-8 INI text into normalized lines.
     * @param text Decoded text whose CRLF or LF delimiters should be consumed.
     * @return Lines without trailing carriage-return characters.
     */
    std::vector<std::string> SplitIniTextIntoLines(const std::string& text)
    {
        std::istringstream stream(text);
        std::vector<std::string> lines;
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r')
            {
                line.pop_back();
            }

            lines.push_back(line);
        }

        return lines;
    }

    /**
     * @brief Converts one UTF-16 string into UTF-8 without replacing malformed input.
     * @param text UTF-16 text that should be converted.
     * @return UTF-8 text when conversion succeeds; otherwise no value.
     */
    std::optional<std::string> TryConvertUtf16ToUtf8(const std::wstring& text)
    {
        if (text.empty())
        {
            return std::string();
        }

        if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }

        const int text_length = static_cast<int>(text.size());
        const int required_length = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            text.data(),
            text_length,
            nullptr,
            0,
            nullptr,
            nullptr);
        if (required_length <= 0)
        {
            return std::nullopt;
        }

        std::string converted(static_cast<std::size_t>(required_length), '\0');
        const int actual_length = WideCharToMultiByte(
            CP_UTF8,
            WC_ERR_INVALID_CHARS,
            text.data(),
            text_length,
            converted.data(),
            required_length,
            nullptr,
            nullptr);
        if (actual_length != required_length)
        {
            return std::nullopt;
        }

        return converted;
    }

    /**
     * @brief Converts one UTF-8 string into UTF-16 without replacing malformed input.
     * @param text UTF-8 text that should be converted.
     * @return UTF-16 text when conversion succeeds; otherwise no value.
     */
    std::optional<std::wstring> TryConvertUtf8ToUtf16(const std::string& text)
    {
        if (text.empty())
        {
            return std::wstring();
        }

        if (text.size() > static_cast<std::size_t>(std::numeric_limits<int>::max()))
        {
            return std::nullopt;
        }

        const int text_length = static_cast<int>(text.size());
        const int required_length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            text_length,
            nullptr,
            0);
        if (required_length <= 0)
        {
            return std::nullopt;
        }

        std::wstring converted(static_cast<std::size_t>(required_length), L'\0');
        const int actual_length = MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            text_length,
            converted.data(),
            required_length);
        if (actual_length != required_length)
        {
            return std::nullopt;
        }

        return converted;
    }

    /**
     * @brief Reads one Batman INI while decoding and remembering its native text encoding.
     * @param path Existing INI path that should be loaded.
     * @return Decoded document for supported single-byte or UTF-16LE input; otherwise no value.
     */
    std::optional<IniTextDocument> TryReadIniDocument(const std::filesystem::path& path)
    {
        const std::optional<std::string> file_bytes = TryReadFileBytes(path);
        if (!file_bytes.has_value())
        {
            return std::nullopt;
        }

        IniTextDocument document;
        document.RawBytes = *file_bytes;
        std::string decoded_text;
        if (file_bytes->size() >= 2 &&
            static_cast<unsigned char>((*file_bytes)[0]) == 0xFF &&
            static_cast<unsigned char>((*file_bytes)[1]) == 0xFE)
        {
            const std::size_t encoded_length = file_bytes->size() - 2;
            if ((encoded_length % sizeof(wchar_t)) != 0)
            {
                return std::nullopt;
            }

            std::wstring wide_text(encoded_length / sizeof(wchar_t), L'\0');
            std::memcpy(wide_text.data(), file_bytes->data() + 2, encoded_length);
            const std::optional<std::string> converted = TryConvertUtf16ToUtf8(wide_text);
            if (!converted.has_value())
            {
                return std::nullopt;
            }

            decoded_text = *converted;
            document.Encoding = IniTextEncoding::Utf16LittleEndian;
        }
        else
        {
            decoded_text = *file_bytes;
            document.Encoding = IniTextEncoding::SingleByte;
        }

        document.Lines = SplitIniTextIntoLines(decoded_text);
        return document;
    }

    /**
     * @brief Encodes one decoded INI document using its original text encoding and CRLF line endings.
     * @param document Decoded lines and required on-disk encoding.
     * @return Complete encoded bytes ready for closed-file publication, or no value when UTF-8 conversion fails.
     */
    std::optional<std::string> TryEncodeIniDocument(const IniTextDocument& document)
    {
        std::string text;
        for (std::size_t index = 0; index < document.Lines.size(); ++index)
        {
            text += document.Lines[index];
            if (index + 1 < document.Lines.size())
            {
                text += "\r\n";
            }
        }

        if (document.Encoding == IniTextEncoding::SingleByte)
        {
            return text;
        }

        const std::optional<std::wstring> wide_text = TryConvertUtf8ToUtf16(text);
        if (!wide_text.has_value())
        {
            return std::nullopt;
        }

        const unsigned char byte_order_mark[] = { 0xFF, 0xFE };
        std::string encoded_bytes(reinterpret_cast<const char*>(byte_order_mark), sizeof(byte_order_mark));
        encoded_bytes.append(
            reinterpret_cast<const char*>(wide_text->data()),
            wide_text->size() * sizeof(wchar_t));
        return encoded_bytes;
    }

    /**
     * @brief Writes exact bytes to a newly-created sibling transaction file and closes it before publication starts.
     * @param path New transaction file path that must not already exist.
     * @param bytes Exact encoded content that should be staged.
     * @param file_created Receives true once this call creates the path, including when a later write fails.
     * @return True when the file is completely written, flushed, and closed; otherwise false.
     */
    bool TryWriteFileBytes(
        const std::filesystem::path& path,
        std::string_view bytes,
        bool& file_created)
    {
        file_created = false;
        const HANDLE handle = CreateFileW(
            path.c_str(),
            GENERIC_WRITE,
            0,
            nullptr,
            CREATE_NEW,
            FILE_ATTRIBUTE_NORMAL,
            nullptr);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return false;
        }

        file_created = true;

        std::size_t offset = 0;
        bool write_succeeded = true;
        while (offset < bytes.size())
        {
            const std::size_t bytes_remaining = bytes.size() - offset;
            const DWORD request_size = bytes_remaining > static_cast<std::size_t>(MAXDWORD)
                ? MAXDWORD
                : static_cast<DWORD>(bytes_remaining);
            DWORD bytes_written = 0;
            if (WriteFile(handle, bytes.data() + offset, request_size, &bytes_written, nullptr) == FALSE || bytes_written == 0)
            {
                write_succeeded = false;
                break;
            }

            offset += static_cast<std::size_t>(bytes_written);
        }

        if (write_succeeded && FlushFileBuffers(handle) == FALSE)
        {
            write_succeeded = false;
        }

        const BOOL close_succeeded = CloseHandle(handle);
        if (!write_succeeded || close_succeeded == FALSE)
        {
            return false;
        }

        return true;
    }

    /**
     * @brief Creates a unique transaction filename beside one target using process, tick, and sequence identity.
     * @param target_path Existing target whose parent directory must contain the transaction file.
     * @param role_suffix Distinct suffix identifying generated, launcher, or recovery ownership.
     * @return Sibling path that the caller must reserve through `TryWriteFileBytes`.
     */
    std::filesystem::path CreateSiblingTransactionPath(
        const std::filesystem::path& target_path,
        std::wstring_view role_suffix)
    {
        std::wstring transaction_path = target_path.wstring();
        transaction_path += L".helenhook-";
        transaction_path += std::to_wstring(GetCurrentProcessId());
        transaction_path += L"-";
        transaction_path += std::to_wstring(GetTickCount64());
        transaction_path += L"-";
        transaction_path += std::to_wstring(BatmanGraphicsTransactionSequence.fetch_add(1, std::memory_order_relaxed));
        transaction_path += role_suffix;
        return std::filesystem::path(transaction_path);
    }


    /**
     * @brief Stages exact recovery bytes into a newly-created sibling candidate without overwriting any existing path.
     * @param recovery_path Closed recovery artifact whose exact bytes should be copied.
     * @param candidate_path New sibling path that will be consumed by the recovery move.
     * @param candidate_created Receives true when this attempt created the candidate, including a partial write.
     * @param error_code Receives a Win32-compatible diagnostic when reading or staging fails.
     * @return True when the candidate is completely written and closed; otherwise false.
     */
    bool TryStageRecoveryCandidate(
        const std::filesystem::path& recovery_path,
        const std::filesystem::path& candidate_path,
        bool& candidate_created,
        DWORD& error_code)
    {
        candidate_created = false;
        const std::optional<std::string> recovery_bytes = TryReadFileBytes(recovery_path);
        if (!recovery_bytes.has_value())
        {
            error_code = ERROR_READ_FAULT;
            return false;
        }

        if (!TryWriteFileBytes(candidate_path, *recovery_bytes, candidate_created))
        {
            error_code = ERROR_WRITE_FAULT;
            return false;
        }

        error_code = ERROR_SUCCESS;
        return true;
    }



    /**
     * @brief Cleans every transaction path successfully created by one dual-INI publication attempt.
     * @param owned_transaction_files Paths created by this attempt; paths not in this collection are never deleted.
     * @return True when all owned paths are absent after cleanup; otherwise false, with each failure logged by path.
     */
    bool CleanupBatmanGraphicsTransactionFiles(const helen::BatmanGraphicsFileOperations& file_operations,
        const std::vector<std::filesystem::path>& owned_transaction_files)
    {
        bool cleanup_succeeded = true;
        for (const std::filesystem::path& transaction_file : owned_transaction_files)
        {
            if (!file_operations.Remove(transaction_file))
            {
                cleanup_succeeded = false;
            }
        }

        return cleanup_succeeded;
    }

    /**
     * @brief Reconciles one published target to exact original bytes after a Win32 publication failure.
     * @param target_path Target whose state must match the pre-apply snapshot.
     * @param recovery_path Closed sibling artifact containing the exact pre-apply bytes.
     * @param restore_candidate_path Temporary sibling candidate used for a supported recovery move.
     * @param original_bytes Exact pre-apply bytes that must be proven on disk after recovery.
     * @param target_label Human-readable target label used by hard-compensation diagnostics.
     * @return True when the target was already equal or was restored and verified byte-for-byte; otherwise false.
     */
    bool TryReconcileBatmanGraphicsIniTarget(const helen::BatmanGraphicsFileOperations& file_operations,
        const std::filesystem::path& target_path,
        const std::filesystem::path& recovery_path,
        const std::filesystem::path& restore_candidate_path,
        std::vector<std::filesystem::path>& owned_transaction_files,
        std::string_view original_bytes,
        const wchar_t* target_label)
    {
        const std::optional<std::string> current_bytes = TryReadFileBytes(target_path);
        if (current_bytes.has_value() && *current_bytes == original_bytes)
        {
            return true;
        }

        bool candidate_created = false;
        DWORD copy_error = ERROR_SUCCESS;
        const bool copied_recovery = TryStageRecoveryCandidate(
            recovery_path,
            restore_candidate_path,
            candidate_created,
            copy_error);
        if (candidate_created)
        {
            owned_transaction_files.push_back(restore_candidate_path);
        }

        DWORD move_error = ERROR_SUCCESS;
        if (copied_recovery)
        {
            file_operations.Restore(target_path, restore_candidate_path, move_error);
        }

        const std::optional<std::string> verified_bytes = TryReadFileBytes(target_path);
        if (verified_bytes.has_value() && *verified_bytes == original_bytes)
        {
            return true;
        }

        DWORD diagnostic_error = copy_error;
        if (diagnostic_error == ERROR_SUCCESS)
        {
            diagnostic_error = move_error;
        }
        if (diagnostic_error == ERROR_SUCCESS)
        {
            diagnostic_error = ERROR_INVALID_DATA;
        }

        helen::Logf(
            L"[graphics] Apply failed: hard compensation failure for %ls path=%ls error=%lu.",
            target_label,
            target_path.wstring().c_str(),
            static_cast<unsigned long>(diagnostic_error));
        return false;
    }

    /**
     * @brief Reconciles both INI targets against exact original snapshots after either publication reports failure.
     * @param generated_path Target generated `BmEngine.ini` path.
     * @param user_path Target launcher-owned `UserEngine.ini` path.
     * @param generated_recovery_path Closed generated recovery artifact.
     * @param user_recovery_path Closed launcher recovery artifact.
     * @param generated_restore_candidate_path Temporary generated recovery candidate path.
     * @param user_restore_candidate_path Temporary launcher recovery candidate path.
     * @param original_generated_bytes Exact generated bytes captured before preparation.
     * @param original_user_bytes Exact launcher bytes captured before preparation.
     * @return True only when both targets are proven byte-for-byte equal to their original snapshots.
     */
    bool TryReconcileBatmanGraphicsIniPair(const helen::BatmanGraphicsFileOperations& file_operations,
        const std::filesystem::path& generated_path,
        const std::filesystem::path& user_path,
        const std::filesystem::path& generated_recovery_path,
        const std::filesystem::path& user_recovery_path,
        const std::filesystem::path& generated_restore_candidate_path,
        const std::filesystem::path& user_restore_candidate_path,
        std::vector<std::filesystem::path>& owned_transaction_files,
        std::string_view original_generated_bytes,
        std::string_view original_user_bytes)
    {
        const bool generated_reconciled = TryReconcileBatmanGraphicsIniTarget(file_operations,
            generated_path,
            generated_recovery_path,
            generated_restore_candidate_path,
            owned_transaction_files,
            original_generated_bytes,
            L"generated INI");
        const bool user_reconciled = TryReconcileBatmanGraphicsIniTarget(file_operations,
            user_path,
            user_recovery_path,
            user_restore_candidate_path,
            owned_transaction_files,
            original_user_bytes,
            L"launcher INI");
        return generated_reconciled && user_reconciled;
    }

    /**
     * @brief Publishes generated and launcher INI bytes as one compensating dual-file transaction.
     * @param generated_path Target generated `BmEngine.ini` path.
     * @param user_path Target launcher-owned `UserEngine.ini` path.
     * @param generated_bytes Newly encoded generated INI bytes.
     * @param user_bytes Newly encoded launcher INI bytes.
     * @param original_generated_bytes Exact generated bytes captured before preparation for compensation.
     * @param original_user_bytes Exact launcher bytes captured before preparation for recovery and cleanup ownership.
     * @return Verified publication outcome and retained evidence paths; uncertain reconciliation never discards recovery artifacts.
     */
    helen::BatmanGraphicsApplyResult PublishBatmanGraphicsIniPair(const helen::BatmanGraphicsFileOperations& file_operations,
        const std::filesystem::path& generated_path,
        const std::filesystem::path& user_path,
        std::string_view generated_bytes,
        std::string_view user_bytes,
        std::string_view original_generated_bytes,
        std::string_view original_user_bytes)
    {
        const std::filesystem::path generated_stage_path = CreateSiblingTransactionPath(generated_path, L"-generated-stage");
        const std::filesystem::path user_stage_path = CreateSiblingTransactionPath(user_path, L"-user-stage");
        const std::filesystem::path generated_recovery_path = CreateSiblingTransactionPath(generated_path, L"-generated-recovery");
        const std::filesystem::path user_recovery_path = CreateSiblingTransactionPath(user_path, L"-user-recovery");
        const std::filesystem::path generated_restore_candidate_path = CreateSiblingTransactionPath(generated_path, L"-generated-restore-candidate");
        const std::filesystem::path user_restore_candidate_path = CreateSiblingTransactionPath(user_path, L"-user-restore-candidate");
        std::vector<std::filesystem::path> owned_transaction_files;
        bool file_created = false;

        if (!TryWriteFileBytes(generated_stage_path, generated_bytes, file_created))
        {
            if (file_created)
            {
                owned_transaction_files.push_back(generated_stage_path);
            }

            const bool cleanup_succeeded = CleanupBatmanGraphicsTransactionFiles(file_operations, owned_transaction_files);
            if (!cleanup_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to clean transaction files after generated staging failed.");
            }

            helen::Logf(L"[graphics] Apply failed: unable to stage generated INI path=%ls.", generated_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }
        owned_transaction_files.push_back(generated_stage_path);

        if (!TryWriteFileBytes(user_stage_path, user_bytes, file_created))
        {
            if (file_created)
            {
                owned_transaction_files.push_back(user_stage_path);
            }

            const bool cleanup_succeeded = CleanupBatmanGraphicsTransactionFiles(file_operations, owned_transaction_files);
            if (!cleanup_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to clean transaction files after launcher staging failed.");
            }

            helen::Logf(L"[graphics] Apply failed: unable to stage launcher INI path=%ls.", user_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }
        owned_transaction_files.push_back(user_stage_path);

        if (!TryWriteFileBytes(generated_recovery_path, original_generated_bytes, file_created))
        {
            if (file_created)
            {
                owned_transaction_files.push_back(generated_recovery_path);
            }

            const bool cleanup_succeeded = CleanupBatmanGraphicsTransactionFiles(file_operations, owned_transaction_files);
            if (!cleanup_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to clean transaction files after generated recovery staging failed.");
            }

            helen::Logf(L"[graphics] Apply failed: unable to stage generated INI recovery bytes path=%ls.", generated_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }
        owned_transaction_files.push_back(generated_recovery_path);

        if (!TryWriteFileBytes(user_recovery_path, original_user_bytes, file_created))
        {
            if (file_created)
            {
                owned_transaction_files.push_back(user_recovery_path);
            }

            const bool cleanup_succeeded = CleanupBatmanGraphicsTransactionFiles(file_operations, owned_transaction_files);
            if (!cleanup_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to clean transaction files after launcher recovery staging failed.");
            }

            helen::Logf(L"[graphics] Apply failed: unable to stage launcher INI recovery bytes path=%ls.", user_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }
        owned_transaction_files.push_back(user_recovery_path);

        DWORD generated_publication_error = ERROR_SUCCESS;
        if (!file_operations.Replace(generated_path, generated_stage_path, generated_publication_error))
        {
            const bool reconciliation_succeeded = TryReconcileBatmanGraphicsIniPair(file_operations,
                generated_path,
                user_path,
                generated_recovery_path,
                user_recovery_path,
                generated_restore_candidate_path,
                user_restore_candidate_path,
                owned_transaction_files,
                original_generated_bytes,
                original_user_bytes);
            const bool cleanup_succeeded = reconciliation_succeeded && CleanupBatmanGraphicsTransactionFiles(file_operations, owned_transaction_files);
            if (!reconciliation_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to prove both INI targets were restored after generated publication failure.");
            }
            if (!cleanup_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to clean transaction files after generated publication failed.");
            }

            helen::Logf(
                L"[graphics] Apply failed: unable to publish generated INI path=%ls error=%lu.",
                generated_path.wstring().c_str(),
                static_cast<unsigned long>(generated_publication_error));
            return {reconciliation_succeeded ? helen::BatmanGraphicsApplyOutcome::NotApplied :
                helen::BatmanGraphicsApplyOutcome::IntegrityUncertain,
                cleanup_succeeded ? std::vector<std::filesystem::path>{} : std::move(owned_transaction_files)};
        }

        DWORD user_publication_error = ERROR_SUCCESS;
        if (!file_operations.Replace(user_path, user_stage_path, user_publication_error))
        {
            const bool reconciliation_succeeded = TryReconcileBatmanGraphicsIniPair(file_operations,
                generated_path,
                user_path,
                generated_recovery_path,
                user_recovery_path,
                generated_restore_candidate_path,
                user_restore_candidate_path,
                owned_transaction_files,
                original_generated_bytes,
                original_user_bytes);
            const bool cleanup_succeeded = reconciliation_succeeded && CleanupBatmanGraphicsTransactionFiles(file_operations, owned_transaction_files);
            if (!reconciliation_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to prove both INI targets were restored after launcher publication failure.");
            }
            if (!cleanup_succeeded)
            {
                helen::Logf(L"[graphics] Apply failed: unable to clean transaction files after launcher publication failed.");
            }

            helen::Logf(
                L"[graphics] Apply failed: unable to publish launcher INI path=%ls error=%lu.",
                user_path.wstring().c_str(),
                static_cast<unsigned long>(user_publication_error));
            if (reconciliation_succeeded && cleanup_succeeded)
            {
                helen::Logf(
                    L"[graphics] Apply failed: launcher INI publication failed; both INIs were restored generated_path=%ls.",
                    generated_path.wstring().c_str());
            }

            return {reconciliation_succeeded ? helen::BatmanGraphicsApplyOutcome::NotApplied :
                helen::BatmanGraphicsApplyOutcome::IntegrityUncertain,
                cleanup_succeeded ? std::vector<std::filesystem::path>{} : std::move(owned_transaction_files)};
        }

        const std::optional<std::string> verified_generated = TryReadFileBytes(generated_path);
        const std::optional<std::string> verified_user = TryReadFileBytes(user_path);
        if (!verified_generated.has_value() || !verified_user.has_value() ||
            *verified_generated != generated_bytes || *verified_user != user_bytes) {
            helen::Logf(L"[graphics] Apply integrity uncertain: published targets could not be verified; retaining recovery files.");
            return {helen::BatmanGraphicsApplyOutcome::IntegrityUncertain, std::move(owned_transaction_files)};
        }

        if (!CleanupBatmanGraphicsTransactionFiles(file_operations, owned_transaction_files)) {
            helen::Logf(L"[graphics] Apply committed, but transaction cleanup failed.");
            return {helen::BatmanGraphicsApplyOutcome::CommittedCleanupFailed, std::move(owned_transaction_files)};
        }
        return {helen::BatmanGraphicsApplyOutcome::Committed, {}};
    }

    /**
     * @brief Reads one required INI value from a specific section.
     * @param lines Loaded INI file lines that should be searched.
     * @param section_name Exact section name that owns the required value.
     * @param key_name Exact key name that should be resolved within the section.
     * @return Trimmed INI value when found; otherwise no value.
     */
    std::optional<std::string> TryReadIniValue(
        const std::vector<std::string>& lines,
        std::string_view section_name,
        std::string_view key_name)
    {
        bool in_target_section = false;
        for (const std::string& line : lines)
        {
            const std::string trimmed = TrimAscii(line);
            if (trimmed.empty() || trimmed[0] == ';')
            {
                continue;
            }

            if (trimmed.front() == '[')
            {
                in_target_section = IsIniSectionDeclaration(trimmed, section_name);
                continue;
            }

            if (!in_target_section)
            {
                continue;
            }

            std::string key;
            std::string value;
            if (!TrySplitIniAssignment(trimmed, key, value))
            {
                continue;
            }

            if (key == key_name)
            {
                return value;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Updates one required INI value inside a specific section.
     * @param lines Loaded INI file lines that should be updated in place.
     * @param section_name Exact section name that owns the target value.
     * @param key_name Exact key name that should be updated.
     * @param encoded_value Replacement value text that should be written.
     * @return True when the target key is found and replaced; otherwise false.
     */
    bool UpdateIniValue(
        std::vector<std::string>& lines,
        std::string_view section_name,
        std::string_view key_name,
        std::string_view encoded_value)
    {
        bool in_target_section = false;
        for (std::string& line : lines)
        {
            const std::string trimmed = TrimAscii(line);
            if (trimmed.empty() || trimmed[0] == ';')
            {
                continue;
            }

            if (trimmed.front() == '[')
            {
                in_target_section = IsIniSectionDeclaration(trimmed, section_name);
                continue;
            }

            if (!in_target_section)
            {
                continue;
            }

            std::string key;
            std::string value;
            if (!TrySplitIniAssignment(trimmed, key, value))
            {
                continue;
            }

            if (key == key_name)
            {
                line = std::string(key_name) + "=" + std::string(encoded_value);
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Ensures one INI assignment exists by updating it when present or appending it when missing.
     * @param lines Loaded INI file lines that should be updated in place.
     * @param section_name Exact section name that owns the target value.
     * @param key_name Exact key name that should be updated or inserted.
     * @param encoded_value Replacement value text that should be written.
     * @return True when the section exists and is updated, or when a new section/key is appended.
     */
    bool UpsertIniValue(
        std::vector<std::string>& lines,
        std::string_view section_name,
        std::string_view key_name,
        std::string_view encoded_value)
    {
        if (UpdateIniValue(lines, section_name, key_name, encoded_value))
        {
            return true;
        }

        const std::string section_header = std::string("[") + std::string(section_name) + "]";
        bool in_target_section = false;
        bool section_found = false;
        std::size_t insert_index = lines.size();

        for (std::size_t index = 0; index < lines.size(); ++index)
        {
            const std::string trimmed = TrimAscii(lines[index]);
            if (trimmed.empty() || trimmed[0] == ';')
            {
                continue;
            }

            if (trimmed.front() == '[')
            {
                if (in_target_section)
                {
                    insert_index = index;
                    in_target_section = false;
                }

                if (IsIniSectionDeclaration(trimmed, section_name))
                {
                    section_found = true;
                    in_target_section = true;
                }

                continue;
            }

            if (!in_target_section)
            {
                continue;
            }

            std::string key;
            std::string value;
            if (!TrySplitIniAssignment(trimmed, key, value))
            {
                continue;
            }

            if (key == key_name)
            {
                lines[index] = std::string(key_name) + "=" + std::string(encoded_value);
                return true;
            }
        }

        if (in_target_section)
        {
            section_found = true;
            insert_index = lines.size();
        }

        const std::string new_line = std::string(key_name) + "=" + std::string(encoded_value);
        if (section_found)
        {
            lines.insert(lines.begin() + static_cast<std::ptrdiff_t>(insert_index), new_line);
            return true;
        }

        if (!lines.empty() && !TrimAscii(lines.back()).empty())
        {
            lines.push_back("");
        }

        lines.push_back(section_header);
        lines.push_back(new_line);
        return true;
    }

    /**
     * @brief Parses one strict integer setting from trimmed INI text.
     * @param text Trimmed INI value that should encode an integer.
     * @param value Receives the parsed integer on success.
     * @return True when the full value parses as an integer; otherwise false.
     */
    bool TryParseIntValue(std::string_view text, int& value)
    {
        const std::string trimmed = TrimAscii(text);
        if (trimmed.empty())
        {
            return false;
        }

        const char* const begin = trimmed.data();
        const char* const end = trimmed.data() + trimmed.size();
        const std::from_chars_result result = std::from_chars(begin, end, value);
        return result.ec == std::errc() && result.ptr == end;
    }

    /**
     * @brief Parses one UE3 boolean setting into the normalized menu representation.
     * @param text Trimmed INI value that should encode `True` or `False`.
     * @param value Receives `1` for true and `0` for false on success.
     * @return True when the value is a supported UE3 boolean token; otherwise false.
     */
    bool TryParseBoolValue(std::string_view text, int& value)
    {
        const std::string trimmed = TrimAscii(text);
        if (EqualsIgnoreCaseAscii(trimmed, "True"))
        {
            value = 1;
            return true;
        }

        if (EqualsIgnoreCaseAscii(trimmed, "False"))
        {
            value = 0;
            return true;
        }

        return false;
    }

    /**
     * @brief Encodes one normalized menu boolean into the UE3 `True` or `False` text format.
     * @param value Normalized menu boolean where `0` means false and `1` means true.
     * @param encoded_value Receives the encoded UE3 boolean token on success.
     * @return True when the value is a supported normalized boolean; otherwise false.
     */
    bool TryEncodeBoolValue(int value, std::string& encoded_value)
    {
        if (value == 0)
        {
            encoded_value = "False";
            return true;
        }

        if (value == 1)
        {
            encoded_value = "True";
            return true;
        }

        return false;
    }

    /**
     * @brief Maps one UE3 `MaxMultisamples` integer into the normalized Batman menu MSAA state.
     * @param encoded_value Raw UE3 `MaxMultisamples` integer value.
     * @param normalized_value Receives the normalized Batman MSAA state on success.
     * @return True when the encoded value maps to a supported Batman menu state; otherwise false.
     */
    bool TryMapMsaaFromIniValue(int encoded_value, int& normalized_value)
    {
        if (encoded_value <= 1)
        {
            normalized_value = 0;
            return true;
        }

        if (encoded_value == 2)
        {
            normalized_value = 1;
            return true;
        }

        if (encoded_value == 4)
        {
            normalized_value = 2;
            return true;
        }

        if (encoded_value == 8)
        {
            normalized_value = 3;
            return true;
        }

        if (encoded_value == 16)
        {
            normalized_value = 5;
            return true;
        }

        return false;
    }

    /**
     * @brief Encodes one normalized Batman menu MSAA state into the UE3 `MaxMultisamples` integer.
     * @param normalized_value Batman menu MSAA state that should be persisted.
     * @param encoded_value Receives the UE3 `MaxMultisamples` integer on success.
     * @return True when the normalized state maps to a supported UE3 sample count; otherwise false.
     */
    bool TryEncodeMsaaValue(int normalized_value, int& encoded_value)
    {
        if (normalized_value == 0)
        {
            encoded_value = 1;
            return true;
        }

        if (normalized_value == 1)
        {
            encoded_value = 2;
            return true;
        }

        if (normalized_value == 2)
        {
            encoded_value = 4;
            return true;
        }

        if (normalized_value == 3 || normalized_value == 4)
        {
            encoded_value = 8;
            return true;
        }

        if (normalized_value == 5 || normalized_value == 6)
        {
            encoded_value = 16;
            return true;
        }

        return false;
    }

    /**
     * @brief Maps one normalized subtitle-size state into the expected `Engine.HUD.ConsoleFontSize` INI value.
     * @param subtitle_size_state Normalized subtitle option from the config store.
     * @param encoded_value Receives the encoded font-size integer for `Engine.HUD.ConsoleFontSize`.
     * @return True when `subtitle_size_state` is a supported option; otherwise false.
     */
    bool TryMapSubtitleSizeStateToFontSize(int subtitle_size_state, int& encoded_value)
    {
        if (subtitle_size_state == 0)
        {
            encoded_value = 5;
            return true;
        }

        if (subtitle_size_state == 1)
        {
            encoded_value = 6;
            return true;
        }

        if (subtitle_size_state == 2)
        {
            encoded_value = 7;
            return true;
        }

        if (subtitle_size_state == 3)
        {
            encoded_value = 8;
            return true;
        }

        if (subtitle_size_state == 4)
        {
            encoded_value = 9;
            return true;
        }

        if (subtitle_size_state == 5)
        {
            encoded_value = 10;
            return true;
        }

        return false;
    }

    /**
     * @brief Maps one persisted `Engine.HUD.ConsoleFontSize` integer to a normalized subtitle-size state.
     * @param encoded_value Parsed font-size integer from INI.
     * @param state Receives the normalized subtitle option value on success.
     * @return True when `encoded_value` is a supported value; otherwise false.
     */
    bool TryMapSubtitleSizeFontSizeToState(int encoded_value, int& state)
    {
        if (encoded_value == 5)
        {
            state = 0;
            return true;
        }

        if (encoded_value == 6)
        {
            state = 1;
            return true;
        }

        if (encoded_value == 7)
        {
            state = 2;
            return true;
        }

        if (encoded_value == 8)
        {
            state = 3;
            return true;
        }

        if (encoded_value == 9)
        {
            state = 4;
            return true;
        }

        if (encoded_value == 10)
        {
            state = 5;
            return true;
        }

        return false;
    }

    /**
     * @brief Reads one persisted subtitle-size state from INI file lines.
     * @param lines INI lines already loaded from disk.
     * @param state Receives the mapped subtitle state on success.
     * @param encoded_value Receives the exact parsed `Engine.HUD.ConsoleFontSize` value on success.
     * @return True when `Engine.HUD.ConsoleFontSize` is present and maps to a supported state; otherwise false.
     */
    bool TryReadSubtitleSizeStateFromIniLines(
        const std::vector<std::string>& lines,
        int& state,
        int& encoded_value)
    {
        const std::optional<std::string> value = TryReadIniValue(lines, "Engine.HUD", "ConsoleFontSize");
        if (!value.has_value())
        {
            return false;
        }

        if (!TryParseIntValue(*value, encoded_value))
        {
            return false;
        }

        return TryMapSubtitleSizeFontSizeToState(encoded_value, state);
    }

    /**
     * @brief Tries to resolve the active subtitle INI file path that already stores `Engine.HUD.ConsoleFontSize`.
     * @param engine_ini_path Existing `BmEngine.ini` path used by this service.
     * @param resolved_path Receives the resolved subtitle INI path when this returns true.
     * @param lines Receives the loaded lines from the resolved path.
     * @return True when one candidate file exists and contains `Engine.HUD.ConsoleFontSize`.
     */
    bool TryReadSubtitleIniLinesWithFontSize(
        const std::filesystem::path& engine_ini_path,
        std::filesystem::path& resolved_path,
        std::vector<std::string>& lines)
    {
        const std::filesystem::path game_ini_path = engine_ini_path.parent_path() / "BmGame.ini";

        const std::optional<std::vector<std::string>> engine_lines = TryReadAllLines(engine_ini_path);
        if (engine_lines.has_value())
        {
            if (TryReadIniValue(*engine_lines, "Engine.HUD", "ConsoleFontSize").has_value())
            {
                resolved_path = engine_ini_path;
                lines = *engine_lines;
                return true;
            }
        }

        const std::optional<std::vector<std::string>> game_lines = TryReadAllLines(game_ini_path);
        if (game_lines.has_value())
        {
            if (TryReadIniValue(*game_lines, "Engine.HUD", "ConsoleFontSize").has_value())
            {
                resolved_path = game_ini_path;
                lines = *game_lines;
                return true;
            }
        }

        return false;
    }

    /**
     * @brief Resolves the preferred subtitle INI target for writes.
     *
     * This function prefers the file that already stores `Engine.HUD.ConsoleFontSize`.
     * When no existing value is found, it prefers `BmGame.ini` and then falls back to `BmEngine.ini`.
     *
     * @param engine_ini_path Existing `BmEngine.ini` path used by this service.
     * @param resolved_path Receives the resolved subtitle INI path.
     * @return True when one path can be produced.
     */
    bool TryResolveSubtitleIniPathForWrite(
        const std::filesystem::path& engine_ini_path,
        std::filesystem::path& resolved_path)
    {
        std::vector<std::string> lines;
        if (TryReadSubtitleIniLinesWithFontSize(engine_ini_path, resolved_path, lines))
        {
            return true;
        }

        const std::filesystem::path game_ini_path = engine_ini_path.parent_path() / "BmGame.ini";
        std::error_code error;

        if (std::filesystem::exists(game_ini_path, error))
        {
            resolved_path = game_ini_path;
            return true;
        }

        if (error)
        {
            return false;
        }

        if (std::filesystem::exists(engine_ini_path, error))
        {
            resolved_path = engine_ini_path;
            return true;
        }

        return false;
    }

    /**
     * @brief Returns the canonical Batman detail preset for one normalized detail-level state.
     * @param detail_level Normalized Batman menu detail-level state.
     * @return Matching preset definition when the detail level is one of the declared presets; otherwise no value.
     */
    std::optional<BatmanGraphicsPresetDefinition> TryGetPresetByDetailLevel(int detail_level)
    {
        for (const BatmanGraphicsPresetDefinition& preset : BatmanGraphicsPresets)
        {
            if (preset.DetailLevel == detail_level)
            {
                return preset;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Resolves the declared Batman detail preset that matches the current individual toggle values.
     * @param state Current normalized Batman graphics draft state.
     * @return Matching preset definition when the current toggles exactly match one preset; otherwise no value.
     */
    std::optional<BatmanGraphicsPresetDefinition> TryResolvePresetFromDraft(const LegacyBatmanGraphicsDraftState& state)
    {
        for (const BatmanGraphicsPresetDefinition& preset : BatmanGraphicsPresets)
        {
            if (preset.Bloom == state.Bloom &&
                preset.DynamicShadows == state.DynamicShadows &&
                preset.MotionBlur == state.MotionBlur &&
                preset.Distortion == state.Distortion &&
                preset.FogVolumes == state.FogVolumes &&
                preset.SphericalHarmonicLighting == state.SphericalHarmonicLighting &&
                preset.AmbientOcclusion == state.AmbientOcclusion)
            {
                return preset;
            }
        }

        return std::nullopt;
    }

    /**
     * @brief Derives the most appropriate UE3 `DetailMode` value for one normalized Batman graphics draft.
     * @param state Current normalized Batman graphics draft state.
     * @return Derived UE3 `DetailMode` integer that best matches the current draft state.
     */
    int DeriveDetailModeFromDraft(const LegacyBatmanGraphicsDraftState& state)
    {
        const std::optional<BatmanGraphicsPresetDefinition> preset = TryResolvePresetFromDraft(state);
        if (preset.has_value())
        {
            return preset->DetailMode;
        }

        if (state.AmbientOcclusion != 0)
        {
            return 2;
        }

        if (state.MotionBlur != 0 ||
            state.Distortion != 0 ||
            state.FogVolumes != 0 ||
            state.SphericalHarmonicLighting != 0 ||
            state.Bloom != 0 ||
            state.DynamicShadows != 0)
        {
            return 1;
        }

        return 0;
    }

    /**
     * @brief Reads one required dispatcher integer value and copies it into the supplied storage slot.
     * @param dispatcher Dispatcher that owns the required config key.
     * @param key Registered config key that should be resolved.
     * @param value Receives the resolved integer on success.
     * @return True when the key exists in the dispatcher; otherwise false.
     */
    bool TryReadDispatcherValue(const helen::CommandDispatcher& dispatcher, const char* key, int& value)
    {
        const std::optional<int> resolved_value = dispatcher.TryGetInt(key);
        if (!resolved_value.has_value())
        {
            return false;
        }

        value = *resolved_value;
        return true;
    }

    /**
     * @brief Writes one required dispatcher integer value back into the registered config store.
     * @param dispatcher Dispatcher that owns the required config key.
     * @param key Registered config key that should be updated.
     * @param value New integer value that should be stored.
     * @return True when the key exists in the dispatcher; otherwise false.
     */
    bool TryWriteDispatcherValue(helen::CommandDispatcher& dispatcher, const char* key, int value)
    {
        return dispatcher.TrySetInt(key, value);
    }

    /**
     * @brief Reads the full Batman graphics draft state from the current dispatcher values.
     * @param dispatcher Dispatcher that should supply the registered Batman graphics config keys.
     * @param state Receives the full normalized Batman graphics draft on success.
     * @return True when every required config key exists; otherwise false.
     */
    bool TryReadDraftStateFromDispatcher(const helen::CommandDispatcher& dispatcher, LegacyBatmanGraphicsDraftState& state)
    {
        const std::optional<helen::CommandIntPair> resolution_pair = dispatcher.TryGetIntPair(
            "resolutionWidth",
            "resolutionHeight");
        if (!resolution_pair.has_value())
        {
            return false;
        }

        state.ResolutionWidth = resolution_pair->FirstValue;
        state.ResolutionHeight = resolution_pair->SecondValue;
        return
            TryReadDispatcherValue(dispatcher, "fullscreen", state.Fullscreen) &&
            TryReadDispatcherValue(dispatcher, "vsync", state.Vsync) &&
            TryReadDispatcherValue(dispatcher, "msaa", state.Msaa) &&
            TryReadDispatcherValue(dispatcher, "detailLevel", state.DetailLevel) &&
            TryReadDispatcherValue(dispatcher, "bloom", state.Bloom) &&
            TryReadDispatcherValue(dispatcher, "dynamicShadows", state.DynamicShadows) &&
            TryReadDispatcherValue(dispatcher, "motionBlur", state.MotionBlur) &&
            TryReadDispatcherValue(dispatcher, "distortion", state.Distortion) &&
            TryReadDispatcherValue(dispatcher, "fogVolumes", state.FogVolumes) &&
            TryReadDispatcherValue(dispatcher, "sphericalHarmonicLighting", state.SphericalHarmonicLighting) &&
            TryReadDispatcherValue(dispatcher, "ambientOcclusion", state.AmbientOcclusion) &&
            TryReadDispatcherValue(dispatcher, "physx", state.Physx) &&
            TryReadDispatcherValue(dispatcher, "stereo", state.Stereo);
    }

    /**
     * @brief Writes the full Batman graphics draft state back into the registered dispatcher keys.
     * @param dispatcher Dispatcher that should receive the normalized Batman graphics draft values.
     * @param state Fully populated Batman graphics draft state that should be stored.
     * @return True when every required config key exists; otherwise false.
     */
    bool TryWriteDraftStateToDispatcher(helen::CommandDispatcher& dispatcher, const LegacyBatmanGraphicsDraftState& state)
    {
        return
            TryWriteDispatcherValue(dispatcher, "fullscreen", state.Fullscreen) &&
            dispatcher.TrySetIntPair("resolutionWidth", state.ResolutionWidth, "resolutionHeight", state.ResolutionHeight) &&
            TryWriteDispatcherValue(dispatcher, "vsync", state.Vsync) &&
            TryWriteDispatcherValue(dispatcher, "msaa", state.Msaa) &&
            TryWriteDispatcherValue(dispatcher, "detailLevel", state.DetailLevel) &&
            TryWriteDispatcherValue(dispatcher, "bloom", state.Bloom) &&
            TryWriteDispatcherValue(dispatcher, "dynamicShadows", state.DynamicShadows) &&
            TryWriteDispatcherValue(dispatcher, "motionBlur", state.MotionBlur) &&
            TryWriteDispatcherValue(dispatcher, "distortion", state.Distortion) &&
            TryWriteDispatcherValue(dispatcher, "fogVolumes", state.FogVolumes) &&
            TryWriteDispatcherValue(dispatcher, "sphericalHarmonicLighting", state.SphericalHarmonicLighting) &&
            TryWriteDispatcherValue(dispatcher, "ambientOcclusion", state.AmbientOcclusion) &&
            TryWriteDispatcherValue(dispatcher, "physx", state.Physx) &&
            TryWriteDispatcherValue(dispatcher, "stereo", state.Stereo);
    }

    /** @brief Decodes one independently valid launcher field using the existing INI parser and mappings. */
    std::optional<int> ReadGraphicsField(const std::vector<std::string>& lines, helen::BatmanGraphicsField field) {
        using helen::BatmanGraphicsField;
        /** @brief Launcher keys in the exact stable scalar order; PhysX uses Engine.Engine rather than SystemSettings. */
        static constexpr std::array<const char*, 14> Keys{
            "Fullscreen", "UseVsync", "MaxMultisamples", "Bloom", "DynamicShadows", "MotionBlur",
            "Distortion", "FogVolumes", "DisableSphericalHarmonicLights", "AmbientOcclusion",
            "PhysXLevel", "Stereo", "ResX", "ResY"
        };
        const std::size_t index = static_cast<std::size_t>(field);
        if (index >= Keys.size()) {
            return std::nullopt;
        }
        const char* section = field == BatmanGraphicsField::Physx ? "Engine.Engine" : "SystemSettings";
        const std::optional<std::string> raw = TryReadIniValue(lines, section, Keys[index]);
        int value;
        if (!raw.has_value()) {
            return std::nullopt;
        } else if (field == BatmanGraphicsField::Msaa || field == BatmanGraphicsField::Physx ||
            field == BatmanGraphicsField::PersistedWidth || field == BatmanGraphicsField::PersistedHeight) {
            if (!TryParseIntValue(*raw, value)) {
                return std::nullopt;
            }
            if (field == BatmanGraphicsField::Msaa) {
                int normalized;
                if (value < 0 || !TryMapMsaaFromIniValue(value, normalized)) {
                    return std::nullopt;
                }
                return normalized;
            } else if (field == BatmanGraphicsField::Physx) {
                return value >= 0 && value <= 2 ? std::optional<int>(value) : std::nullopt;
            } else {
                return value > 0 ? std::optional<int>(value) : std::nullopt;
            }
        } else if (!TryParseBoolValue(*raw, value)) {
            return std::nullopt;
        }
        return field == BatmanGraphicsField::SphericalHarmonicLighting ? 1 - value : value;
    }

    /** @brief Captures all independent parse outcomes and invalidates configured dimensions as a pair. */
    helen::BatmanGraphicsSnapshot ReadGraphicsSnapshot(const std::vector<std::string>& lines) {
        helen::BatmanGraphicsSnapshot::Values values;
        for (std::size_t index = 0; index < values.size(); ++index) {
            values[index] = ReadGraphicsField(lines, static_cast<helen::BatmanGraphicsField>(index));
            if (!values[index].has_value()) {
                helen::Logf(L"[graphics] Launcher snapshot field=%u missing or invalid.", static_cast<unsigned>(index));
            }
        }
        if (!values[12].has_value() || !values[13].has_value()) {
            values[12].reset();
            values[13].reset();
        }
        return helen::BatmanGraphicsSnapshot(std::move(values));
    }

    /** @brief Converts a complete independently parsed snapshot into the legacy writer shape; partial snapshots fail. */
    bool TryReadDraftStateFromIniLines(const std::vector<std::string>& lines, LegacyBatmanGraphicsDraftState& state) {
        const helen::BatmanGraphicsSnapshot snapshot = ReadGraphicsSnapshot(lines);
        if (!snapshot.IsComplete()) {
            return false;
        }
        using helen::BatmanGraphicsField;
        state.Fullscreen = *snapshot.Get(BatmanGraphicsField::Fullscreen);
        state.ResolutionWidth = *snapshot.Get(BatmanGraphicsField::PersistedWidth);
        state.ResolutionHeight = *snapshot.Get(BatmanGraphicsField::PersistedHeight);
        state.Vsync = *snapshot.Get(BatmanGraphicsField::Vsync);
        state.Msaa = *snapshot.Get(BatmanGraphicsField::Msaa);
        state.Bloom = *snapshot.Get(BatmanGraphicsField::Bloom);
        state.DynamicShadows = *snapshot.Get(BatmanGraphicsField::DynamicShadows);
        state.MotionBlur = *snapshot.Get(BatmanGraphicsField::MotionBlur);
        state.Distortion = *snapshot.Get(BatmanGraphicsField::Distortion);
        state.FogVolumes = *snapshot.Get(BatmanGraphicsField::FogVolumes);
        state.SphericalHarmonicLighting = *snapshot.Get(BatmanGraphicsField::SphericalHarmonicLighting);
        state.AmbientOcclusion = *snapshot.Get(BatmanGraphicsField::AmbientOcclusion);
        state.Physx = *snapshot.Get(BatmanGraphicsField::Physx);
        state.Stereo = *snapshot.Get(BatmanGraphicsField::Stereo);
        const std::optional<BatmanGraphicsPresetDefinition> preset = TryResolvePresetFromDraft(state);
        state.DetailLevel = preset.has_value() ? preset->DetailLevel : 4;
        return true;
    }
    /**
     * @brief Applies one declared Batman detail preset to the supplied draft state.
     * @param state Draft state that should receive the preset-controlled toggle values.
     * @return True when `state.DetailLevel` resolves to a declared Batman preset; otherwise false.
     */
    bool ApplyDetailPresetToDraftState(LegacyBatmanGraphicsDraftState& state)
    {
        const std::optional<BatmanGraphicsPresetDefinition> preset = TryGetPresetByDetailLevel(state.DetailLevel);
        if (!preset.has_value())
        {
            return false;
        }

        state.Bloom = preset->Bloom;
        state.DynamicShadows = preset->DynamicShadows;
        state.MotionBlur = preset->MotionBlur;
        state.Distortion = preset->Distortion;
        state.FogVolumes = preset->FogVolumes;
        state.SphericalHarmonicLighting = preset->SphericalHarmonicLighting;
        state.AmbientOcclusion = preset->AmbientOcclusion;
        return true;
    }

    /**
     * @brief Encodes one normalized Batman graphics draft into an existing engine-configuration document.
     * @param state Fully populated normalized graphics state that should be persisted.
     * @param lines Existing INI lines whose required graphics assignments should be replaced in place.
     * @param failed_setting Receives the section-qualified setting being processed when an update fails.
     * @return True when every required setting exists and accepts the normalized value; otherwise false.
     */
    bool TryApplyDraftStateToIniLines(
        const LegacyBatmanGraphicsDraftState& state,
        std::vector<std::string>& lines,
        std::wstring& failed_setting)
    {
        std::string encoded_value;

        failed_setting = L"SystemSettings.Fullscreen";
        if (!TryEncodeBoolValue(state.Fullscreen, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Fullscreen", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.ResX";
        encoded_value = std::to_string(state.ResolutionWidth);
        if (!UpdateIniValue(lines, "SystemSettings", "ResX", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.ResY";
        encoded_value = std::to_string(state.ResolutionHeight);
        if (!UpdateIniValue(lines, "SystemSettings", "ResY", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.UseVsync";
        if (!TryEncodeBoolValue(state.Vsync, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "UseVsync", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.MaxMultisamples";
        int encoded_msaa = 0;
        if (!TryEncodeMsaaValue(state.Msaa, encoded_msaa))
        {
            return false;
        }

        encoded_value = std::to_string(encoded_msaa);
        if (!UpdateIniValue(lines, "SystemSettings", "MaxMultisamples", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.DetailMode";
        encoded_value = std::to_string(DeriveDetailModeFromDraft(state));
        if (!UpdateIniValue(lines, "SystemSettings", "DetailMode", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.Bloom";
        if (!TryEncodeBoolValue(state.Bloom, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Bloom", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.DynamicShadows";
        if (!TryEncodeBoolValue(state.DynamicShadows, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "DynamicShadows", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.MotionBlur";
        if (!TryEncodeBoolValue(state.MotionBlur, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "MotionBlur", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.Distortion";
        if (!TryEncodeBoolValue(state.Distortion, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Distortion", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.FogVolumes";
        if (!TryEncodeBoolValue(state.FogVolumes, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "FogVolumes", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.DisableSphericalHarmonicLights";
        const int encoded_disable_spherical_harmonic_lights = state.SphericalHarmonicLighting == 0 ? 1 : 0;
        if (!TryEncodeBoolValue(encoded_disable_spherical_harmonic_lights, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "DisableSphericalHarmonicLights", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.AmbientOcclusion";
        if (!TryEncodeBoolValue(state.AmbientOcclusion, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "AmbientOcclusion", encoded_value))
        {
            return false;
        }

        failed_setting = L"Engine.Engine.PhysXLevel";
        encoded_value = std::to_string(state.Physx);
        if (!UpdateIniValue(lines, "Engine.Engine", "PhysXLevel", encoded_value))
        {
            return false;
        }

        failed_setting = L"SystemSettings.Stereo";
        if (!TryEncodeBoolValue(state.Stereo, encoded_value) ||
            !UpdateIniValue(lines, "SystemSettings", "Stereo", encoded_value))
        {
            return false;
        }

        failed_setting.clear();
        return true;
    }
    /** @brief Serializes a complete legacy-shaped draft through the shared publisher without using dispatcher storage. */
    helen::BatmanGraphicsApplyResult ApplyGraphicsDraftState(const helen::BatmanGraphicsFileOperations& file_operations,
        const std::filesystem::path& ini_path,
        const std::shared_ptr<helen::FileWriteRoutingService>& routing_service,
        LegacyBatmanGraphicsDraftState state) {
        const std::lock_guard<std::mutex> transaction_lock(BatmanGraphicsApplyMutex);
        if (BatmanGraphicsIntegrityUncertain.load()) {
            return {helen::BatmanGraphicsApplyOutcome::IntegrityUncertain, {}};
        }
        if (state.DetailLevel != 4 && !ApplyDetailPresetToDraftState(state))
        {
            helen::Logf(L"[graphics] Apply failed: unsupported detailLevel=%d.", state.DetailLevel);
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }

        const std::filesystem::path user_ini_path = ini_path.parent_path() / "UserEngine.ini";
        std::unique_ptr<helen::FileWriteRoutingTransaction> trusted_transaction;
        if (routing_service != nullptr)
        {
            DWORD routing_error = ERROR_SUCCESS;
            trusted_transaction = routing_service->BeginTrustedWrite({ ini_path, user_ini_path }, routing_error);
            if (trusted_transaction == nullptr)
            {
                helen::Logf(L"[graphics] Apply failed before original read: trusted route transaction error=%lu.",
                    static_cast<unsigned long>(routing_error));
                return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
            }
        }

        const std::optional<std::string> original_engine_bytes = TryReadFileBytes(ini_path);
        const std::optional<IniTextDocument> existing_user_document = TryReadIniDocument(user_ini_path);
        if (!original_engine_bytes.has_value())
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            helen::Logf(L"[graphics] Apply failed: unable to read generated INI path=%ls.", ini_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }

        if (!existing_user_document.has_value())
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            helen::Logf(L"[graphics] Apply failed: unable to read launcher INI path=%ls.", user_ini_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }

        std::vector<std::string> engine_lines = SplitIniTextIntoLines(*original_engine_bytes);
        IniTextDocument user_document = *existing_user_document;
        std::wstring failed_setting;
        if (!TryApplyDraftStateToIniLines(state, engine_lines, failed_setting))
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            helen::Logf(L"[graphics] Apply failed: generated INI rejected setting=%ls path=%ls.", failed_setting.c_str(), ini_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }

        if (!TryApplyDraftStateToIniLines(state, user_document.Lines, failed_setting))
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            helen::Logf(L"[graphics] Apply failed: launcher INI rejected setting=%ls path=%ls.", failed_setting.c_str(), user_ini_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }

        IniTextDocument generated_document;
        generated_document.Lines = std::move(engine_lines);
        const std::optional<std::string> generated_bytes = TryEncodeIniDocument(generated_document);
        if (!generated_bytes.has_value())
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            helen::Logf(L"[graphics] Apply failed: unable to encode generated INI path=%ls.", ini_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }

        const std::optional<std::string> user_bytes = TryEncodeIniDocument(user_document);
        if (!user_bytes.has_value())
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            helen::Logf(L"[graphics] Apply failed: unable to encode launcher INI path=%ls.", user_ini_path.wstring().c_str());
            return {helen::BatmanGraphicsApplyOutcome::NotApplied, {}};
        }

        // Conservatively retain the process lockout if publication throws before returning its verified outcome.
        BatmanGraphicsIntegrityUncertain.store(true);
        try {
            helen::BatmanGraphicsApplyResult result = PublishBatmanGraphicsIniPair(file_operations,
                ini_path, user_ini_path, *generated_bytes, *user_bytes, *original_engine_bytes, user_document.RawBytes);
            if (trusted_transaction != nullptr)
            {
                DWORD synchronization_error = ERROR_SUCCESS;
                if (!trusted_transaction->Synchronize(synchronization_error))
                {
                    helen::Logf(L"[graphics] Apply original publication outcome=%d, but session synchronization failed error=%lu.",
                        static_cast<int>(result.Outcome), static_cast<unsigned long>(synchronization_error));
                    if (result.Outcome == helen::BatmanGraphicsApplyOutcome::Committed ||
                        result.Outcome == helen::BatmanGraphicsApplyOutcome::CommittedCleanupFailed)
                    {
                        BatmanGraphicsIntegrityUncertain.store(true);
                        return {helen::BatmanGraphicsApplyOutcome::CommittedSessionSyncFailed, std::move(result.RecoveryPaths)};
                    }

                    BatmanGraphicsIntegrityUncertain.store(true);
                    return {helen::BatmanGraphicsApplyOutcome::IntegrityUncertain, std::move(result.RecoveryPaths)};
                }
            }

            BatmanGraphicsIntegrityUncertain.store(result.Outcome == helen::BatmanGraphicsApplyOutcome::IntegrityUncertain);
            helen::Logf(L"[graphics] Apply completed outcome=%d vsync=%d.", static_cast<int>(result.Outcome), state.Vsync);
            return result;
        } catch (...) {
            if (trusted_transaction != nullptr)
            {
                DWORD synchronization_error = ERROR_SUCCESS;
                if (!trusted_transaction->Synchronize(synchronization_error))
                {
                    helen::Logf(L"[graphics] Apply exception left original/session state uncertain; synchronization error=%lu.",
                        static_cast<unsigned long>(synchronization_error));
                }
            }
            helen::Logf(L"[graphics] Apply integrity uncertain: exception during publication; retained transaction evidence.");
            BatmanGraphicsIntegrityUncertain.store(true);
            return {helen::BatmanGraphicsApplyOutcome::IntegrityUncertain, {}};
        }
    }

}

namespace helen
{
    /**
     * @brief Binds the service to one concrete `BmEngine.ini` anchor path.
     * @param ini_path Absolute or relative path to the Batman user engine INI file.
     * @param display_mode_service Required service that owns the current Batman display-mode catalog and revalidation.
     * @throws std::invalid_argument Thrown when `ini_path` is empty.
     */
    BatmanGraphicsConfigService::BatmanGraphicsConfigService(
        std::filesystem::path ini_path,
        BatmanDisplayModeService& display_mode_service)
        : BatmanGraphicsConfigService(std::move(ini_path), display_mode_service, BatmanGraphicsFileOperations::Native(), nullptr) {
    }

    BatmanGraphicsConfigService::BatmanGraphicsConfigService(
        std::filesystem::path ini_path, BatmanDisplayModeService& display_mode_service,
        BatmanGraphicsFileOperations& file_operations)
        : BatmanGraphicsConfigService(std::move(ini_path), display_mode_service, file_operations, nullptr) {
    }

    BatmanGraphicsConfigService::BatmanGraphicsConfigService(
        std::filesystem::path ini_path, BatmanDisplayModeService& display_mode_service,
        std::shared_ptr<FileWriteRoutingService> routing_service)
        : BatmanGraphicsConfigService(std::move(ini_path), display_mode_service, BatmanGraphicsFileOperations::Native(), std::move(routing_service)) {
    }

    BatmanGraphicsConfigService::BatmanGraphicsConfigService(
        std::filesystem::path ini_path, BatmanDisplayModeService& display_mode_service,
        BatmanGraphicsFileOperations& file_operations, std::shared_ptr<FileWriteRoutingService> routing_service)
        : ini_path_(std::move(ini_path)),
          display_mode_service_(display_mode_service),
          file_operations_(file_operations),
          routing_service_(std::move(routing_service))
    {
        if (ini_path_.empty())
        {
            throw std::invalid_argument("Batman graphics config service requires a non-empty INI path.");
        }
    }

    /** @brief Captures one launcher document without writing config; individual malformed fields remain absent. */
    BatmanGraphicsSnapshot BatmanGraphicsConfigService::CaptureReadSnapshot() const {
        const std::filesystem::path user_ini_path = ini_path_.parent_path() / "UserEngine.ini";
        const std::optional<IniTextDocument> document = TryReadIniDocument(user_ini_path);
        if (!document.has_value()) {
            Logf(L"[graphics] Snapshot failed: unable to read launcher INI path=%ls.", user_ini_path.wstring().c_str());
            return BatmanGraphicsSnapshot(BatmanGraphicsSnapshot::Values{});
        }
        return ReadGraphicsSnapshot(document->Lines);
    }

    /** @brief Loads registered legacy config keys only after the shared decoder supplies a complete valid draft. */
    bool BatmanGraphicsConfigService::LoadIntoDispatcher(CommandDispatcher& dispatcher) const
    {
        const std::filesystem::path user_ini_path = ini_path_.parent_path() / "UserEngine.ini";
        const std::optional<IniTextDocument> user_document = TryReadIniDocument(user_ini_path);
        if (!user_document.has_value())
        {
            Logf(L"[graphics] Load failed: unable to read launcher INI path=%ls.", user_ini_path.wstring().c_str());
            return false;
        }

        LegacyBatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromIniLines(user_document->Lines, state))
        {
            return false;
        }

        return TryWriteDraftStateToDispatcher(dispatcher, state);
    }

    /**
     * @brief Writes the current graphics draft into the generated and launcher-owned engine INI files.
     * @param dispatcher Config dispatcher that supplies the normalized graphics draft values.
     * @return True when both files contain every required setting and are written successfully; otherwise false.
     */
    BatmanGraphicsApplyResult BatmanGraphicsConfigService::ApplySessionDraft(const BatmanGraphicsDraftState& baseline,
        const BatmanGraphicsDraftState& draft) const {
        (void)baseline;
        return ApplyDraft(draft);
    }

    BatmanGraphicsApplyResult BatmanGraphicsConfigService::ApplyDraft(const BatmanGraphicsDraftState& draft) const {
        LegacyBatmanGraphicsDraftState state;
        state.Fullscreen = draft.Get(BatmanGraphicsField::Fullscreen);
        state.ResolutionWidth = draft.Get(BatmanGraphicsField::PersistedWidth);
        state.ResolutionHeight = draft.Get(BatmanGraphicsField::PersistedHeight);
        state.Vsync = draft.Get(BatmanGraphicsField::Vsync);
        state.Msaa = draft.Get(BatmanGraphicsField::Msaa);
        state.Bloom = draft.Get(BatmanGraphicsField::Bloom);
        state.DynamicShadows = draft.Get(BatmanGraphicsField::DynamicShadows);
        state.MotionBlur = draft.Get(BatmanGraphicsField::MotionBlur);
        state.Distortion = draft.Get(BatmanGraphicsField::Distortion);
        state.FogVolumes = draft.Get(BatmanGraphicsField::FogVolumes);
        state.SphericalHarmonicLighting = draft.Get(BatmanGraphicsField::SphericalHarmonicLighting);
        state.AmbientOcclusion = draft.Get(BatmanGraphicsField::AmbientOcclusion);
        state.Physx = draft.Get(BatmanGraphicsField::Physx);
        state.Stereo = draft.Get(BatmanGraphicsField::Stereo);
        const std::optional<BatmanGraphicsPresetDefinition> preset = TryResolvePresetFromDraft(state);
        state.DetailLevel = preset.has_value() ? preset->DetailLevel : 4;
        return ApplyGraphicsDraftState(file_operations_, ini_path_, routing_service_, state);
    }

    bool BatmanGraphicsConfigService::IsApplyLocked() const noexcept {
        return BatmanGraphicsIntegrityUncertain.load();
    }

    bool BatmanGraphicsConfigService::ApplyFromDispatcher(const CommandDispatcher& dispatcher) const {
        LegacyBatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromDispatcher(dispatcher, state)) {
            Logf(L"[graphics] Apply failed: missing dispatcher draft values.");
            return false;
        }
        return ApplyGraphicsDraftState(file_operations_, ini_path_, routing_service_, state).Outcome == BatmanGraphicsApplyOutcome::Committed;
    }

    /**
     * @brief Writes one normalized subtitle-size config value back into the resolved subtitle INI file.
     * @param dispatcher Config dispatcher that supplies `ui.subtitleSize`.
     * @return True when dispatcher values can be read and `Engine.HUD.ConsoleFontSize` is persisted.
     */
    bool BatmanGraphicsConfigService::ApplySubtitleSizeFromDispatcher(const CommandDispatcher& dispatcher) const
    {
        const std::lock_guard<std::mutex> transaction_lock(BatmanGraphicsApplyMutex);
        if (BatmanGraphicsIntegrityUncertain.load())
        {
            Logf(L"[subtitle] Apply rejected: graphics persistence is locked after an integrity failure.");
            return false;
        }

        int subtitle_size_state = 0;
        if (!TryReadDispatcherValue(dispatcher, "ui.subtitleSize", subtitle_size_state))
        {
            Logf(L"[subtitle] Apply failed: missing ui.subtitleSize in dispatcher.");
            return false;
        }

        int subtitle_font_size = 0;
        if (!TryMapSubtitleSizeStateToFontSize(subtitle_size_state, subtitle_font_size))
        {
            Logf(L"[subtitle] Apply failed: unsupported ui.subtitleSize=%d.", subtitle_size_state);
            return false;
        }

        std::filesystem::path subtitle_ini_path;
        if (!TryResolveSubtitleIniPathForWrite(ini_path_, subtitle_ini_path))
        {
            Logf(L"[subtitle] Apply failed: unable to resolve subtitle INI path from %ls or %ls/BmGame.ini.",
                ini_path_.wstring().c_str(),
                ini_path_.parent_path().wstring().c_str());
            return false;
        }

        std::unique_ptr<FileWriteRoutingTransaction> trusted_transaction;
        if (routing_service_ != nullptr)
        {
            DWORD routing_error = ERROR_SUCCESS;
            trusted_transaction = routing_service_->BeginTrustedWrite({ subtitle_ini_path }, routing_error);
            if (trusted_transaction == nullptr)
            {
                Logf(L"[subtitle] Apply rejected before original read: trusted route transaction error=%lu.",
                    static_cast<unsigned long>(routing_error));
                return false;
            }
        }

        std::filesystem::path verified_subtitle_ini_path;
        if (!TryResolveSubtitleIniPathForWrite(ini_path_, verified_subtitle_ini_path) ||
            verified_subtitle_ini_path != subtitle_ini_path)
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            Logf(L"[subtitle] Apply canceled: subtitle target changed during trusted transaction resolution.");
            return false;
        }

        const std::optional<std::vector<std::string>> existing_lines = TryReadAllLines(subtitle_ini_path);
        if (!existing_lines.has_value())
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            Logf(L"[subtitle] Apply failed: unable to read %ls before writing subtitle size.", subtitle_ini_path.wstring().c_str());
            return false;
        }

        std::vector<std::string> lines = *existing_lines;
        const std::string encoded_value = std::to_string(subtitle_font_size);
        if (!UpsertIniValue(lines, "Engine.HUD", "ConsoleFontSize", encoded_value))
        {
            if (trusted_transaction != nullptr)
            {
                trusted_transaction->CancelWithoutWrite();
            }
            Logf(L"[subtitle] Apply failed: could not write Engine.HUD.ConsoleFontSize into %ls.", subtitle_ini_path.wstring().c_str());
            return false;
        }

        const bool original_write_succeeded = WriteAllLines(subtitle_ini_path, lines);
        if (trusted_transaction != nullptr)
        {
            DWORD synchronization_error = ERROR_SUCCESS;
            if (!trusted_transaction->Synchronize(synchronization_error))
            {
                BatmanGraphicsIntegrityUncertain.store(true);
                Logf(L"[subtitle] Apply %s, but session synchronization failed error=%lu; persistence is locked.",
                    original_write_succeeded ? L"committed original" : L"failed original write",
                    static_cast<unsigned long>(synchronization_error));
                return false;
            }
        }

        if (!original_write_succeeded)
        {
            Logf(L"[subtitle] Apply failed: unable to write %ls.", subtitle_ini_path.wstring().c_str());
            return false;
        }

        Logf(L"[subtitle] Applied size=%hs from ui.subtitleSize=%d to Engine.HUD.ConsoleFontSize=%hs in %ls.",
            encoded_value.c_str(),
            subtitle_size_state,
            std::to_string(subtitle_font_size).c_str(),
            subtitle_ini_path.wstring().c_str());
        return true;
    }

    /**
     * @brief Recomputes the derived `detailLevel` draft state from the current individual detail toggles.
     * @param dispatcher Config dispatcher that stores the current Batman graphics draft.
     * @return True when the required draft keys exist and the derived `detailLevel` is updated successfully; otherwise false.
     */
    bool BatmanGraphicsConfigService::SyncDetailLevelFromDispatcher(CommandDispatcher& dispatcher) const
    {
        LegacyBatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromDispatcher(dispatcher, state))
        {
            return false;
        }

        const std::optional<BatmanGraphicsPresetDefinition> preset = TryResolvePresetFromDraft(state);
        state.DetailLevel = preset.has_value() ? preset->DetailLevel : 4;
        return TryWriteDispatcherValue(dispatcher, "detailLevel", state.DetailLevel);
    }

    /**
     * @brief Applies the currently selected `detailLevel` preset to the individual detail-toggle draft values.
     * @param dispatcher Config dispatcher that stores the current Batman graphics draft.
     * @return True when `detailLevel` resolves to a supported preset and the dependent draft values update successfully; otherwise false.
     */
    bool BatmanGraphicsConfigService::ApplySelectedDetailLevelToDispatcher(CommandDispatcher& dispatcher) const
    {
        LegacyBatmanGraphicsDraftState state;
        if (!TryReadDraftStateFromDispatcher(dispatcher, state))
        {
            return false;
        }

        if (!ApplyDetailPresetToDraftState(state))
        {
            return false;
        }

        return
            TryWriteDispatcherValue(dispatcher, "bloom", state.Bloom) &&
            TryWriteDispatcherValue(dispatcher, "dynamicShadows", state.DynamicShadows) &&
            TryWriteDispatcherValue(dispatcher, "motionBlur", state.MotionBlur) &&
            TryWriteDispatcherValue(dispatcher, "distortion", state.Distortion) &&
            TryWriteDispatcherValue(dispatcher, "fogVolumes", state.FogVolumes) &&
            TryWriteDispatcherValue(dispatcher, "sphericalHarmonicLighting", state.SphericalHarmonicLighting) &&
            TryWriteDispatcherValue(dispatcher, "ambientOcclusion", state.AmbientOcclusion);
    }

    /**
     * @brief Revalidates the selected display mode and atomically writes its exact dimensions into the graphics draft.
     * @param dispatcher Dispatcher containing resolutionModeIndex, resolutionWidth, and resolutionHeight.
     * @return True when the selected index remains supported and both draft dimensions update together; otherwise false with the prior pair retained.
     */
    bool BatmanGraphicsConfigService::ApplySelectedResolutionModeToDispatcher(CommandDispatcher& dispatcher)
    {
        const std::optional<int> index_value = dispatcher.TryGetInt("resolutionModeIndex");
        if (!index_value.has_value() || *index_value < 0)
        {
            return false;
        }

        const std::optional<int> fullscreen_value = dispatcher.TryGetInt("fullscreen");
        if (!fullscreen_value.has_value() || (*fullscreen_value != 0 && *fullscreen_value != 1))
        {
            return false;
        }
        const BatmanDisplayModeCatalogKind expected_catalog_kind = fullscreen_value.has_value() && *fullscreen_value != 0
            ? BatmanDisplayModeCatalogKind::Fullscreen
            : BatmanDisplayModeCatalogKind::Windowed;
        const std::optional<BatmanDisplayMode> selected_mode = display_mode_service_.GetModeCount(expected_catalog_kind) == 0
            ? std::nullopt
            : display_mode_service_.RevalidateMode(expected_catalog_kind, static_cast<std::size_t>(*index_value));
        if (!selected_mode.has_value())
        {
            return false;
        }

        return dispatcher.TrySetIntPair(
            "resolutionWidth",
            selected_mode->GetWidth(),
            "resolutionHeight",
            selected_mode->GetHeight());
    }

    /**
     * @brief Returns the `BmEngine.ini` anchor path used by this service.
     * @return Bound `BmEngine.ini` anchor path.
     */
    const std::filesystem::path& BatmanGraphicsConfigService::GetIniPath() const noexcept
    {
        return ini_path_;
    }

    /**
     * @brief Loads persisted subtitle size from INI and writes it into `ui.subtitleSize`.
     * @param dispatcher Config dispatcher that owns `ui.subtitleSize`.
     * @return True when INI read and conversion succeed, and dispatcher value is updated; otherwise false.
     */
    bool BatmanGraphicsConfigService::LoadSubtitleSizeIntoDispatcher(CommandDispatcher& dispatcher) const
    {
        std::filesystem::path subtitle_ini_path;
        std::vector<std::string> lines;
        if (!TryReadSubtitleIniLinesWithFontSize(ini_path_, subtitle_ini_path, lines))
        {
            const std::filesystem::path game_ini_path = ini_path_.parent_path() / "BmGame.ini";
            Logf(
                L"[subtitle] Load failed: missing or unsupported Engine.HUD.ConsoleFontSize in %ls or %ls.",
                ini_path_.wstring().c_str(),
                game_ini_path.wstring().c_str());
            return false;
        }

        int subtitle_size_state = 0;
        int subtitle_font_size = 0;
        if (!TryReadSubtitleSizeStateFromIniLines(lines, subtitle_size_state, subtitle_font_size))
        {
            Logf(L"[subtitle] Load failed: unsupported Engine.HUD.ConsoleFontSize in %ls.", subtitle_ini_path.wstring().c_str());
            return false;
        }

        if (!TryWriteDispatcherValue(dispatcher, "ui.subtitleSize", subtitle_size_state))
        {
            Logf(L"[subtitle] Load failed: missing ui.subtitleSize in dispatcher.");
            return false;
        }

        Logf(
            L"[subtitle] Loaded ui.subtitleSize=%d from Engine.HUD.ConsoleFontSize=%d in %ls.",
            subtitle_size_state,
            subtitle_font_size,
            subtitle_ini_path.wstring().c_str());
        return true;
    }
}
