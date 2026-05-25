#pragma once

// Header file for the inclusion of plartform specific x86/x64 intrinsics header files.

// Only include x86/x64 intrinsics on x86 architectures
#if !defined(__aarch64__) && !defined(__arm64__) && !defined(_M_ARM64) && !defined(__ARM_ARCH)
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <immintrin.h>
#endif
#endif
