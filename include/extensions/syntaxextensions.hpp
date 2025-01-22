#pragma once

#include <utility>
#include <system.hpp>
#include <exception>
#include <iostream>

#define rval(expr) std::move(expr)

#define try_catch(expr, catchcsr, catchother) \
    try { \
        expr; \
    } \
    catch (const CSRException& exc) { \
        std::cerr << exc.Stringify(); \
        catchcsr; \
    } \
    catch (const std::exception& exc) { \
        std::cerr << "An unexpected exception occured during process.\n\tProvided information: " << exc.what() << '\n'; \
        catchother;  \
    } \
