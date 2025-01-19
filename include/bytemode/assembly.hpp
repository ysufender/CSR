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
    friend class Assembly;

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

        ROMIndex operator[](systembit_t index) const;

    private:
        char* data = nullptr;
        systembit_t size = 0;
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


        Assembly() = delete;
        Assembly(AssemblySettings&& settings);

        const System::ErrorCode Load() noexcept;
        const AssemblySettings& Settings() const noexcept;
        const ROM& Rom() const noexcept;
        const BoardCollection& Boards() const noexcept;
        const System::ErrorCode Run();

    private:
        ROM rom;
        AssemblySettings settings;
        BoardCollection boards;
};
