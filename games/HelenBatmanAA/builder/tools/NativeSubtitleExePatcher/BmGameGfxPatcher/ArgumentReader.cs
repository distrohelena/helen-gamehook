namespace BmGameGfxPatcher;

internal sealed class ArgumentReader
{
    private readonly Dictionary<string, string?> values = new(StringComparer.Ordinal);
    private readonly HashSet<string> flags = new(StringComparer.Ordinal);
    private readonly List<string> unknown = [];

    public ArgumentReader(string[] args)
    {
        for (int index = 0; index < args.Length; index++)
        {
            string token = args[index];

            if (!token.StartsWith("--", StringComparison.Ordinal))
            {
                unknown.Add(token);
                continue;
            }

            if (index + 1 < args.Length && !args[index + 1].StartsWith("--", StringComparison.Ordinal))
            {
                values[token] = args[index + 1];
                index++;
            }
            else
            {
                flags.Add(token);
            }
        }
    }

    public string RequireValue(string name)
    {
        string? value = GetValue(name);
        if (string.IsNullOrWhiteSpace(value))
        {
            throw new InvalidOperationException($"Missing required option '{name}'.");
        }

        return value;
    }

    public string? GetValue(string name)
    {
        return values.TryGetValue(name, out string? value) ? value : null;
    }

    public bool GetFlag(string name)
    {
        return flags.Contains(name);
    }

    public int? GetOptionalInt32(string name)
    {
        string? value = GetValue(name);
        if (string.IsNullOrWhiteSpace(value))
        {
            return null;
        }

        if (!int.TryParse(value, out int parsed))
        {
            throw new InvalidOperationException($"Option '{name}' must be a valid integer.");
        }

        return parsed;
    }

    public double? GetOptionalDouble(string name)
    {
        string? value = GetValue(name);
        if (string.IsNullOrWhiteSpace(value))
        {
            return null;
        }

        if (!double.TryParse(value, out double parsed))
        {
            throw new InvalidOperationException($"Option '{name}' must be a valid number.");
        }

        return parsed;
    }

    public void ThrowIfAnyUnknown()
    {
        if (unknown.Count == 0)
        {
            return;
        }

        throw new InvalidOperationException($"Unknown argument(s): {string.Join(", ", unknown)}");
    }
}
