#pragma once

#include <cstdint>

using byte_t   = unsigned char;
using sign_t   = ptrdiff_t;
using size_t   = size_t;
using string_t = const char*;

// Declares a -default- Dtor, copy/move Ctors and assignment Otors.
#define RULE_OF_ZERO(T) T(const T&)                = default; \
                        T& operator=(const T&)     = default; \
                        T(T&&) noexcept            = default; \
                        T& operator=(T&&) noexcept = default; \
                        ~T() noexcept              = default

// Declares a -default- Dtor, a move Ctor and an assignment Otor.
// Forbids copy construction and copy assignment.
#define RULE_OF_ZERO_MOVE_ONLY(T) T(const T&)                = delete;  \
                                  T& operator=(const T&)     = delete;  \
                                  T(T&&) noexcept            = default; \
                                  T& operator=(T&&) noexcept = default; \
                                  ~T() noexcept              = default

// Declares a -custom- Dtor, copy/move Ctors and assignment Otors.
#define RULE_OF_FIVE(T) T(const T&);                \
                        T& operator=(const T&);     \
                        T(T&&) noexcept;            \
                        T& operator=(T&&) noexcept; \
                        ~T() noexcept

// Declares a -custom- Dtor, a move Ctor and an assignment Otor.
// Forbids copy construction and copy assignment.
#define RULE_OF_FIVE_MOVE_ONLY(T) T(const T&)            = delete; \
                                  T& operator=(const T&) = delete; \
                                  T(T&&) noexcept;                 \
                                  T& operator=(T&&) noexcept;      \
                                  ~T() noexcept

// Clean up Windows header includes.
#define NOIME
#define NOMCX
#define NOMINMAX
#define NOSERVICE
#define STRICT
#define WIN32_LEAN_AND_MEAN
