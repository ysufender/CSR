#pragma once

#include "CSRConfig.hpp"
#include "slice.hpp"

class BaseROM
{
    public:
        BaseROM() = default;
        BaseROM(BaseROM&) = delete;
        void operator=(BaseROM const&) = delete;

        virtual uchar_t operator[](const sysbit_t index) const = 0;
        virtual const char* operator&(sysbit_t index) const = 0;
        virtual const char* operator&() const = 0;
        virtual const Slice Data() const = 0;
        virtual sysbit_t Size() const = 0;
        virtual uchar_t Read(sysbit_t index) const = 0;
        virtual const System::ErrorCode TryRead(sysbit_t index, uchar_t& data) const noexcept = 0;
        virtual const Slice ReadSome(const sysbit_t index, const sysbit_t size) const = 0;
};
