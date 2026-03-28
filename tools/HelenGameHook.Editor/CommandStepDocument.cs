using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents one step declaration inside a build command.
/// </summary>
public sealed class CommandStepDocument
{
    /// <summary>
    /// Gets or sets the step kind used by the runtime command dispatcher.
    /// </summary>
    [JsonPropertyName("kind")]
    public string Kind { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the config key read or written by the step when applicable.
    /// </summary>
    [JsonPropertyName("configKey")]
    public string ConfigKey { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the named temporary value produced or consumed by the step.
    /// </summary>
    [JsonPropertyName("valueName")]
    public string ValueName { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the named target written by the step when applicable.
    /// </summary>
    [JsonPropertyName("target")]
    public string Target { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the message logged by the step when applicable.
    /// </summary>
    [JsonPropertyName("message")]
    public string Message { get; set; } = string.Empty;
}
