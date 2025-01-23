#pragma once

#include <filesystem>
#include <functional>
#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "system.hpp"
#include "bytemode/board.hpp"

using BoardCollection = std::unordered_map<systembit_t, Board>;

class ROM
{
    friend class Assembly;

    public:
        ROM() = default;
        ROM(ROM&) = delete;
        void operator=(ROM const&) = delete;
        void operator=(ROM const&&) = delete;

        char operator[](systembit_t index) const noexcept;
        char* operator&(systembit_t index) const noexcept;
        char* operator&() const noexcept;
        bool TryRead(systembit_t index, char& data, bool raise = false, std::function<void()> failAct = { }) const;

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
            systembit_t id;
        };

        Assembly() = delete;
        Assembly(AssemblySettings&& settings);

        const System::ErrorCode Load() noexcept;
        const AssemblySettings& Settings() const noexcept;
        const ROM& Rom() const noexcept;
        const BoardCollection& Boards() const noexcept;
        const System::ErrorCode Run();
        std::string Stringify() const noexcept;

    private:
        ROM rom;
        AssemblySettings settings;
        BoardCollection boards;
};
