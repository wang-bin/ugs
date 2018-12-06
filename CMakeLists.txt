cmake_minimum_required(VERSION 3.0)
# targets: ugs(shared or reloc obj), ugs-static, ugs-static-dydep
if(NOT DEFINED UGS_PROJECT_NAME)
  set(UGS_PROJECT_NAME ugs)
else()
    add_definitions(-DUGS_NS=${UGS_PROJECT_NAME})
endif()
if(POLICY CMP0025)
# MUST call before project() and enable_language
  cmake_policy(SET CMP0025 NEW) # since 3.0, prefer AppleClang instead of Clang. FIXME: ios is still Clang
endif()
project(${UGS_PROJECT_NAME} VERSION 0.0.0) # VERSION: cmake 3.0+
set(TARGET_NAME ${PROJECT_NAME})

# reset var inherited from parent
set(EXTRA_LIBS)
set(EXTRA_DYLIBS)
set(EXTRA_LIB_DIR)

# https://crascit.com/2015/03/28/enabling-cxx11-in-cmake/ (global and specified target)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#SET(CMAKE_VERBOSE_MAKEFILE ON)
#list(APPEND CMAKE_MODULE_PATH cmake)
include(cmake/tools/tools.cmake NO_POLICY_SCOPE)

#########################################################################################################
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
if(IOS AND NOT IOS_EMBEDDED_FRAMEWORK)
  set(UGS_RELOCATABLE_DEFAULT ON)
else()
  set(UGS_RELOCATABLE_DEFAULT OFF)
endif()
option(UGS_RELOCATABLE "Build ${PROJECT_NAME} as a relocatable object instead of a shared library" ${UGS_RELOCATABLE_DEFAULT})

if(EXISTS ${PROJECT_SOURCE_DIR}/include)
  set(${PROJECT_NAME}_INCLUDE_PATH ${PROJECT_SOURCE_DIR}/include)
  include_directories(${PROJECT_SOURCE_DIR}/include) # should not affect sub-projects
endif()

set(EXTRA_CFLAGS)
set(SDK_HEADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
file(GLOB SDK_HEADERS LIST_DIRECTORIES false  ${SDK_HEADERS_DIR}/${PROJECT_NAME}/*.h)

set(SRC
    PlatformSurface.cpp
    RenderLoop.cpp
    )
if(WIN32)
  if(WINDOWS_PHONE OR WINDOWS_STORE)
    list(APPEND SRC WinRTSurface.cpp)
  else()
    list(APPEND SRC Win32Surface.cpp)
    list(APPEND EXTRA_DYLIBS user32)
  endif()
endif()
if(ANDROID)
  list(APPEND SRC AndroidSurface.cpp base/jmi/jmi.cpp)
  set_property(SOURCE AndroidSurface.cpp APPEND PROPERTY COMPILE_FLAGS "-I${${PROJECT_NAME}_SOURCE_DIR}/base") # COMPILE_OPTIONS/INCLUDE_DIRECTORIES does not work
  list(APPEND EXTRA_DYLIBS android log)
endif()
if(IOS)
  list(APPEND SRC UIKitSurface.mm)
  list(APPEND EXTRA_DYLIBS "-framework QuartzCore" "-framework UIKit" "-framework Foundation") # Foundation: objc, CoreGraphics: CGRectNull, QuartzCore: CALayer
endif()
if(RPI)
  list(APPEND SRC RPiSurface.cpp)
  set_source_files_properties(RPiSurface.cpp PROPERTIES COMPILE_FLAGS -I${RPI_VC_DIR}/include)
  list(APPEND EXTRA_DYLIBS -L${RPI_VC_DIR}/lib bcm_host)
endif()
if(NOT WIN32 AND NOT APPLE)
  list(APPEND SRC MaliFBSurface.cpp)
endif()
include(FindX11)
if(X11_FOUND)
  list(APPEND SRC X11Surface.cpp)
  set_source_files_properties(X11Surface.cpp PROPERTIES COMPILE_FLAGS -I${X11_X11_INCLUDE_PATH})
  set(EXTRA_CFLAGS "${EXTRA_CFLAGS} -DHAVE_X11=1")
  # already marked as weak in cpp, so -weak-lX11 is not required.
  list(APPEND EXTRA_DYLIBS ${X11_X11_LIB})
endif()

find_package(Wayland COMPONENTS Client Egl)
if(WAYLAND_CLIENT_FOUND AND WAYLAND_EGL_FOUND)
  set(HAVE_WAYLAND 1)
  list(APPEND SRC WaylandSurface.cpp WaylandEGLSurface.cpp)
  set(EXTRA_CFLAGS "${EXTRA_CFLAGS} -DHAVE_WAYLAND=1")
  include_directories(${WAYLAND_CLIENT_INCLUDE_DIR} ${WAYLAND_EGL_INCLUDE_DIR})
  list(APPEND EXTRA_DYLIBS ${WAYLAND_CLIENT_LIBRARIES}) # ${WAYLAND_EGL_LIBRARIES} may not exist at runtime
endif()

if(UNIX AND NOT APPLE AND NOT CYGWIN AND NOT ANDROID) #clang-cl
  find_package(PkgConfig)
  pkg_check_modules(PKG_GBM gbm)
  pkg_check_modules(PKG_LIBDRM libdrm)
endif()
if(PKG_GBM_FOUND AND PKG_LIBDRM_FOUND)
  set(HAVE_GBM 1)
  set(EXTRA_CFLAGS "${EXTRA_CFLAGS} -DHAVE_GBM=1")
  list(APPEND SRC GBMSurface.cpp)
  if(CMAKE_CROSSCOMPILING)
    string(REPLACE "-I/usr" "-I${CMAKE_SYSROOT}/usr" PKG_LIBDRM_CFLAGS ${PKG_LIBDRM_CFLAGS}) # -I/usr/include/libdrm
  endif()
  set_source_files_properties(GBMSurface.cpp PROPERTIES COMPILE_FLAGS "-DHAVE_GBM=1 ${PKG_GBM_CFLAGS} ${PKG_LIBDRM_CFLAGS}")
  list(APPEND EXTRA_DYLIBS ${PKG_GBM_LIBRARIES} ${PKG_LIBDRM_LIBRARIES})
endif()

if(EXTRA_CFLAGS)
  set_source_files_properties(PlatformSurface.cpp PROPERTIES COMPILE_FLAGS ${EXTRA_CFLAGS})
endif()

add_library(${TARGET_NAME}-objs OBJECT ${SRC})
add_library(${TARGET_NAME}-static-objs OBJECT ${SRC})
add_library(${TARGET_NAME} SHARED ${SDK_HEADERS} $<TARGET_OBJECTS:${TARGET_NAME}-objs>)
add_library(${TARGET_NAME}-static STATIC $<TARGET_OBJECTS:${TARGET_NAME}-static-objs>)
if(UGS_RELOCATABLE)
  add_library(${TARGET_NAME}-relocobj STATIC $<TARGET_OBJECTS:${TARGET_NAME}-static-objs>)
endif()

target_compile_definitions(${TARGET_NAME}-objs PRIVATE BUILD_UGS_LIB)
target_compile_definitions(${TARGET_NAME}-static-objs PRIVATE BUILD_UGS_STATIC)
include(CheckLibraryExists)
CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_LIBDL)
if(HAVE_LIBDL)
  list(APPEND EXTRA_DYLIBS dl)
endif()
CHECK_LIBRARY_EXISTS(pthread pthread_create "" HAVE_PTHREAD)
if(HAVE_PTHREAD)
  list(APPEND EXTRA_DYLIBS pthread) # rpi
endif()

if(EXTRA_LIB_DIR)
  list(REMOVE_DUPLICATES EXTRA_LIB_DIR)
endif()

set_target_properties(${TARGET_NAME} PROPERTIES
    MACOSX_RPATH ON
    FRAMEWORK ON
    RELOCATABLE ${UGS_RELOCATABLE} # custom property. use set_relocatable_flags() to apply linker flags. TODO: patch for cmake
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    OUTPUT_NAME ${TARGET_NAME}
    CLEAN_DIRECT_OUTPUT 1
    PUBLIC_HEADER "${SDK_HEADERS}"
)
set_relocatable_flags(${TARGET_NAME})
set_rpath() # not reloc obj
exclude_libs_all(${TARGET_NAME})
mkdsym(${TARGET_NAME})  # MUST after VERSION set because VERSION is used un mkdsym for cmake <3.0

if(EXTRA_DYLIBS)
  list(REMOVE_DUPLICATES EXTRA_DYLIBS)
endif()
if(EXTRA_LIBS)
  list(REMOVE_DUPLICATES EXTRA_LIBS)
endif()
#add_library(${TARGET_NAME}-static-dydep INTERFACE) # interface lib is not supported by cmake 2.x
#target_link_libraries(${TARGET_NAME}-static-dydep INTERFACE ${EXTRA_DYLIBS})

target_link_libraries(${TARGET_NAME} PRIVATE ${EXTRA_LIBS}) # only static libs are enough for reloc obj targets, otherwise static + shared
if(UGS_RELOCATABLE)
  target_link_libraries(${TARGET_NAME}-relocobj PUBLIC ${EXTRA_LIBS}) # static dependencies only, may be used by reloc obj
else()
  target_link_libraries(${TARGET_NAME} PRIVATE ${EXTRA_DYLIBS})
endif()
target_link_libraries(${TARGET_NAME}-static PUBLIC ${EXTRA_LIBS} ${EXTRA_DYLIBS})

setup_deploy(${TARGET_NAME})