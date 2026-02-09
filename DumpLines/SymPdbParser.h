#pragma once

#include <windows.h>
#include <cor.h>
#include <CorHdr.h>
#include <corsym.h>
#include <string>
#include <vector>
#include "PdbCommon.h"

// Parser that uses ISymUnmanagedReader COM interface to read PDB files
class SymPdbParser
{
public:
    SymPdbParser();
    ~SymPdbParser();

    bool LoadPdbFile(const std::string& pdbFilePath);
    std::vector<MethodInfo> GetMethods();
    std::vector<std::string> GetSourceFiles();
    std::vector<TokenInfo> GetTokens();
    std::string GetGuid() const { return _guid; }
    DWORD GetAge() const { return _age; }

private:
    bool ComputeMethodsInfo();
    bool ComputeMethodsInfoByTypes();
    bool ComputeSourceFiles();
    bool ComputeTokens();
    bool GetMethodInfoFromSymbol(ISymUnmanagedMethod* pMethod, MethodInfo& info);

private:
    ISymUnmanagedReader* _pReader;
    IMetaDataImport* _pMetaDataImport;
    std::vector<MethodInfo> _methods;
    std::vector<std::string> _sourceFiles;
    std::vector<TokenInfo> _tokens;
    std::string _guid;
    DWORD _age;
    std::string _pdbFilePath;
};
