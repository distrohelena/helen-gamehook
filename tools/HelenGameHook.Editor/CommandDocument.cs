using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents one command declaration from a build workspace command file.
/// </summary>
public sealed class CommandDocument
{
    /// <summary>
    /// Gets or sets the stable command identifier.
    /// </summary>
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the human-readable command name.
    /// </summary>
    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the ordered command steps executed by the runtime.
    /// </summary>
    [JsonPropertyName("steps")]
    public List<CommandStepDocument> Steps { get; set; } = [];
}
