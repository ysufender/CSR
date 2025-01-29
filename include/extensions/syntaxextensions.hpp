#pragma once

#include <utility>
#include <exception>
#include <iostream>
#include <string_view>

#include "system.hpp"

#define rval(expr) std::move(expr)

#define nameof(variable) #variable

#define try_catch(expr, catchcsr, catchother) \
    try { \
        expr \
    } \
    catch (const CSRException& exc) { \
        std::cerr << exc.Stringify(); \
        catchcsr \
    } \
    catch (const std::exception& exc) { \
        std::cerr << "An unexpected exception occured during process.\n\tProvided information: " << exc.what() << '\n'; \
        catchother  \
    }


#define FOREACH_ENUM(ENUM, ENUMER) ENUM(ENUMER)
#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,
#define MAKE_ENUM(name, values, start) \
    enum class name : uchar_t { \
        FOREACH_ENUM(values, GENERATE_ENUM) \
    }; \
    namespace { \
        namespace { \
            constexpr std::string_view name##ToString[] { \
                FOREACH_ENUM(values, GENERATE_STRING) \
            }; \
        } \
        constexpr std::string_view name##String(name enumer) { \
            return name##ToString[static_cast<uchar_t>(enumer)-start]; \
        } \
    }
