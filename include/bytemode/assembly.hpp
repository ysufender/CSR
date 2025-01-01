#pragma once

#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "bytemode/board.hpp"

class Assembly 
{
    public:
        enum class AssemblyType
        {
            Static,
            Shared,
            Executable
        };

        struct AssemblySettings
        {
            bool jit;
            std::string name;
            std::string path;
            AssemblyType type;
        };

        std::unordered_map<uchar_t, Board> boards;

        Assembly(AssemblySettings&& settings);

        const AssemblySettings& Settings() const noexcept;

    private:
        //const ROM rom;
        AssemblySettings settings;
};
