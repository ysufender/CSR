#include "slice.hpp"
#include "CSRConfig.hpp"
#include "system.hpp"

Slice::Slice(const char* const memory, const sysbit_t size) :
    data(memory), size(size)
{
    if (memory == nullptr)
        CRASH(System::ErrorCode::Bad, "Can't initialize Slice with nullptr");
}

char Slice::operator[](const sysbit_t index) const
{
    if (index < 0 || index >= size)
        CRASH(System::ErrorCode::IndexOutOfBounds, "Can't access out-of-bounds memory in a slice."); 
    return data[index];
}

Error Slice::TryRead(const sysbit_t index, char& out) noexcept
{
    if (index < 0 || index >= size)
        return System::ErrorCode::IndexOutOfBounds;
    out = data[index];
    return System::ErrorCode::Ok;
}
