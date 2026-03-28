using System.Text.Json.Serialization;

namespace HelenGameHook.Editor;

/// <summary>
/// Represents one build workspace directory with its match data and split declaration files.
/// </summary>
public sealed class BuildDocument
{
    /// <summary>
    /// Gets or sets the stable build identifier used for the directory name.
    /// </summary>
    [JsonPropertyName("id")]
    public string Id { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the executable name matched by the build.
    /// </summary>
    [JsonPropertyName("executable")]
    public string Executable { get; set; } = string.Empty;

    /// <summary>
    /// Gets or sets the executable fingerprint used to match the build at runtime.
    /// </summary>
    [JsonPropertyName("match")]
    public BuildMatchDocument Match { get; set; } = new();

    /// <summary>
    /// Gets or sets the virtual file declarations stored in <c>files.json</c>.
    /// </summary>
    public List<VirtualFileDocument> VirtualFiles { get; set; } = [];

    /// <summary>
    /// Gets or sets the hook declarations stored in <c>hooks.json</c>.
    /// </summary>
    public List<HookDocument> Hooks { get; set; } = [];

    /// <summary>
    /// Gets or sets the command declarations stored in <c>commands.json</c>.
    /// </summary>
    public List<CommandDocument> Commands { get; set; } = [];

    /// <summary>
    /// Gets or sets the build file size through the nested match document for grid binding.
    /// </summary>
    [JsonIgnore]
    public long FileSize
    {
        get => Match.FileSize;
        set => Match.FileSize = value;
    }

    /// <summary>
    /// Gets or sets the build SHA-256 hash through the nested match document for grid binding.
    /// </summary>
    [JsonIgnore]
    public string Sha256
    {
        get => Match.Sha256;
        set => Match.Sha256 = value;
    }

    /// <summary>
    /// Gets the number of virtual file declarations loaded for the build.
    /// </summary>
    [JsonIgnore]
    public int VirtualFileCount => VirtualFiles.Count;

    /// <summary>
    /// Gets the number of hook declarations loaded for the build.
    /// </summary>
    [JsonIgnore]
    public int HookCount => Hooks.Count;

    /// <summary>
    /// Gets the number of command declarations loaded for the build.
    /// </summary>
    [JsonIgnore]
    public int CommandCount => Commands.Count;
}
