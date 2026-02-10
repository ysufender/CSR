#pragma once

#include "CSRConfig.hpp"
#include "system.hpp"

struct Slice
{
    private:
        VM_INLINE Slice(const char* const memory, const sysbit_t size) :
            data(memory), size(size)
        { }

    public:
        VM_INLINE Slice(const Slice& other) : data(other.data), size(other.size) { }
        VM_INLINE Slice(const Slice&& other) : data(other.data), size(other.size) { }

        static VM_INLINE __attribute__((hot)) System::Result<Slice> New(const char* memory, const sysbit_t size)
        {
            if (memory == nullptr) [[unlikely]]
            {
                LOGE(System::LogLevel::High, "Can't initialize Slice with nullptr");
                return System::ErrorCode::Bad;
            }

            [[likely]]
            return Slice { memory, size };
        }

        VM_INLINE System::Result<char> operator[](const sysbit_t index) const
        {
            if (index < 0 || index >= size) [[unlikely]]
            {
                LOGE(System::LogLevel::High, "Can't access out-of-bounds memory in a slice."); 
                return System::ErrorCode::IndexOutOfBounds;
            }

            [[likely]]
            return data[index];
        }

        VM_INLINE System::ErrorCode TryRead(const sysbit_t index, char& out) noexcept
        {
            if (index < 0 || index >= size) [[unlikely]]
                return System::ErrorCode::IndexOutOfBounds;

            [[likely]]
            out = data[index];
            return System::ErrorCode::Ok;
        }

        Slice& operator=(const Slice& other) = delete;
        Slice& operator=(const Slice&& other) = delete;

        void* operator new(size_t) = delete;
        void operator delete(void*) = delete;
        void* operator new[](size_t) = delete;
        void operator delete[](void*) = delete;

        const char* const data;
        const sysbit_t size { 0 };
};
