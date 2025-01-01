#include "bytemode/vm.hpp"

VM& VM::GetVM() const noexcept
{
    static VM singletonVM { };
    return singletonVM; 
}
