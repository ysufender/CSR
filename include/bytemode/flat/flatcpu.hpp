#pragma once

#include <limits>
#include <memory>

#include "bytemode/flat/flatram.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"
#include "slice.hpp"


class FlatCPU
{
    friend class FlatVM;

    public:
        struct State
        {
            sysbit_t eax { 0 };
            sysbit_t ebx { 0 };
            sysbit_t ecx { 0 };
            sysbit_t edx { 0 };
            sysbit_t esi { 0 };
            sysbit_t edi { 0 };

            sysbit_t pc { 0 };
            sysbit_t sp { 0 };
            sysbit_t bp { 0 };

            uchar_t al { 0 };
            uchar_t bl { 0 };
            uchar_t cl { 0 };
            uchar_t dl { 0 };
            uchar_t flg { 0 };
        };

        FlatCPU() = delete;
        VM_INLINE FlatCPU(FlatRAM& ram) : ram(ram) { }
        VM_INLINE FlatCPU(FlatRAM& ram, State state) : ram(ram), state(state) { }
        // VM_INLINE FlatCPU(FlatCPU&& other) : ram(other.ram), state(other.state) { }

        FlatCPU& operator=(FlatCPU&& other)
        {
            ram = std::move(other.ram);
            state = other.state;
            return *this;
        }
        
        VM_INLINE const State& DumpState() const noexcept
        { return this->state; }

        VM_INLINE void LoadState(const State& loadFrom) noexcept
        { this->state = loadFrom; }

        VM_INLINE const System::ErrorCode Push(const char value) noexcept
        {
            if (this->state.sp+1 > this->ram.StackSize())
            {
                LOGE(
                    System::LogLevel::High,
                    "In FlatCPU, can't push value onto stack, stack is full."
                );
                return System::ErrorCode::StackOverflow;
            }

            return this->ram.Write(this->state.sp++, value);
        }

        VM_INLINE const System::ErrorCode Pop() noexcept
        {
            if (this->state.sp < 1)
            {
                LOGE(
                        System::LogLevel::High,
                        "In FlatCPU, error while popping value from stack. Can't pop while SP < 1. const System::ErrorCode Code: ",
                        System::ErrorCodeString(System::ErrorCode::IndexOutOfBounds)
                    );

                return System::ErrorCode::IndexOutOfBounds;
            }

            this->state.sp--;
            return System::ErrorCode::Ok;
        }

        VM_INLINE const System::ErrorCode PushSome(const Slice values) noexcept
        {
            if (this->state.sp+values.size > this->ram.StackSize())
            {
                LOGE(
                    System::LogLevel::High,
                    "In FlatCPU, can't push value onto stack, stack is full."
                );
                return System::ErrorCode::StackOverflow;
            }

            const System::ErrorCode errc { this->ram.WriteSome(this->state.sp, values) };

            this->state.sp+=values.size;

            return errc;
        }

        VM_INLINE const System::ErrorCode PopSome(const sysbit_t size) noexcept
        {
            if (this->state.sp-size < 0)
            {
                LOGE(
                    System::LogLevel::High,
                    "In FlatCPU, error while popping value from stack. Can't pop while SP-size < 0. const System::ErrorCode Code: ",
                    System::ErrorCodeString(System::ErrorCode::IndexOutOfBounds)
                );

                return System::ErrorCode::IndexOutOfBounds;
            }

            this->state.sp -= size;
            return System::ErrorCode::Ok;
        }


    private: 
        FlatRAM& ram;
        State state;
};
