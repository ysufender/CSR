#pragma once

#include <filesystem>
#include <functional>
#include <memory>
#include <unordered_map>
#include <string>

#include "CSRConfig.hpp"
#include "message.hpp"
#include "system.hpp"
#include "bytemode/board.hpp"

using BoardCollection = std::unordered_map<sysbit_t, Board>;

class ROM
{
    friend class Assembly;

    public:
        ROM(const Assembly& assembly) : assembly(assembly)
        { }

        ROM(ROM&) = delete;
        void operator=(ROM const&) = delete;
        void operator=(ROM const&&) = delete;

        char operator[](sysbit_t index) const noexcept;
        const char* operator&(sysbit_t index) const noexcept;
        const char* operator&() const noexcept;
        const System::ErrorCode TryRead(sysbit_t index, char& data, std::function<void()> failAct = { }) const noexcept;

    private:
        std::unique_ptr<char[]> data = nullptr;
        sysbit_t size = 0;
        const Assembly& assembly;
};

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

        inline const AssemblySettings& Settings() const noexcept 
        { return this->settings; }

        inline const ROM& Rom() const noexcept 
        { return this->rom; }

        inline const BoardCollection& Boards() const noexcept 
        { return this->boards; }

        const System::ErrorCode DispatchMessages() noexcept override;
        const System::ErrorCode ReceiveMessage(Message message) noexcept override;
        const System::ErrorCode SendMessage(Message message) noexcept override;

        const System::ErrorCode Load() noexcept;
        const System::ErrorCode Run() noexcept;
        const System::ErrorCode AddBoard() noexcept;
        const System::ErrorCode RemoveBoard(sysbit_t id) noexcept;

        const std::string& Stringify() const noexcept;

    private:
        ROM rom { *this };
        AssemblySettings settings;
        BoardCollection boards;

        mutable std::string reprStr;

        sysbit_t GenerateNewBoardID() const;
};
