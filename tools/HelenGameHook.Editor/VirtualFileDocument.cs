using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents one virtual file declaration from a build workspace file list.
/// </summary>
public sealed class VirtualFileDocument
{
    /// <summary>
    /// Gets or sets the stable virtual file identifier.
    /// </summary>
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the in-game file path intercepted by the runtime.
    /// </summary>
    [JsonPropertyName("path")]
    public string Path { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the virtualization mode applied to the file.
    /// </summary>
    [JsonPropertyName("mode")]
    public string Mode { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the workspace-relative source file used by the virtualization rule.
    /// </summary>
    [JsonPropertyName("source")]
    public string Source { get; set; } = string.Empty;
}
