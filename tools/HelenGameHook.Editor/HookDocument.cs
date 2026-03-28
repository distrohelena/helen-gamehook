using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents one hook declaration from a build workspace hook file.
/// </summary>
public sealed class HookDocument
{
    /// <summary>
    /// Gets or sets the stable hook identifier.
    /// </summary>
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the module name scanned by the hook declaration.
    /// </summary>
    [JsonPropertyName("module")]
    public string Module { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the section name searched for the hook pattern.
    /// </summary>
    [JsonPropertyName("section")]
    public string Section { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the byte pattern used to find the hook site.
    /// </summary>
    [JsonPropertyName("pattern")]
    public string Pattern { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the expected byte sequence used to confirm the match.
    /// </summary>
    [JsonPropertyName("expectedBytes")]
    public string ExpectedBytes { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the hook action requested by the declaration.
    /// </summary>
    [JsonPropertyName("action")]
    public string Action { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the runtime command triggered by the hook action.
    /// </summary>
    [JsonPropertyName("command")]
    public string Command { get; set; } = string.Empty;
}
