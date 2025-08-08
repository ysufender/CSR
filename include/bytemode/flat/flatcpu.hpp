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
        VM_INLINE FlatCPU(FlatRAM& ram) : ram(ram), paramBuf(std::make_unique_for_overwrite<char[]>(std::numeric_limits<uchar_t>::max())) { }
        
        VM_INLINE const State& DumpState() const noexcept
        { return this->state; }

        VM_INLINE void LoadState(const State& loadFrom) noexcept
        { this->state = loadFrom; }

        VM_INLINE Error Push(const char value) noexcept
        {
            if (this->state.sp+1 > this->ram.StackSize())
            {
                LOGE(
                    System::LogLevel::Medium,
                    "In FlatCPU, can't push value onto stack, stack is full."
                );
                return System::ErrorCode::StackOverflow;
            }

            return this->ram.Write(this->state.sp++, value);

            //if (errc == System::ErrorCode::Ok)
                //this->state.sp++;
            //else
            //    LOGE(
            //        System::LogLevel::Medium,
            //        "In FlatCPU, error while pushing value onto stack. Error code: ",
            //        System::ErrorCodeString(errc)
            //    );

            //return errc;
        }

        VM_INLINE Error Pop() noexcept
        {
            if (this->state.sp < 1)
            {
                LOGE(
                        System::LogLevel::Medium,
                        "In FlatCPU, error while popping value from stack. Can't pop while SP < 1. Error Code: ",
                        System::ErrorCodeString(System::ErrorCode::IndexOutOfBounds)
                    );

                return System::ErrorCode::IndexOutOfBounds;
            }

            this->state.sp--;
            return Error::Ok;
        }

        VM_INLINE Error PushSome(const Slice values) noexcept
        {
            if (this->state.sp+values.size > this->ram.StackSize())
            {
                LOGE(
                    System::LogLevel::Medium,
                    "In FlatCPU, can't push value onto stack, stack is full."
                );
                return System::ErrorCode::StackOverflow;
            }

            Error errc { this->ram.WriteSome(this->state.sp, values) };

            //if (errc == System::ErrorCode::Ok)
                this->state.sp+=values.size;
            //else
            //    LOGE(
            //        System::LogLevel::Medium,
            //        "In FlatCPU, error while pushing value onto stack. Error code: ",
            //        System::ErrorCodeString(errc)
            //    );

            return errc;
        }

        VM_INLINE Error PopSome(const sysbit_t size) noexcept
        {
            if (this->state.sp-size < 0)
            {
                LOGE(
                    System::LogLevel::Medium,
                    "In FlatCPU, error while popping value from stack. Can't pop while SP-size < 0. Error Code: ",
                    System::ErrorCodeString(System::ErrorCode::IndexOutOfBounds)
                );

                return System::ErrorCode::IndexOutOfBounds;
            }

            this->state.sp -= size;
            return Error::Ok;
        }


    private: 
        FlatRAM& ram;
        State state;
        std::unique_ptr<char[]> paramBuf;
};
