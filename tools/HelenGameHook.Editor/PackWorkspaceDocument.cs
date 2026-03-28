namespace HelenGameHook.Editor;

/// <summary>
/// Represents an editable pack workspace composed of shared metadata and one or more build-specific documents.
/// </summary>
public sealed class PackWorkspaceDocument
{
    /// <summary>
    /// Gets or sets the shared metadata that is stored in <c>pack.json</c>.
    /// </summary>
    public PackMetadataDocument Metadata { get; set; } = new();

    /// <summary>
    /// Gets or sets the build documents loaded from the workspace <c>builds</c> directory.
    /// </summary>
    public List<BuildDocument> Builds { get; set; } = [];
}
