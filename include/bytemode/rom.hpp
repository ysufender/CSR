#pragma once

#include <functional>
#include <memory>

#include "CSRConfig.hpp"
#include "bytemode/slice.hpp"
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

        char operator[](sysbit_t index) const noexcept;
        const char* operator&(sysbit_t index) const noexcept;
        const char* operator&() const noexcept;

        const Slice Data() { return { this->data.get(), this->size }; }
        sysbit_t Size() { return this->size; }

        const System::ErrorCode TryRead(sysbit_t index, char& data, std::function<void()> failAct = { }) const noexcept;
        const Slice ReadSome(const sysbit_t index, const sysbit_t size) const;

    private:
        std::unique_ptr<char[]> data = nullptr;
        sysbit_t size = 0;
        const Assembly& assembly;
};
