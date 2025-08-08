#ifndef __CSR__NATIVE__CALL__HEADER__
#define __CSR__NATIVE__CALL__HEADER__

#ifdef __cplusplus
extern "C" {
#include <cstdint>
#else
#include "stdint.h"
#endif

#define ret(size) \
    params[0] = size; \
    return ReturnCode::Ok;

const uint8_t VM_API_VERSION = 0;

namespace ReturnCode
{
    const char Ok = 0;
    const char Bad = 1;
    const char UnhandledException = 2;
    const char ROMAccessError = 3;
    const char RAMAccessError = 4;
    const char SourceFileNotFound = 5;
    const char UnsupportedFileType = 6;
    const char HeapOverflow = 7;
    const char StackOverflow = 8;
    const char NoSourceFile = 9;
    const char InvalidSpecifier = 10;
    const char FileIOError = 11;
    const char MessageSendError = 12;
    const char IndexOutOfBounds = 13;
    const char InvalidInstruction = 14;
    const char MessageReceiveError = 15;
    const char MessageDispatchError = 16;
    const char MemoryOverflow = 17;
    const char NotImplemented = 18;
    const char FragmentedHeap = 19;
    const char StackUnderflow = 20;
    const char DuplicateSysBind = 21;
    const char InvalidKey = 22;
    const char DLLoadError = 23;
    const char DLSymbolError = 24;
    const char VMError = 25;
    const char DLInitError = 26;
    const char DoubleFree = 27;
    const char Shutdown = 28;
    const char ProcessInterrupt = 29;
    const char IOError = 30;
    const char NativeCallError = 31;
}

typedef char Code;

typedef struct VMContext {
    uint32_t size;
    void* context;
    int (*Validate)(VMContext* context, uint8_t version);
    void* (*GetRealAddress)(VMContext*, const uint32_t addr);
    void* (*Allocate)(VMContext*, const uint32_t size);
    Code (*Deallocate)(VMContext*, const uint32_t addr, const uint32_t size);
    Code (*BindFunction)(VMContext*, const uint32_t id, char (*handler)(VMContext* context, char* paramBuffer));
    Code (*UnbindFunction)(VMContext*, const uint32_t id);
} VMContext;

typedef char (*SysFunctionHandler)(VMContext* context, char* paramBuffer);
typedef SysFunctionHandler sysfnh_t;

// Initializer function signature for extender DLs
// name must be specifically InitExtender
typedef char (*extenderInit_t )(VMContext* context);

// In InitExtender, you must do
//  >>> context.Validate(context, VM_API_VERSION)
// and check the return value. If it is 0 then either
// sizes/alignment doesn't match or API versions are different.


#ifdef __cplusplus
}
#endif

#endif
