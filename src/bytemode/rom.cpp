#include "CSRConfig.hpp"
#include "bytemode/assembly.hpp"
#include "bytemode/slice.hpp"
#include "system.hpp"
#include <string>
#include "bytemode/rom.hpp"

//
// ROM Implementation
//
char ROM::operator[](const sysbit_t index) const
{
    if (index >= size || index < 0)
        CRASH(
            System::ErrorCode::ROMAccessError,
            "In ", this->assembly.Stringify(), " can't access ROM index: ",
            std::to_string(index)
        );

    return data[index];
}

const char* ROM::operator&(const sysbit_t index) const
{
    if (index >= size || index < 0)
        CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of ROM is invalid.");

    return data.get()+index;
}

const char* ROM::operator&() const
{
    return this->operator&(0); 
}

const Slice ROM::ReadSome(const sysbit_t index, const sysbit_t size) const
{
    if (index >= this->size || index < 0 || (index + size) > this->size)
        CRASH(System::ErrorCode::ROMAccessError, "Index '", std::to_string(index), "' of ROM is invalid.");

    return {
        this->data.get()+index,
        size
    };
}

const System::ErrorCode ROM::TryRead(const sysbit_t index, char& data, const std::function<void()> failAct) const noexcept
{
    System::ErrorCode isOk { index >= this->size || index < 0 };

    if (isOk == System::ErrorCode::Ok)
    {
        data = (*this)[index];
        return System::ErrorCode::Ok;
    }

    LOGE(
        System::LogLevel::Medium, 
        "Cannot access index '", std::to_string(index), "' of ROM ",
        this->assembly.Stringify()
    );
    if (failAct)
        failAct();
    return System::ErrorCode::ROMAccessError;
}
