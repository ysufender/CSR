#pragma once

#include <functional>
#include <memory>

#include "CSRConfig.hpp"
#include "slice.hpp"
#include "system.hpp"
#include "bytemode/baserom.hpp"

class Assembly;

class ROM : public BaseROM
{
    friend class Assembly;

    public:
        ROM(const Assembly& assembly) : assembly(assembly)
        { }

        ROM(ROM&) = delete;
        void operator=(ROM const&) = delete;
        void operator=(ROM const&&) = delete;

        VM_INLINE uchar_t operator[](const sysbit_t index) const override
        {
            if (index >= size || index < 0)
                operatorCrash(index);

            return data[index];
        }

        VM_INLINE const char* operator&(sysbit_t index) const override
        {
            if (index >= size || index < 0)
                CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of ROM is invalid.");

            return data.get()+index;
        }

        VM_INLINE const char* operator&() const override { return this->operator&(0); }

        VM_INLINE const Slice Data() const override { return { this->data.get(), this->size }; }
        VM_INLINE sysbit_t Size() const override { return this->size; }

        VM_INLINE uchar_t Read(sysbit_t index) const override { return static_cast<uchar_t>((*this)[index]); }
        Error TryRead(sysbit_t index, uchar_t& data, std::function<void()> failAct = { }) const noexcept;

        VM_INLINE const Slice ReadSome(const sysbit_t index, const sysbit_t size) const override
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
