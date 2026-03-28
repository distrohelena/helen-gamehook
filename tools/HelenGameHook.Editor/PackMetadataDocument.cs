using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents the shared pack metadata stored in the workspace <c>pack.json</c> file.
/// </summary>
public sealed class PackMetadataDocument
{
    /// <summary>
    /// Gets or sets the schema version written to the workspace pack file.
    /// </summary>
    [JsonPropertyName("schemaVersion")]
    public int SchemaVersion { get; set; } = 1;

    /// <summary>
    /// Gets or sets the stable pack identifier used to locate and reference the workspace.
    /// </summary>
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the human-readable pack name shown in the editor.
    /// </summary>
    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the pack description shown to authors and users.
    /// </summary>
    [JsonPropertyName("description")]
    public string Description { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the logical game identifier stored in the pack target declaration.
    /// </summary>
    public string GameId { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the executable names matched by the workspace target declaration.
    /// </summary>
    public List<string> Executables { get; set; } = [];

    /// <summary>
    /// Gets or sets the feature declarations authored in the shared metadata file.
    /// </summary>
    [JsonPropertyName("features")]
    public List<FeatureDocument> Features { get; set; } = [];

    /// <summary>
    /// Gets or sets the config entry declarations authored in the shared metadata file.
    /// </summary>
    [JsonPropertyName("config")]
    public List<ConfigEntryDocument> ConfigEntries { get; set; } = [];
}
