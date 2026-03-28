namespace HelenGameHook.Editor;

/// <summary>
/// Starts the WinForms editor against the repository sample workspace when it is available.
/// </summary>
internal static class Program
{
    /// <summary>
    /// Configures WinForms application services and opens the main editor window.
    /// </summary>
    [STAThread]
    private static void Main()
    {
        ApplicationConfiguration.Initialize();
        Application.Run(new MainForm());
    }
}
