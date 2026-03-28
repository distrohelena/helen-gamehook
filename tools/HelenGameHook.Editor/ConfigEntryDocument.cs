using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents one config entry declaration from the shared workspace metadata.
/// </summary>
public sealed class ConfigEntryDocument
{
    /// <summary>
    /// Gets or sets the stable config key used by commands and feature bindings.
    /// </summary>
    [JsonPropertyName("key")]
    public string Key { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the config value type authored for the runtime.
    /// </summary>
    [JsonPropertyName("type")]
    public string Type { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the default integer value written for the config entry.
    /// </summary>
    [JsonPropertyName("defaultValue")]
    public int DefaultValue { get; set; }
}
