#pragma once

#include <functional>
#include <memory>

#include "CSRConfig.hpp"
#include "slice.hpp"
#include "system.hpp"

class Assembly;

class ROM
{
    friend class Assembly;

    public:
        ROM(const Assembly& assembly) : assembly(assembly)
        { }

        ROM(ROM&) = delete;
        void operator=(ROM const&) = delete;
        void operator=(ROM const&&) = delete;

        inline char operator[](const sysbit_t index) const
        {
            if (index >= size || index < 0)
                operatorCrash(index);

            return data[index];
        }

        inline const char* operator&(sysbit_t index) const
        {
            if (index >= size || index < 0)
                CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of ROM is invalid.");

            return data.get()+index;
        }

        inline const char* operator&() const { return this->operator&(0); }

        const Slice Data() const { return { this->data.get(), this->size }; }
        sysbit_t Size() const { return this->size; }

        char Read(sysbit_t index) const { return (*this)[index]; }
        Error TryRead(sysbit_t index, char& data, std::function<void()> failAct = { }) const noexcept;

        inline const Slice ReadSome(const sysbit_t index, const sysbit_t size) const
        {
            if (index >= this->size || index < 0 || (index + size) > this->size)
                CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of ROM is invalid.");

            return {
                this->data.get()+index,
                size
            };
        }


    private:
        void operatorCrash(const sysbit_t index) const;
        std::unique_ptr<char[]> data = nullptr;
        sysbit_t size = 0;
        const Assembly& assembly;
};
