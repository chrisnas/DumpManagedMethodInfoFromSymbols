#include <Windows.h>
#include "DbgHelpParser.h"
#include "SymPdbParser.h"
#include <iostream>

void ShowHeader()
{
    std::cout << "DumpLines v1.0 - Dump source code and start line for methods from Windows PDB file\n";
    std::cout << "by Christophe Nasarre\n\n";
}

void ShowHelp(const char* message)
{
    if (message != nullptr)
    {
        std::cout << "|> " << message << "\n";
    }
    std::cout << "\nUsage: DumpLines [options] <path to .pdb file>\n";
    std::cout << "\nOptions:\n";
    std::cout << "  --sym     : Use ISymUnmanagedReader parser instead of DbgHelp\n";
    std::cout << "  --source  : Dump list of source files instead of methods\n";
    std::cout << "  --token   : Dump list of managed tokens instead of methods\n";
    std::cout << "\nOptions can be combined. Default behavior shows methods with source locations.\n";
}

int main(int argc, char* argv[])
{
    // Initialize COM for ISymUnmanagedReader usage
    CoInitialize(NULL);

    ShowHeader();

    if (argc < 2)
    {
        ShowHelp("Invalid arguments...");
        CoUninitialize();
        return -1;
    }

    bool showSourceFiles = false;
    bool showTokens = false;
    bool useSymParser = false;
    std::string pdbFilename;

    // Parse command line arguments
    // Last argument is always the PDB filename
    pdbFilename = argv[argc - 1];

    // Check if last argument looks like a flag
    if (pdbFilename.length() > 2 && pdbFilename.substr(0, 2) == "--")
    {
        ShowHelp("Missing PDB filename...");
        CoUninitialize();
        return -1;
    }

    // Parse all arguments except the last one (which is the filename)
    for (int i = 1; i < argc - 1; i++)
    {
        std::string arg = argv[i];
        if (arg == "--source")
        {
            showSourceFiles = true;
        }
        else if (arg == "--token")
        {
            showTokens = true;
        }
        else if (arg == "--sym")
        {
            useSymParser = true;
        }
        else
        {
            std::string error = "Invalid option: ";
            error += arg;
            ShowHelp(error.c_str());
            CoUninitialize();
            return -1;
        }
    }

    // Check for conflicting options
    if (showSourceFiles && showTokens)
    {
        ShowHelp("Cannot combine --source and --token options");
        CoUninitialize();
        return -1;
    }

    // Choose parser based on command line argument
    std::vector<MethodInfo> methods;
    std::vector<std::string> sourceFiles;
    std::vector<TokenInfo> tokens;
    std::string guid;
    DWORD age;

    if (useSymParser)
    {
        SymPdbParser parser;
        if (!parser.LoadPdbFile(pdbFilename))
        {
            std::string error = "Failed to load PDB file with ISymUnmanagedReader: ";
            error += pdbFilename;
            ShowHelp(error.c_str());
            CoUninitialize();
            return -2;
        }

        printf("PDB File: %s (ISymUnmanagedReader)\n", pdbFilename.c_str());
        printf("     Age: %u\n", parser.GetAge());
        printf("    GUID: %s\n", parser.GetGuid().c_str());
        printf("\n");

        methods = parser.GetMethods();
        sourceFiles = parser.GetSourceFiles();
        tokens = parser.GetTokens();
        guid = parser.GetGuid();
        age = parser.GetAge();
    }
    else
    {
        DbgHelpParser parser;
        if (!parser.LoadPdbFile(pdbFilename))
        {
            std::string error = "Failed to load PDB file with DbgHelp: ";
            error += pdbFilename;
            ShowHelp(error.c_str());
            CoUninitialize();
            return -2;
        }

        printf("PDB File: %s (DbgHelp)\n", pdbFilename.c_str());
        printf("     Age: %u\n", parser.GetAge());
        printf("    GUID: %s\n", parser.GetGuid().c_str());
        printf("\n");

        methods = parser.GetMethods();
        sourceFiles = parser.GetSourceFiles();
        tokens = parser.GetTokens();
        guid = parser.GetGuid();
        age = parser.GetAge();
    }

    if (showSourceFiles)
    {
        // Dump source files
        if (sourceFiles.empty())
        {
            std::string error = "No source files found in PDB file: ";
            error += pdbFilename;
            ShowHelp(error.c_str());
            CoUninitialize();
            return -3;
        }

        printf("Source Files (%zu total):\n", sourceFiles.size());
        printf("%s\n", std::string(90, '-').c_str());

        for (const std::string& sourceFile : sourceFiles)
        {
            printf("%s\n", sourceFile.c_str());
        }
    }
    else if (showTokens)
    {
        // Dump managed tokens
        if (tokens.empty())
        {
            std::string error = "No tokens found in PDB file: ";
            error += pdbFilename;
            ShowHelp(error.c_str());
            CoUninitialize();
            return -3;
        }

        printf("Managed Tokens (%zu total):\n", tokens.size());
        printf("%-10s | %-10s | %-10s | %-18s | %-18s | %-6s | %s\n",
            "Token", "Index", "Flags", "Value", "Address", "Tag", "Name");
        printf("%s\n", std::string(120, '-').c_str());

        for (const TokenInfo& token : tokens)
        {
            printf("0x%08X | 0x%08X | 0x%08X | 0x%016I64X | 0x%016I64X | %-6u | %s\n",
                token.token,
                token.index,
                token.flags,
                token.value,
                token.address,
                token.tag,
                token.name.c_str());
        }
    }
    else
    {
        // Dump methods (default behavior)
        if (methods.empty())
        {
            std::string error = "No methods found in PDB file: ";
            error += pdbFilename;
            ShowHelp(error.c_str());
            CoUninitialize();
            return -3;
        }

        printf("Methods (%zu total)\n\n", methods.size());
        printf("%s\n", std::string(75, '-').c_str());
        printf("%-32s | %-10s | %s\n",
            "Method Name", "Token", "Source Location");
        printf("%s\n", std::string(75, '-').c_str());

        for (const MethodInfo& sym : methods)
        {
            // Print method name and token
            printf("%-32s | 0x%08X | ",
                sym.name.c_str(),
                sym.index
                );

            // Print source location
            if (!sym.sourceFile.empty() && sym.lineNumber > 0)
            {
                // Extract just the filename
                const char* fileName = sym.sourceFile.c_str();
                const char* lastSlash = strrchr(fileName, '\\');
                if (!lastSlash) lastSlash = strrchr(fileName, '/');
                if (lastSlash) fileName = lastSlash + 1;

                // 16707566 (0xFEEFEE) is a special marker for hidden/compiler-generated code
                if (sym.lineNumber == 16707566)
                {
                    printf("%s:hidden", fileName);
                }
                else
                {
                    printf("%s:%u", fileName, sym.lineNumber);
                }
            }
            else
            {
                printf("N/A");
            }

            printf("\n");
        }
    }

    CoUninitialize();
    return 0;
}
