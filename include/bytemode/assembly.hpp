#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "system.hpp"
#include "bytemode/board.hpp"

using BoardCollection = std::unordered_map<uchar_t, Board>;

class ROM
{
    public:
        struct ROMIndex
        {
            bool isOk;
            char data;
        };

        ROM() = default;
        ROM(ROM&) = delete;
        void operator=(ROM const&) = delete;
        void operator=(ROM const&&) = delete;

        char* data = nullptr;
        systembit_t size = 0;
        ROMIndex operator[](systembit_t index) const;
};

class Assembly 
{
    public:
        enum class AssemblyType
        {
            Library,
            Executable
        };

        struct AssemblySettings
        {
            bool jit;
            std::string name;
            std::filesystem::path path;
            AssemblyType type;
        };


        Assembly(AssemblySettings& settings);
        Assembly() = delete;

        const System::ErrorCode Load() noexcept;
        const AssemblySettings& Settings() const noexcept;
        const ROM& Rom() const noexcept;
        const BoardCollection& Boards() const noexcept;

    private:
        ROM rom;
        AssemblySettings settings;
        BoardCollection boards;
};
