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

        char operator[](sysbit_t index) const;
        const char* operator&(sysbit_t index) const;
        const char* operator&() const;

        const Slice Data() const { return { this->data.get(), this->size }; }
        sysbit_t Size() const { return this->size; }

        char Read(sysbit_t index) const { return (*this)[index]; }
        Error TryRead(sysbit_t index, char& data, std::function<void()> failAct = { }) const noexcept;
        const Slice ReadSome(const sysbit_t index, const sysbit_t size) const;

    private:
        std::unique_ptr<char[]> data = nullptr;
        sysbit_t size = 0;
        const Assembly& assembly;
};
