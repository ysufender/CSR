#include <string>

#include "CSRConfig.hpp"
#include "bytemode/structured/assembly.hpp"
#include "system.hpp"
#include "bytemode/structured/rom.hpp"

//
// ROM Implementation
//
void ROM::operatorCrash(const sysbit_t index) const
{
    CRASH(
        System::ErrorCode::ROMAccessError,
        "In ", this->assembly.Stringify(), " can't access ROM index: ",
        std::to_string(index)
    );
}

Error ROM::TryRead(const sysbit_t index, uchar_t& data, const std::function<void()> failAct) const noexcept
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
