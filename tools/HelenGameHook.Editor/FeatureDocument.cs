using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents one feature declaration from the shared workspace metadata.
/// </summary>
public sealed class FeatureDocument
{
    /// <summary>
    /// Gets or sets the stable feature identifier.
    /// </summary>
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the human-readable feature name.
    /// </summary>
    [JsonPropertyName("name")]
    public string Name { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the feature kind understood by the runtime and editor.
    /// </summary>
    [JsonPropertyName("kind")]
    public string Kind { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the config key used to persist the feature value.
    /// </summary>
    [JsonPropertyName("configKey")]
    public string ConfigKey { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the default integer value used when no user override exists yet.
    /// </summary>
    [JsonPropertyName("defaultValue")]
    public int DefaultValue { get; set; }
}
