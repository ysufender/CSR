#pragma once

#include "CSRConfig.hpp"

// char* is parameter buffer for passed/returned values so read/write there.
using SysFunctionHandler = char (*)(char*) noexcept;
using sysfnh_t = SysFunctionHandler;

// void* is internal, pass the one passed to the InitExtender
using binder_t = char (*)(void*, sysbit_t, SysFunctionHandler) noexcept;
using unbinder_t = char (*)(void*, sysbit_t) noexcept;

// Initializer function signature for extender DLs
// name must be specifically InitExtender
//
// void* is internal you don't need to know.
// rest is clear methinks.
using extenderInit_t = char (*) (void*, binder_t, unbinder_t) noexcept;
