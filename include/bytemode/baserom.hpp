#pragma once

#include <type_traits>

#include "CSRConfig.hpp"
#include "slice.hpp"

class BaseROM
{
    public:
        BaseROM() = default;
        BaseROM(BaseROM&) = delete;
        void operator=(BaseROM const&) = delete;

        virtual System::Result<uchar_t> operator[](const sysbit_t index) const = 0;
        virtual System::Result<const char*> operator&(sysbit_t index) const = 0;
        virtual System::Result<const char*> operator&() const = 0;
        virtual System::Result<Slice> Data() const = 0;
        virtual sysbit_t Size() const = 0;
        virtual System::Result<uchar_t> Read(sysbit_t index) const = 0;
        virtual const System::ErrorCode TryRead(sysbit_t index, std::make_unsigned_t<char>& data) const noexcept = 0;
        virtual System::Result<Slice> ReadSome(const sysbit_t index, const sysbit_t size) const = 0;
};
