#include <Windows.h>
#include "DbgHelpParser.h"
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
    std::cout << "\nUsage: DumpLines <path to .pdb file>\n";
}

int main(int argc, char* argv[])
{
    ShowHeader();

    if (argc != 2)
    {
        ShowHelp("Missing .pdb file...");

        return -1;
    }

    std::string pdbFilename = argv[1];

    DbgHelpParser parser;
    if (!parser.LoadPdbFile(pdbFilename))
    {
        std::string error = "Failed to load PDB file: ";
        error += pdbFilename;
        ShowHelp(error.c_str());

        return -2;
    }

    printf("PDB File: %s\n", pdbFilename.c_str());
    printf("     Age: %u\n", parser.GetAge());
    printf("    GUID: %s\n", parser.GetGuid().c_str());
    printf("\n");

    std::vector<MethodInfo> methods = parser.GetMethods();
    if (methods.empty())
    {
        std::string error = "No methods found in PDB file: ";
        error += pdbFilename;
        ShowHelp(error.c_str());
        return -3;
    }

    printf("%-20s | %-18s | %-6s | %s\n",
        "Method Name", "Address", "Size", "Source Location");
    printf("%s\n", std::string(80, '-').c_str());

    for (const MethodInfo& sym : methods)
    {
        // Print method name and address
        printf("%-20s | 0x%016I64X | ",
            sym.name.c_str(),
            sym.address);

        // Print size
        if (sym.size > 0)
        {
            printf("%-6u | ", sym.size);
        }
        else
        {
            printf("%-6s | ", "N/A");
        }

        // Print source location
        if (!sym.sourceFile.empty() && sym.lineNumber > 0)
        {
            // Extract just the filename
            const char* fileName = sym.sourceFile.c_str();
            const char* lastSlash = strrchr(fileName, '\\');
            if (!lastSlash) lastSlash = strrchr(fileName, '/');
            if (lastSlash) fileName = lastSlash + 1;

            printf("%s:%u", fileName, sym.lineNumber);
        }
        else
        {
            printf("N/A");
        }

        printf("\n");
    }

    return 0;
}
