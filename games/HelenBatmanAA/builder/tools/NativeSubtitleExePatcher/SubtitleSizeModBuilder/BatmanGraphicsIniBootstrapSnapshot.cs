namespace SubtitleSizeModBuilder;

/// <summary>
/// Represents the normalized graphics-option values that should be visible when the menu opens.
/// </summary>
internal sealed class BatmanGraphicsIniBootstrapSnapshot
{
    /// <summary>
    /// Gets the normalized fullscreen state where <c>0</c> means windowed and <c>1</c> means fullscreen.
    /// </summary>
    public int Fullscreen { get; }

    /// <summary>
    /// Gets the normalized horizontal resolution in pixels.
    /// </summary>
    public int ResolutionWidth { get; }

    /// <summary>
    /// Gets the normalized vertical resolution in pixels.
    /// </summary>
    public int ResolutionHeight { get; }

    /// <summary>
    /// Gets the normalized VSync toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int Vsync { get; }

    /// <summary>
    /// Gets the normalized MSAA index used by the frontend row.
    /// </summary>
    public int Msaa { get; }

    /// <summary>
    /// Gets the normalized detail-level preset index or the custom sentinel value.
    /// </summary>
    public int DetailLevel { get; }

    /// <summary>
    /// Gets the normalized bloom toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int Bloom { get; }

    /// <summary>
    /// Gets the normalized dynamic-shadows toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int DynamicShadows { get; }

    /// <summary>
    /// Gets the normalized motion-blur toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int MotionBlur { get; }

    /// <summary>
    /// Gets the normalized distortion toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int Distortion { get; }

    /// <summary>
    /// Gets the normalized fog-volumes toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int FogVolumes { get; }

    /// <summary>
    /// Gets the normalized spherical-harmonic-lighting toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int SphericalHarmonicLighting { get; }

    /// <summary>
    /// Gets the normalized ambient-occlusion toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int AmbientOcclusion { get; }

    /// <summary>
    /// Gets the normalized PhysX level integer used by the frontend row.
    /// </summary>
    public int Physx { get; }

    /// <summary>
    /// Gets the normalized stereo toggle where <c>0</c> means disabled and <c>1</c> means enabled.
    /// </summary>
    public int Stereo { get; }

    /// <summary>
    /// Initializes a snapshot with the exact normalized values that should be baked into the frontend script fallbacks.
    /// </summary>
    /// <param name="fullscreen">The normalized fullscreen state where <c>0</c> means windowed and <c>1</c> means fullscreen.</param>
    /// <param name="resolutionWidth">The normalized horizontal resolution in pixels.</param>
    /// <param name="resolutionHeight">The normalized vertical resolution in pixels.</param>
    /// <param name="vsync">The normalized VSync toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="msaa">The normalized MSAA index used by the frontend row.</param>
    /// <param name="detailLevel">The normalized detail-level preset index or the custom sentinel value.</param>
    /// <param name="bloom">The normalized bloom toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="dynamicShadows">The normalized dynamic-shadows toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="motionBlur">The normalized motion-blur toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="distortion">The normalized distortion toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="fogVolumes">The normalized fog-volumes toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="sphericalHarmonicLighting">The normalized spherical-harmonic-lighting toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="ambientOcclusion">The normalized ambient-occlusion toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    /// <param name="physx">The normalized PhysX level integer used by the frontend row.</param>
    /// <param name="stereo">The normalized stereo toggle where <c>0</c> means disabled and <c>1</c> means enabled.</param>
    public BatmanGraphicsIniBootstrapSnapshot(
        int fullscreen,
        int resolutionWidth,
        int resolutionHeight,
        int vsync,
        int msaa,
        int detailLevel,
        int bloom,
        int dynamicShadows,
        int motionBlur,
        int distortion,
        int fogVolumes,
        int sphericalHarmonicLighting,
        int ambientOcclusion,
        int physx,
        int stereo)
    {
        Fullscreen = fullscreen;
        ResolutionWidth = resolutionWidth;
        ResolutionHeight = resolutionHeight;
        Vsync = vsync;
        Msaa = msaa;
        DetailLevel = detailLevel;
        Bloom = bloom;
        DynamicShadows = dynamicShadows;
        MotionBlur = motionBlur;
        Distortion = distortion;
        FogVolumes = fogVolumes;
        SphericalHarmonicLighting = sphericalHarmonicLighting;
        AmbientOcclusion = ambientOcclusion;
        Physx = physx;
        Stereo = stereo;
    }
}
