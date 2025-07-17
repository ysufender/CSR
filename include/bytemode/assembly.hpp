#pragma once

#include <filesystem>
#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"
#include "bytemode/syscall.hpp"
#include "bytemode/board.hpp"
#include "bytemode/rom.hpp"

using BoardCollection = std::unordered_map<sysbit_t, Board>;

class Assembly : IMessageObject
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
            sysbit_t id;
        };

        Assembly() = delete;
        Assembly(AssemblySettings&& settings);

        const AssemblySettings& Settings() const noexcept 
        { return this->settings; }

        const ROM& Rom() const noexcept 
        { return this->rom; }

        const BoardCollection& Boards() const noexcept 
        { return this->boards; }

        Error DispatchMessages() noexcept override;
        Error ReceiveMessage(Message message) noexcept override;
        Error SendMessage(Message message) noexcept override;

        Error Load() noexcept;
        Error Run() noexcept;
        Error AddBoard() noexcept;
        Error RemoveBoard(sysbit_t id) noexcept;

        const std::string& Stringify() const noexcept;

        const SysCallHandler& SysCallHandler() const noexcept
        { return this->syscallHandler; }

    private:
        ROM rom { *this };
        AssemblySettings settings;
        BoardCollection boards;
        class SysCallHandler syscallHandler;

        mutable std::string reprStr;

        sysbit_t GenerateNewBoardID() const;
};
