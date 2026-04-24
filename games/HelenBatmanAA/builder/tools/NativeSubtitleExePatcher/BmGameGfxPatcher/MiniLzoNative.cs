using System.Runtime.InteropServices;

namespace BmGameGfxPatcher;

/// <summary>
/// Locates and loads the native MiniLZO helper DLL required by the compressed-package tools.
/// </summary>
internal static class MiniLzoNative
{
    /// <summary>
    /// Stores the MiniLZO DLL file name.
    /// </summary>
    private const string LibraryFileName = "MiniLzoDll.dll";

    /// <summary>
    /// Stores the Batman-local tools path that contains the checked-in MiniLZO DLL.
    /// </summary>
    private static readonly string[] BatmanToolRelativeSegments =
    [
        "games",
        "HelenBatmanAA",
        "builder",
        "tools",
        "NativeSubtitleExePatcher",
        "BmGameGfxPatcher",
        LibraryFileName
    ];

    /// <summary>
    /// Stores the Batman-local native project path that contains the built MiniLZO DLL.
    /// </summary>
    private static readonly string[] BatmanNativeRelativeSegments =
    [
        "games",
        "HelenBatmanAA",
        "builder",
        "tools",
        "NativeSubtitleExePatcher",
        "MiniLzoDll",
        LibraryFileName
    ];

    /// <summary>
    /// Serializes the one-time DLL load.
    /// </summary>
    private static readonly object SyncRoot = new();

    /// <summary>
    /// Stores the loaded native library handle after the first successful load.
    /// </summary>
    private static IntPtr libraryHandle = IntPtr.Zero;

    /// <summary>
    /// Ensures the MiniLZO native library is loaded before any P/Invoke call executes.
    /// </summary>
    public static void EnsureLoaded()
    {
        if (libraryHandle != IntPtr.Zero)
        {
            return;
        }

        lock (SyncRoot)
        {
            if (libraryHandle != IntPtr.Zero)
            {
                return;
            }

            Exception? lastLoadException = null;
            foreach (string candidatePath in EnumerateCandidatePaths())
            {
                if (!File.Exists(candidatePath))
                {
                    continue;
                }

                try
                {
                    libraryHandle = NativeLibrary.Load(candidatePath);
                    return;
                }
                catch (Exception ex) when (ex is BadImageFormatException || ex is DllNotFoundException)
                {
                    lastLoadException = ex;
                }
            }

            if (lastLoadException is not null)
            {
                throw new InvalidOperationException(
                    "MiniLzoDll.dll was found but could not be loaded from any candidate path.",
                    lastLoadException);
            }
        }

        throw new InvalidOperationException(
            "MiniLzoDll.dll was not found. Expected it next to the tool output or under the Batman builder tools directory.");
    }

    /// <summary>
    /// Enumerates the candidate DLL paths relative to the app base, working directory, and their ancestors.
    /// </summary>
    /// <returns>The ordered set of candidate DLL paths.</returns>
    private static IEnumerable<string> EnumerateCandidatePaths()
    {
        var yieldedPaths = new HashSet<string>(StringComparer.OrdinalIgnoreCase);
        var rootDirectories = new HashSet<string>(StringComparer.OrdinalIgnoreCase);

        foreach (string seedDirectory in new[] { AppContext.BaseDirectory, Directory.GetCurrentDirectory() })
        {
            if (string.IsNullOrWhiteSpace(seedDirectory))
            {
                continue;
            }

            string currentDirectory = Path.GetFullPath(seedDirectory);
            while (!string.IsNullOrWhiteSpace(currentDirectory) && rootDirectories.Add(currentDirectory))
            {
                foreach (string candidatePath in EnumerateCandidatePathsForRoot(currentDirectory))
                {
                    if (yieldedPaths.Add(candidatePath))
                    {
                        yield return candidatePath;
                    }
                }

                DirectoryInfo? parent = Directory.GetParent(currentDirectory);
                if (parent is null)
                {
                    break;
                }

                currentDirectory = parent.FullName;
            }
        }
    }

    /// <summary>
    /// Enumerates the candidate DLL paths rooted at one directory.
    /// </summary>
    /// <param name="rootDirectory">The directory used as the candidate root.</param>
    /// <returns>The candidate DLL paths for that root.</returns>
    private static IEnumerable<string> EnumerateCandidatePathsForRoot(string rootDirectory)
    {
        yield return Path.Combine(rootDirectory, LibraryFileName);
        yield return Path.Combine(rootDirectory, "MiniLzoDll", LibraryFileName);
        yield return CombinePathSegments(rootDirectory, BatmanToolRelativeSegments);
        yield return CombinePathSegments(rootDirectory, BatmanNativeRelativeSegments);
    }

    /// <summary>
    /// Combines one root directory with a fixed list of relative segments.
    /// </summary>
    /// <param name="rootDirectory">The root directory.</param>
    /// <param name="relativeSegments">The relative path segments to append.</param>
    /// <returns>The combined absolute path.</returns>
    private static string CombinePathSegments(string rootDirectory, IReadOnlyList<string> relativeSegments)
    {
        string combinedPath = rootDirectory;
        for (int index = 0; index < relativeSegments.Count; index++)
        {
            combinedPath = Path.Combine(combinedPath, relativeSegments[index]);
        }

        return combinedPath;
    }
}
