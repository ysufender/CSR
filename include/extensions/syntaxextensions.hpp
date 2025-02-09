#pragma once

#include <utility>
#include <exception>
#include <iostream>
#include <string_view>

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


using namespace std::string_view_literals;

#define OUT_CLASS constexpr
#define IN_CLASS static constexpr
#define FOREACH_ENUM(ENUM, ENUMER) ENUM(ENUMER)
#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING##sv,
#define MAKE_ENUM(name, fenum, fval, values, mode) \
    enum class name : char { \
        fenum = fval, \
        FOREACH_ENUM(values, GENERATE_ENUM) \
    }; \
    mode const std::string_view __##name##__strings__[] { \
        #fenum##sv, \
        FOREACH_ENUM(values, GENERATE_STRING) \
    }; \
    mode const std::string_view& name##String(char enumer) { \
        return __##name##__strings__[static_cast<char>(enumer)-fval]; \
    } \
    mode const std::string_view& name##String(name enumer) { \
        return name##String(static_cast<char>(enumer)); \
    }
