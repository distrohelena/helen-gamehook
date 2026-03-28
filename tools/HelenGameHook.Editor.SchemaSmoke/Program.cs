using HelenGameHook.Editor;

namespace HelenGameHook.Editor.SchemaSmoke;

/// <summary>
/// Provides a minimal serializer smoke test for the split workspace sample pack.
/// </summary>
internal static class Program
{
    /// <summary>
    /// Loads the repository sample workspace and confirms that shared metadata and builds are present.
    /// </summary>
    /// <param name="args">Unused command-line arguments.</param>
    /// <returns>Zero when the sample workspace loads successfully.</returns>
    private static int Main(string[] args)
    {
        _ = args;

        string packRoot = PackWorkspaceSerializer.GetRepositorySamplePackRoot();
        PackWorkspaceDocument workspace = PackWorkspaceSerializer.Load(packRoot);

        if (workspace.Metadata.Id != "batman-aa-subtitles")
        {
            throw new InvalidOperationException("Workspace metadata did not load.");
        }

        if (workspace.Builds.Count == 0)
        {
            throw new InvalidOperationException("Workspace builds did not load.");
        }

        Console.WriteLine("PASS");
        return 0;
    }
}
