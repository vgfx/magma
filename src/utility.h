#pragma once

#include "definitions.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <malloc.h>

// For internal use only!
static inline void PrintInternal(FILE* stream, string_t prefix, string_t fmt, const va_list args)
{
    // Print the time stamp.
    time_t rawTime;
    time(&rawTime);
    struct tm timeInfo;
    localtime_s(&timeInfo, &rawTime);
    fprintf(stream, "[%i:%i:%i] ", timeInfo.tm_hour, timeInfo.tm_min, timeInfo.tm_sec);
    // Print the prefix if there is one.
    if (prefix)
    {
        fprintf(stream, "%s ", prefix);
    }
    // Print the arguments.
    vfprintf(stream, fmt, args);
}

// Prints information to stdout (printf syntax) and appends a newline at the end.
static inline void PrintInfo(string_t fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintInternal(stdout, nullptr, fmt, args);
    va_end(args);
    fputs("\n", stdout);
}

// Prints warnings to stdout (printf syntax) and appends a newline at the end.
static inline void PrintWarning(string_t fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintInternal(stdout, "Warning:", fmt, args);
    va_end(args);
    fputs("\n", stdout);
}

// Prints fatal errors to stderr (printf syntax) and appends a newline at the end.
static inline void PrintError(string_t fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    PrintInternal(stderr, "Error:", fmt, args);
    va_end(args);
    fputs("\n", stderr);
}

// For internal use only!
[[noreturn]] static inline void Panic(string_t file, const int line)
{
    fprintf(stderr, "Error location: %s : %i\n", file, line);
    abort();
}

// Prints the location of the fatal error and terminates the program.
#define TERMINATE() Panic(__FILE__, __LINE__)

// Prints the C string 'errMsg' if the error code is not 0.
// Then prints the location of the fatal error and terminates the program.
#define CHECK_INT(err, errMsg, ...)      \
do                                       \
{                                        \
    volatile const sign_t result = err;  \
    if (result != 0)                     \
    {                                    \
        PrintError(errMsg, __VA_ARGS__); \
        __debugbreak();                  \
        TERMINATE();                     \
    }                                    \
} while (0)

// Prints the C string 'errMsg' if the value is convertible to 'false'.
// Then prints the location of the fatal error and terminates the program.
#define ASSERT(value, errMsg, ...)       \
do                                       \
{                                        \
    if (!(value))                        \
    {                                    \
        PrintError(errMsg, __VA_ARGS__); \
        __debugbreak();                  \
        TERMINATE();                     \
    }                                    \
} while (0)

// Performs stack allocation for 'count' objects of the type T.
template <typename T>
T* StackAlloc(size_t count)
{
    return static_cast<T*>(_alloca(count * sizeof(T)));
}

template <typename T>
void TrivialMoveConstruct(T* dst, T* src)
{
    memcpy(dst, src, sizeof(T));
    memset(src, 0,   sizeof(T));
}

template <typename T>
T& TrivialMoveAssign(T* dst, T* src)
{
    if (dst != src)
    {
        TrivialMoveConstruct<T>(dst, src);
    }

    return *dst;
}
