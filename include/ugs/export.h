/*
 * Copyright (c) 2016-2024 WangBin <wbsecg1 at gmail.com>
 * This file is part of UGS(Universal Graphics Surface)
 * Source code: https://github.com/wang-bin/ugs
 *
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 */
#ifndef UGS__EXPORT_H
#define UGS__EXPORT_H

#ifndef UGS_NS
#define UGS_NS UGS
#endif
#define UGS_NS_BEGIN namespace UGS_NS {
#define UGS_NS_END }
#define UGS_NS_PREPEND(X) ::UGS_NS::X

#define UGS_STRINGIFY(X) _UGS_STRINGIFY(X)

#if defined(_WIN32)
# include <winapifamily.h>
# if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#   define UGS_OS_WINRT 1
# endif
#define UGS_EXPORT __declspec(dllexport)
#define UGS_IMPORT __declspec(dllimport)
#define UGS_LOCAL
#else
#define UGS_EXPORT __attribute__((visibility("default")))
#define UGS_IMPORT __attribute__((visibility("default")))
#define UGS_LOCAL  __attribute__((visibility("hidden"))) // mingw gcc set hidden symbol in a visible class has no effect and will make it visible
#endif

#ifdef BUILD_UGS_STATIC
# define UGS_API
#else
# if defined(BUILD_UGS_LIB)
#  define UGS_API UGS_EXPORT
# else
#  define UGS_API UGS_IMPORT // default is import if nothing is defined
# endif
#endif //BUILD_UGS_STATIC

#define _UGS_STRINGIFY(X) #X

#if (_MSC_VER + 0) >= 1500
# define VC_NO_WARN(number)       __pragma(warning(disable: number))
#else
# define VC_NO_WARN(number)
#endif
VC_NO_WARN(4251) /* class 'type' needs to have dll-interface to be used by clients of class 'type2' */
#endif //UGS__EXPORT_H
