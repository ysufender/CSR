#pragma once

#include <string>
#include <cstddef>
#include <utility>
#include <vector>
#include <unordered_map>

#include "CSRConfig.hpp"
#include "system.hpp"

namespace AssemblyFlags
{
    inline constexpr uchar_t Shared       = 1;
    inline constexpr uchar_t Static       = 2;
    inline constexpr uchar_t Executable   = 4;
    inline constexpr uchar_t SymbolInfo   = 8;
    inline constexpr uchar_t StoreName    = 16; // For possible future debugging
};

struct AssemblyInfo
{
    public:
        using SymbolInfo = std::pair<size_t, sysbit_t>;
        using ImportCollection = std::vector<std::string>;
        using UnknownSymbolCollection = std::unordered_map<SymbolInfo::first_type, SymbolInfo::second_type>;
        using SymbolMap = std::unordered_map<size_t, sysbit_t>;

#ifdef TOOLCHAIN_MODE
        AssemblyInfo() = default;

        AssemblyInfo(
            const std::string& path,
            uchar_t flags,
            const UnknownSymbolCollection& unknownSymbols,
            const ImportCollection& runtimeImports,
            const SymbolMap& symbolMap
        )
            : path(path),
            flags(flags),
            unknownSymbols(unknownSymbols),
            runtimeImports(runtimeImports),
            symbolMap(symbolMap)
        {}
#endif

        Error Deserialize(std::istream& inFile) noexcept;

        VM_INLINE std::string_view Path() const { return path; }
        VM_INLINE uchar_t Flags() const { return flags; }
        VM_INLINE bool HasSymbol(const size_t symbol) const { return symbolMap.contains(symbol); }
        VM_INLINE const SymbolMap& Symbols() const { return symbolMap; }
        VM_INLINE sysbit_t Symbol(const size_t symbol) const { 
            if (symbolMap.contains(symbol)) [[likely]]
                return symbolMap.at(symbol);
            [[unlikely]]
            return unknownSymbols.at(symbol);
        }

    private:
        std::string path { };
        uchar_t flags { };
        UnknownSymbolCollection unknownSymbols { };
        ImportCollection runtimeImports { };
        SymbolMap symbolMap { };
};
