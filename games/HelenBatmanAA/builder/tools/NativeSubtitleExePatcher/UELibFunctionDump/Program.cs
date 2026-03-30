using System.IO;
using System.Reflection;
using System.Text;
using UELib;
using UELib.Core;

Encoding.RegisterProvider(CodePagesEncodingProvider.Instance);

if (args.Length == 0)
{
    PrintUsage();
    return 1;
}

string command = args[0];

if (!command.StartsWith("-", StringComparison.Ordinal) &&
    command is not "dump-function" &&
    command is not "inspect-object" &&
    command is not "inspect-property-meta" &&
    command is not "find-name" &&
    args.Length == 3)
{
    return DumpFunction(args[0], args[1], args[2]);
}

return command switch
{
    "dump-function" when args.Length == 4 => DumpFunction(args[1], args[2], args[3]),
    "inspect-object" when args.Length == 4 => InspectObject(args[1], args[2], args[3]),
    "inspect-property-meta" when args.Length == 4 => InspectPropertyMeta(args[1], args[2], args[3]),
    "find-name" when args.Length == 3 => FindName(args[1], args[2]),
    _ => FailUsage()
};

static int DumpFunction(string packagePathArg, string ownerName, string functionName)
{
    UnrealPackage package = LoadPackage(packagePathArg);

    List<UFunction> matches = new();
    List<string> candidatePaths = new();

    foreach (UObject? obj in package.Objects)
    {
        if (obj is not UFunction function)
        {
            continue;
        }

        string name = function.Name.ToString();
        string owner = function.Outer?.Name.ToString() ?? string.Empty;

        if (name.Equals(functionName, StringComparison.Ordinal))
        {
            candidatePaths.Add(function.GetPath());
        }

        if (!name.Equals(functionName, StringComparison.Ordinal) ||
            !owner.Equals(ownerName, StringComparison.Ordinal))
        {
            continue;
        }

        function.Load();
        matches.Add(function);
    }

    if (matches.Count == 0)
    {
        Console.Error.WriteLine($"No function found for {ownerName}.{functionName}");
        if (candidatePaths.Count > 0)
        {
            Console.Error.WriteLine("Candidates:");
            foreach (string path in candidatePaths)
            {
                Console.Error.WriteLine(path);
            }
        }

        return 2;
    }

    foreach (UFunction function in matches)
    {
        Console.WriteLine($"Path: {function.GetPath()}");
        Console.WriteLine($"ScriptOffset: {function.ScriptOffset}");
        Console.WriteLine($"ScriptSize: {function.ScriptSize}");
        Console.WriteLine();
        Console.WriteLine(function.Decompile());
    }

    return 0;
}

static int InspectObject(string packagePathArg, string ownerName, string objectName)
{
    UnrealPackage package = LoadPackage(packagePathArg);

    List<UObject> matches = new();
    List<string> candidatePaths = new();

    foreach (UObject? obj in package.Objects)
    {
        if (obj is null)
        {
            continue;
        }

        string name = obj.Name.ToString();
        string owner = obj.Outer?.Name.ToString() ?? string.Empty;

        if (name.Equals(objectName, StringComparison.Ordinal))
        {
            candidatePaths.Add(obj.GetPath());
        }

        if (!name.Equals(objectName, StringComparison.Ordinal) ||
            !owner.Equals(ownerName, StringComparison.Ordinal))
        {
            continue;
        }

        obj.Load();
        matches.Add(obj);
    }

    if (matches.Count == 0)
    {
        Console.Error.WriteLine($"No object found for {ownerName}.{objectName}");
        if (candidatePaths.Count > 0)
        {
            Console.Error.WriteLine("Candidates:");
            foreach (string path in candidatePaths)
            {
                Console.Error.WriteLine(path);
            }
        }

        return 2;
    }

    foreach (UObject obj in matches)
    {
        Console.WriteLine($"Path: {obj.GetPath()}");
        Console.WriteLine($"Class: {obj.Class?.Name}");
        Console.WriteLine($"Outer: {obj.Outer?.Name}");
        Console.WriteLine($"Properties: {obj.Properties.Count}");

        if (obj is UFunction function)
        {
            TryPrintFunctionMetadata(function);
        }

        foreach (UDefaultProperty property in obj.Properties)
        {
            string value = property.Value ?? string.Empty;
            value = value.Replace("\r", "\\r").Replace("\n", "\\n");
            Console.WriteLine(
                $"{property.Name,-24} type={property.Type,-16} size={property.Size,4} array={property.ArrayIndex,3} value={value}");
        }

        Console.WriteLine();
    }

    return 0;
}

static int InspectPropertyMeta(string packagePathArg, string ownerName, string objectName)
{
    UnrealPackage package = LoadPackage(packagePathArg);

    List<UProperty> matches = new();
    List<string> candidatePaths = new();

    foreach (UObject? obj in package.Objects)
    {
        if (obj is null)
        {
            continue;
        }

        string name = obj.Name.ToString();
        string owner = obj.Outer?.Name.ToString() ?? string.Empty;

        if (name.Equals(objectName, StringComparison.Ordinal))
        {
            candidatePaths.Add(obj.GetPath());
        }

        if (obj is not UProperty property ||
            !name.Equals(objectName, StringComparison.Ordinal) ||
            !owner.Equals(ownerName, StringComparison.Ordinal))
        {
            continue;
        }

        property.Load();
        matches.Add(property);
    }

    if (matches.Count == 0)
    {
        Console.Error.WriteLine($"No property found for {ownerName}.{objectName}");
        if (candidatePaths.Count > 0)
        {
            Console.Error.WriteLine("Candidates:");
            foreach (string path in candidatePaths)
            {
                Console.Error.WriteLine(path);
            }
        }

        return 2;
    }

    foreach (UProperty property in matches)
    {
        Console.WriteLine($"Path: {property.GetPath()}");
        Console.WriteLine($"Class: {property.Class?.Name}");
        Console.WriteLine($"Outer: {property.Outer?.Name}");
        Console.WriteLine($"Type: {property.Type}");
        Console.WriteLine($"ElementSize: {property.ElementSize}");
        Console.WriteLine($"RepOffset: {property.RepOffset}");
        Console.WriteLine($"CategoryIndex: {property.CategoryIndex}");
        Console.WriteLine($"ObjectFlags: {property.ObjectFlags}");

        foreach (KeyValuePair<string, object?> item in GetRelevantMembers(property))
        {
            Console.WriteLine($"{item.Key}: {FormatValue(item.Value)}");
        }

        object? binaryMetaData = property.BinaryMetaData;
        object? fieldsStack = binaryMetaData?.GetType().GetField("Fields")?.GetValue(binaryMetaData);
        if (fieldsStack is System.Collections.IEnumerable fields)
        {
            Console.WriteLine("BinaryMetaData:");
            foreach (object field in fields)
            {
                Type fieldType = field.GetType();
                object? name = fieldType.GetProperty("Name")?.GetValue(field);
                object? offset = fieldType.GetProperty("Offset")?.GetValue(field);
                object? size = fieldType.GetProperty("Size")?.GetValue(field);
                object? value = fieldType.GetProperty("Value")?.GetValue(field);
                object? tag = fieldType.GetProperty("Tag")?.GetValue(field);
                Console.WriteLine(
                    $"  {name,-24} offset={FormatValue(offset),-8} size={FormatValue(size),-8} value={FormatValue(value)} tag={FormatValue(tag)}");
            }
        }

        Console.WriteLine();
    }

    return 0;
}

static int FindName(string packagePathArg, string objectName)
{
    UnrealPackage package = LoadPackage(packagePathArg);
    bool foundAny = false;

    foreach (UObject? obj in package.Objects)
    {
        if (obj is null)
        {
            continue;
        }

        string name = obj.Name.ToString();
        if (!name.Equals(objectName, StringComparison.Ordinal))
        {
            continue;
        }

        foundAny = true;
        string outer = obj.Outer?.Name.ToString() ?? string.Empty;
        string className = obj.Class?.Name.ToString() ?? string.Empty;

        Console.WriteLine($"Path: {obj.GetPath()}");
        Console.WriteLine($"Class: {className}");
        Console.WriteLine($"Outer: {outer}");

        if (obj is UFunction function)
        {
            function.Load();
            Console.WriteLine($"ScriptOffset: {function.ScriptOffset}");
            Console.WriteLine($"ScriptSize: {function.ScriptSize}");

            var nativeTokenProperty = typeof(UFunction).GetProperty("NativeToken");
            if (nativeTokenProperty is not null)
            {
                object? nativeToken = nativeTokenProperty.GetValue(function);
                Console.WriteLine($"NativeToken: {nativeToken}");
            }
        }

        Console.WriteLine();
    }

    if (!foundAny)
    {
        Console.Error.WriteLine($"No object found with name '{objectName}'.");
        return 2;
    }

    return 0;
}

static UnrealPackage LoadPackage(string packagePathArg)
{
    string packagePath = Path.GetFullPath(packagePathArg);
    UnrealPackage package = UnrealLoader.LoadPackage(
        packagePath,
        UnrealPackage.GameBuild.BuildName.Batman1,
        FileAccess.Read);

    package.InitializePackage(UnrealPackage.InitFlags.All);
    return package;
}

static int FailUsage()
{
    PrintUsage();
    return 1;
}

static void PrintUsage()
{
    Console.Error.WriteLine("Usage:");
    Console.Error.WriteLine("  UELibFunctionDump <package> <owner> <function-name>");
    Console.Error.WriteLine("  UELibFunctionDump dump-function <package> <owner> <function-name>");
    Console.Error.WriteLine("  UELibFunctionDump inspect-object <package> <owner> <object-name>");
    Console.Error.WriteLine("  UELibFunctionDump inspect-property-meta <package> <owner> <object-name>");
    Console.Error.WriteLine("  UELibFunctionDump find-name <package> <object-name>");
}

static IEnumerable<KeyValuePair<string, object?>> GetRelevantMembers(UProperty property)
{
    Type type = property.GetType();
    BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
    List<KeyValuePair<string, object?>> results = new();
    HashSet<string> seen = new(StringComparer.Ordinal);

    foreach (PropertyInfo info in type.GetProperties(flags).OrderBy(p => p.Name, StringComparer.Ordinal))
    {
        if (!IsRelevantMember(info.Name) || info.GetIndexParameters().Length != 0)
        {
            continue;
        }

        try
        {
            if (seen.Add($"P:{info.Name}"))
            {
                results.Add(new KeyValuePair<string, object?>($"Property.{info.Name}", info.GetValue(property)));
            }
        }
        catch
        {
        }
    }

    foreach (FieldInfo info in type.GetFields(flags).OrderBy(f => f.Name, StringComparer.Ordinal))
    {
        if (!IsRelevantMember(info.Name))
        {
            continue;
        }

        try
        {
            if (seen.Add($"F:{info.Name}"))
            {
                results.Add(new KeyValuePair<string, object?>($"Field.{info.Name}", info.GetValue(property)));
            }
        }
        catch
        {
        }
    }

    return results;
}

static void TryPrintFunctionMetadata(UFunction function)
{
    try
    {
        function.Load();
    }
    catch
    {
    }

    Console.WriteLine($"ScriptOffset: {function.ScriptOffset}");
    Console.WriteLine($"ScriptSize: {function.ScriptSize}");

    BindingFlags flags = BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic;
    PropertyInfo? nativeTokenProperty = typeof(UFunction).GetProperty("NativeToken", flags);
    if (nativeTokenProperty is not null)
    {
        try
        {
            object? nativeToken = nativeTokenProperty.GetValue(function);
            Console.WriteLine($"NativeToken: {FormatValue(nativeToken)}");
        }
        catch
        {
        }
    }

    FieldInfo? functionFlagsField = typeof(UFunction).GetField("FunctionFlags", flags);
    if (functionFlagsField is not null)
    {
        try
        {
            object? functionFlags = functionFlagsField.GetValue(function);
            Console.WriteLine($"FunctionFlags: {FormatValue(functionFlags)}");
        }
        catch
        {
        }
    }
}

static bool IsRelevantMember(string name)
{
    return name.Contains("Offset", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("Mask", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("Bit", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("Bool", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("Array", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("Size", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("Category", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("Rep", StringComparison.OrdinalIgnoreCase) ||
           name.Contains("PropertyFlags", StringComparison.OrdinalIgnoreCase);
}

static string FormatValue(object? value)
{
    return value switch
    {
        null => "<null>",
        Array array => $"[{array.Length}] {string.Join(", ", array.Cast<object?>().Select(FormatValue))}",
        _ => value.ToString() ?? string.Empty
    };
}
