#include "SymPdbParser.h"
#include <atlbase.h>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cor.h>
#include <CorHdr.h>
#include <metahost.h>

// Link with ole32.lib for COM and mscoree.lib for metadata APIs
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "mscoree.lib")

// Define the GUIDs for ISymUnmanagedBinder and IMetaDataDispenser
#include <initguid.h>

// CLSID for CorSymBinder
// {0A29FF9E-7F9C-4437-8B11-F424491E3931}
DEFINE_GUID(CLSID_CorSymBinder_SxS,
    0x0A29FF9E, 0x7F9C, 0x4437, 0x8B, 0x11, 0xF4, 0x24, 0x49, 0x1E, 0x39, 0x31);

// IID for ISymUnmanagedBinder
// {AA544D42-28CB-11d3-BD22-0000F80849BD}
DEFINE_GUID(IID_ISymUnmanagedBinder,
    0xAA544D42, 0x28CB, 0x11d3, 0xBD, 0x22, 0x00, 0x00, 0xF8, 0x08, 0x49, 0xBD);


SymPdbParser::SymPdbParser()
    : _pReader(nullptr)
    , _pMetaDataImport(nullptr)
    , _age(0)
{
}

SymPdbParser::~SymPdbParser()
{
    if (_pReader != nullptr)
    {
        _pReader->Release();
        _pReader = nullptr;
    }
    if (_pMetaDataImport != nullptr)
    {
        _pMetaDataImport->Release();
        _pMetaDataImport = nullptr;
    }
}

bool SymPdbParser::LoadPdbFile(const std::string& pdbFilePath)
{
    _pdbFilePath = pdbFilePath;

    // Check if file exists
    if (GetFileAttributesA(pdbFilePath.c_str()) == INVALID_FILE_ATTRIBUTES)
    {
        return false;
    }

    // Derive the assembly file path from the PDB file path
    // Replace .pdb extension with .dll or .exe
    std::string assemblyPath = pdbFilePath;
    size_t dotPos = assemblyPath.rfind(".pdb");
    if (dotPos == std::string::npos || dotPos != assemblyPath.length() - 4)
    {
        return false; // Not a .pdb file
    }

    // Try .dll first, then .exe
    // NOTE: .exe are not managed assemblies in .NET Core so .dll first
    std::string dllPath = assemblyPath.substr(0, dotPos) + ".dll";
    std::string exePath = assemblyPath.substr(0, dotPos) + ".exe";

    std::string moduleFilePath;
    if (GetFileAttributesA(dllPath.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        moduleFilePath = dllPath;
    }
    else
    if (GetFileAttributesA(exePath.c_str()) != INVALID_FILE_ATTRIBUTES)
    {
        moduleFilePath = exePath;
    }
    else
    {
        return false; // Cannot find corresponding assembly file
    }

    // Convert paths to wide strings
    int len = MultiByteToWideChar(CP_ACP, 0, pdbFilePath.c_str(), -1, NULL, 0);
    std::wstring wPdbPath(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, pdbFilePath.c_str(), -1, &wPdbPath[0], len);

    len = MultiByteToWideChar(CP_ACP, 0, moduleFilePath.c_str(), -1, NULL, 0);
    std::wstring wModulePath(len, L'\0');
    MultiByteToWideChar(CP_ACP, 0, moduleFilePath.c_str(), -1, &wModulePath[0], len);

    // Create metadata dispenser using the CLR hosting API for .NET Framework 4.0+
    CComPtr<ICLRMetaHost> pMetaHost;
    HRESULT hr = CLRCreateInstance(CLSID_CLRMetaHost, IID_ICLRMetaHost, (void**)&pMetaHost);
    if (FAILED(hr))
    {
        return false;
    }

    // Get the installed .NET Framework runtime (v4.0+)
    CComPtr<ICLRRuntimeInfo> pRuntimeInfo;
    hr = pMetaHost->GetRuntime(L"v4.0.30319", IID_ICLRRuntimeInfo, (void**)&pRuntimeInfo);
    if (FAILED(hr))
    {
        return false;
    }

    // Get the metadata dispenser interface from the runtime
    CComPtr<IMetaDataDispenser> pDispenser;
    hr = pRuntimeInfo->GetInterface(CLSID_CorMetaDataDispenser, IID_IMetaDataDispenser, (void**)&pDispenser);
    if (FAILED(hr))
    {
        return false;
    }

    // Open the metadata scope from the assembly file
    hr = pDispenser->OpenScope(
        wModulePath.c_str(),
        ofRead,
        IID_IMetaDataImport,
        (IUnknown**)&_pMetaDataImport
    );

    if (FAILED(hr) || (_pMetaDataImport == nullptr))
    {
        return false;
    }

    // Create the symbol binder
    CComPtr<ISymUnmanagedBinder> pBinder;
    hr = CoCreateInstance(
        CLSID_CorSymBinder_SxS,
        NULL,
        CLSCTX_INPROC_SERVER,
        IID_ISymUnmanagedBinder,
        (void**)&pBinder
    );

    if (FAILED(hr))
    {
        return false;
    }

    // Get symbol reader from the module file (not PDB) with metadata import
    // GetReaderForFile expects the module path and will automatically find the PDB
    hr = pBinder->GetReaderForFile(_pMetaDataImport, wModulePath.c_str(), nullptr, &_pReader);
    if (FAILED(hr))
    {
        return false;
    }

    // Note: ISymUnmanagedReader doesn't expose GUID/Age directly
    // We would need to parse the PDB stream directly or use another method
    // For now, we use placeholder values
    _guid = "N/A (ISymUnmanagedReader)";
    _age = 0;

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

bool SymPdbParser::GetMethodInfoFromSymbol(ISymUnmanagedMethod* pMethod, MethodInfo& info)
{
    if (pMethod == nullptr)
    {
        return false;
    }

    // Get method token
    mdMethodDef token = 0;
    HRESULT hr = pMethod->GetToken(&token);
    if (FAILED(hr))
    {
        return false;
    }

    info.index = token;
    info.modBase = 0;
    info.address = 0;
    info.size = 0;
    info.rva = token;

    // Get method name from metadata because not available from ISymUnmanagedMethod
    if (_pMetaDataImport != nullptr)
    {
        WCHAR methodName[1024];
        ULONG cchMethodName = 0;
        mdTypeDef classToken = 0;
        DWORD methodAttr = 0;
        PCCOR_SIGNATURE sigBlob = nullptr;
        ULONG sigBlobSize = 0;
        ULONG codeRVA = 0;
        DWORD implFlags = 0;

        hr = _pMetaDataImport->GetMethodProps(
            token,
            &classToken,
            methodName,
            1024,
            &cchMethodName,
            &methodAttr,
            &sigBlob,
            &sigBlobSize,
            &codeRVA,
            &implFlags
        );

        if (SUCCEEDED(hr))
        {
            // Convert method name to narrow string
            int len = WideCharToMultiByte(CP_UTF8, 0, methodName, -1, NULL, 0, NULL, NULL);
            std::string narrowMethodName(len - 1, '\0');
            WideCharToMultiByte(CP_UTF8, 0, methodName, -1, &narrowMethodName[0], len, NULL, NULL);
            info.name = narrowMethodName;
        }
        else
        {
            // Fallback to token if we can't get the name
            std::ostringstream oss;
            oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << token;
            info.name = oss.str();
        }
    }
    else
    {
        // Fallback to token if metadata import is not available
        std::ostringstream oss;
        oss << "0x" << std::hex << std::setw(8) << std::setfill('0') << token;
        info.name = oss.str();
    }

    // Get sequence points (source line information)
    ULONG32 cPoints = 0;
    hr = pMethod->GetSequencePointCount(&cPoints);
    if (SUCCEEDED(hr) && (cPoints > 0))
    {
        cPoints = 1; // We only need the first sequence point for start line
        std::vector<ULONG32> offsets(cPoints);
        std::vector<ULONG32> lines(cPoints);
        std::vector<ULONG32> columns(cPoints);
        std::vector<ULONG32> endLines(cPoints);
        std::vector<ULONG32> endColumns(cPoints);
        std::vector<ISymUnmanagedDocument*> documents(cPoints);

        ULONG32 actualCount = 0;
        hr = pMethod->GetSequencePoints(
            cPoints,
            &actualCount,
            &offsets[0],
            &documents[0],
            &lines[0],
            &columns[0],
            &endLines[0],
            &endColumns[0]
        );

        if (SUCCEEDED(hr) && (actualCount > 0))
        {
            // Get the first sequence point's document and line
            ISymUnmanagedDocument* pDoc = documents[0];
            if (pDoc != nullptr)
            {
                // Get document URL (file path)
                ULONG32 urlLen = 0;
                hr = pDoc->GetURL(0, &urlLen, NULL);
                if (SUCCEEDED(hr) && (urlLen > 0))
                {
                    std::vector<WCHAR> url(urlLen);
                    hr = pDoc->GetURL(urlLen, &urlLen, &url[0]);
                    if (SUCCEEDED(hr))
                    {
                        // Convert wide string to narrow string
                        int len = WideCharToMultiByte(CP_UTF8, 0, &url[0], urlLen, NULL, 0, NULL, NULL);
                        std::string narrowUrl(len, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, &url[0], urlLen, &narrowUrl[0], len, NULL, NULL);
                        info.sourceFile = narrowUrl;
                    }
                }

                // NOTE: 0xFEEFEE is a special value indicating hidden lines
                info.lineNumber = lines[0];
                pDoc->Release();
            }
        }
    }

    if (info.sourceFile.empty())
    {
        info.sourceFile = "";
        info.lineNumber = 0;
    }

    return true;
}

bool SymPdbParser::ComputeMethodsInfo()
{
    if (_pReader == nullptr)
    {
        return false;
    }

    HRESULT hr;

    // Try to enumerate all method tokens
    // Since we don't have direct access to all methods, we'll try a range of common tokens
    // This is a workaround since ISymUnmanagedReader doesn't have a direct EnumMethods API
    for (ULONG32 token = 0x06000001; token < 0x06010000; token++)
    {
        ISymUnmanagedMethod* pMethod = nullptr;
        hr = _pReader->GetMethod(token, &pMethod);
        if (SUCCEEDED(hr))
        {
            MethodInfo info;
            if (GetMethodInfoFromSymbol(pMethod, info))
            {
                _methods.push_back(info);
            }
            pMethod->Release();
        }
    }

    // NOTE: methods are by design sorted by token

    return true;
}

bool SymPdbParser::ComputeSourceFiles()
{
    if (_pReader == nullptr)
    {
        return false;
    }

    // Get all documents (source files)
    ULONG32 docCount = 0;
    HRESULT hr = _pReader->GetDocuments(0, &docCount, NULL);
    if (FAILED(hr))
    {
        return false;
    }

    if (docCount == 0)
    {
        return false;
    }

    std::vector<ISymUnmanagedDocument*> documents(docCount);
    ULONG32 actualDocCount = 0;
    hr = _pReader->GetDocuments(docCount, &actualDocCount, &documents[0]);
    if (FAILED(hr))
    {
        return false;
    }

    for (ULONG32 i = 0; i < actualDocCount; i++)
    {
        ISymUnmanagedDocument* pDoc = documents[i];
        if (!pDoc)
        {
            continue;
        }

        // Get document URL (file path)
        ULONG32 urlLen = 0;
        hr = pDoc->GetURL(0, &urlLen, NULL);
        if (SUCCEEDED(hr) && urlLen > 0)
        {
            std::vector<WCHAR> url(urlLen);
            hr = pDoc->GetURL(urlLen, &urlLen, &url[0]);
            if (SUCCEEDED(hr))
            {
                // Convert wide string to narrow string
                int len = WideCharToMultiByte(CP_UTF8, 0, &url[0], urlLen, NULL, 0, NULL, NULL);
                std::string narrowUrl(len, '\0');
                WideCharToMultiByte(CP_UTF8, 0, &url[0], urlLen, &narrowUrl[0], len, NULL, NULL);
                _sourceFiles.push_back(narrowUrl);
            }
        }

        pDoc->Release();
    }

    // Sort alphabetically
    std::sort(_sourceFiles.begin(), _sourceFiles.end());

    return true;
}

bool SymPdbParser::ComputeTokens()
{
    // ISymUnmanagedReader doesn't have a direct way to enumerate all tokens
    // We'll try a range of common metadata tokens
    // NOTE: we could use the metadata import to enumerate methods instead
    if (_pReader == nullptr)
    {
        return false;
    }

    // method tokens are from the 0x06000000 MethodDef table
    for (ULONG32 token = 0x06000001; token < 0x06010000; token++)
    {
        ISymUnmanagedMethod* pMethod = nullptr;
        HRESULT hr = _pReader->GetMethod(token, &pMethod);
        if (SUCCEEDED(hr))
        {
            TokenInfo info;
            info.token = token;
            info.index = token;
            info.flags = 0;
            info.value = 0;
            info.address = 0;
            info.tag = 0;

            std::ostringstream oss;
            oss << " 0x" << std::hex << std::setw(8) << std::setfill('0') << token;
            info.name = oss.str();

            _tokens.push_back(info);
            pMethod->Release();
        }
    }

    return true;
}

std::vector<MethodInfo> SymPdbParser::GetMethods()
{
    return _methods;
}

std::vector<std::string> SymPdbParser::GetSourceFiles()
{
    return _sourceFiles;
}

std::vector<TokenInfo> SymPdbParser::GetTokens()
{
    return _tokens;
}
