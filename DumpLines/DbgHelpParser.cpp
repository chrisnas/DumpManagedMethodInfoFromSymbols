#include "DbgHelpParser.h"

#include <algorithm>
#include <sstream>
#include <string>

// Link with dbghelp.lib
#pragma comment(lib, "dbghelp.lib")


DbgHelpParser::DbgHelpParser()
    :
    _baseAddress(0),
    _age(0)
{
    DWORD options = SymGetOptions();
    options |= SYMOPT_DEBUG;
    options |= SYMOPT_LOAD_LINES;           // Load line number information
    options |= SYMOPT_UNDNAME;              // Undecorate symbol names
    //options |= SYMOPT_DEFERRED_LOADS;       // Defer symbol loading
    options |= SYMOPT_EXACT_SYMBOLS;        // Require exact symbol match
    options |= SYMOPT_FAIL_CRITICAL_ERRORS; // Don't show error dialogs
    SymSetOptions(options);

    _hProcess = GetCurrentProcess();
    if (!SymInitialize(_hProcess, NULL, FALSE))
    {
        _hProcess = NULL;
    }
}

DbgHelpParser::~DbgHelpParser()
{
    if (_hProcess != NULL)
    {
        if (_baseAddress != 0)
        {
            SymUnloadModule64(_hProcess, _baseAddress);
            _baseAddress = 0;
        }

        SymCleanup(_hProcess);
        _hProcess = NULL;
    }
}

bool DbgHelpParser::LoadPdbFile(const std::string& pdbFilePath)
{
    if (_hProcess == NULL)
    {
        return false;
    }

    // BUG? : dbghelp does not fail if the .pdb file does not exist...
    if (GetFileAttributesA(pdbFilePath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    _baseAddress = SymLoadModuleEx(
        _hProcess,
        NULL,
        pdbFilePath.c_str(),
        NULL,
        0x10000000, // arbitrary base address
        0,
        NULL,
        0
    );

    if (_baseAddress == 0)
    {
        return false;
    }

    IMAGEHLP_MODULE64 moduleInfo = { 0 };
    moduleInfo.SizeOfStruct = sizeof(IMAGEHLP_MODULE64);
    if (!SymGetModuleInfo64(_hProcess, _baseAddress, &moduleInfo))
    {
        return false;
    }

    _age = moduleInfo.PdbAge;
    GUID guid = moduleInfo.PdbSig70;
    char strGUID[80];
    sprintf_s(strGUID, 80, "%08x%04x%04x%02x%02x%02x%02x%02x%02x%02x%02x",
        guid.Data1, guid.Data2, guid.Data3,
        guid.Data4[0], guid.Data4[1], guid.Data4[2], guid.Data4[3],
        guid.Data4[4], guid.Data4[5], guid.Data4[6], guid.Data4[7]
        );
    _guid = strGUID;

    // Compute method info
    if (!ComputeMethodsInfo())
    {
        return false;
    }

    // Compute source files
    if (!ComputeSourceFiles())
    {
        return false;
    }

    // Compute tokens
    if (!ComputeTokens())
    {
        return false;
    }

    return true;
}

BOOL CALLBACK DbgHelpParser::EnumMethodSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext)
{
    DbgHelpParser* parser = reinterpret_cast<DbgHelpParser*>(UserContext);

    if (
        (pSymInfo->Tag == SymTagFunction)
        //&& ((pSymInfo->Flags & (SYMFLAG_CLR_TOKEN | SYMFLAG_METADATA)) == (SYMFLAG_CLR_TOKEN | SYMFLAG_METADATA))
        )
    {
        MethodInfo info;
        info.name = pSymInfo->Name;
        info.modBase = pSymInfo->ModBase;
        info.address = pSymInfo->Address;
        info.size = pSymInfo->Size;
        info.rva = static_cast<uint32_t>(pSymInfo->Address - info.modBase);
        info.index = pSymInfo->Index;

        // Try to get source file and line information
        IMAGEHLP_LINE64 line = { 0 };
        line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
        DWORD displacement = 0;

        if (SymGetLineFromAddr64(parser->_hProcess, pSymInfo->Address, &displacement, &line))
        {
            info.sourceFile = line.FileName ? line.FileName : "";
            info.lineNumber = line.LineNumber;
        }
        else {
            info.sourceFile = "";
            info.lineNumber = 0;
        }

        parser->_methods.push_back(info);
    }

    return TRUE; // Continue enumeration
}

bool DbgHelpParser::ComputeMethodsInfo()
{
    if (!SymEnumSymbols(
            _hProcess,
            _baseAddress,
            "*!*",  // Mask (all symbols)
            EnumMethodSymbolsCallback,
            this    // User context to store the methods in _methods instance field
    ))
    {
        return false;
    }

    // sort by address
    std::sort(_methods.begin(), _methods.end(),
        [](const MethodInfo& a, const MethodInfo& b)
        {
            return a.address < b.address;
        });
    return true;
}


std::vector<MethodInfo> DbgHelpParser::GetMethods()
{
    return _methods;
}

BOOL CALLBACK DbgHelpParser::EnumSourceFilesCallback(PSOURCEFILE pSourceFile, PVOID UserContext)
{
    DbgHelpParser* parser = reinterpret_cast<DbgHelpParser*>(UserContext);

    if (pSourceFile && pSourceFile->FileName)
    {
        parser->_sourceFiles.push_back(pSourceFile->FileName);
    }

    return TRUE; // Continue enumeration
}

bool DbgHelpParser::ComputeSourceFiles()
{
    if (!SymEnumSourceFiles(
            _hProcess,
            _baseAddress,
            "*",  // Mask (all source files)
            EnumSourceFilesCallback,
            this  // User context to store the source files in _sourceFiles instance field
    ))
    {
        return false;
    }

    // Sort alphabetically
    std::sort(_sourceFiles.begin(), _sourceFiles.end());

    return true;
}

std::vector<std::string> DbgHelpParser::GetSourceFiles()
{
    return _sourceFiles;
}

bool DbgHelpParser::ComputeTokens()
{
    // Allocate buffer for SYMBOL_INFO
    const size_t bufferSize = sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR);
    BYTE buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
    PSYMBOL_INFO pSymInfo = (PSYMBOL_INFO)buffer;

    // Start from token 1 and increment until SymFromToken returns false
    ULONG token = 1;
    while (true)
    {
        memset(buffer, 0, bufferSize);
        pSymInfo->SizeOfStruct = sizeof(SYMBOL_INFO);
        pSymInfo->MaxNameLen = MAX_SYM_NAME;

        if (!SymFromToken(_hProcess, _baseAddress, token, pSymInfo))
        {
            // No more tokens
            break;
        }

        TokenInfo info;
        info.token = token;
        info.index = pSymInfo->Index;
        info.flags = pSymInfo->Flags;
        info.value = pSymInfo->Value;
        info.address = pSymInfo->Address;
        info.tag = pSymInfo->Tag;
        info.name = pSymInfo->Name ? pSymInfo->Name : "";

        _tokens.push_back(info);
        token++;
    }

    return true;
}

std::vector<TokenInfo> DbgHelpParser::GetTokens()
{
    return _tokens;
}
