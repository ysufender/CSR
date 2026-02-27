#pragma once

#include <memory>

#include "extensions/syntaxextensions.hpp"
#include "bytemode/baserom.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"
#include "slice.hpp"

class FlatROM : public BaseROM
{
    public:
        VM_INLINE FlatROM() :
            data(nullptr),
            size(0)
        { }

        VM_INLINE FlatROM(std::unique_ptr<const char[]> data, sysbit_t size) :
            data(rval(data)),
            size(size)
        { }

        FlatROM(FlatROM&) = delete;
        void operator=(FlatROM const&) = delete;

        VM_INLINE void operator=(FlatROM&& other)
        {
            this->data = rval(other.data);
            this->size = other.size;
        }

        VM_INLINE uchar_t operator[](const sysbit_t index) const override
        {
            if (index >= size) [[unlikely]]
                CRASH(
                    System::ErrorCode::ROMAccessError,
                    "Can't access ROM index: ",
                    std::to_string(index)
                );
            return data[index];
        }

        VM_INLINE const char* operator&(sysbit_t index) const override
        {
            if (index >= this->size) [[unlikely]]
                CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of FlatROM is invalid.");

            return data.get()+index;
        }

        VM_INLINE const char* operator&() const override { return this->operator&(0); }

        VM_INLINE const Slice Data() const override { return { this->data.get(), this->size }; }
        VM_INLINE sysbit_t Size() const override { return this->size; }

        VM_INLINE uchar_t Read(sysbit_t index) const override { return (*this)[index]; }
        VM_INLINE const System::ErrorCode TryRead(sysbit_t index, uchar_t& data) const noexcept override
        {
            // ErrorCode::Bad == 1
            System::ErrorCode isOk { index >= this->size };
            if (isOk == System::ErrorCode::Ok)
                data = (*this)[index];
            return isOk;
        }

        VM_INLINE const Slice ReadSome(const sysbit_t index, const sysbit_t size) const override
        {
            if (index >= this->size || (index + size) > this->size) [[unlikely]]
                CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of FlatROM is invalid.");

            return {
                this->data.get()+index,
                size
            };
        }

    private:
        std::unique_ptr<const char[]> data;
        sysbit_t size;
};
