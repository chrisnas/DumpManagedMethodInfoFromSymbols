#pragma once

#include <windows.h>

// needed for symTag definitions
#define _NO_CVCONST_H
#include "DbgHelp.h"

#include <string>
#include <vector>


struct MethodInfo
{
    std::string name;
    uint64_t address;
    uint32_t size;
    std::string sourceFile;
    uint32_t lineNumber;
};


class DbgHelpParser
{
public:
    DbgHelpParser();
    ~DbgHelpParser();

    bool LoadPdbFile(const std::string& pdbFilePath);
    std::vector<MethodInfo> GetMethods();
    std::string GetGuid() const { return _guid; }
    DWORD GetAge() const { return _age; }

private:
    static BOOL CALLBACK EnumMethodSymbolsCallback(PSYMBOL_INFO pSymInfo, ULONG SymbolSize, PVOID UserContext);
    bool ComputeMethodsInfo();

private:
    HANDLE _hProcess;
    uint64_t _baseAddress;

    std::vector<MethodInfo> _methods;
    std::string _guid;
    DWORD _age;

};

