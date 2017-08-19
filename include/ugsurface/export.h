/*
 * Copyright (c) 2016-2017 WangBin <wbsecg1 at gmail.com>
 */
#ifndef UGSURFACE__EXPORT_H
#define UGSURFACE__EXPORT_H

#ifndef UGSURFACE_NS
#define UGSURFACE_NS UGSurface
#endif
#define UGSURFACE_NS_BEGIN namespace UGSURFACE_NS {
#define UGSURFACE_NS_END }
#define UGSURFACE_NS_PREPEND(X) ::UGSURFACE_NS::X

#define UGSURFACE_STRINGIFY(X) _UGSURFACE_STRINGIFY(X)

#if defined(_WIN32)
#define UGSURFACE_EXPORT __declspec(dllexport)
#define UGSURFACE_IMPORT __declspec(dllimport)
#define UGSURFACE_LOCAL
#else
#define UGSURFACE_EXPORT __attribute__((visibility("default")))
#define UGSURFACE_IMPORT __attribute__((visibility("default")))
#define UGSURFACE_LOCAL  __attribute__((visibility("hidden"))) // mingw gcc set hidden symbol in a visible class has no effect and will make it visible
#endif

#ifdef BUILD_UGSURFACE_STATIC
# define UGSURFACE_API
#else
# if defined(BUILD_UGSURFACE_LIB)
#  define UGSURFACE_API UGSURFACE_EXPORT
# else
#  define UGSURFACE_API UGSURFACE_IMPORT // default is import if nothing is defined
# endif
#endif //BUILD_UGSURFACE_STATIC

#define _UGSURFACE_STRINGIFY(X) #X

#if defined(_MSC_VER) && _MSC_VER >= 1500
# define VC_NO_WARN(number)       __pragma(warning(disable: number))
#else
# define VC_NO_WARN(number)
#endif
VC_NO_WARN(4251) /* class 'type' needs to have dll-interface to be used by clients of class 'type2' */
#endif //UGSURFACE__EXPORT_H
