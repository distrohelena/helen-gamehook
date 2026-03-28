using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents the executable fingerprint used to match a workspace build.
/// </summary>
public sealed class BuildMatchDocument
{
    /// <summary>
    /// Gets or sets the expected executable size in bytes.
    /// </summary>
    [JsonPropertyName("fileSize")]
    public long FileSize { get; set; }

    /// <summary>
    /// Gets or sets the expected executable SHA-256 fingerprint.
    /// </summary>
    [JsonPropertyName("sha256")]
    public string Sha256 { get; set; } = string.Empty;
}
