using System.Globalization;
using System.Security.Cryptography;
using System.Text;
using System.Text.Json;
using System.Diagnostics;
using System.Runtime.InteropServices;

internal static class Program
{
    private const uint ImageBase = 0x0040_0000;
    private const uint BrightnessHookRva = 0x002B96E0;
    private const uint BrightnessReturnRva = BrightnessHookRva + 10;
    private const uint TextScaleHookRva = 0x006B00DA;
    private const uint TextScaleReturnRva = TextScaleHookRva + 9;
    private const uint CaveRva = 0x01A2_85E1;
    private const uint WorkerCaveRva = CaveRva + 0x100;
    private const int WorkerCaveLength = 0x091F;
    private const uint GlobalScaleConstOffset = 85;
    private const int GlobalTextScalePrimarySlotOffset = 2;
    private const int GlobalTextScaleSecondarySlotOpcodeOffset = 32;
    private const int GlobalTextScaleSecondarySlotOffset = 36;
    private const int GlobalTextScaleResumeJumpOpcodeOffset = 57;
    private const int GlobalTextScaleResumeRel32Offset = 58;
    private const int SubtitleSizeSmallCode = 4101;
    private const int SubtitleSizeMediumCode = 4102;
    private const int SubtitleSizeLargeCode = 4103;
    private const int SubtitleSizeVeryLargeCode = 4104;
    private const int SubtitleSizeHugeCode = 4105;
    private const int SubtitleSizeMassiveCode = 4106;
    private const int StateCurrentScaleOffset = 0x00;
    private const int StatePathReadyOffset = 0x04;
    private const int StateBurstCountOffset = StatePathReadyOffset;
    private const int StateLastPollTickOffset = 0x08;
    private const int StateFileBufferPtrOffset = 0x0C;
    private const int StateBytesReadOffset = 0x10;
    private const int StateFileHandleOffset = 0x14;
    private const int StateSmallScaleOffset = 0x18;
    private const int StateMediumScaleOffset = 0x1C;
    private const int StateLargeScaleOffset = 0x20;
    private const int StateVeryLargeScaleOffset = 0x24;
    private const int StateHugeScaleOffset = 0x28;
    private const int StateMassiveScaleOffset = 0x2C;
    private const int StatePathBufferOffset = 0x30;
    private const int StatePathBufferLength = 520;
    private const int StateDebugMagicOffset = StatePathBufferOffset + StatePathBufferLength;
    private const int StateDebugVersionOffset = StateDebugMagicOffset + 0x04;
    private const int StateLastSeenSignalCodeOffset = StateDebugVersionOffset + 0x04;
    private const int StateLastSeenSignalArg2Offset = StateLastSeenSignalCodeOffset + 0x04;
    private const int StateLastSeenHookRvaOffset = StateLastSeenSignalArg2Offset + 0x04;
    private const int StateSignalHitCountOffset = StateLastSeenHookRvaOffset + 0x04;
    private const int StateLastAppliedCodeOffset = StateSignalHitCountOffset + 0x04;
    private const int StateApplyCountOffset = StateLastAppliedCodeOffset + 0x04;
    private const int StateLastMethodPtrOffset = StateApplyCountOffset + 0x04;
    private const int StateLastMethodTextOffset = StateLastMethodPtrOffset + 0x04;
    private const int StateLastMethodTextLength = 32;
    private const int StateBlockLength = StateLastMethodTextOffset + StateLastMethodTextLength;
    private const int FxSaveBlockLength = 0x200;
    private const uint SubtitleDebugMagic = 0x53444248; // HDBS
    private const uint SubtitleDebugVersion = 0x00000001;
    private const uint BinkTextScaleReturnLowVa = 0x006B9C88;
    private const uint BinkTextScaleReturnHighVa = 0x006B9D7A;
    private const uint SubtitleUiStateScanStartVa = 0x1000_0000;
    private const uint SubtitleUiStateScanEndVa = 0x1200_0000;
    private const int SubtitleUiStateRescanIntervalMs = 50;
    private const int SubtitleUiStateMaxRegionsPerPass = 32;
    private const uint SubtitleUiStateScannerHookRva = 0xFFFF_FFFD;
    private const int MemoryBasicInformationLength = 28;
    private const int MbiBaseAddressOffset = 0x00;
    private const int MbiRegionSizeOffset = 0x0C;
    private const int MbiStateOffset = 0x10;
    private const int MbiProtectOffset = 0x14;
    private const uint MemCommit = 0x1000;
    private const uint PageNoAccess = 0x01;
    private const uint PageReadOnly = 0x02;
    private const uint PageReadWrite = 0x04;
    private const uint PageWriteCopy = 0x08;
    private const uint PageExecuteRead = 0x20;
    private const uint PageExecuteReadWrite = 0x40;
    private const uint PageExecuteWriteCopy = 0x80;
    private const uint PageGuard = 0x100;
    private static readonly byte[] ExpectedBrightnessHookBytes = Convert.FromHexString("8B4F088B118B420C6A01");
    private static readonly byte[] ExpectedTextScaleHookBytes = Convert.FromHexString("D9E8D9542404D91C24");
    private static readonly byte[] ExpectedSubtitleSetterHookBytes = Convert.FromHexString("8BC65E59C3");
    private static readonly byte[] ExpectedSubtitleSignalWrapperHookBytes = Convert.FromHexString("8B44240450B9");
    private static readonly byte[] ExpectedInvokeTraceHookBytes = Convert.FromHexString("83EC345357");
    private static readonly uint[] KnownRender3DTailRvas =
    [
        0x0000EFB4,
    ];
    private static readonly uint[] KnownInvokeTraceHookRvas =
    [
        0x015A4CD0,
        0x015A4E50,
    ];
    private static readonly uint[] KnownSubtitleSetterClusterRvas =
    [
        0x0000E893,
        0x0000E8E3,
        0x0000E933,
        0x0000E983,
        0x0000E9D3,
        0x0000EA23,
        0x0000EA73,
        0x0000EAC3,
        0x0000ED0B,
        0x0000ED56,
        0x0000EDA6,
        0x0000F009,
    ];
    private const int SubtitleSignalWrapperOverwrittenLength = 10;

    private static int Main(string[] args)
    {
        if (args.Length == 0)
        {
            PrintUsage();
            return 1;
        }

        try
        {
            string command = args[0];
            string[] tail = args[1..];

            return command switch
            {
                "patch-bink-subtitles" => RunPatchBinkSubtitles(tail),
                "patch-bink-text-scale" => RunPatchBinkTextScale(tail),
                "export-global-text-scale-blob" => RunExportGlobalTextScaleBlob(tail),
                "set-live-text-scale" => RunSetLiveTextScale(tail),
                "watch-live-text-scale" => RunWatchLiveTextScale(tail),
                "watch-live-subtitle-debug" => RunWatchLiveSubtitleDebug(tail),
                "dump-live-subtitle-debug" => RunDumpLiveSubtitleDebug(tail),
                "snapshot-live-subtitle-candidates" => RunSnapshotLiveSubtitleCandidates(tail),
                "diff-live-subtitle-candidates" => RunDiffLiveSubtitleCandidates(tail),
                "verify-bink-subtitles" => RunVerifyBinkSubtitles(tail),
                "verify-bink-text-scale" => RunVerifyBinkTextScale(tail),
                "help" or "--help" or "-h" => PrintHelpAndReturn(),
                _ => throw new InvalidOperationException($"Unknown command '{command}'.")
            };
        }
        catch (Exception ex)
        {
            Console.Error.WriteLine(ex.Message);
            return 2;
        }
    }

    private static int RunPatchBinkSubtitles(string[] args)
    {
        var options = new ArgumentReader(args);
        string exePath = Path.GetFullPath(options.RequireValue("--exe"));
        float scaleMultiplier = ParseFloat(options.GetValue("--scale-multiplier") ?? "3.0");
        bool writeBackup = options.GetFlag("--backup");
        string? backupPath = options.GetValue("--backup-path");
        options.ThrowIfAnyUnknown();

        byte[] bytes = File.ReadAllBytes(exePath);
        var sections = PeSection.ReadSections(bytes);

        int hookOffset = RvaToOffset(sections, BrightnessHookRva);
        int caveOffset = RvaToOffset(sections, CaveRva);

        EnsureOriginalHook(bytes, hookOffset, ExpectedBrightnessHookBytes, BrightnessHookRva);

        if (writeBackup)
        {
            string finalBackupPath = Path.GetFullPath(
                backupPath ?? $"{exePath}.bink-subtitle-scale-backup-{DateTime.Now:yyyyMMdd-HHmmss}");

            File.Copy(exePath, finalBackupPath, overwrite: false);
            Console.WriteLine($"Backup:          {finalBackupPath}");
        }

        byte[] cavePatch = BuildBrightnessCavePatch(scaleMultiplier);
        WriteCave(bytes, caveOffset, 0x100, cavePatch);
        WriteHook(bytes, hookOffset, CaveRva, ExpectedBrightnessHookBytes.Length, BrightnessHookRva);

        File.WriteAllBytes(exePath, bytes);

        Console.WriteLine($"Patched exe:     {exePath}");
        Console.WriteLine($"Scale multiplier:{scaleMultiplier.ToString("0.###", CultureInfo.InvariantCulture)}");
        Console.WriteLine($"Hook RVA:        0x{BrightnessHookRva:X8}");
        Console.WriteLine($"Cave RVA:        0x{CaveRva:X8}");
        Console.WriteLine($"SHA-256:         {ComputeSha256(exePath)}");
        return 0;
    }

    private static int RunPatchBinkTextScale(string[] args)
    {
        var options = new ArgumentReader(args);
        string exePath = Path.GetFullPath(options.RequireValue("--exe"));
        float scaleMultiplier = ParseFloat(options.GetValue("--scale-multiplier") ?? "2.0");
        bool global = options.GetFlag("--global");
        bool uiStateLive = options.GetFlag("--ui-state-live");
        bool internalIniLive = options.GetFlag("--internal-ini-live");
        bool subtitleSizeSignal = options.GetFlag("--subtitle-size-signal");
        bool subtitleTailDebugSignal = options.GetFlag("--subtitle-tail-debug-signal");
        bool render3DTailDebugSignal = options.GetFlag("--render3d-tail-debug-signal");
        bool invokeTrace = options.GetFlag("--invoke-trace");
        bool writeBackup = options.GetFlag("--backup");
        string? backupPath = options.GetValue("--backup-path");
        float smallScale = ParseFloat(options.GetValue("--small-scale") ?? "1.0");
        float mediumScale = ParseFloat(options.GetValue("--medium-scale") ?? "1.5");
        float largeScale = ParseFloat(options.GetValue("--large-scale") ?? "2.0");
        float veryLargeScale = ParseFloat(options.GetValue("--very-large-scale") ?? "4.0");
        float hugeScale = ParseFloat(options.GetValue("--huge-scale") ?? "6.0");
        float massiveScale = ParseFloat(options.GetValue("--massive-scale") ?? "8.0");
        int pollMs = ParseInt(options.GetValue("--poll-ms") ?? "250");
        options.ThrowIfAnyUnknown();

        if (uiStateLive && !global)
        {
            throw new InvalidOperationException("--ui-state-live currently requires --global.");
        }

        if (internalIniLive && !global)
        {
            throw new InvalidOperationException("--internal-ini-live currently requires --global.");
        }

        if (subtitleSizeSignal && !global)
        {
            throw new InvalidOperationException("--subtitle-size-signal currently requires --global.");
        }

        if (subtitleTailDebugSignal && !global)
        {
            throw new InvalidOperationException("--subtitle-tail-debug-signal currently requires --global.");
        }

        if (render3DTailDebugSignal && !global)
        {
            throw new InvalidOperationException("--render3d-tail-debug-signal currently requires --global.");
        }

        if ((uiStateLive && internalIniLive) ||
            (uiStateLive && subtitleSizeSignal) ||
            (uiStateLive && subtitleTailDebugSignal) ||
            (uiStateLive && render3DTailDebugSignal) ||
            (uiStateLive && invokeTrace) ||
            (internalIniLive && subtitleSizeSignal) ||
            (internalIniLive && subtitleTailDebugSignal) ||
            (internalIniLive && render3DTailDebugSignal) ||
            (internalIniLive && invokeTrace) ||
            (subtitleSizeSignal && subtitleTailDebugSignal) ||
            (subtitleSizeSignal && render3DTailDebugSignal) ||
            (subtitleSizeSignal && invokeTrace) ||
            (subtitleTailDebugSignal && render3DTailDebugSignal) ||
            (subtitleTailDebugSignal && invokeTrace) ||
            (render3DTailDebugSignal && invokeTrace))
        {
            throw new InvalidOperationException("--ui-state-live, --internal-ini-live, --subtitle-size-signal, --subtitle-tail-debug-signal, --render3d-tail-debug-signal, and --invoke-trace are mutually exclusive.");
        }

        byte[] bytes = File.ReadAllBytes(exePath);
        var sections = PeSection.ReadSections(bytes);

        int hookOffset = RvaToOffset(sections, TextScaleHookRva);
        int caveOffset = RvaToOffset(sections, CaveRva);
        int workerCaveOffset = RvaToOffset(sections, WorkerCaveRva);
        SignalWrapperHook[] subtitleSignalWrappers = subtitleSizeSignal
            ? FindSubtitleSignalWrappers(bytes, sections)
            : Array.Empty<SignalWrapperHook>();
        TailSignalHook[] subtitleTailSignalHooks = subtitleTailDebugSignal
            ? FindKnownSubtitleTailSignalHooks(bytes, sections)
            : Array.Empty<TailSignalHook>();
        TailSignalHook[] render3DTailSignalHooks = render3DTailDebugSignal
            ? FindKnownRender3DTailHooks(bytes, sections)
            : Array.Empty<TailSignalHook>();
        InvokeTraceHook[] invokeTraceHooks = invokeTrace
            ? FindKnownInvokeTraceHooks(bytes, sections)
            : Array.Empty<InvokeTraceHook>();
        if (subtitleSizeSignal && subtitleSignalWrappers.Length == 0)
        {
            throw new InvalidOperationException("Could not find any executable one-argument FE wrapper stubs to hook.");
        }
        if (subtitleTailDebugSignal && subtitleTailSignalHooks.Length == 0)
        {
            throw new InvalidOperationException("Could not find the expected FE setter tail cluster to hook.");
        }
        if (render3DTailDebugSignal && render3DTailSignalHooks.Length == 0)
        {
            throw new InvalidOperationException("Could not find the expected FE_SetRender3D tail candidate to hook.");
        }
        if (invokeTrace && invokeTraceHooks.Length == 0)
        {
            throw new InvalidOperationException("Could not find the expected Scaleform invoke entrypoints to hook.");
        }
        WritableStateBlock? stateBlock = null;
        byte[]? stateBlockBytes = null;
        WritableStateBlock? fxSaveBlock = null;
        uint? dynamicScaleVa = null;
        uint? dynamicFxSaveVa = null;
        bool callTextHelperWorker = uiStateLive || internalIniLive;

        EnsureOriginalHook(bytes, hookOffset, ExpectedTextScaleHookBytes, TextScaleHookRva);
        if (subtitleSignalWrappers.Length > 0)
        {
            for (int index = 0; index < subtitleSignalWrappers.Length; index++)
            {
                EnsureOriginalHook(
                    bytes,
                    subtitleSignalWrappers[index].HookOffset,
                    ExpectedSubtitleSignalWrapperHookBytes,
                    subtitleSignalWrappers[index].HookRva);
            }
        }
        if (subtitleTailSignalHooks.Length > 0)
        {
            for (int index = 0; index < subtitleTailSignalHooks.Length; index++)
            {
                EnsureOriginalHook(
                    bytes,
                    subtitleTailSignalHooks[index].HookOffset,
                    ExpectedSubtitleSetterHookBytes,
                    subtitleTailSignalHooks[index].HookRva);
            }
        }
        if (render3DTailSignalHooks.Length > 0)
        {
            for (int index = 0; index < render3DTailSignalHooks.Length; index++)
            {
                EnsureOriginalHook(
                    bytes,
                    render3DTailSignalHooks[index].HookOffset,
                    ExpectedSubtitleSetterHookBytes,
                    render3DTailSignalHooks[index].HookRva);
            }
        }
        if (invokeTraceHooks.Length > 0)
        {
            for (int index = 0; index < invokeTraceHooks.Length; index++)
            {
                EnsureOriginalHook(
                    bytes,
                    invokeTraceHooks[index].HookOffset,
                    ExpectedInvokeTraceHookBytes,
                    invokeTraceHooks[index].HookRva);
            }
        }

        if (writeBackup)
        {
            string finalBackupPath = Path.GetFullPath(
                backupPath ?? $"{exePath}.bink-text-scale-backup-{DateTime.Now:yyyyMMdd-HHmmss}");

            File.Copy(exePath, finalBackupPath, overwrite: false);
            Console.WriteLine($"Backup:          {finalBackupPath}");
        }

        if (uiStateLive || internalIniLive || subtitleSizeSignal || subtitleTailDebugSignal || render3DTailDebugSignal || invokeTrace)
        {
            stateBlockBytes = BuildDynamicScaleStateBlock(
                smallScale,
                mediumScale,
                largeScale,
                veryLargeScale,
                hugeScale,
                massiveScale);
            stateBlock = ReserveWritableStateBlock(bytes, sections, stateBlockBytes.Length);
            dynamicScaleVa = ImageBase + stateBlock.Value.Rva + StateCurrentScaleOffset;
            if (callTextHelperWorker)
            {
                fxSaveBlock = ReserveWritableStateBlock(bytes, sections, FxSaveBlockLength, stateBlock.Value);
                dynamicFxSaveVa = ImageBase + fxSaveBlock.Value.Rva;
            }
        }

        SubtitleSignalWrapperPatch? subtitleSignalPatch = null;
        byte[] workerCavePatch;
        if (uiStateLive)
        {
            workerCavePatch = BuildUiStateLiveWorkerCavePatch(
                bytes,
                sections,
                stateBlock!.Value);
        }
        else if (internalIniLive)
        {
            workerCavePatch = BuildTextScaleWorkerCavePatch(
                bytes,
                sections,
                stateBlock!.Value,
                new LiveIniSignalOptions(
                    SmallScale: smallScale,
                    MediumScale: mediumScale,
                    LargeScale: largeScale,
                    VeryLargeScale: veryLargeScale,
                    HugeScale: hugeScale,
                    MassiveScale: massiveScale,
                    PollMs: pollMs));
        }
        else if (subtitleSizeSignal)
        {
            subtitleSignalPatch = BuildSubtitleSignalWrapperCavePatch(
                stateBlock!.Value,
                subtitleSignalWrappers,
                new SubtitleSignalOptions(
                    SmallScale: smallScale,
                    MediumScale: mediumScale,
                    LargeScale: largeScale,
                    VeryLargeScale: veryLargeScale,
                    HugeScale: hugeScale,
                    MassiveScale: massiveScale));
            workerCavePatch = subtitleSignalPatch.Value.WorkerBytes;
        }
        else if (subtitleTailDebugSignal)
        {
            TailSignalPatch tailSignalPatch = BuildSubtitleTailSignalClusterPatch(
                stateBlock!.Value,
                subtitleTailSignalHooks,
                new SubtitleSignalOptions(
                    SmallScale: smallScale,
                    MediumScale: mediumScale,
                    LargeScale: largeScale,
                    VeryLargeScale: veryLargeScale,
                    HugeScale: hugeScale,
                    MassiveScale: massiveScale));
            workerCavePatch = tailSignalPatch.WorkerBytes;
            for (int index = 0; index < subtitleTailSignalHooks.Length; index++)
            {
                WriteHook(
                    bytes,
                    subtitleTailSignalHooks[index].HookOffset,
                    tailSignalPatch.EntryRvas[index],
                    ExpectedSubtitleSetterHookBytes.Length,
                    subtitleTailSignalHooks[index].HookRva);
            }
        }
        else if (render3DTailDebugSignal)
        {
            TailSignalPatch tailSignalPatch = BuildRender3DTailSignalPatch(
                stateBlock!.Value,
                render3DTailSignalHooks,
                new SubtitleSignalOptions(
                    SmallScale: smallScale,
                    MediumScale: mediumScale,
                    LargeScale: largeScale,
                    VeryLargeScale: veryLargeScale,
                    HugeScale: hugeScale,
                    MassiveScale: massiveScale));
            workerCavePatch = tailSignalPatch.WorkerBytes;
            for (int index = 0; index < render3DTailSignalHooks.Length; index++)
            {
                WriteHook(
                    bytes,
                    render3DTailSignalHooks[index].HookOffset,
                    tailSignalPatch.EntryRvas[index],
                    ExpectedSubtitleSetterHookBytes.Length,
                    render3DTailSignalHooks[index].HookRva);
            }
        }
        else if (invokeTrace)
        {
            TailSignalPatch invokeTracePatch = BuildInvokeTracePatch(
                stateBlock!.Value,
                invokeTraceHooks);
            workerCavePatch = invokeTracePatch.WorkerBytes;
            for (int index = 0; index < invokeTraceHooks.Length; index++)
            {
                WriteHook(
                    bytes,
                    invokeTraceHooks[index].HookOffset,
                    invokeTracePatch.EntryRvas[index],
                    ExpectedInvokeTraceHookBytes.Length,
                    invokeTraceHooks[index].HookRva);
            }
        }
        else
        {
            workerCavePatch = new byte[] { 0xC3 };
        }
        byte[] cavePatch = BuildTextScaleCavePatch(
            scaleMultiplier,
            global,
            callTextHelperWorker,
            WorkerCaveRva,
            dynamicScaleVa,
            dynamicFxSaveVa);
        WriteCave(bytes, caveOffset, 0x100, cavePatch);
        WriteCave(bytes, workerCaveOffset, WorkerCaveLength, workerCavePatch);
        if (stateBlock.HasValue && stateBlockBytes is not null)
        {
            WriteBlock(bytes, stateBlock.Value.Offset, stateBlockBytes, "state block");
        }
        WriteHook(bytes, hookOffset, CaveRva, ExpectedTextScaleHookBytes.Length, TextScaleHookRva);
        if (subtitleSignalWrappers.Length > 0)
        {
            for (int index = 0; index < subtitleSignalWrappers.Length; index++)
            {
                WriteHook(
                    bytes,
                    subtitleSignalWrappers[index].HookOffset,
                    subtitleSignalPatch!.Value.EntryRvas[index],
                    SubtitleSignalWrapperOverwrittenLength,
                    subtitleSignalWrappers[index].HookRva);
            }
        }

        File.WriteAllBytes(exePath, bytes);

        Console.WriteLine($"Patched exe:     {exePath}");
        Console.WriteLine($"Scale multiplier:{scaleMultiplier.ToString("0.###", CultureInfo.InvariantCulture)}");
        Console.WriteLine($"Scope:           {(global ? "global" : "filtered")}");
        Console.WriteLine($"Signal source:   {DescribeSignalSource(uiStateLive, internalIniLive, subtitleSizeSignal, subtitleTailDebugSignal, render3DTailDebugSignal, invokeTrace)}");
        Console.WriteLine($"Hook RVA:        0x{TextScaleHookRva:X8}");
        Console.WriteLine($"Cave RVA:        0x{CaveRva:X8}");
        if (global || subtitleSizeSignal)
        {
            Console.WriteLine($"Worker cave RVA: 0x{WorkerCaveRva:X8}");
        }
        if (stateBlock.HasValue)
        {
            Console.WriteLine($"State block RVA: 0x{stateBlock.Value.Rva:X8}");
        }
        if (fxSaveBlock.HasValue)
        {
            Console.WriteLine($"FXSAVE block RVA:0x{fxSaveBlock.Value.Rva:X8}");
        }
        if (subtitleSignalWrappers.Length > 0)
        {
            Console.WriteLine($"Signal hook count:{subtitleSignalWrappers.Length}");
            Console.WriteLine($"Signal hook RVAs:{Environment.NewLine}  {string.Join(Environment.NewLine + "  ", subtitleSignalWrappers.Select(wrapper => $"0x{wrapper.HookRva:X8}"))}");
        }
        if (subtitleTailSignalHooks.Length > 0)
        {
            Console.WriteLine($"Tail signal hook count:{subtitleTailSignalHooks.Length}");
            Console.WriteLine($"Tail signal hook RVAs:{Environment.NewLine}  {string.Join(Environment.NewLine + "  ", subtitleTailSignalHooks.Select(hook => $"0x{hook.HookRva:X8}"))}");
        }
        if (invokeTraceHooks.Length > 0)
        {
            Console.WriteLine($"Invoke trace hook count:{invokeTraceHooks.Length}");
            Console.WriteLine($"Invoke trace hook RVAs:{Environment.NewLine}  {string.Join(Environment.NewLine + "  ", invokeTraceHooks.Select(hook => $"0x{hook.HookRva:X8}"))}");
        }
        Console.WriteLine($"SHA-256:         {ComputeSha256(exePath)}");
        return 0;
    }

    private static int RunVerifyBinkSubtitles(string[] args)
    {
        var options = new ArgumentReader(args);
        string exePath = Path.GetFullPath(options.RequireValue("--exe"));
        options.ThrowIfAnyUnknown();

        byte[] bytes = File.ReadAllBytes(exePath);
        var sections = PeSection.ReadSections(bytes);
        int hookOffset = RvaToOffset(sections, BrightnessHookRva);
        int caveOffset = RvaToOffset(sections, CaveRva);

        bool hookInstalled = bytes[hookOffset] == 0xE9;
        byte[] cavePrefix = bytes[caveOffset..(caveOffset + 4)];
        bool caveInstalled = cavePrefix.SequenceEqual(Convert.FromHexString("F30F5905"));

        Console.WriteLine($"Hook installed:  {hookInstalled}");
        Console.WriteLine($"Cave installed:  {caveInstalled}");
        Console.WriteLine($"SHA-256:         {ComputeSha256(exePath)}");
        return hookInstalled && caveInstalled ? 0 : 1;
    }

    private static int RunVerifyBinkTextScale(string[] args)
    {
        var options = new ArgumentReader(args);
        string exePath = Path.GetFullPath(options.RequireValue("--exe"));
        options.ThrowIfAnyUnknown();

        byte[] bytes = File.ReadAllBytes(exePath);
        var sections = PeSection.ReadSections(bytes);
        int hookOffset = RvaToOffset(sections, TextScaleHookRva);
        int caveOffset = RvaToOffset(sections, CaveRva);

        bool hookInstalled = bytes[hookOffset] == 0xE9;
        byte[] filteredPrefix = bytes[caveOffset..(caveOffset + 3)];
        byte[] legacyGlobalPrefix = bytes[caveOffset..(caveOffset + 2)];
        byte[] currentGlobalPrefix = bytes[caveOffset..(caveOffset + 3)];
        byte[] internalLivePrefix = bytes[caveOffset..(caveOffset + 3)];
        bool caveInstalled =
            filteredPrefix.SequenceEqual(Convert.FromHexString("8B4D04")) ||
            legacyGlobalPrefix.SequenceEqual(Convert.FromHexString("D905")) ||
            currentGlobalPrefix.SequenceEqual(Convert.FromHexString("9C60E8")) ||
            internalLivePrefix.SequenceEqual(Convert.FromHexString("9C600F"));

        Console.WriteLine($"Hook installed:  {hookInstalled}");
        Console.WriteLine($"Cave installed:  {caveInstalled}");
        Console.WriteLine($"SHA-256:         {ComputeSha256(exePath)}");
        return hookInstalled && caveInstalled ? 0 : 1;
    }

    private static void ValidateExportedGlobalTextScaleBlobLayout(byte[] blob)
    {
        if (blob.Length <= GlobalTextScaleResumeRel32Offset + sizeof(uint) - 1)
        {
            throw new InvalidOperationException("Global text-scale blob is shorter than the declared relocation layout.");
        }

        if (blob[0] != 0xD9 || blob[1] != 0x05)
        {
            throw new InvalidOperationException("Global text-scale blob no longer starts with fld [abs].");
        }

        if (blob[GlobalTextScaleSecondarySlotOpcodeOffset] != 0xF3 ||
            blob[GlobalTextScaleSecondarySlotOpcodeOffset + 1] != 0x0F ||
            blob[GlobalTextScaleSecondarySlotOpcodeOffset + 2] != 0x59 ||
            blob[GlobalTextScaleSecondarySlotOpcodeOffset + 3] != 0x05)
        {
            throw new InvalidOperationException("Global text-scale blob no longer encodes mulss xmm0,[abs] at the expected location.");
        }

        if (blob[GlobalTextScaleResumeJumpOpcodeOffset] != 0xE9)
        {
            throw new InvalidOperationException("Global text-scale blob no longer encodes the resume jump at the expected location.");
        }
    }

    private static int RunExportGlobalTextScaleBlob(string[] args)
    {
        var options = new ArgumentReader(args);
        string outputPath = Path.GetFullPath(options.RequireValue("--output"));
        float scaleMultiplier = ParseFloat(options.GetValue("--scale-multiplier") ?? "1.5");
        options.ThrowIfAnyUnknown();

        byte[] blob = BuildGlobalTextScaleCavePatch(scaleMultiplier, 0u);
        ValidateExportedGlobalTextScaleBlobLayout(blob);
        Array.Clear(blob, GlobalTextScalePrimarySlotOffset, sizeof(uint));
        Array.Clear(blob, GlobalTextScaleSecondarySlotOffset, sizeof(uint));
        Array.Clear(blob, GlobalTextScaleResumeRel32Offset, sizeof(uint));

        string? outputDirectory = Path.GetDirectoryName(outputPath);
        if (string.IsNullOrWhiteSpace(outputDirectory))
        {
            throw new InvalidOperationException("Output path must include a directory.");
        }

        Directory.CreateDirectory(outputDirectory);
        File.WriteAllBytes(outputPath, blob);

        Console.WriteLine($"Exported blob:   {outputPath}");
        Console.WriteLine($"Blob length:     {blob.Length}");
        Console.WriteLine($"Scale multiplier:{scaleMultiplier.ToString("0.###", CultureInfo.InvariantCulture)}");
        Console.WriteLine($"Relocations:     runtime-slot@+{GlobalTextScalePrimarySlotOffset} abs32, runtime-slot@+{GlobalTextScaleSecondarySlotOffset} abs32, hook-resume@+{GlobalTextScaleResumeRel32Offset} rel32");
        return 0;
    }

    private static int RunSetLiveTextScale(string[] args)
    {
        var options = new ArgumentReader(args);
        string processName = options.GetValue("--process-name") ?? "ShippingPC-BmGame";
        float scaleMultiplier = ParseFloat(options.RequireValue("--scale-multiplier"));
        options.ThrowIfAnyUnknown();

        Process process = ResolveSingleProcess(processName);
        ApplyLiveScale(process, scaleMultiplier);

        Console.WriteLine($"Process:         {process.ProcessName} ({process.Id})");
        Console.WriteLine($"Scale multiplier:{scaleMultiplier.ToString("0.###", CultureInfo.InvariantCulture)}");
        Console.WriteLine($"Scale VA:        0x{GetScaleConstantAddress(process.MainModule!.BaseAddress):X8}");
        return 0;
    }

    private static int RunWatchLiveTextScale(string[] args)
    {
        var options = new ArgumentReader(args);
        string iniPath = Path.GetFullPath(options.RequireValue("--ini"));
        string processName = options.GetValue("--process-name") ?? "ShippingPC-BmGame";
        int pollMs = ParseInt(options.GetValue("--poll-ms") ?? "500");
        options.ThrowIfAnyUnknown();

        if (!File.Exists(iniPath))
        {
            throw new InvalidOperationException($"INI file not found: {iniPath}");
        }

        Console.WriteLine($"Watching:        {iniPath}");
        Console.WriteLine($"Process name:    {processName}");
        Console.WriteLine("Press Ctrl+C to stop.");

        int? lastConsoleFontSize = null;
        float? lastScale = null;
        int? lastPid = null;
        bool waitingForProcessLogged = false;

        while (true)
        {
            SubtitleScaleSignal signal = ReadSubtitleScaleSignal(iniPath);
            float scale = signal.Scale;

            Process? process = TryResolveSingleProcess(processName);
            if (process is not null)
            {
                waitingForProcessLogged = false;
                bool processChanged = lastPid != process.Id;
                bool scaleChanged = !lastScale.HasValue || Math.Abs(lastScale.Value - scale) > 0.0001f;
                IntPtr scaleAddress = GetScaleConstantAddress(process.MainModule!.BaseAddress);

                if (processChanged)
                {
                    Console.WriteLine(
                        $"[{DateTime.Now:HH:mm:ss}] attached pid={process.Id} process={process.ProcessName} scaleVA=0x{scaleAddress.ToInt64():X8}");
                }

                if (processChanged || scaleChanged || lastConsoleFontSize != signal.DisplayValue)
                {
                    ApplyLiveScale(process, scale);
                    Console.WriteLine(
                        $"[{DateTime.Now:HH:mm:ss}] apply pid={process.Id} signal={signal.DisplayValue} scale={scale.ToString("0.###", CultureInfo.InvariantCulture)} scaleVA=0x{scaleAddress.ToInt64():X8}");
                    lastPid = process.Id;
                    lastScale = scale;
                }
            }
            else if (lastPid.HasValue)
            {
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] process '{processName}' not running");
                lastPid = null;
                lastScale = null;
                waitingForProcessLogged = false;
            }
            else if (!waitingForProcessLogged)
            {
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] waiting for process '{processName}'");
                waitingForProcessLogged = true;
            }

            lastConsoleFontSize = signal.DisplayValue;
            Thread.Sleep(Math.Max(100, pollMs));
        }
    }

    private static int RunDumpLiveSubtitleDebug(string[] args)
    {
        var options = new ArgumentReader(args);
        string processName = options.GetValue("--process-name") ?? "ShippingPC-BmGame";
        options.ThrowIfAnyUnknown();

        Process process = ResolveSingleProcess(processName);
        SubtitleDebugSnapshot snapshot = ReadLiveSubtitleDebugSnapshot(process);
        PrintSubtitleDebugSnapshot(snapshot, includeTimestamp: false);
        return 0;
    }

    private static int RunWatchLiveSubtitleDebug(string[] args)
    {
        var options = new ArgumentReader(args);
        string processName = options.GetValue("--process-name") ?? "ShippingPC-BmGame";
        int intervalMs = ParseInt(options.GetValue("--interval-ms") ?? "250");
        options.ThrowIfAnyUnknown();

        Console.WriteLine($"Process name:    {processName}");
        Console.WriteLine("Press Ctrl+C to stop.");

        int? lastPid = null;
        SubtitleDebugSnapshot? lastSnapshot = null;
        bool waitingForProcessLogged = false;

        while (true)
        {
            Process? process = TryResolveSingleProcess(processName);
            if (process is not null)
            {
                waitingForProcessLogged = false;
                bool processChanged = lastPid != process.Id;
                try
                {
                    SubtitleDebugSnapshot snapshot = ReadLiveSubtitleDebugSnapshot(process);
                    if (processChanged)
                    {
                        Console.WriteLine(
                            $"[{DateTime.Now:HH:mm:ss}] attached pid={process.Id} process={process.ProcessName} stateVA=0x{snapshot.StateBlockVa:X8}");
                    }

                    if (processChanged || !lastSnapshot.HasValue || ShouldPrintSubtitleDebugSnapshot(lastSnapshot.Value, snapshot))
                    {
                        PrintSubtitleDebugSnapshot(snapshot, includeTimestamp: true);
                    }

                    lastSnapshot = snapshot;
                    lastPid = process.Id;
                }
                catch (Exception ex)
                {
                    Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] debug-read-failed pid={process.Id} error={ex.Message}");
                    lastPid = process.Id;
                    lastSnapshot = null;
                }
            }
            else if (lastPid.HasValue)
            {
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] process '{processName}' not running");
                lastPid = null;
                lastSnapshot = null;
                waitingForProcessLogged = false;
            }
            else if (!waitingForProcessLogged)
            {
                Console.WriteLine($"[{DateTime.Now:HH:mm:ss}] waiting for process '{processName}'");
                waitingForProcessLogged = true;
            }

            Thread.Sleep(Math.Max(100, intervalMs));
        }
    }

    private static int RunSnapshotLiveSubtitleCandidates(string[] args)
    {
        var options = new ArgumentReader(args);
        string processName = options.GetValue("--process-name") ?? "ShippingPC-BmGame";
        string outputPath = Path.GetFullPath(options.RequireValue("--output"));
        string valuesText = options.GetValue("--values") ?? "4101,4102,4103,4104,4105,4106";
        options.ThrowIfAnyUnknown();

        int[] values = ParseIntList(valuesText);
        Process process = ResolveSingleProcess(processName);
        CandidateSnapshot snapshot = SnapshotLiveInt32Candidates(process, values);

        Directory.CreateDirectory(Path.GetDirectoryName(outputPath)!);
        File.WriteAllText(outputPath, JsonSerializer.Serialize(snapshot, new JsonSerializerOptions { WriteIndented = true }));

        Console.WriteLine($"Process:         {process.ProcessName} ({process.Id})");
        Console.WriteLine($"Values:          {string.Join(", ", values)}");
        Console.WriteLine($"Candidates:      {snapshot.Candidates.Length}");
        Console.WriteLine($"Output:          {outputPath}");
        return 0;
    }

    private static int RunDiffLiveSubtitleCandidates(string[] args)
    {
        var options = new ArgumentReader(args);
        string beforePath = Path.GetFullPath(options.RequireValue("--before"));
        string afterPath = Path.GetFullPath(options.RequireValue("--after"));
        options.ThrowIfAnyUnknown();

        CandidateSnapshot before = JsonSerializer.Deserialize<CandidateSnapshot>(File.ReadAllText(beforePath))
            ?? throw new InvalidOperationException($"Could not parse snapshot: {beforePath}");
        CandidateSnapshot after = JsonSerializer.Deserialize<CandidateSnapshot>(File.ReadAllText(afterPath))
            ?? throw new InvalidOperationException($"Could not parse snapshot: {afterPath}");

        var beforeMap = before.Candidates.ToDictionary(candidate => candidate.Address, candidate => candidate.Value);
        var afterMap = after.Candidates.ToDictionary(candidate => candidate.Address, candidate => candidate.Value);

        CandidateEntry[] added = after.Candidates.Where(candidate => !beforeMap.ContainsKey(candidate.Address)).ToArray();
        CandidateEntry[] removed = before.Candidates.Where(candidate => !afterMap.ContainsKey(candidate.Address)).ToArray();
        (uint Address, int Before, int After)[] changed = after.Candidates
            .Where(candidate => beforeMap.TryGetValue(candidate.Address, out int beforeValue) && beforeValue != candidate.Value)
            .Select(candidate => (candidate.Address, beforeMap[candidate.Address], candidate.Value))
            .OrderBy(entry => entry.Address)
            .ToArray();

        Console.WriteLine($"Before:          {beforePath}");
        Console.WriteLine($"After:           {afterPath}");
        Console.WriteLine($"Added:           {added.Length}");
        foreach (CandidateEntry entry in added.Take(40))
        {
            Console.WriteLine($"  + 0x{entry.Address:X8} = {entry.Value} region=0x{entry.RegionBase:X8}+0x{entry.RegionSize:X}");
        }

        Console.WriteLine($"Removed:         {removed.Length}");
        foreach (CandidateEntry entry in removed.Take(40))
        {
            Console.WriteLine($"  - 0x{entry.Address:X8} = {entry.Value} region=0x{entry.RegionBase:X8}+0x{entry.RegionSize:X}");
        }

        Console.WriteLine($"Changed:         {changed.Length}");
        foreach ((uint address, int beforeValue, int afterValue) in changed.Take(80))
        {
            Console.WriteLine($"  * 0x{address:X8}: {beforeValue} -> {afterValue}");
        }

        return 0;
    }

    private static byte[] BuildBrightnessCavePatch(float scaleMultiplier)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream, Encoding.ASCII, leaveOpen: true);

        // mulss xmm0, dword ptr [abs caveConst]
        writer.Write((byte)0xF3);
        writer.Write((byte)0x0F);
        writer.Write((byte)0x59);
        writer.Write((byte)0x05);
        uint constVa = ImageBase + CaveRva + 23;
        writer.Write(constVa);

        // Replayed instructions from the overwritten hook block.
        writer.Write(ExpectedBrightnessHookBytes);

        // jmp back to the original function
        writer.Write((byte)0xE9);
        uint jumpBackSourceRva = CaveRva + 18;
        int jumpBackRel = unchecked((int)BrightnessReturnRva - unchecked((int)(jumpBackSourceRva + 5)));
        writer.Write(jumpBackRel);

        writer.Write(scaleMultiplier);
        return stream.ToArray();
    }

    private static byte[] BuildTextScaleCavePatch(
        float scaleMultiplier,
        bool global,
        bool callWorker,
        uint workerCaveRva,
        uint? scalePointerVa = null,
        uint? fxSaveVa = null)
    {
        if (global)
        {
            return callWorker
                ? BuildGlobalTextScaleWorkerCavePatch(scaleMultiplier, workerCaveRva, scalePointerVa, fxSaveVa)
                : BuildGlobalTextScaleCavePatch(scaleMultiplier, scalePointerVa);
        }

        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream, Encoding.ASCII, leaveOpen: true);

        writer.Write(Convert.FromHexString("8B4D04")); // mov ecx,[ebp+4]
        writer.Write(Convert.FromHexString("81F9889C6B00")); // cmp ecx,0x006B9C88
        writer.Write(Convert.FromHexString("720C")); // jb default
        writer.Write(Convert.FromHexString("81F97A9D6B00")); // cmp ecx,0x006B9D7A
        writer.Write(Convert.FromHexString("760E")); // jbe custom
        writer.Write(Convert.FromHexString("D9E8")); // fld1
        writer.Write(Convert.FromHexString("D9542404")); // fst dword ptr [esp+4]
        writer.Write(Convert.FromHexString("D91C24")); // fstp dword ptr [esp]
        writer.Write((byte)0xE9);
        uint defaultJumpSourceRva = CaveRva + 28;
        int defaultJumpRel = unchecked((int)TextScaleReturnRva - unchecked((int)(defaultJumpSourceRva + 5)));
        writer.Write(defaultJumpRel);
        writer.Write(Convert.FromHexString("D905")); // fld dword ptr [abs]
        uint constVa = ImageBase + CaveRva + 51;
        writer.Write(constVa);
        writer.Write(Convert.FromHexString("D9542404")); // fst dword ptr [esp+4]
        writer.Write(Convert.FromHexString("D91C24")); // fstp dword ptr [esp]
        writer.Write((byte)0xE9);
        uint scaledJumpSourceRva = CaveRva + 46;
        int scaledJumpRel = unchecked((int)TextScaleReturnRva - unchecked((int)(scaledJumpSourceRva + 5)));
        writer.Write(scaledJumpRel);

        writer.Write(scaleMultiplier);
        return stream.ToArray();
    }

    private static byte[] BuildGlobalTextScaleCavePatch(float scaleMultiplier, uint? scalePointerVa = null)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream, Encoding.ASCII, leaveOpen: true);

        writer.Write(Convert.FromHexString("D905")); // fld dword ptr [abs]
        uint constVa = scalePointerVa ?? (ImageBase + CaveRva + GlobalScaleConstOffset);
        writer.Write(constVa);
        writer.Write(Convert.FromHexString("D9542404")); // fst dword ptr [esp+4]
        writer.Write(Convert.FromHexString("D91C24")); // fstp dword ptr [esp]
        writer.Write(Convert.FromHexString("D944242C")); // fld dword ptr [esp+2c]
        writer.Write((byte)0x52); // push edx
        writer.Write((byte)0x99); // cdq
        writer.Write((byte)0x57); // push edi
        writer.Write(Convert.FromHexString("2BC2")); // sub eax,edx
        writer.Write((byte)0x56); // push esi
        writer.Write(Convert.FromHexString("D1F8")); // sar eax,1
        writer.Write(Convert.FromHexString("83EC08")); // sub esp,8
        writer.Write(Convert.FromHexString("F30F2AC0")); // cvtsi2ss xmm0,eax
        writer.Write(Convert.FromHexString("F30F5905")); // mulss xmm0,[abs]
        writer.Write(constVa);
        writer.Write(Convert.FromHexString("8B442438")); // mov eax,[esp+38]
        writer.Write(Convert.FromHexString("D95C2404")); // fstp dword ptr [esp+4]
        writer.Write(Convert.FromHexString("F30F5CC8")); // subss xmm1,xmm0
        writer.Write(Convert.FromHexString("F30F110C24")); // movss dword ptr [esp],xmm1
        writer.Write((byte)0xE9);
        uint jumpBackSourceRva = CaveRva + 57;
        int jumpBackRel = unchecked((int)0x006B0107 - unchecked((int)(jumpBackSourceRva + 5)));
        writer.Write(jumpBackRel);

        while (stream.Position < GlobalScaleConstOffset)
        {
            writer.Write((byte)0x90);
        }

        if (stream.Position != GlobalScaleConstOffset)
        {
            throw new InvalidOperationException("Global text-scale cave overflowed the reserved constant slot.");
        }

        writer.Write(scaleMultiplier);
        return stream.ToArray();
    }

    private static byte[] BuildGlobalTextScaleWorkerCavePatch(
        float scaleMultiplier,
        uint workerCaveRva,
        uint? scalePointerVa = null,
        uint? fxSaveVa = null)
    {
        using var stream = new MemoryStream();
        using var writer = new BinaryWriter(stream, Encoding.ASCII, leaveOpen: true);

        uint saveAreaVa = fxSaveVa ?? throw new InvalidOperationException("Global worker cave requires a writable FXSAVE area.");
        uint constVa = scalePointerVa ?? (ImageBase + CaveRva + GlobalScaleConstOffset);

        writer.Write((byte)0x9C); // pushfd
        writer.Write((byte)0x60); // pushad
        writer.Write(Convert.FromHexString("0FAE05")); // fxsave [abs]
        writer.Write(saveAreaVa);
        writer.Write((byte)0xE8); // call worker
        uint workerCallSourceRva = CaveRva + 9;
        int workerCallRel = unchecked((int)workerCaveRva - unchecked((int)(workerCallSourceRva + 5)));
        writer.Write(workerCallRel);
        writer.Write(Convert.FromHexString("0FAE0D")); // fxrstor [abs]
        writer.Write(saveAreaVa);
        writer.Write((byte)0x61); // popad
        writer.Write((byte)0x9D); // popfd

        writer.Write(Convert.FromHexString("D905")); // fld dword ptr [abs]
        writer.Write(constVa);
        writer.Write(Convert.FromHexString("D9542404")); // fst dword ptr [esp+4]
        writer.Write(Convert.FromHexString("D91C24")); // fstp dword ptr [esp]
        writer.Write(Convert.FromHexString("D944242C")); // fld dword ptr [esp+2c]
        writer.Write((byte)0x52); // push edx
        writer.Write((byte)0x99); // cdq
        writer.Write((byte)0x57); // push edi
        writer.Write(Convert.FromHexString("2BC2")); // sub eax,edx
        writer.Write((byte)0x56); // push esi
        writer.Write(Convert.FromHexString("D1F8")); // sar eax,1
        writer.Write(Convert.FromHexString("83EC08")); // sub esp,8
        writer.Write(Convert.FromHexString("F30F2AC0")); // cvtsi2ss xmm0,eax
        writer.Write(Convert.FromHexString("F30F5905")); // mulss xmm0,[abs]
        writer.Write(constVa);
        writer.Write(Convert.FromHexString("8B442438")); // mov eax,[esp+38]
        writer.Write(Convert.FromHexString("D95C2404")); // fstp dword ptr [esp+4]
        writer.Write(Convert.FromHexString("F30F5CC8")); // subss xmm1,xmm0
        writer.Write(Convert.FromHexString("F30F110C24")); // movss dword ptr [esp],xmm1
        writer.Write((byte)0xE9);
        uint jumpBackSourceRva = CaveRva + 80;
        int jumpBackRel = unchecked((int)0x006B0107 - unchecked((int)(jumpBackSourceRva + 5)));
        writer.Write(jumpBackRel);
        writer.Write(scaleMultiplier);
        return stream.ToArray();
    }

    private static byte[] BuildTextScaleWorkerCavePatch(
        byte[] bytes,
        IReadOnlyList<PeSection> sections,
        WritableStateBlock stateBlock,
        LiveIniSignalOptions config)
    {

        uint getTickCountIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "GetTickCount");
        uint createFileWIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "CreateFileW");
        uint readFileIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "ReadFile");
        uint closeHandleIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "CloseHandle");
        uint localAllocIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "LocalAlloc");
        uint shGetFolderPathWIatVa = ResolveImportIatVa(bytes, sections, "SHELL32.dll", "SHGetFolderPathW");

        const uint fileBufferSize = 0x0001_0000;

        string allowKey = "bAllowMatureLanguage=";
        string kismetKey = "bEnableKismetLogging=";
        string suffix = "\\Square Enix\\Batman Arkham Asylum GOTY\\BmGame\\Config\\BmEngine.ini";
        uint currentScaleVa = ImageBase + stateBlock.Rva + StateCurrentScaleOffset;
        uint pathReadyVa = ImageBase + stateBlock.Rva + StatePathReadyOffset;
        uint lastPollTickVa = ImageBase + stateBlock.Rva + StateLastPollTickOffset;
        uint fileBufferPtrVa = ImageBase + stateBlock.Rva + StateFileBufferPtrOffset;
        uint bytesReadVa = ImageBase + stateBlock.Rva + StateBytesReadOffset;
        uint fileHandleVa = ImageBase + stateBlock.Rva + StateFileHandleOffset;
        uint pathBufferVa = ImageBase + stateBlock.Rva + StatePathBufferOffset;
        uint smallScaleVa = ImageBase + stateBlock.Rva + StateSmallScaleOffset;
        uint mediumScaleVa = ImageBase + stateBlock.Rva + StateMediumScaleOffset;
        uint largeScaleVa = ImageBase + stateBlock.Rva + StateLargeScaleOffset;
        uint veryLargeScaleVa = ImageBase + stateBlock.Rva + StateVeryLargeScaleOffset;
        uint hugeScaleVa = ImageBase + stateBlock.Rva + StateHugeScaleOffset;
        uint massiveScaleVa = ImageBase + stateBlock.Rva + StateMassiveScaleOffset;

        var code = new X86Builder(ImageBase, WorkerCaveRva);

        code.Label("entry");
        code.Emit(0xFC); // cld
        code.Emit(0x80, 0x3D);
        code.EmitUInt32(pathReadyVa);
        code.Emit(0x00); // cmp byte ptr [path_ready],0
        code.EmitJccNear(0x85, "check_tick"); // jne
        code.EmitCallLabel("init_state");

        code.Label("check_tick");
        code.EmitCallImport(getTickCountIatVa);
        code.Emit(0x89, 0xC2); // mov edx,eax
        code.Emit(0xA1);
        code.EmitUInt32(lastPollTickVa);
        code.Emit(0x2B, 0x15);
        code.EmitUInt32(lastPollTickVa); // sub edx,[last_poll_tick]
        code.Emit(0x81, 0xFA);
        code.EmitInt32(config.PollMs); // cmp edx,pollMs
        code.EmitJccNear(0x82, "ret"); // jb
        code.EmitCallImport(getTickCountIatVa);
        code.Emit(0xA3);
        code.EmitUInt32(lastPollTickVa);

        code.Emit(0x80, 0x3D);
        code.EmitUInt32(pathReadyVa);
        code.Emit(0x00); // cmp byte ptr [path_ready],0
        code.EmitJccNear(0x84, "ret"); // je

        code.Emit(0x8B, 0x3D);
        code.EmitUInt32(fileBufferPtrVa); // mov edi,[file_buffer_ptr]
        code.Emit(0x85, 0xFF); // test edi,edi
        code.EmitJccNear(0x84, "ret"); // je

        code.Emit(0xC7, 0x05);
        code.EmitUInt32(bytesReadVa);
        code.EmitUInt32(0); // bytes_read = 0

        code.EmitPushImm32(0);
        code.EmitPushImm32(0x80);
        code.EmitPushImm32(3);
        code.EmitPushImm32(0);
        code.EmitPushImm32(7);
        code.EmitPushImm32(0x8000_0000);
        code.EmitPushImm32(pathBufferVa);
        code.EmitCallImport(createFileWIatVa);
        code.Emit(0x83, 0xF8, 0xFF); // cmp eax,-1
        code.EmitJccNear(0x84, "ret"); // je

        code.Emit(0xA3);
        code.EmitUInt32(fileHandleVa);
        code.EmitPushImm32(0);
        code.EmitPushImm32(bytesReadVa);
        code.EmitPushImm32(fileBufferSize);
        code.Emit(0xFF, 0x35);
        code.EmitUInt32(fileBufferPtrVa); // push dword ptr [file_buffer_ptr]
        code.Emit(0x50); // push eax
        code.EmitCallImport(readFileIatVa);

        code.Emit(0xA1);
        code.EmitUInt32(fileHandleVa);
        code.Emit(0x50); // push eax
        code.EmitCallImport(closeHandleIatVa);

        code.Emit(0x83, 0x3D);
        code.EmitUInt32(bytesReadVa);
        code.Emit(0x00); // cmp dword ptr [bytes_read],0
        code.EmitJccNear(0x84, "ret"); // je

        code.Emit(0xA1);
        code.EmitUInt32(mediumScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);

        code.Emit(0x8B, 0x35);
        code.EmitUInt32(fileBufferPtrVa); // mov esi,[file_buffer_ptr]
        code.Emit(0x8B, 0x0D);
        code.EmitUInt32(bytesReadVa); // mov ecx,[bytes_read]
        code.EmitMovEdiLabelAddress("kismet_key");
        code.Emit(0xBA);
        code.EmitInt32(Encoding.ASCII.GetByteCount(kismetKey)); // mov edx,keyLen
        code.EmitCallLabel("find_last_true");
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x85, "apply_large"); // jne

        code.Emit(0x8B, 0x35);
        code.EmitUInt32(fileBufferPtrVa); // mov esi,[file_buffer_ptr]
        code.Emit(0x8B, 0x0D);
        code.EmitUInt32(bytesReadVa); // mov ecx,[bytes_read]
        code.EmitMovEdiLabelAddress("allow_key");
        code.Emit(0xBA);
        code.EmitInt32(Encoding.ASCII.GetByteCount(allowKey)); // mov edx,keyLen
        code.EmitCallLabel("find_last_true");
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x85, "apply_small"); // jne
        code.EmitJmpLabel("ret");

        code.Label("apply_small");
        code.Emit(0xA1);
        code.EmitUInt32(smallScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.EmitJmpLabel("ret");

        code.Label("apply_large");
        code.Emit(0xA1);
        code.EmitUInt32(largeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.EmitJmpLabel("ret");

        code.Label("find_last_true");
        code.Emit(0x89, 0xF5); // mov ebp,esi
        code.Emit(0x39, 0xD1); // cmp ecx,edx
        code.EmitJccNear(0x86, "find_not_found"); // jbe
        code.Emit(0x8D, 0x5C, 0x0E, 0xFF); // lea ebx,[esi+ecx-1]
        code.Emit(0x29, 0xD3); // sub ebx,edx

        code.Label("find_loop");
        code.Emit(0x51); // push ecx
        code.Emit(0x57); // push edi
        code.Emit(0x56); // push esi
        code.Emit(0x89, 0xD1); // mov ecx,edx
        code.Emit(0x89, 0xDE); // mov esi,ebx
        code.Emit(0xF3, 0xA6); // repe cmpsb
        code.Emit(0x5E); // pop esi
        code.Emit(0x5F); // pop edi
        code.Emit(0x59); // pop ecx
        code.EmitJccNear(0x84, "find_found"); // je
        code.Emit(0x4B); // dec ebx
        code.Emit(0x39, 0xEB); // cmp ebx,ebp
        code.EmitJccNear(0x83, "find_loop"); // jae

        code.Label("find_not_found");
        code.Emit(0x31, 0xC0); // xor eax,eax
        code.Emit(0xC3); // ret

        code.Label("find_found");
        code.Emit(0x8A, 0x04, 0x13); // mov al,[ebx+edx]
        code.Emit(0x3C, 0x31); // cmp al,'1'
        code.EmitJccNear(0x84, "find_true");
        code.Emit(0x3C, 0x54); // cmp al,'T'
        code.EmitJccNear(0x84, "find_true");
        code.Emit(0x3C, 0x74); // cmp al,'t'
        code.EmitJccNear(0x84, "find_true");
        code.Emit(0x31, 0xC0); // xor eax,eax
        code.Emit(0xC3); // ret

        code.Label("find_true");
        code.Emit(0xB8);
        code.EmitUInt32(1); // mov eax,1
        code.Emit(0xC3); // ret

        code.Label("init_state");
        code.Emit(0xA1);
        code.EmitUInt32(fileBufferPtrVa);
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x85, "init_path"); // jne
        code.EmitPushImm32(fileBufferSize);
        code.EmitPushImm32(0x40);
        code.EmitCallImport(localAllocIatVa);
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x84, "init_path"); // je
        code.Emit(0xA3);
        code.EmitUInt32(fileBufferPtrVa);

        code.Label("init_path");
        code.EmitPushImm32(pathBufferVa);
        code.EmitPushImm32(0);
        code.EmitPushImm32(0);
        code.EmitPushImm32(5);
        code.EmitPushImm32(0);
        code.EmitCallImport(shGetFolderPathWIatVa);
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x85, "ret"); // jne

        code.Emit(0xBF);
        code.EmitUInt32(pathBufferVa);
        code.Label("find_path_end");
        code.Emit(0x66, 0x83, 0x3F, 0x00); // cmp word ptr [edi],0
        code.EmitJccNear(0x84, "copy_suffix"); // je
        code.Emit(0x83, 0xC7, 0x02); // add edi,2
        code.EmitJmpLabel("find_path_end");

        code.Label("copy_suffix");
        code.EmitMovEsiLabelAddress("path_suffix");
        code.Label("copy_suffix_loop");
        code.Emit(0x66, 0x8B, 0x06); // mov ax,[esi]
        code.Emit(0x66, 0x89, 0x07); // mov [edi],ax
        code.Emit(0x83, 0xC6, 0x02); // add esi,2
        code.Emit(0x83, 0xC7, 0x02); // add edi,2
        code.Emit(0x66, 0x85, 0xC0); // test ax,ax
        code.EmitJccNear(0x85, "copy_suffix_loop"); // jne
        code.Emit(0xC6, 0x05);
        code.EmitUInt32(pathReadyVa);
        code.Emit(0x01); // mov byte ptr [path_ready],1
        code.Emit(0xC3); // ret

        code.Label("ret");
        code.Emit(0xC3); // ret
        code.Label("path_suffix");
        code.Emit(Encoding.Unicode.GetBytes(suffix + "\0"));
        code.Label("allow_key");
        code.Emit(Encoding.ASCII.GetBytes(allowKey));
        code.Label("kismet_key");
        code.Emit(Encoding.ASCII.GetBytes(kismetKey));

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"Worker cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return worker;
    }

    private static byte[] BuildUiStateLiveWorkerCavePatch(
        byte[] bytes,
        IReadOnlyList<PeSection> sections,
        WritableStateBlock stateBlock)
    {
        uint getTickCountIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "GetTickCount");
        uint virtualQueryIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "VirtualQuery");
        uint currentScaleVa = ImageBase + stateBlock.Rva + StateCurrentScaleOffset;
        uint scanCursorVa = ImageBase + stateBlock.Rva + StateLastPollTickOffset;
        uint cachedPtrVa = ImageBase + stateBlock.Rva + StateFileBufferPtrOffset;
        uint lastScanTickVa = ImageBase + stateBlock.Rva + StateBytesReadOffset;
        uint smallScaleVa = ImageBase + stateBlock.Rva + StateSmallScaleOffset;
        uint mediumScaleVa = ImageBase + stateBlock.Rva + StateMediumScaleOffset;
        uint largeScaleVa = ImageBase + stateBlock.Rva + StateLargeScaleOffset;
        uint mbiBufferVa = ImageBase + stateBlock.Rva + StatePathBufferOffset;
        uint mbiBaseVa = mbiBufferVa + MbiBaseAddressOffset;
        uint mbiRegionSizeVa = mbiBufferVa + MbiRegionSizeOffset;
        uint mbiStateVa = mbiBufferVa + MbiStateOffset;
        uint mbiProtectVa = mbiBufferVa + MbiProtectOffset;
        uint lastSeenSignalCodeVa = ImageBase + stateBlock.Rva + StateLastSeenSignalCodeOffset;
        uint lastSeenSignalArg2Va = ImageBase + stateBlock.Rva + StateLastSeenSignalArg2Offset;
        uint lastSeenHookRvaVa = ImageBase + stateBlock.Rva + StateLastSeenHookRvaOffset;
        uint signalHitCountVa = ImageBase + stateBlock.Rva + StateSignalHitCountOffset;
        uint lastAppliedCodeVa = ImageBase + stateBlock.Rva + StateLastAppliedCodeOffset;
        uint applyCountVa = ImageBase + stateBlock.Rva + StateApplyCountOffset;

        var code = new X86Builder(ImageBase, WorkerCaveRva);

        code.Label("entry");
        code.Emit(0x8B, 0x3D);
        code.EmitUInt32(cachedPtrVa); // mov edi,[cached_ptr]
        code.Emit(0x85, 0xFF); // test edi,edi
        code.EmitJccNear(0x84, "start_scan"); // je
        code.EmitPushImm32((uint)MemoryBasicInformationLength);
        code.EmitPushImm32(mbiBufferVa);
        code.Emit(0x57); // push edi
        code.EmitCallImport(virtualQueryIatVa);
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x84, "clear_cached"); // je
        EmitReadableCommittedRegionCheck(code, mbiStateVa, mbiProtectVa, "cached_region_ok", "clear_cached");

        code.Label("cached_region_ok");
        code.Emit(0xA1);
        code.EmitUInt32(mbiBaseVa); // mov eax,[mbi.base]
        code.Emit(0x8D, 0x50, 0x10); // lea edx,[eax+10h]
        code.Emit(0x3B, 0xFA); // cmp edi,edx
        code.EmitJccNear(0x82, "clear_cached"); // jb
        code.Emit(0xA1);
        code.EmitUInt32(mbiBaseVa); // mov eax,[mbi.base]
        code.Emit(0x03, 0x05);
        code.EmitUInt32(mbiRegionSizeVa); // add eax,[mbi.region_size]
        code.Emit(0x8D, 0x57, 0x20); // lea edx,[edi+20h]
        code.Emit(0x3B, 0xC2); // cmp eax,edx
        code.EmitJccNear(0x86, "clear_cached"); // jbe
        code.EmitCallLabel("validate_candidate");
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x85, "have_code"); // jne

        code.Label("clear_cached");
        code.Emit(0xC7, 0x05);
        code.EmitUInt32(cachedPtrVa);
        code.EmitUInt32(0); // cached_ptr = 0

        code.EmitCallImport(getTickCountIatVa);
        code.Emit(0x89, 0xC2); // mov edx,eax
        code.Emit(0x8B, 0x0D);
        code.EmitUInt32(lastScanTickVa); // mov ecx,[last_scan_tick]
        code.Emit(0x2B, 0xD1); // sub edx,ecx
        code.Emit(0x81, 0xFA);
        code.EmitInt32(SubtitleUiStateRescanIntervalMs); // cmp edx,rescanIntervalMs
        code.EmitJccNear(0x82, "ret"); // jb
        code.Emit(0xA3);
        code.EmitUInt32(lastScanTickVa); // mov [last_scan_tick],eax

        code.Label("start_scan");
        code.Emit(0x8B, 0x35);
        code.EmitUInt32(scanCursorVa); // mov esi,[scan_cursor]
        code.Emit(0x81, 0xFE);
        code.EmitUInt32(SubtitleUiStateScanStartVa); // cmp esi,start
        code.EmitJccNear(0x83, "cursor_clamped"); // jae
        code.Emit(0xBE);
        code.EmitUInt32(SubtitleUiStateScanStartVa); // mov esi,start

        code.Label("cursor_clamped");
        code.Emit(0x81, 0xFE);
        code.EmitUInt32(SubtitleUiStateScanEndVa); // cmp esi,end
        code.EmitJccNear(0x82, "scan_budget_ready"); // jb
        code.Emit(0xBE);
        code.EmitUInt32(SubtitleUiStateScanStartVa); // mov esi,start

        code.Label("scan_budget_ready");
        code.Emit(0xBD);
        code.EmitUInt32(SubtitleUiStateMaxRegionsPerPass); // mov ebp,maxRegionsPerPass

        code.Label("query_region");
        code.EmitPushImm32((uint)MemoryBasicInformationLength);
        code.EmitPushImm32(mbiBufferVa);
        code.Emit(0x56); // push esi
        code.EmitCallImport(virtualQueryIatVa);
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x85, "region_ok"); // jne
        code.Emit(0xC7, 0x05);
        code.EmitUInt32(scanCursorVa);
        code.EmitUInt32(SubtitleUiStateScanStartVa);
        code.EmitJmpLabel("ret");

        code.Label("region_ok");
        code.Emit(0xA1);
        code.EmitUInt32(mbiBaseVa); // mov eax,[mbi.base]
        code.Emit(0x89, 0xC2); // mov edx,eax
        code.Emit(0x03, 0x15);
        code.EmitUInt32(mbiRegionSizeVa); // add edx,[mbi.region_size]
        code.Emit(0x3B, 0xD6); // cmp edx,esi
        code.EmitJccNear(0x87, "next_cursor_ok"); // ja
        code.Emit(0x8D, 0x96);
        code.EmitUInt32(0x0000_1000); // lea edx,[esi+1000h]

        code.Label("next_cursor_ok");
        code.Emit(0x81, 0xFA);
        code.EmitUInt32(SubtitleUiStateScanEndVa); // cmp edx,end
        code.EmitJccNear(0x82, "store_next_cursor"); // jb
        code.Emit(0xBA);
        code.EmitUInt32(SubtitleUiStateScanStartVa); // mov edx,start

        code.Label("store_next_cursor");
        code.Emit(0x89, 0x15);
        code.EmitUInt32(scanCursorVa); // mov [scan_cursor],edx
        EmitReadableCommittedRegionCheck(code, mbiStateVa, mbiProtectVa, "region_readable", "next_region");

        code.Label("region_readable");
        code.Emit(0x8B, 0x3D);
        code.EmitUInt32(mbiBaseVa); // mov edi,[mbi.base]
        code.Emit(0x83, 0xC7, 0x10); // add edi,10h
        code.Emit(0x81, 0xFF);
        code.EmitUInt32(SubtitleUiStateScanStartVa + 0x10); // cmp edi,start+10h
        code.EmitJccNear(0x83, "scan_base_ok"); // jae
        code.Emit(0xBF);
        code.EmitUInt32(SubtitleUiStateScanStartVa + 0x10); // mov edi,start+10h

        code.Label("scan_base_ok");
        code.Emit(0x8B, 0x1D);
        code.EmitUInt32(mbiBaseVa); // mov ebx,[mbi.base]
        code.Emit(0x03, 0x1D);
        code.EmitUInt32(mbiRegionSizeVa); // add ebx,[mbi.region_size]
        code.Emit(0x81, 0xFB);
        code.EmitUInt32(SubtitleUiStateScanEndVa); // cmp ebx,end
        code.EmitJccNear(0x86, "scan_end_clamped"); // jbe
        code.Emit(0xBB);
        code.EmitUInt32(SubtitleUiStateScanEndVa); // mov ebx,end

        code.Label("scan_end_clamped");
        code.Emit(0x83, 0xEB, 0x20); // sub ebx,20h
        code.Emit(0x3B, 0xFB); // cmp edi,ebx
        code.EmitJccNear(0x87, "next_region"); // ja

        code.Label("scan_loop");
        code.EmitCallLabel("validate_candidate");
        code.Emit(0x85, 0xC0); // test eax,eax
        code.EmitJccNear(0x85, "found_candidate"); // jne
        code.Emit(0x83, 0xC7, 0x04); // add edi,4
        code.Emit(0x3B, 0xFB); // cmp edi,ebx
        code.EmitJccNear(0x86, "scan_loop"); // jbe
        code.EmitJmpLabel("next_region");

        code.Label("next_region");
        code.Emit(0x4D); // dec ebp
        code.EmitJccNear(0x85, "query_region"); // jne
        code.EmitJmpLabel("ret");

        code.Label("found_candidate");
        code.Emit(0x89, 0x3D);
        code.EmitUInt32(cachedPtrVa); // mov [cached_ptr],edi

        code.Label("have_code");
        code.Emit(0xA3);
        code.EmitUInt32(lastSeenSignalCodeVa); // mov [last_code],eax
        code.Emit(0x89, 0xF8); // mov eax,edi
        code.Emit(0xA3);
        code.EmitUInt32(lastSeenSignalArg2Va); // mov [last_arg2],eax
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleUiStateScannerHookRva); // mov eax,scanner_hook
        code.Emit(0xA3);
        code.EmitUInt32(lastSeenHookRvaVa); // mov [last_hook],eax
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(signalHitCountVa); // inc [hit_count]
        code.Emit(0xA1);
        code.EmitUInt32(lastSeenSignalCodeVa); // mov eax,[last_code]
        code.Emit(0x8B, 0x15);
        code.EmitUInt32(lastAppliedCodeVa); // mov edx,[last_applied]
        code.Emit(0x3B, 0xD0); // cmp edx,eax
        code.EmitJccNear(0x84, "map_scale"); // je
        code.Emit(0xA3);
        code.EmitUInt32(lastAppliedCodeVa); // mov [last_applied],eax
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa); // inc [apply_count]

        code.Label("map_scale");
        code.Emit(0x3D);
        code.EmitUInt32(SubtitleSizeSmallCode); // cmp eax,small
        code.EmitJccNear(0x84, "apply_small"); // je
        code.Emit(0x3D);
        code.EmitUInt32(SubtitleSizeMediumCode); // cmp eax,normal
        code.EmitJccNear(0x84, "apply_medium"); // je

        code.Label("apply_large");
        code.Emit(0xA1);
        code.EmitUInt32(largeScaleVa); // mov eax,[large_scale]
        code.Emit(0xA3);
        code.EmitUInt32(currentScaleVa); // mov [current_scale],eax
        code.EmitJmpLabel("ret");

        code.Label("apply_small");
        code.Emit(0xA1);
        code.EmitUInt32(smallScaleVa); // mov eax,[small_scale]
        code.Emit(0xA3);
        code.EmitUInt32(currentScaleVa); // mov [current_scale],eax
        code.EmitJmpLabel("ret");

        code.Label("apply_medium");
        code.Emit(0xA1);
        code.EmitUInt32(mediumScaleVa); // mov eax,[normal_scale]
        code.Emit(0xA3);
        code.EmitUInt32(currentScaleVa); // mov [current_scale],eax
        code.EmitJmpLabel("ret");

        code.Label("validate_candidate");
        EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xF0), 50, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xF4), 100, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xF8), 100, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, unchecked((sbyte)0xFC), 100, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, 0x00, SubtitleSizeMediumCode, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, 0x04, 1, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, 0x08, 0, "invalid_candidate");
        code.Emit(0x8B, 0x47, 0x0C); // mov eax,[edi+0Ch]
        code.Emit(0x3D);
        code.EmitUInt32(SubtitleSizeSmallCode); // cmp eax,small
        code.EmitJccNear(0x84, "candidate_tail"); // je
        code.Emit(0x3D);
        code.EmitUInt32(SubtitleSizeMediumCode); // cmp eax,normal
        code.EmitJccNear(0x84, "candidate_tail"); // je
        code.Emit(0x3D);
        code.EmitUInt32(SubtitleSizeLargeCode); // cmp eax,large
        code.EmitJccNear(0x85, "invalid_candidate"); // jne

        code.Label("candidate_tail");
        EmitCompareDwordPtrEdiDisp32(code, 0x10, SubtitleSizeMediumCode, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, 0x14, 2, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, 0x1C, 3, "invalid_candidate");
        EmitCompareDwordPtrEdiDisp32(code, 0x20, 3, "invalid_candidate");
        code.Emit(0xC3); // ret

        code.Label("invalid_candidate");
        code.Emit(0x31, 0xC0); // xor eax,eax
        code.Emit(0xC3); // ret

        code.Label("ret");
        code.Emit(0xC3); // ret

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"UI-state worker cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return worker;
    }

    private static byte[] BuildSubtitleSignalWorkerCavePatch(
        byte[] bytes,
        IReadOnlyList<PeSection> sections,
        WritableStateBlock stateBlock,
        SubtitleSignalOptions config)
    {
        _ = bytes;
        _ = sections;
        _ = config;

        uint currentScaleVa = ImageBase + stateBlock.Rva + StateCurrentScaleOffset;
        uint smallScaleVa = ImageBase + stateBlock.Rva + StateSmallScaleOffset;
        uint mediumScaleVa = ImageBase + stateBlock.Rva + StateMediumScaleOffset;
        uint largeScaleVa = ImageBase + stateBlock.Rva + StateLargeScaleOffset;
        uint veryLargeScaleVa = ImageBase + stateBlock.Rva + StateVeryLargeScaleOffset;
        uint hugeScaleVa = ImageBase + stateBlock.Rva + StateHugeScaleOffset;
        uint massiveScaleVa = ImageBase + stateBlock.Rva + StateMassiveScaleOffset;
        uint lastSeenSignalCodeVa = ImageBase + stateBlock.Rva + StateLastSeenSignalCodeOffset;
        uint lastSeenSignalArg2Va = ImageBase + stateBlock.Rva + StateLastSeenSignalArg2Offset;
        uint lastSeenHookRvaVa = ImageBase + stateBlock.Rva + StateLastSeenHookRvaOffset;
        uint signalHitCountVa = ImageBase + stateBlock.Rva + StateSignalHitCountOffset;
        uint lastAppliedCodeVa = ImageBase + stateBlock.Rva + StateLastAppliedCodeOffset;
        uint applyCountVa = ImageBase + stateBlock.Rva + StateApplyCountOffset;

        var code = new X86Builder(ImageBase, WorkerCaveRva);

        code.Label("entry");
        code.Emit(0x9C); // pushfd
        code.Emit(0x60); // pushad
        code.Emit(0x89, 0x35);
        code.EmitUInt32(lastSeenSignalCodeVa);
        code.Emit(0xC7, 0x05);
        code.EmitUInt32(lastSeenSignalArg2Va);
        code.EmitUInt32(0);
        code.Emit(0xB8);
        code.EmitUInt32(0xFFFF_FFFE);
        code.EmitMovAbsLabelEaxAbsolute(lastSeenHookRvaVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(signalHitCountVa);
        code.EmitCallLabel("apply_scale_from_signal");
        code.Emit(0x61); // popad
        code.Emit(0x9D); // popfd
        code.Emit(0x8B, 0xC6); // mov eax,esi
        code.Emit(0x5E); // pop esi
        code.Emit(0x59); // pop ecx
        code.Emit(0xC3); // ret

        code.Label("apply_scale_from_signal");
        code.Emit(0x81, 0xFE); // cmp esi,smallCode
        code.EmitInt32(SubtitleSizeSmallCode);
        code.EmitJccNear(0x84, "apply_small"); // je
        code.Emit(0x81, 0xFE); // cmp esi,mediumCode
        code.EmitInt32(SubtitleSizeMediumCode);
        code.EmitJccNear(0x84, "apply_medium"); // je
        code.Emit(0x81, 0xFE); // cmp esi,largeCode
        code.EmitInt32(SubtitleSizeLargeCode);
        code.EmitJccNear(0x84, "apply_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,veryLargeCode
        code.EmitInt32(SubtitleSizeVeryLargeCode);
        code.EmitJccNear(0x84, "apply_very_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,hugeCode
        code.EmitInt32(SubtitleSizeHugeCode);
        code.EmitJccNear(0x84, "apply_huge"); // je
        code.Emit(0x81, 0xFE); // cmp esi,massiveCode
        code.EmitInt32(SubtitleSizeMassiveCode);
        code.EmitJccNear(0x84, "apply_massive"); // je
        code.EmitJmpLabel("ret");

        code.Label("apply_small");
        code.Emit(0xA1);
        code.EmitUInt32(smallScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeSmallCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("ret");

        code.Label("ret");
        code.Emit(0xC3); // ret

        code.Label("apply_medium");
        code.Emit(0xA1);
        code.EmitUInt32(mediumScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMediumCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("ret");

        code.Label("apply_large");
        code.Emit(0xA1);
        code.EmitUInt32(largeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("ret");

        code.Label("apply_very_large");
        code.Emit(0xA1);
        code.EmitUInt32(veryLargeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeVeryLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("ret");

        code.Label("apply_huge");
        code.Emit(0xA1);
        code.EmitUInt32(hugeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeHugeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("ret");

        code.Label("apply_massive");
        code.Emit(0xA1);
        code.EmitUInt32(massiveScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMassiveCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("ret");

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"Worker cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return worker;
    }

    private static SubtitleSignalWrapperPatch BuildSubtitleSignalWrapperCavePatch(
        WritableStateBlock stateBlock,
        IReadOnlyList<SignalWrapperHook> wrappers,
        SubtitleSignalOptions config)
    {
        _ = config;

        uint currentScaleVa = ImageBase + stateBlock.Rva + StateCurrentScaleOffset;
        uint smallScaleVa = ImageBase + stateBlock.Rva + StateSmallScaleOffset;
        uint mediumScaleVa = ImageBase + stateBlock.Rva + StateMediumScaleOffset;
        uint largeScaleVa = ImageBase + stateBlock.Rva + StateLargeScaleOffset;
        uint veryLargeScaleVa = ImageBase + stateBlock.Rva + StateVeryLargeScaleOffset;
        uint hugeScaleVa = ImageBase + stateBlock.Rva + StateHugeScaleOffset;
        uint massiveScaleVa = ImageBase + stateBlock.Rva + StateMassiveScaleOffset;
        uint lastSeenSignalCodeVa = ImageBase + stateBlock.Rva + StateLastSeenSignalCodeOffset;
        uint lastSeenSignalArg2Va = ImageBase + stateBlock.Rva + StateLastSeenSignalArg2Offset;
        uint lastSeenHookRvaVa = ImageBase + stateBlock.Rva + StateLastSeenHookRvaOffset;
        uint signalHitCountVa = ImageBase + stateBlock.Rva + StateSignalHitCountOffset;
        uint lastAppliedCodeVa = ImageBase + stateBlock.Rva + StateLastAppliedCodeOffset;
        uint applyCountVa = ImageBase + stateBlock.Rva + StateApplyCountOffset;

        var code = new X86Builder(ImageBase, WorkerCaveRva);
        var entryRvas = new List<uint>(wrappers.Count);

        code.Label("apply_scale_from_signal");
        code.Emit(0x81, 0xFE); // cmp esi,smallCode
        code.EmitInt32(SubtitleSizeSmallCode);
        code.EmitJccNear(0x84, "apply_small"); // je
        code.Emit(0x81, 0xFE); // cmp esi,mediumCode
        code.EmitInt32(SubtitleSizeMediumCode);
        code.EmitJccNear(0x84, "apply_medium"); // je
        code.Emit(0x81, 0xFE); // cmp esi,largeCode
        code.EmitInt32(SubtitleSizeLargeCode);
        code.EmitJccNear(0x84, "apply_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,veryLargeCode
        code.EmitInt32(SubtitleSizeVeryLargeCode);
        code.EmitJccNear(0x84, "apply_very_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,hugeCode
        code.EmitInt32(SubtitleSizeHugeCode);
        code.EmitJccNear(0x84, "apply_huge"); // je
        code.Emit(0x81, 0xFE); // cmp esi,massiveCode
        code.EmitInt32(SubtitleSizeMassiveCode);
        code.EmitJccNear(0x84, "apply_massive"); // je
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_small");
        code.Emit(0xA1);
        code.EmitUInt32(smallScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeSmallCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_medium");
        code.Emit(0xA1);
        code.EmitUInt32(mediumScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMediumCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_large");
        code.Emit(0xA1);
        code.EmitUInt32(largeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_very_large");
        code.Emit(0xA1);
        code.EmitUInt32(veryLargeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeVeryLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_huge");
        code.Emit(0xA1);
        code.EmitUInt32(hugeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeHugeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_massive");
        code.Emit(0xA1);
        code.EmitUInt32(massiveScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMassiveCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);

        code.Label("apply_ret");
        code.Emit(0xC3); // ret

        for (int index = 0; index < wrappers.Count; index++)
        {
            SignalWrapperHook wrapper = wrappers[index];
            entryRvas.Add(WorkerCaveRva + (uint)code.Position);

            code.Label($"entry_{index}");
            code.Emit(0x9C); // pushfd
            code.Emit(0x60); // pushad
            code.Emit(0x8B, 0x74, 0x24, 0x28); // mov esi,[esp+28]
            code.Emit(0x89, 0x35);
            code.EmitUInt32(lastSeenSignalCodeVa);
            code.Emit(0xC7, 0x05);
            code.EmitUInt32(lastSeenSignalArg2Va);
            code.EmitUInt32(0);
            code.Emit(0xB8);
            code.EmitUInt32(wrapper.HookRva);
            code.EmitMovAbsLabelEaxAbsolute(lastSeenHookRvaVa);
            code.Emit(0xFF, 0x05);
            code.EmitUInt32(signalHitCountVa);
            code.EmitCallLabel("apply_scale_from_signal");
            code.Emit(0x61); // popad
            code.Emit(0x9D); // popfd
            code.Emit(0x8B, 0x44, 0x24, 0x04); // mov eax,[esp+4]
            code.Emit(0x50); // push eax
            code.Emit(0xB9); // mov ecx,<function object>
            code.EmitUInt32(wrapper.FunctionObjectVa);
            code.EmitJmpRva(wrapper.HookRva + SubtitleSignalWrapperOverwrittenLength);
        }

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"Wrapper signal cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return new SubtitleSignalWrapperPatch(worker, entryRvas.ToArray());
    }

    private static TailSignalPatch BuildSubtitleTailSignalClusterPatch(
        WritableStateBlock stateBlock,
        IReadOnlyList<TailSignalHook> hooks,
        SubtitleSignalOptions config)
    {
        _ = config;

        uint currentScaleVa = ImageBase + stateBlock.Rva + StateCurrentScaleOffset;
        uint smallScaleVa = ImageBase + stateBlock.Rva + StateSmallScaleOffset;
        uint mediumScaleVa = ImageBase + stateBlock.Rva + StateMediumScaleOffset;
        uint largeScaleVa = ImageBase + stateBlock.Rva + StateLargeScaleOffset;
        uint veryLargeScaleVa = ImageBase + stateBlock.Rva + StateVeryLargeScaleOffset;
        uint hugeScaleVa = ImageBase + stateBlock.Rva + StateHugeScaleOffset;
        uint massiveScaleVa = ImageBase + stateBlock.Rva + StateMassiveScaleOffset;
        uint lastSeenSignalCodeVa = ImageBase + stateBlock.Rva + StateLastSeenSignalCodeOffset;
        uint lastSeenSignalArg2Va = ImageBase + stateBlock.Rva + StateLastSeenSignalArg2Offset;
        uint lastSeenHookRvaVa = ImageBase + stateBlock.Rva + StateLastSeenHookRvaOffset;
        uint signalHitCountVa = ImageBase + stateBlock.Rva + StateSignalHitCountOffset;
        uint lastAppliedCodeVa = ImageBase + stateBlock.Rva + StateLastAppliedCodeOffset;
        uint applyCountVa = ImageBase + stateBlock.Rva + StateApplyCountOffset;

        var code = new X86Builder(ImageBase, WorkerCaveRva);
        var entryRvas = new List<uint>(hooks.Count);

        code.Label("apply_scale_from_signal");
        code.Emit(0x81, 0xFE); // cmp esi,smallCode
        code.EmitInt32(SubtitleSizeSmallCode);
        code.EmitJccNear(0x84, "apply_small"); // je
        code.Emit(0x81, 0xFE); // cmp esi,mediumCode
        code.EmitInt32(SubtitleSizeMediumCode);
        code.EmitJccNear(0x84, "apply_medium"); // je
        code.Emit(0x81, 0xFE); // cmp esi,largeCode
        code.EmitInt32(SubtitleSizeLargeCode);
        code.EmitJccNear(0x84, "apply_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,veryLargeCode
        code.EmitInt32(SubtitleSizeVeryLargeCode);
        code.EmitJccNear(0x84, "apply_very_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,hugeCode
        code.EmitInt32(SubtitleSizeHugeCode);
        code.EmitJccNear(0x84, "apply_huge"); // je
        code.Emit(0x81, 0xFE); // cmp esi,massiveCode
        code.EmitInt32(SubtitleSizeMassiveCode);
        code.EmitJccNear(0x84, "apply_massive"); // je
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_small");
        code.Emit(0xA1);
        code.EmitUInt32(smallScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeSmallCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_medium");
        code.Emit(0xA1);
        code.EmitUInt32(mediumScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMediumCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_large");
        code.Emit(0xA1);
        code.EmitUInt32(largeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_very_large");
        code.Emit(0xA1);
        code.EmitUInt32(veryLargeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeVeryLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_huge");
        code.Emit(0xA1);
        code.EmitUInt32(hugeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeHugeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_massive");
        code.Emit(0xA1);
        code.EmitUInt32(massiveScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMassiveCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);

        code.Label("apply_ret");
        code.Emit(0xC3); // ret

        for (int index = 0; index < hooks.Count; index++)
        {
            TailSignalHook hook = hooks[index];
            entryRvas.Add(WorkerCaveRva + (uint)code.Position);

            code.Label($"entry_tail_{index}");
            code.Emit(0x9C); // pushfd
            code.Emit(0x60); // pushad
            code.Emit(0x8B, 0x44, 0x24, 0x30); // mov eax,[esp+30] -> original [esp+0C]
            code.EmitMovAbsLabelEaxAbsolute(lastSeenSignalCodeVa);
            code.Emit(0x8B, 0x44, 0x24, 0x34); // mov eax,[esp+34] -> original [esp+10]
            code.EmitMovAbsLabelEaxAbsolute(lastSeenSignalArg2Va);
            code.Emit(0xB8);
            code.EmitUInt32(hook.HookRva);
            code.EmitMovAbsLabelEaxAbsolute(lastSeenHookRvaVa);
            code.Emit(0xFF, 0x05);
            code.EmitUInt32(signalHitCountVa);
            code.EmitCallLabel("apply_scale_from_signal");
            code.Emit(0x61); // popad
            code.Emit(0x9D); // popfd
            code.Emit(0x8B, 0xC6); // mov eax,esi
            code.Emit(0x5E); // pop esi
            code.Emit(0x59); // pop ecx
            code.Emit(0xC3); // ret
        }

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"Tail signal cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return new TailSignalPatch(worker, entryRvas.ToArray());
    }

    private static TailSignalPatch BuildRender3DTailSignalPatch(
        WritableStateBlock stateBlock,
        IReadOnlyList<TailSignalHook> hooks,
        SubtitleSignalOptions config)
    {
        _ = config;

        uint currentScaleVa = ImageBase + stateBlock.Rva + StateCurrentScaleOffset;
        uint smallScaleVa = ImageBase + stateBlock.Rva + StateSmallScaleOffset;
        uint mediumScaleVa = ImageBase + stateBlock.Rva + StateMediumScaleOffset;
        uint largeScaleVa = ImageBase + stateBlock.Rva + StateLargeScaleOffset;
        uint veryLargeScaleVa = ImageBase + stateBlock.Rva + StateVeryLargeScaleOffset;
        uint hugeScaleVa = ImageBase + stateBlock.Rva + StateHugeScaleOffset;
        uint massiveScaleVa = ImageBase + stateBlock.Rva + StateMassiveScaleOffset;
        uint lastSeenSignalCodeVa = ImageBase + stateBlock.Rva + StateLastSeenSignalCodeOffset;
        uint lastSeenSignalArg2Va = ImageBase + stateBlock.Rva + StateLastSeenSignalArg2Offset;
        uint lastSeenHookRvaVa = ImageBase + stateBlock.Rva + StateLastSeenHookRvaOffset;
        uint signalHitCountVa = ImageBase + stateBlock.Rva + StateSignalHitCountOffset;
        uint lastAppliedCodeVa = ImageBase + stateBlock.Rva + StateLastAppliedCodeOffset;
        uint applyCountVa = ImageBase + stateBlock.Rva + StateApplyCountOffset;

        int smallBits = BitConverter.SingleToInt32Bits(SubtitleSizeSmallCode);
        int mediumBits = BitConverter.SingleToInt32Bits(SubtitleSizeMediumCode);
        int largeBits = BitConverter.SingleToInt32Bits(SubtitleSizeLargeCode);
        int veryLargeBits = BitConverter.SingleToInt32Bits(SubtitleSizeVeryLargeCode);
        int hugeBits = BitConverter.SingleToInt32Bits(SubtitleSizeHugeCode);
        int massiveBits = BitConverter.SingleToInt32Bits(SubtitleSizeMassiveCode);

        var code = new X86Builder(ImageBase, WorkerCaveRva);
        var entryRvas = new List<uint>(hooks.Count);

        code.Label("apply_scale_from_signal");
        code.Emit(0x81, 0xFE); // cmp esi,smallBits
        code.EmitInt32(smallBits);
        code.EmitJccNear(0x84, "apply_small"); // je
        code.Emit(0x81, 0xFE); // cmp esi,mediumBits
        code.EmitInt32(mediumBits);
        code.EmitJccNear(0x84, "apply_medium"); // je
        code.Emit(0x81, 0xFE); // cmp esi,largeBits
        code.EmitInt32(largeBits);
        code.EmitJccNear(0x84, "apply_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,veryLargeBits
        code.EmitInt32(veryLargeBits);
        code.EmitJccNear(0x84, "apply_very_large"); // je
        code.Emit(0x81, 0xFE); // cmp esi,hugeBits
        code.EmitInt32(hugeBits);
        code.EmitJccNear(0x84, "apply_huge"); // je
        code.Emit(0x81, 0xFE); // cmp esi,massiveBits
        code.EmitInt32(massiveBits);
        code.EmitJccNear(0x84, "apply_massive"); // je
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_small");
        code.Emit(0xA1);
        code.EmitUInt32(smallScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeSmallCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_medium");
        code.Emit(0xA1);
        code.EmitUInt32(mediumScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMediumCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_large");
        code.Emit(0xA1);
        code.EmitUInt32(largeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_very_large");
        code.Emit(0xA1);
        code.EmitUInt32(veryLargeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeVeryLargeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_huge");
        code.Emit(0xA1);
        code.EmitUInt32(hugeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeHugeCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);
        code.EmitJmpLabel("apply_ret");

        code.Label("apply_massive");
        code.Emit(0xA1);
        code.EmitUInt32(massiveScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.Emit(0xB8);
        code.EmitUInt32(SubtitleSizeMassiveCode);
        code.EmitMovAbsLabelEaxAbsolute(lastAppliedCodeVa);
        code.Emit(0xFF, 0x05);
        code.EmitUInt32(applyCountVa);

        code.Label("apply_ret");
        code.Emit(0xC3); // ret

        for (int index = 0; index < hooks.Count; index++)
        {
            TailSignalHook hook = hooks[index];
            entryRvas.Add(WorkerCaveRva + (uint)code.Position);

            code.Label($"entry_render3d_tail_{index}");
            code.Emit(0x9C); // pushfd
            code.Emit(0x60); // pushad
            code.Emit(0x8B, 0x74, 0x24, 0x2C); // mov esi,[esp+2C] -> original [esp+08]
            code.Emit(0x89, 0x35);
            code.EmitUInt32(lastSeenSignalCodeVa);
            code.Emit(0xC7, 0x05);
            code.EmitUInt32(lastSeenSignalArg2Va);
            code.EmitUInt32(0);
            code.Emit(0xB8);
            code.EmitUInt32(hook.HookRva);
            code.EmitMovAbsLabelEaxAbsolute(lastSeenHookRvaVa);
            code.Emit(0xFF, 0x05);
            code.EmitUInt32(signalHitCountVa);
            code.EmitCallLabel("apply_scale_from_signal");
            code.Emit(0x61); // popad
            code.Emit(0x9D); // popfd
            code.Emit(0x8B, 0xC6); // mov eax,esi
            code.Emit(0x5E); // pop esi
            code.Emit(0x59); // pop ecx
            code.Emit(0xC3); // ret
        }

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"Render3D signal cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return new TailSignalPatch(worker, entryRvas.ToArray());
    }

    private static TailSignalPatch BuildInvokeTracePatch(
        WritableStateBlock stateBlock,
        IReadOnlyList<InvokeTraceHook> hooks)
    {
        uint lastSeenSignalCodeVa = ImageBase + stateBlock.Rva + StateLastSeenSignalCodeOffset;
        uint lastSeenSignalArg2Va = ImageBase + stateBlock.Rva + StateLastSeenSignalArg2Offset;
        uint lastSeenHookRvaVa = ImageBase + stateBlock.Rva + StateLastSeenHookRvaOffset;
        uint signalHitCountVa = ImageBase + stateBlock.Rva + StateSignalHitCountOffset;
        uint lastMethodPtrVa = ImageBase + stateBlock.Rva + StateLastMethodPtrOffset;
        uint lastMethodTextVa = ImageBase + stateBlock.Rva + StateLastMethodTextOffset;

        var code = new X86Builder(ImageBase, WorkerCaveRva);
        var entryRvas = new List<uint>(hooks.Count);

        for (int index = 0; index < hooks.Count; index++)
        {
            InvokeTraceHook hook = hooks[index];
            string skipLabel = $"invoke_skip_log_{index}";
            entryRvas.Add(WorkerCaveRva + (uint)code.Position);

            code.Label($"invoke_entry_{index}");
            code.Emit(0x83, 0xEC, 0x34); // sub esp,34
            code.Emit(0x53); // push ebx
            code.Emit(0x57); // push edi
            code.Emit(0x9C); // pushfd
            code.Emit(0x60); // pushad
            code.Emit(0x8B, 0x44, 0x24, 0x64); // mov eax,[esp+64] ; method ptr
            code.Emit(0x85, 0xC0); // test eax,eax
            code.EmitJccNear(0x84, skipLabel); // je
            code.Emit(0x80, 0x38, 0x46); // cmp byte ptr [eax],'F'
            code.EmitJccNear(0x85, skipLabel); // jne
            code.Emit(0x80, 0x78, 0x01, 0x45); // cmp byte ptr [eax+1],'E'
            code.EmitJccNear(0x85, skipLabel); // jne
            code.Emit(0x80, 0x78, 0x02, 0x5F); // cmp byte ptr [eax+2],'_'
            code.EmitJccNear(0x85, skipLabel); // jne
            code.EmitMovAbsLabelEaxAbsolute(lastMethodPtrVa);
            code.Emit(0x8B, 0x44, 0x24, 0x68); // mov eax,[esp+68]
            code.EmitMovAbsLabelEaxAbsolute(lastSeenSignalCodeVa);
            code.Emit(0x8B, 0x44, 0x24, 0x6C); // mov eax,[esp+6C]
            code.EmitMovAbsLabelEaxAbsolute(lastSeenSignalArg2Va);
            code.Emit(0xB8);
            code.EmitUInt32(hook.HookRva);
            code.EmitMovAbsLabelEaxAbsolute(lastSeenHookRvaVa);
            code.Emit(0x8B, 0x44, 0x24, 0x64); // mov eax,[esp+64]
            code.Emit(0x8B, 0x10); // mov edx,[eax]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x00);
            code.Emit(0x8B, 0x50, 0x04); // mov edx,[eax+4]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x04);
            code.Emit(0x8B, 0x50, 0x08); // mov edx,[eax+8]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x08);
            code.Emit(0x8B, 0x50, 0x0C); // mov edx,[eax+C]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x0C);
            code.Emit(0x8B, 0x50, 0x10); // mov edx,[eax+10]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x10);
            code.Emit(0x8B, 0x50, 0x14); // mov edx,[eax+14]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x14);
            code.Emit(0x8B, 0x50, 0x18); // mov edx,[eax+18]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x18);
            code.Emit(0x8B, 0x50, 0x1C); // mov edx,[eax+1C]
            code.Emit(0x89, 0x15);
            code.EmitUInt32(lastMethodTextVa + 0x1C);
            code.Emit(0xFF, 0x05);
            code.EmitUInt32(signalHitCountVa);
            code.Label(skipLabel);
            code.Emit(0x61); // popad
            code.Emit(0x9D); // popfd
            code.EmitJmpRva(hook.HookRva + (uint)ExpectedInvokeTraceHookBytes.Length);
        }

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"Invoke trace cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return new TailSignalPatch(worker, entryRvas.ToArray());
    }

    private static byte[] BuildSubtitleBurstWorkerCavePatch(
        byte[] bytes,
        IReadOnlyList<PeSection> sections,
        WritableStateBlock stateBlock,
        BurstScaleOptions config)
    {
        uint getTickCountIatVa = ResolveImportIatVa(bytes, sections, "KERNEL32.dll", "GetTickCount");
        uint currentScaleVa = ImageBase + stateBlock.Rva + StateCurrentScaleOffset;
        uint burstCountVa = ImageBase + stateBlock.Rva + StateBurstCountOffset;
        uint lastTickVa = ImageBase + stateBlock.Rva + StateLastPollTickOffset;
        uint smallScaleVa = ImageBase + stateBlock.Rva + StateSmallScaleOffset;
        uint mediumScaleVa = ImageBase + stateBlock.Rva + StateMediumScaleOffset;
        uint largeScaleVa = ImageBase + stateBlock.Rva + StateLargeScaleOffset;

        var code = new X86Builder(ImageBase, WorkerCaveRva);

        code.Label("entry");
        code.Emit(0x8B, 0xC6); // mov eax,esi
        code.Emit(0x9C); // pushfd
        code.Emit(0x60); // pushad
        code.EmitCallLabel("update_scale");
        code.Emit(0x61); // popad
        code.Emit(0x9D); // popfd
        code.Emit(0x5E); // pop esi
        code.Emit(0x59); // pop ecx
        code.Emit(0xC3); // ret

        code.Label("update_scale");
        code.EmitCallImport(getTickCountIatVa);
        code.Emit(0x89, 0xC2); // mov edx,eax
        code.Emit(0x8B, 0x0D); // mov ecx,[last_tick]
        code.EmitUInt32(lastTickVa);
        code.Emit(0x89, 0x15); // mov [last_tick],edx
        code.EmitUInt32(lastTickVa);
        code.Emit(0x29, 0xCA); // sub edx,ecx
        code.Emit(0x81, 0xFA); // cmp edx,resetMs
        code.EmitInt32(config.BurstResetMs);
        code.EmitJccNear(0x87, "reset_burst"); // ja

        code.Emit(0x0F, 0xB6, 0x05); // movzx eax,byte ptr [burst_count]
        code.EmitUInt32(burstCountVa);
        code.Emit(0x40); // inc eax
        code.Emit(0x83, 0xF8, 0x06); // cmp eax,6
        code.EmitJccNear(0x86, "store_count"); // jbe
        code.Emit(0xB8); // mov eax,6
        code.EmitUInt32(6);
        code.EmitJmpLabel("store_count");

        code.Label("reset_burst");
        code.Emit(0xB8); // mov eax,1
        code.EmitUInt32(1);

        code.Label("store_count");
        code.Emit(0xA2); // mov [burst_count],al
        code.EmitUInt32(burstCountVa);
        code.Emit(0x83, 0xF8, 0x02); // cmp eax,2
        code.EmitJccNear(0x82, "ret"); // jb
        code.EmitJccNear(0x84, "apply_small"); // je
        code.Emit(0x83, 0xF8, 0x04); // cmp eax,4
        code.EmitJccNear(0x82, "ret"); // jb
        code.EmitJccNear(0x84, "apply_medium"); // je
        code.Emit(0x83, 0xF8, 0x06); // cmp eax,6
        code.EmitJccNear(0x82, "ret"); // jb

        code.Label("apply_large");
        code.Emit(0xA1);
        code.EmitUInt32(largeScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.EmitJmpLabel("ret");

        code.Label("apply_small");
        code.Emit(0xA1);
        code.EmitUInt32(smallScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);
        code.EmitJmpLabel("ret");

        code.Label("apply_medium");
        code.Emit(0xA1);
        code.EmitUInt32(mediumScaleVa);
        code.EmitMovAbsLabelEaxAbsolute(currentScaleVa);

        code.Label("ret");
        code.Emit(0xC3); // ret

        byte[] worker = code.ToArray();
        if (worker.Length > WorkerCaveLength)
        {
            throw new InvalidOperationException(
                $"Worker cave patch length 0x{worker.Length:X} exceeds the reserved cave length 0x{WorkerCaveLength:X}.");
        }

        return worker;
    }

    private static void EmitReadableCommittedRegionCheck(
        X86Builder code,
        uint mbiStateVa,
        uint mbiProtectVa,
        string readableLabel,
        string notReadableLabel)
    {
        code.Emit(0xA1);
        code.EmitUInt32(mbiStateVa); // mov eax,[mbi.state]
        code.Emit(0x3D);
        code.EmitUInt32(MemCommit); // cmp eax,MEM_COMMIT
        code.EmitJccNear(0x85, notReadableLabel); // jne
        code.Emit(0xA1);
        code.EmitUInt32(mbiProtectVa); // mov eax,[mbi.protect]
        code.Emit(0xA9);
        code.EmitUInt32(PageGuard); // test eax,PAGE_GUARD
        code.EmitJccNear(0x85, notReadableLabel); // jne
        code.Emit(0x25);
        code.EmitUInt32(0x0000_00FF); // and eax,0xFF
        EmitCompareEaxImm32Jump(code, PageReadOnly, readableLabel);
        EmitCompareEaxImm32Jump(code, PageReadWrite, readableLabel);
        EmitCompareEaxImm32Jump(code, PageWriteCopy, readableLabel);
        EmitCompareEaxImm32Jump(code, PageExecuteRead, readableLabel);
        EmitCompareEaxImm32Jump(code, PageExecuteReadWrite, readableLabel);
        EmitCompareEaxImm32Jump(code, PageExecuteWriteCopy, readableLabel);
        code.EmitJmpLabel(notReadableLabel);
    }

    private static void EmitCompareEaxImm32Jump(X86Builder code, uint value, string equalLabel)
    {
        code.Emit(0x3D);
        code.EmitUInt32(value); // cmp eax,imm32
        code.EmitJccNear(0x84, equalLabel); // je
    }

    private static void EmitCompareDwordPtrEdiDisp32(X86Builder code, sbyte displacement, int value, string notEqualLabel)
    {
        code.Emit(0x81, 0x7F, unchecked((byte)displacement));
        code.EmitInt32(value); // cmp dword ptr [edi+disp],imm32
        code.EmitJccNear(0x85, notEqualLabel); // jne
    }

    private static uint ResolveImportIatVa(
        byte[] bytes,
        IReadOnlyList<PeSection> sections,
        string dllName,
        string importName)
    {
        int peHeaderOffset = BitConverter.ToInt32(bytes, 0x3C);
        int optionalHeaderOffset = peHeaderOffset + 24;
        ushort magic = BitConverter.ToUInt16(bytes, optionalHeaderOffset);
        if (magic != 0x10B)
        {
            throw new InvalidOperationException($"Unsupported PE optional header magic 0x{magic:X}.");
        }

        int importDirectoryOffset = optionalHeaderOffset + 96 + (1 * 8);
        uint importTableRva = BitConverter.ToUInt32(bytes, importDirectoryOffset);
        if (importTableRva == 0)
        {
            throw new InvalidOperationException("The executable does not have an import table.");
        }

        int descriptorOffset = RvaToOffset(sections, importTableRva);
        for (;; descriptorOffset += 20)
        {
            uint originalFirstThunk = BitConverter.ToUInt32(bytes, descriptorOffset + 0);
            uint nameRva = BitConverter.ToUInt32(bytes, descriptorOffset + 12);
            uint firstThunk = BitConverter.ToUInt32(bytes, descriptorOffset + 16);
            if (originalFirstThunk == 0 && nameRva == 0 && firstThunk == 0)
            {
                break;
            }

            string currentDllName = ReadNullTerminatedAscii(bytes, RvaToOffset(sections, nameRva));
            if (!currentDllName.Equals(dllName, StringComparison.OrdinalIgnoreCase))
            {
                continue;
            }

            uint thunkRva = originalFirstThunk != 0 ? originalFirstThunk : firstThunk;
            int thunkOffset = RvaToOffset(sections, thunkRva);
            int iatOffset = RvaToOffset(sections, firstThunk);

            for (int index = 0; ; index++)
            {
                uint thunkData = BitConverter.ToUInt32(bytes, thunkOffset + (index * 4));
                if (thunkData == 0)
                {
                    break;
                }

                if ((thunkData & 0x8000_0000) != 0)
                {
                    continue;
                }

                int importNameOffset = RvaToOffset(sections, thunkData);
                string currentImportName = ReadNullTerminatedAscii(bytes, importNameOffset + 2);
                if (currentImportName.Equals(importName, StringComparison.Ordinal))
                {
                    return ImageBase + firstThunk + (uint)(index * 4);
                }
            }
        }

        throw new InvalidOperationException($"Could not resolve import '{dllName}!{importName}'.");
    }

    private static string ReadNullTerminatedAscii(byte[] bytes, int offset)
    {
        int end = offset;
        while (end < bytes.Length && bytes[end] != 0)
        {
            end++;
        }

        return Encoding.ASCII.GetString(bytes, offset, end - offset);
    }

    private static byte[] BuildDynamicScaleStateBlock(
        float smallScale,
        float mediumScale,
        float largeScale,
        float veryLargeScale,
        float hugeScale,
        float massiveScale)
    {
        byte[] state = new byte[StateBlockLength];
        WriteUInt32(state, StateCurrentScaleOffset, BitConverter.ToUInt32(BitConverter.GetBytes(mediumScale), 0));
        WriteUInt32(state, StateSmallScaleOffset, BitConverter.ToUInt32(BitConverter.GetBytes(smallScale), 0));
        WriteUInt32(state, StateMediumScaleOffset, BitConverter.ToUInt32(BitConverter.GetBytes(mediumScale), 0));
        WriteUInt32(state, StateLargeScaleOffset, BitConverter.ToUInt32(BitConverter.GetBytes(largeScale), 0));
        WriteUInt32(state, StateVeryLargeScaleOffset, BitConverter.ToUInt32(BitConverter.GetBytes(veryLargeScale), 0));
        WriteUInt32(state, StateHugeScaleOffset, BitConverter.ToUInt32(BitConverter.GetBytes(hugeScale), 0));
        WriteUInt32(state, StateMassiveScaleOffset, BitConverter.ToUInt32(BitConverter.GetBytes(massiveScale), 0));
        WriteUInt32(state, StateDebugMagicOffset, SubtitleDebugMagic);
        WriteUInt32(state, StateDebugVersionOffset, SubtitleDebugVersion);
        return state;
    }

    private static WritableStateBlock ReserveWritableStateBlock(
        byte[] bytes,
        IReadOnlyList<PeSection> sections,
        int blockLength,
        params WritableStateBlock[] reservedBlocks)
    {
        foreach (PeSection section in sections)
        {
            if (!section.IsWritable)
            {
                continue;
            }

            int sectionStart = checked((int)section.PointerToRawData);
            int sectionEnd = checked((int)(section.PointerToRawData + section.SizeOfRawData));
            int runStart = -1;
            int runLength = 0;

            for (int offset = sectionStart; offset < sectionEnd; offset++)
            {
                if (bytes[offset] == 0x00)
                {
                    if (runStart < 0)
                    {
                        runStart = offset;
                        runLength = 1;
                    }
                    else
                    {
                        runLength++;
                    }

                    int alignedStart = AlignUp(runStart, 16);
                    if (alignedStart >= runStart && alignedStart + blockLength <= sectionEnd)
                    {
                        int alignedLength = offset - alignedStart + 1;
                        if (alignedLength >= blockLength)
                        {
                            if (reservedBlocks.Any(block => RangesOverlap(alignedStart, blockLength, block.Offset, block.Length)))
                            {
                                continue;
                            }

                            uint rva = section.VirtualAddress + (uint)(alignedStart - sectionStart);
                            return new WritableStateBlock(rva, alignedStart, blockLength);
                        }
                    }
                }
                else
                {
                    runStart = -1;
                    runLength = 0;
                }
            }
        }

        throw new InvalidOperationException(
            $"Could not reserve a writable state block of length 0x{blockLength:X} in the executable.");
    }

    private static int AlignUp(int value, int alignment)
    {
        int mask = alignment - 1;
        return (value + mask) & ~mask;
    }

    private static bool RangesOverlap(int offsetA, int lengthA, int offsetB, int lengthB)
    {
        int endA = checked(offsetA + lengthA);
        int endB = checked(offsetB + lengthB);
        return offsetA < endB && offsetB < endA;
    }

    private static void WriteHook(byte[] bytes, int hookOffset, uint caveRva, int overwrittenLength, uint hookRva)
    {
        uint jumpSourceRva = hookRva;
        int rel = unchecked((int)caveRva - unchecked((int)(jumpSourceRva + 5)));

        bytes[hookOffset + 0] = 0xE9;
        WriteInt32(bytes, hookOffset + 1, rel);
        for (int i = 5; i < overwrittenLength; i++)
        {
            bytes[hookOffset + i] = 0x90;
        }
    }

    private static void WriteBlock(byte[] bytes, int offset, byte[] block, string description)
    {
        byte[] target = bytes[offset..(offset + block.Length)];
        if (target.Any(static b => b != 0x00))
        {
            throw new InvalidOperationException($"The target {description} region is not empty. Refusing to overwrite it.");
        }

        Array.Copy(block, 0, bytes, offset, block.Length);
    }

    private static void WriteCave(byte[] bytes, int caveOffset, int caveLength, byte[] cavePatch)
    {
        if (cavePatch.Length > caveLength)
        {
            throw new InvalidOperationException(
                $"Patch length 0x{cavePatch.Length:X} exceeds cave length 0x{caveLength:X}.");
        }

        byte[] caveArea = bytes[caveOffset..(caveOffset + caveLength)];
        if (caveArea.Any(static b => b != 0x00))
        {
            throw new InvalidOperationException("The code cave is not empty. Refusing to overwrite it.");
        }

        Array.Copy(cavePatch, 0, bytes, caveOffset, cavePatch.Length);
    }

    private static void EnsureOriginalHook(byte[] bytes, int hookOffset, byte[] expectedBytes, uint hookRva)
    {
        byte[] current = bytes[hookOffset..(hookOffset + expectedBytes.Length)];
        if (current.SequenceEqual(expectedBytes))
        {
            return;
        }

        if (current[0] == 0xE9)
        {
            throw new InvalidOperationException("The executable already has a jump at the subtitle hook site.");
        }

        throw new InvalidOperationException(
            $"Unexpected bytes at hook RVA 0x{hookRva:X8}: {Convert.ToHexString(current)}");
    }

    private static int RvaToOffset(IReadOnlyList<PeSection> sections, uint rva)
    {
        foreach (var section in sections)
        {
            if (rva >= section.VirtualAddress && rva < section.VirtualAddress + section.SizeOfRawData)
            {
                return checked((int)(section.PointerToRawData + (rva - section.VirtualAddress)));
            }
        }

        throw new InvalidOperationException($"Could not map RVA 0x{rva:X8} to a file offset.");
    }

    private static IEnumerable<uint> FindPatternRvasInExecutableSections(
        byte[] bytes,
        IReadOnlyList<PeSection> sections,
        byte[] pattern)
    {
        foreach (var section in sections)
        {
            if (!section.IsExecutable)
            {
                continue;
            }

            int start = checked((int)section.PointerToRawData);
            int endExclusive = checked((int)Math.Min((long)bytes.Length, section.PointerToRawData + section.SizeOfRawData));
            int lastStart = endExclusive - pattern.Length;

            for (int offset = start; offset <= lastStart; offset++)
            {
                bool matched = true;
                for (int i = 0; i < pattern.Length; i++)
                {
                    if (bytes[offset + i] != pattern[i])
                    {
                        matched = false;
                        break;
                    }
                }

                if (matched)
                {
                    yield return section.VirtualAddress + (uint)(offset - start);
                }
            }
        }
    }

    private static SignalWrapperHook[] FindSubtitleSignalWrappers(
        byte[] bytes,
        IReadOnlyList<PeSection> sections)
    {
        return FindPatternRvasInExecutableSections(bytes, sections, ExpectedSubtitleSignalWrapperHookBytes)
            .Select(rva =>
            {
                int offset = RvaToOffset(sections, rva);
                uint functionObjectVa = BitConverter.ToUInt32(bytes, offset + 6);
                return new SignalWrapperHook(rva, offset, functionObjectVa);
            })
            .ToArray();
    }

    private static TailSignalHook[] FindKnownSubtitleTailSignalHooks(
        byte[] bytes,
        IReadOnlyList<PeSection> sections)
    {
        var hooks = new List<TailSignalHook>(KnownSubtitleSetterClusterRvas.Length);
        foreach (uint rva in KnownSubtitleSetterClusterRvas)
        {
            int offset = RvaToOffset(sections, rva);
            byte[] current = bytes[offset..(offset + ExpectedSubtitleSetterHookBytes.Length)];
            if (!current.SequenceEqual(ExpectedSubtitleSetterHookBytes))
            {
                continue;
            }

            hooks.Add(new TailSignalHook(rva, offset));
        }

        return hooks.ToArray();
    }

    private static TailSignalHook[] FindKnownRender3DTailHooks(
        byte[] bytes,
        IReadOnlyList<PeSection> sections)
    {
        var hooks = new List<TailSignalHook>(KnownRender3DTailRvas.Length);
        foreach (uint rva in KnownRender3DTailRvas)
        {
            int offset = RvaToOffset(sections, rva);
            byte[] current = bytes[offset..(offset + ExpectedSubtitleSetterHookBytes.Length)];
            if (!current.SequenceEqual(ExpectedSubtitleSetterHookBytes))
            {
                continue;
            }

            hooks.Add(new TailSignalHook(rva, offset));
        }

        return hooks.ToArray();
    }

    private static InvokeTraceHook[] FindKnownInvokeTraceHooks(
        byte[] bytes,
        IReadOnlyList<PeSection> sections)
    {
        var hooks = new List<InvokeTraceHook>(KnownInvokeTraceHookRvas.Length);
        foreach (uint rva in KnownInvokeTraceHookRvas)
        {
            int offset = RvaToOffset(sections, rva);
            if (offset < 0 || offset + ExpectedInvokeTraceHookBytes.Length > bytes.Length)
            {
                continue;
            }

            hooks.Add(new InvokeTraceHook(rva, offset));
        }

        return hooks.ToArray();
    }

    private static void WriteInt32(byte[] bytes, int offset, int value)
    {
        byte[] raw = BitConverter.GetBytes(value);
        Array.Copy(raw, 0, bytes, offset, 4);
    }

    private static void WriteUInt32(byte[] bytes, int offset, uint value)
    {
        byte[] raw = BitConverter.GetBytes(value);
        Array.Copy(raw, 0, bytes, offset, 4);
    }

    private static uint ReadUInt32(byte[] bytes, int offset) => BitConverter.ToUInt32(bytes, offset);

    private static float ReadFloat(byte[] bytes, int offset) => BitConverter.ToSingle(bytes, offset);

    private static int ParseInt(string value)
    {
        if (!int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int parsed))
        {
            throw new InvalidOperationException($"Invalid integer value '{value}'.");
        }

        return parsed;
    }

    private static float ParseFloat(string value)
    {
        if (!float.TryParse(value, NumberStyles.Float, CultureInfo.InvariantCulture, out float parsed))
        {
            throw new InvalidOperationException($"Invalid float value '{value}'.");
        }

        return parsed;
    }

    private static string ComputeSha256(string path)
    {
        using var stream = File.OpenRead(path);
        byte[] hash = SHA256.HashData(stream);
        return Convert.ToHexString(hash).ToLowerInvariant();
    }

    private static Process ResolveSingleProcess(string processName)
    {
        Process? process = TryResolveSingleProcess(processName);
        if (process is null)
        {
            throw new InvalidOperationException($"Process '{processName}' is not running.");
        }

        return process;
    }

    private static Process? TryResolveSingleProcess(string processName)
    {
        string normalized = Path.GetFileNameWithoutExtension(processName);
        Process[] matches = Process.GetProcessesByName(normalized);
        if (matches.Length == 0)
        {
            return null;
        }

        if (matches.Length > 1)
        {
            throw new InvalidOperationException($"More than one '{normalized}' process is running.");
        }

        return matches[0];
    }

    private static void ApplyLiveScale(Process process, float scale)
    {
        IntPtr processHandle = NativeMethods.OpenProcess(
            NativeMethods.PROCESS_VM_OPERATION | NativeMethods.PROCESS_VM_WRITE,
            inheritHandle: false,
            process.Id);

        if (processHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException($"Could not open process {process.Id} for memory writes.");
        }

        try
        {
            IntPtr targetAddress = GetScaleConstantAddress(process.MainModule!.BaseAddress);
            IntPtr size = new(sizeof(float));
            if (!NativeMethods.VirtualProtectEx(processHandle, targetAddress, size, NativeMethods.PAGE_EXECUTE_READWRITE, out uint oldProtect))
            {
                throw new InvalidOperationException($"VirtualProtectEx failed at 0x{targetAddress.ToInt64():X8}.");
            }

            try
            {
                byte[] raw = BitConverter.GetBytes(scale);
                if (!NativeMethods.WriteProcessMemory(processHandle, targetAddress, raw, raw.Length, out IntPtr written) ||
                    written.ToInt64() != raw.Length)
                {
                    throw new InvalidOperationException($"WriteProcessMemory failed at 0x{targetAddress.ToInt64():X8}.");
                }
            }
            finally
            {
                NativeMethods.VirtualProtectEx(processHandle, targetAddress, size, oldProtect, out _);
            }
        }
        finally
        {
            NativeMethods.CloseHandle(processHandle);
        }
    }

    private static IntPtr GetScaleConstantAddress(IntPtr moduleBase)
    {
        return IntPtr.Add(moduleBase, checked((int)(CaveRva + GlobalScaleConstOffset)));
    }

    private static SubtitleDebugSnapshot ReadLiveSubtitleDebugSnapshot(Process process)
    {
        IntPtr processHandle = NativeMethods.OpenProcess(
            NativeMethods.PROCESS_VM_READ | NativeMethods.PROCESS_QUERY_INFORMATION,
            inheritHandle: false,
            process.Id);

        if (processHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException($"Could not open process {process.Id} for memory reads.");
        }

        try
        {
            IntPtr moduleBase = process.MainModule!.BaseAddress;
            IntPtr caveAddress = IntPtr.Add(moduleBase, checked((int)CaveRva));
            byte[] cave = ReadProcessBytes(processHandle, caveAddress, 96);

            int scalePointerOffset = FindScalePointerOffset(cave);
            uint stateBlockVa = ReadUInt32(cave, scalePointerOffset);
            if (stateBlockVa == 0)
            {
                throw new InvalidOperationException("Could not resolve the live subtitle state block address.");
            }

            byte[] state = ReadProcessBytes(processHandle, new IntPtr(stateBlockVa), StateBlockLength);
            uint debugMagic = ReadUInt32(state, StateDebugMagicOffset);
            if (debugMagic != SubtitleDebugMagic)
            {
                throw new InvalidOperationException($"Unexpected subtitle debug magic 0x{debugMagic:X8}.");
            }

            return new SubtitleDebugSnapshot(
                ProcessId: process.Id,
                StateBlockVa: stateBlockVa,
                CurrentScale: ReadFloat(state, StateCurrentScaleOffset),
                SmallScale: ReadFloat(state, StateSmallScaleOffset),
                MediumScale: ReadFloat(state, StateMediumScaleOffset),
                LargeScale: ReadFloat(state, StateLargeScaleOffset),
                VeryLargeScale: ReadFloat(state, StateVeryLargeScaleOffset),
                HugeScale: ReadFloat(state, StateHugeScaleOffset),
                MassiveScale: ReadFloat(state, StateMassiveScaleOffset),
                ScanCursor: ReadUInt32(state, StateLastPollTickOffset),
                CachedPtr: ReadUInt32(state, StateFileBufferPtrOffset),
                LastSeenSignalCode: ReadUInt32(state, StateLastSeenSignalCodeOffset),
                LastSeenSignalArg2: ReadUInt32(state, StateLastSeenSignalArg2Offset),
                LastSeenHookRva: ReadUInt32(state, StateLastSeenHookRvaOffset),
                SignalHitCount: ReadUInt32(state, StateSignalHitCountOffset),
                LastAppliedCode: ReadUInt32(state, StateLastAppliedCodeOffset),
                ApplyCount: ReadUInt32(state, StateApplyCountOffset),
                LastMethodPtr: ReadUInt32(state, StateLastMethodPtrOffset),
                LastMethodText: ReadAsciiPreview(state, StateLastMethodTextOffset, StateLastMethodTextLength));
        }
        finally
        {
            NativeMethods.CloseHandle(processHandle);
        }
    }

    private static int FindScalePointerOffset(byte[] caveBytes)
    {
        for (int index = 0; index <= caveBytes.Length - 6; index++)
        {
            if (caveBytes[index] == 0xD9 && caveBytes[index + 1] == 0x05)
            {
                return index + 2;
            }
        }

        throw new InvalidOperationException("Could not find the dynamic scale pointer inside the text-scale cave.");
    }

    private static byte[] ReadProcessBytes(IntPtr processHandle, IntPtr address, int size)
    {
        byte[] buffer = new byte[size];
        if (!NativeMethods.ReadProcessMemory(processHandle, address, buffer, size, out IntPtr bytesRead) ||
            bytesRead.ToInt64() != size)
        {
            throw new InvalidOperationException($"ReadProcessMemory failed at 0x{address.ToInt64():X8}.");
        }

        return buffer;
    }

    private static void PrintSubtitleDebugSnapshot(SubtitleDebugSnapshot snapshot, bool includeTimestamp)
    {
        string prefix = includeTimestamp ? $"[{DateTime.Now:HH:mm:ss}] " : string.Empty;
        float arg1Float = BitConverter.Int32BitsToSingle(unchecked((int)snapshot.LastSeenSignalCode));
        float arg2Float = BitConverter.Int32BitsToSingle(unchecked((int)snapshot.LastSeenSignalArg2));
        Console.WriteLine(
            $"{prefix}pid={snapshot.ProcessId} scale={snapshot.CurrentScale.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"small={snapshot.SmallScale.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"medium={snapshot.MediumScale.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"large={snapshot.LargeScale.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"veryLarge={snapshot.VeryLargeScale.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"huge={snapshot.HugeScale.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"massive={snapshot.MassiveScale.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"scanCursor=0x{snapshot.ScanCursor:X8} cachedPtr=0x{snapshot.CachedPtr:X8} " +
            $"hits={snapshot.SignalHitCount} lastArg1={snapshot.LastSeenSignalCode} lastArg1f={arg1Float.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"lastArg2={snapshot.LastSeenSignalArg2} lastArg2f={arg2Float.ToString("0.###", CultureInfo.InvariantCulture)} " +
            $"lastHookRva=0x{snapshot.LastSeenHookRva:X8} applyCount={snapshot.ApplyCount} lastApplied={snapshot.LastAppliedCode} " +
            $"lastMethodPtr=0x{snapshot.LastMethodPtr:X8} lastMethod=\"{snapshot.LastMethodText}\"");
    }

    private static bool ShouldPrintSubtitleDebugSnapshot(SubtitleDebugSnapshot previous, SubtitleDebugSnapshot current)
    {
        return previous.CurrentScale != current.CurrentScale ||
               previous.LastSeenSignalCode != current.LastSeenSignalCode ||
               previous.LastSeenSignalArg2 != current.LastSeenSignalArg2 ||
               previous.LastSeenHookRva != current.LastSeenHookRva ||
               previous.ScanCursor != current.ScanCursor ||
               previous.CachedPtr != current.CachedPtr ||
               previous.ApplyCount != current.ApplyCount ||
               previous.LastAppliedCode != current.LastAppliedCode ||
               previous.LastMethodPtr != current.LastMethodPtr ||
               !string.Equals(previous.LastMethodText, current.LastMethodText, StringComparison.Ordinal) ||
               previous.SmallScale != current.SmallScale ||
               previous.MediumScale != current.MediumScale ||
               previous.LargeScale != current.LargeScale ||
               previous.VeryLargeScale != current.VeryLargeScale ||
               previous.HugeScale != current.HugeScale ||
               previous.MassiveScale != current.MassiveScale;
    }

    private static CandidateSnapshot SnapshotLiveInt32Candidates(Process process, IReadOnlyCollection<int> values)
    {
        IntPtr processHandle = NativeMethods.OpenProcess(
            NativeMethods.PROCESS_VM_READ | NativeMethods.PROCESS_QUERY_INFORMATION,
            inheritHandle: false,
            process.Id);

        if (processHandle == IntPtr.Zero)
        {
            throw new InvalidOperationException($"Could not open process {process.Id} for memory reads.");
        }

        try
        {
            var candidates = new List<CandidateEntry>();
            var seenValues = new HashSet<int>(values);
            long address = 0;
            int mbiSize = Marshal.SizeOf<NativeMethods.MEMORY_BASIC_INFORMATION>();

            while (address < 0x7FFF0000L)
            {
                if (NativeMethods.VirtualQueryEx(processHandle, new IntPtr(address), out NativeMethods.MEMORY_BASIC_INFORMATION mbi, (IntPtr)mbiSize) == IntPtr.Zero)
                {
                    address += 0x1000;
                    continue;
                }

                long regionBase = mbi.BaseAddress.ToInt64();
                long regionSize = mbi.RegionSize.ToInt64();
                long nextAddress = regionBase + Math.Max(regionSize, 0x1000);

                if (mbi.State == NativeMethods.MEM_COMMIT &&
                    regionSize > 0 &&
                    regionSize <= int.MaxValue &&
                    IsReadableProtection(mbi.Protect))
                {
                    TryCollectCandidates(
                        processHandle,
                        (uint)regionBase,
                        (int)regionSize,
                        seenValues,
                        candidates);
                }

                if (nextAddress <= address)
                {
                    break;
                }

                address = nextAddress;
            }

            return new CandidateSnapshot(
                ProcessId: process.Id,
                ProcessName: process.ProcessName,
                Values: seenValues.OrderBy(value => value).ToArray(),
                Candidates: candidates.OrderBy(candidate => candidate.Address).ToArray());
        }
        finally
        {
            NativeMethods.CloseHandle(processHandle);
        }
    }

    private static void TryCollectCandidates(
        IntPtr processHandle,
        uint regionBase,
        int regionSize,
        HashSet<int> values,
        List<CandidateEntry> candidates)
    {
        try
        {
            byte[] region = ReadProcessBytes(processHandle, new IntPtr(regionBase), regionSize);
            for (int offset = 0; offset <= region.Length - sizeof(int); offset += sizeof(int))
            {
                int value = BitConverter.ToInt32(region, offset);
                if (!values.Contains(value))
                {
                    continue;
                }

                candidates.Add(new CandidateEntry(
                    Address: regionBase + (uint)offset,
                    Value: value,
                    RegionBase: regionBase,
                    RegionSize: regionSize));
            }
        }
        catch
        {
            // Ignore unreadable/transient regions.
        }
    }

    private static bool IsReadableProtection(uint protect)
    {
        uint baseProtect = protect & 0xFF;
        if ((protect & NativeMethods.PAGE_GUARD) != 0 || baseProtect == NativeMethods.PAGE_NOACCESS)
        {
            return false;
        }

        return baseProtect is NativeMethods.PAGE_READONLY
            or NativeMethods.PAGE_READWRITE
            or NativeMethods.PAGE_WRITECOPY
            or NativeMethods.PAGE_EXECUTE_READ
            or NativeMethods.PAGE_EXECUTE_READWRITE
            or NativeMethods.PAGE_EXECUTE_WRITECOPY;
    }

    private static int[] ParseIntList(string valuesText)
    {
        return valuesText
            .Split(',', StringSplitOptions.RemoveEmptyEntries | StringSplitOptions.TrimEntries)
            .Select(ParseInt)
            .Distinct()
            .ToArray();
    }

    private static string ReadAsciiPreview(byte[] bytes, int offset, int length)
    {
        int end = offset;
        int max = Math.Min(bytes.Length, offset + length);
        while (end < max && bytes[end] != 0)
        {
            end++;
        }

        var builder = new StringBuilder(end - offset);
        for (int index = offset; index < end; index++)
        {
            byte value = bytes[index];
            builder.Append(value is >= 0x20 and <= 0x7E ? (char)value : '.');
        }

        return builder.ToString();
    }

    private static SubtitleScaleSignal ReadSubtitleScaleSignal(string iniPath)
    {
        bool inHudSection = false;
        bool inGameEngineSection = false;
        bool hasWatcherBits = false;
        bool allowMatureLanguage = false;
        bool enableKismetLogging = false;
        int consoleFontSize = 6;

        foreach (string rawLine in ReadIniLines(iniPath))
        {
            string line = rawLine.Trim();
            if (line.Length == 0 || line.StartsWith(";", StringComparison.Ordinal))
            {
                continue;
            }

            if (line.StartsWith("[", StringComparison.Ordinal) && line.EndsWith("]", StringComparison.Ordinal))
            {
                inHudSection = string.Equals(line, "[Engine.HUD]", StringComparison.OrdinalIgnoreCase);
                inGameEngineSection = string.Equals(line, "[Engine.GameEngine]", StringComparison.OrdinalIgnoreCase);
                continue;
            }

            if (inGameEngineSection)
            {
                if (line.StartsWith("bAllowMatureLanguage=", StringComparison.OrdinalIgnoreCase))
                {
                    hasWatcherBits = true;
                    allowMatureLanguage = ParseIniBool(line[(line.IndexOf('=') + 1)..]);
                    continue;
                }

                if (line.StartsWith("bEnableKismetLogging=", StringComparison.OrdinalIgnoreCase))
                {
                    hasWatcherBits = true;
                    enableKismetLogging = ParseIniBool(line[(line.IndexOf('=') + 1)..]);
                    continue;
                }
            }

            if (inHudSection && line.StartsWith("ConsoleFontSize=", StringComparison.OrdinalIgnoreCase))
            {
                string value = line[(line.IndexOf('=') + 1)..].Trim();
                if (int.TryParse(value, NumberStyles.Integer, CultureInfo.InvariantCulture, out int parsed))
                {
                    consoleFontSize = parsed;
                }
            }
        }

        if (hasWatcherBits)
        {
            int state = enableKismetLogging ? 2 : allowMatureLanguage ? 0 : 1;
            return new SubtitleScaleSignal(
                DisplayValue: state,
                Scale: MapSubtitleStateToScale(state));
        }

        return new SubtitleScaleSignal(
            DisplayValue: consoleFontSize,
            Scale: MapConsoleFontSizeToScale(consoleFontSize));
    }

    private static float MapConsoleFontSizeToScale(int consoleFontSize) => consoleFontSize switch
    {
        <= 5 => 1.0f,
        6 => 1.5f,
        7 => 2.0f,
        8 => 4.0f,
        9 => 6.0f,
        _ => 8.0f
    };

    private static float MapSubtitleStateToScale(int state) => state switch
    {
        <= 0 => 1.0f,
        1 => 1.5f,
        2 => 2.0f,
        3 => 4.0f,
        4 => 6.0f,
        _ => 8.0f
    };

    private static bool ParseIniBool(string value)
    {
        string normalized = value.Trim();
        return normalized.Equals("true", StringComparison.OrdinalIgnoreCase) ||
               normalized.Equals("1", StringComparison.OrdinalIgnoreCase);
    }

    private static IEnumerable<string> ReadIniLines(string iniPath)
    {
        const int maxAttempts = 20;

        for (int attempt = 1; attempt <= maxAttempts; attempt++)
        {
            try
            {
                using var stream = new FileStream(
                    iniPath,
                    FileMode.Open,
                    FileAccess.Read,
                    FileShare.ReadWrite | FileShare.Delete);
                using var reader = new StreamReader(stream, Encoding.UTF8, detectEncodingFromByteOrderMarks: true);
                var lines = new List<string>();

                string? line;
                while ((line = reader.ReadLine()) is not null)
                {
                    lines.Add(line);
                }

                return lines;
            }
            catch (IOException) when (attempt < maxAttempts)
            {
                Thread.Sleep(25);
            }
        }

        throw new IOException($"Could not read '{iniPath}' after repeated retries.");
    }

    private static int PrintHelpAndReturn()
    {
        PrintUsage();
        return 0;
    }

    private static string DescribeSignalSource(bool uiStateLive, bool internalIniLive, bool subtitleSizeSignal, bool subtitleTailDebugSignal, bool render3DTailDebugSignal, bool invokeTrace)
    {
        if (uiStateLive)
        {
            return "live UI state scan";
        }

        if (internalIniLive)
        {
            return "internal BmEngine.ini poll";
        }

        if (subtitleSizeSignal)
        {
            return "FE wrapper entry hook";
        }

        if (subtitleTailDebugSignal)
        {
            return "FE setter tail hook";
        }

        if (render3DTailDebugSignal)
        {
            return "FE_SetRender3D tail hook";
        }

        if (invokeTrace)
        {
            return "Scaleform invoke trace";
        }

        return "fixed";
    }

    private static void PrintUsage()
    {
        Console.WriteLine("NativeSubtitleExePatcher");
        Console.WriteLine();
        Console.WriteLine("Commands:");
        Console.WriteLine("  patch-bink-subtitles --exe <ShippingPC-BmGame.exe> [--scale-multiplier <float>] [--backup] [--backup-path <path>]");
        Console.WriteLine("  patch-bink-text-scale --exe <ShippingPC-BmGame.exe> [--scale-multiplier <float>] [--global] [--ui-state-live] [--subtitle-size-signal] [--subtitle-tail-debug-signal] [--render3d-tail-debug-signal] [--invoke-trace] [--internal-ini-live] [--small-scale <float>] [--medium-scale <float>] [--large-scale <float>] [--very-large-scale <float>] [--huge-scale <float>] [--massive-scale <float>] [--poll-ms <int>] [--backup] [--backup-path <path>]");
        Console.WriteLine("  export-global-text-scale-blob --output <path> [--scale-multiplier <float>]");
        Console.WriteLine("  set-live-text-scale --scale-multiplier <float> [--process-name <ShippingPC-BmGame>]");
        Console.WriteLine("  watch-live-text-scale --ini <BmGame.ini> [--process-name <ShippingPC-BmGame>] [--poll-ms <500>]");
        Console.WriteLine("  watch-live-subtitle-debug [--process-name <ShippingPC-BmGame>] [--interval-ms <250>]");
        Console.WriteLine("  dump-live-subtitle-debug [--process-name <ShippingPC-BmGame>]");
        Console.WriteLine("  snapshot-live-subtitle-candidates --output <snapshot.json> [--process-name <ShippingPC-BmGame>] [--values <4101,4102,4103,4104,4105,4106>]");
        Console.WriteLine("  diff-live-subtitle-candidates --before <snapshot.json> --after <snapshot.json>");
        Console.WriteLine("  verify-bink-subtitles --exe <ShippingPC-BmGame.exe>");
        Console.WriteLine("  verify-bink-text-scale --exe <ShippingPC-BmGame.exe>");
    }

    private sealed class ArgumentReader
    {
        private readonly Dictionary<string, string?> _values = new(StringComparer.OrdinalIgnoreCase);
        private readonly HashSet<string> _flags = new(StringComparer.OrdinalIgnoreCase);
        private readonly List<string> _unknown = new();

        public ArgumentReader(IEnumerable<string> args)
        {
            string[] array = args.ToArray();
            for (int i = 0; i < array.Length; i++)
            {
                string current = array[i];
                if (!current.StartsWith("--", StringComparison.Ordinal))
                {
                    _unknown.Add(current);
                    continue;
                }

                if (i + 1 < array.Length && !array[i + 1].StartsWith("--", StringComparison.Ordinal))
                {
                    _values[current] = array[++i];
                }
                else
                {
                    _flags.Add(current);
                }
            }
        }

        public string? GetValue(string name) => _values.TryGetValue(name, out string? value) ? value : null;

        public string RequireValue(string name) =>
            GetValue(name) ?? throw new InvalidOperationException($"Missing required argument '{name}'.");

        public bool GetFlag(string name) => _flags.Contains(name);

        public void ThrowIfAnyUnknown()
        {
            if (_unknown.Count > 0)
            {
                throw new InvalidOperationException($"Unknown arguments: {string.Join(", ", _unknown)}");
            }
        }
    }

    private sealed class X86Builder
    {
        private readonly MemoryStream _stream = new();
        private readonly uint _imageBase;
        private readonly uint _baseRva;
        private readonly Dictionary<string, int> _labels = new(StringComparer.Ordinal);
        private readonly List<Fixup> _fixups = new();

        public X86Builder(uint imageBase, uint baseRva)
        {
            _imageBase = imageBase;
            _baseRva = baseRva;
        }

        public int Position => checked((int)_stream.Position);

        public void Label(string name) => _labels[name] = Position;

        public void Align(int alignment)
        {
            while ((Position % alignment) != 0)
            {
                Emit(0x00);
            }
        }

        public void Emit(params byte[] bytes) => _stream.Write(bytes, 0, bytes.Length);

        public void EmitUInt32(uint value) => Emit(BitConverter.GetBytes(value));

        public void EmitInt32(int value) => Emit(BitConverter.GetBytes(value));

        public void EmitPushImm32(uint value)
        {
            Emit(0x68);
            EmitUInt32(value);
        }

        public void EmitCallImport(uint iatVa)
        {
            Emit(0xFF, 0x15);
            EmitUInt32(iatVa);
        }

        public void EmitCallAbsLabelPointer(string label)
        {
            Emit(0xFF, 0x15);
            EmitAbsLabel32(label);
        }

        public void EmitPushAbsLabelValue(string label)
        {
            Emit(0xFF, 0x35);
            EmitAbsLabel32(label);
        }

        public void EmitCallLabel(string label)
        {
            Emit(0xE8);
            AddFixup(label, FixupKind.Rel32);
        }

        public void EmitJmpLabel(string label)
        {
            Emit(0xE9);
            AddFixup(label, FixupKind.Rel32);
        }

        public void EmitJmpRva(uint targetRva)
        {
            Emit(0xE9);
            int rel = unchecked((int)targetRva - unchecked((int)(_baseRva + (uint)Position + 4)));
            EmitInt32(rel);
        }

        public void EmitJccNear(byte opcodeLow, string label)
        {
            Emit(0x0F, opcodeLow);
            AddFixup(label, FixupKind.Rel32);
        }

        public void EmitAbsLabel32(string label) => AddFixup(label, FixupKind.Abs32);

        public void EmitPushLabelAddress(string label)
        {
            Emit(0x68);
            EmitAbsLabel32(label);
        }

        public void EmitMovEdiLabelAddress(string label)
        {
            Emit(0xBF);
            EmitAbsLabel32(label);
        }

        public void EmitMovEsiLabelAddress(string label)
        {
            Emit(0xBE);
            EmitAbsLabel32(label);
        }

        public void EmitMovEaxAbsLabel(string label)
        {
            Emit(0xA1);
            EmitAbsLabel32(label);
        }

        public void EmitMovAbsLabelEax(string label)
        {
            Emit(0xA3);
            EmitAbsLabel32(label);
        }

        public void EmitMovAbsLabelEaxAbsolute(uint absoluteVa)
        {
            Emit(0xA3);
            EmitUInt32(absoluteVa);
        }

        public byte[] ToArray()
        {
            byte[] result = _stream.ToArray();
            foreach (Fixup fixup in _fixups)
            {
                if (!_labels.TryGetValue(fixup.Label, out int labelOffset))
                {
                    throw new InvalidOperationException($"Unknown x86 builder label '{fixup.Label}'.");
                }

                if (fixup.Kind == FixupKind.Abs32)
                {
                    uint absoluteVa = _imageBase + _baseRva + (uint)labelOffset;
                    Array.Copy(BitConverter.GetBytes(absoluteVa), 0, result, fixup.Offset, 4);
                    continue;
                }

                uint nextInstructionRva = _baseRva + (uint)fixup.Offset + 4;
                int rel = unchecked((int)(_baseRva + (uint)labelOffset) - unchecked((int)nextInstructionRva));
                Array.Copy(BitConverter.GetBytes(rel), 0, result, fixup.Offset, 4);
            }

            return result;
        }

        private void AddFixup(string label, FixupKind kind)
        {
            _fixups.Add(new Fixup(Position, label, kind));
            EmitUInt32(0);
        }

        private readonly record struct Fixup(int Offset, string Label, FixupKind Kind);

        private enum FixupKind
        {
            Rel32,
            Abs32
        }
    }

    private readonly record struct LiveIniSignalOptions(float SmallScale, float MediumScale, float LargeScale, float VeryLargeScale, float HugeScale, float MassiveScale, int PollMs);
    private readonly record struct SubtitleSignalOptions(float SmallScale, float MediumScale, float LargeScale, float VeryLargeScale, float HugeScale, float MassiveScale);
    private readonly record struct BurstScaleOptions(float SmallScale, float MediumScale, float LargeScale, float VeryLargeScale, float HugeScale, float MassiveScale, int BurstResetMs);

    private sealed record PeSection(uint VirtualAddress, uint PointerToRawData, uint SizeOfRawData, uint Characteristics)
    {
        public bool IsWritable => (Characteristics & 0x8000_0000) != 0;
        public bool IsExecutable => (Characteristics & 0x2000_0000) != 0;

        public static IReadOnlyList<PeSection> ReadSections(byte[] bytes)
        {
            using var stream = new MemoryStream(bytes, writable: false);
            using var reader = new BinaryReader(stream, Encoding.ASCII, leaveOpen: true);

            if (reader.ReadUInt16() != 0x5A4D)
            {
                throw new InvalidOperationException("Not a valid MZ executable.");
            }

            stream.Position = 0x3C;
            int peHeaderOffset = reader.ReadInt32();
            stream.Position = peHeaderOffset;
            if (reader.ReadUInt32() != 0x00004550)
            {
                throw new InvalidOperationException("Not a valid PE executable.");
            }

            stream.Position += 2; // Machine
            ushort sectionCount = reader.ReadUInt16();
            stream.Position += 12; // timestamps + symbol table
            ushort optionalHeaderSize = reader.ReadUInt16();
            stream.Position += 2; // characteristics
            stream.Position += optionalHeaderSize;

            var sections = new List<PeSection>(sectionCount);
            for (int i = 0; i < sectionCount; i++)
            {
                stream.Position += 8; // name
                uint virtualSize = reader.ReadUInt32();
                uint virtualAddress = reader.ReadUInt32();
                uint sizeOfRawData = reader.ReadUInt32();
                uint pointerToRawData = reader.ReadUInt32();
                stream.Position += 12;
                uint characteristics = reader.ReadUInt32();

                sections.Add(new PeSection(virtualAddress, pointerToRawData, Math.Max(sizeOfRawData, virtualSize), characteristics));
            }

            return sections;
        }
    }

    private readonly record struct WritableStateBlock(uint Rva, int Offset, int Length);
    private readonly record struct SignalWrapperHook(uint HookRva, int HookOffset, uint FunctionObjectVa);
    private readonly record struct SubtitleSignalWrapperPatch(byte[] WorkerBytes, uint[] EntryRvas);
    private readonly record struct TailSignalHook(uint HookRva, int HookOffset);
    private readonly record struct InvokeTraceHook(uint HookRva, int HookOffset);
    private readonly record struct TailSignalPatch(byte[] WorkerBytes, uint[] EntryRvas);

    private static class NativeMethods
    {
        public const uint PROCESS_VM_OPERATION = 0x0008;
        public const uint PROCESS_VM_READ = 0x0010;
        public const uint PROCESS_VM_WRITE = 0x0020;
        public const uint PROCESS_QUERY_INFORMATION = 0x0400;
        public const uint MEM_COMMIT = 0x1000;
        public const uint PAGE_NOACCESS = 0x01;
        public const uint PAGE_READONLY = 0x02;
        public const uint PAGE_READWRITE = 0x04;
        public const uint PAGE_WRITECOPY = 0x08;
        public const uint PAGE_EXECUTE_READ = 0x20;
        public const uint PAGE_EXECUTE_READWRITE = 0x40;
        public const uint PAGE_EXECUTE_WRITECOPY = 0x80;
        public const uint PAGE_GUARD = 0x100;

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr OpenProcess(uint desiredAccess, bool inheritHandle, int processId);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool WriteProcessMemory(
            IntPtr process,
            IntPtr baseAddress,
            byte[] buffer,
            int size,
            out IntPtr numberOfBytesWritten);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool ReadProcessMemory(
            IntPtr process,
            IntPtr baseAddress,
            [Out] byte[] buffer,
            int size,
            out IntPtr numberOfBytesRead);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool VirtualProtectEx(
            IntPtr process,
            IntPtr address,
            IntPtr size,
            uint newProtect,
            out uint oldProtect);

        [DllImport("kernel32.dll", SetLastError = true)]
        [return: MarshalAs(UnmanagedType.Bool)]
        public static extern bool CloseHandle(IntPtr handle);

        [DllImport("kernel32.dll", SetLastError = true)]
        public static extern IntPtr VirtualQueryEx(
            IntPtr process,
            IntPtr address,
            out MEMORY_BASIC_INFORMATION buffer,
            IntPtr length);

        [StructLayout(LayoutKind.Sequential)]
        public struct MEMORY_BASIC_INFORMATION
        {
            public IntPtr BaseAddress;
            public IntPtr AllocationBase;
            public uint AllocationProtect;
            public IntPtr RegionSize;
            public uint State;
            public uint Protect;
            public uint Type;
        }
    }

    private readonly record struct SubtitleScaleSignal(int DisplayValue, float Scale);
    private sealed record CandidateSnapshot(int ProcessId, string ProcessName, int[] Values, CandidateEntry[] Candidates);
    private sealed record CandidateEntry(uint Address, int Value, uint RegionBase, int RegionSize);
    private readonly record struct SubtitleDebugSnapshot(
        int ProcessId,
        uint StateBlockVa,
        float CurrentScale,
        float SmallScale,
        float MediumScale,
        float LargeScale,
        float VeryLargeScale,
        float HugeScale,
        float MassiveScale,
        uint ScanCursor,
        uint CachedPtr,
        uint LastSeenSignalCode,
        uint LastSeenSignalArg2,
        uint LastSeenHookRva,
        uint SignalHitCount,
        uint LastAppliedCode,
        uint ApplyCount,
        uint LastMethodPtr,
        string LastMethodText);
}

