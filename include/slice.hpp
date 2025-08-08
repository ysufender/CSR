#pragma once

#include "CSRConfig.hpp"
#include "system.hpp"

struct Slice
{
    public:
        inline __attribute__((hot)) Slice(const Slice& other) : data(other.data), size(other.size) { }
        inline __attribute__((hot)) Slice(const Slice&& other) : data(other.data), size(other.size) { }

        inline __attribute__((hot)) Slice(const char* const memory, const sysbit_t size) :
            data(memory), size(size)
        {
            if (memory == nullptr) [[unlikely]]
                CRASH(System::ErrorCode::Bad, "Can't initialize Slice with nullptr");
        }

        inline __attribute__((hot)) char operator[](const sysbit_t index) const
        {
            if (index < 0 || index >= size) [[unlikely]]
                CRASH(System::ErrorCode::IndexOutOfBounds, "Can't access out-of-bounds memory in a slice."); 
            return data[index];
        }

        inline __attribute__((hot)) Error TryRead(const sysbit_t index, char& out) noexcept
        {
            if (index < 0 || index >= size) [[unlikely]]
                return System::ErrorCode::IndexOutOfBounds;
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
