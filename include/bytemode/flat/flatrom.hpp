#pragma once

#include <memory>
#include <variant>

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

        VM_INLINE FlatROM(FlatROM&& other) :
            data(std::move(other.data)),
            size(other.size)
        { }

        FlatROM(FlatROM&) = delete;
        void operator=(FlatROM const&) = delete;

        VM_INLINE void operator=(FlatROM&& other)
        {
            this->data = rval(other.data);
            this->size = other.size;
        }

        VM_INLINE System::Result<uchar_t> operator[](const sysbit_t index) const override
        {
            if (index >= size) [[unlikely]]
            {
                LOGE(System::LogLevel::High, "Can't access ROM index: ", std::to_string(index));
                return System::ErrorCode::ROMAccessError;
            }

            [[likely]]
            return static_cast<uchar_t>(data[index]);
        }

        VM_INLINE System::Result<const char*> operator&(sysbit_t index) const override
        {
            if (index >= size) [[unlikely]]
            {
                LOGE(System::LogLevel::High, "Index '", std::to_string(index), "' of FlatROM is invalid.");
                return System::ErrorCode::ROMAccessError;
            }

            return data.get()+index;
        }

        VM_INLINE System::Result<const char*> operator&() const override { return this->operator&(0); }

        VM_INLINE System::Result<Slice> Data() const override { return Slice::New(this->data.get(), this->size); }
        VM_INLINE sysbit_t Size() const override { return this->size; }

        VM_INLINE System::Result<uchar_t> Read(sysbit_t index) const override { return (*this)[index]; }
        VM_INLINE const System::ErrorCode TryRead(sysbit_t index, uchar_t& data) const noexcept override
        {
            // ErrorCode::Bad == 1
            System::ErrorCode isOk { index >= this->size };
            if (isOk == System::ErrorCode::Ok)
            {
                System::Result<uchar_t> result {  };

                if (std::holds_alternative<uchar_t>(result))
                    data = std::get<uchar_t>(result);
                else
                    isOk = std::get<System::ErrorCode>(result);
            }
            return isOk;
        }

        VM_INLINE System::Result<Slice> ReadSome(const sysbit_t index, const sysbit_t size) const override
        {
            if (index >= this->size || (index + size) > this->size) [[unlikely]]
                CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of FlatROM is invalid.");

            return Slice::New(this->data.get()+index, size);
        }

    private:
        std::unique_ptr<const char[]> data;
        sysbit_t size;
};
