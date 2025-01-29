#pragma once

#include "CSRConfig.hpp"
#include "system.hpp"

struct Slice
{
    public:
        Slice(const char* const memory, const sysbit_t size);
        Slice(const Slice& other) = default;
        Slice(const Slice&& other) : data(other.data), size(other.size) { }

        Slice& operator=(const Slice& other) = delete;
        Slice& operator=(const Slice&& other) = delete;

        void* operator new(size_t) = delete;
        void operator delete(void*) = delete;
        void* operator new[](size_t) = delete;
        void operator delete[](void*) = delete;

        char operator[](const sysbit_t index) const;
        const System::ErrorCode TryRead(const sysbit_t index, char& out) noexcept;

        const char* const data;
        const sysbit_t size { 0 };
};
