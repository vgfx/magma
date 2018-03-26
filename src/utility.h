#pragma once

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <malloc.h>

using byte_t   = unsigned char;
using size_t   = size_t;
using sign_t   = ptrdiff_t;
using string_t = const char*;

// For internal use only!
static inline void printInternal(FILE* stream, string_t prefix, string_t fmt, const va_list args)
{
    // Print the time stamp.
    time_t rawTime;
    time(&rawTime);
    struct tm timeInfo;
    localtime_s(&timeInfo, &rawTime);
    fprintf(stream, "[%i:%i:%i] ", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    // Print the prefix if there is one.
    if (prefix) {
        fprintf(stream, "%s ", prefix);
    }
    // Print the arguments.
    vfprintf(stream, fmt, args);
}

// Prints information to stdout (printf syntax) and appends a newline at the end.
static inline void printInfo(string_t fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printInternal(stdout, nullptr, fmt, args);
    va_end(args);
    fputs("\n", stdout);
}

// Prints warnings to stdout (printf syntax) and appends a newline at the end.
static inline void printWarning(string_t fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printInternal(stdout, "Warning:", fmt, args);
    va_end(args);
    fputs("\n", stdout);
}

// Prints fatal errors to stderr (printf syntax) and appends a newline at the end.
static inline void printError(string_t fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    printInternal(stderr, "Error:", fmt, args);
    va_end(args);
    fputs("\n", stderr);
}

// For internal use only!
[[noreturn]] static inline void panic(string_t file, const int line)
{
    fprintf(stderr, "Error location: %s : %i\n", file, line);
    abort();
}

// Prints the location of the fatal error and terminates the program.
#define TERMINATE() panic(__FILE__, __LINE__)

// Prints the C string 'errMsg' if the error code is not 0.
#define CHECK_INT(err, errMsg)       \
do                                   \
{                                    \
    volatile const int result = err; \
    if (result != 0)                 \
    {                                \
        __debugbreak();              \
        printError(errMsg);          \
        panic(__FILE__, __LINE__);   \
    }                                \
} while (0)

// Prints the C string 'errMsg' if the value is convertible to 'false'.
#define ASSERT(value, errMsg)        \
do                                   \
{                                    \
    if (!(value))                    \
    {                                \
        __debugbreak();              \
        printError(errMsg);          \
        panic(__FILE__, __LINE__);   \
    }                                \
} while (0)

// Performs stack allocation for 'count' objects of the type T.
template <typename T>
T* Alloca(size_t count)
{
    return static_cast<T*>(_alloca(count * sizeof(T)));
}
