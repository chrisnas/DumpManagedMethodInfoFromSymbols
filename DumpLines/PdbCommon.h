#pragma once

#include <string>
#include <cstdint>

// Common structures used by both DbgHelpParser and SymPdbParser

struct MethodInfo
{
    std::string name;
    uint64_t modBase;
    uint64_t address;
    uint32_t size;
    uint32_t rva;
    uint32_t index;
    std::string sourceFile;
    uint32_t lineNumber;
};

struct TokenInfo
{
    uint32_t token;
    uint32_t index;
    uint32_t flags;
    uint64_t value;
    uint64_t address;
    uint32_t tag;
    std::string name;
};
