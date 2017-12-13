if(NOT DEFINED UGS_PROJECT_NAME)
  set(UGS_PROJECT_NAME ugs)
else()
    add_definitions(-DUGS_NS=${UGS_PROJECT_NAME})
endif()
if(POLICY CMP0025)
# MUST call before project() and enable_language
  cmake_policy(SET CMP0025 NEW) # since 3.0, prefer AppleClang instead of Clang. FIXME: ios is still Clang
endif()
set(VERSION_MAJOR 0)
set(VERSION_MINOR 0)
set(VERSION_MICRO 0)
set(PROJECT_VERSION ${VERSION_MAJOR}.${VERSION_MINOR}.${VERSION_MICRO})
if(NOT CMAKE_VERSION VERSION_LESS 3.0)
  cmake_policy(SET CMP0048 OLD)
endif()
project(${UGS_PROJECT_NAME}) #VERSION ${PROJECT_VERSION}) # VERSION: 3.0
set(TARGET_NAME ${PROJECT_NAME})
set(SO_VERSION ${VERSION_MAJOR})
cmake_minimum_required(VERSION 2.8)

if(POLICY CMP0022) # since 2.8.12. link_libraries()
  cmake_policy(SET CMP0022 NEW)
endif()
# reset var inherited from parent
set(EXTRA_LIBS)
set(EXTRA_DYLIBS)
set(EXTRA_LIB_DIR)
set(EXTRA_INCLUDE)

include(CheckCXXCompilerFlag)
include(CheckLibraryExists)
include(CheckIncludeFiles)
include(CheckIncludeFile)
include(CheckCSourceCompiles)
# https://crascit.com/2015/03/28/enabling-cxx11-in-cmake/ (global and specified target)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)
if(CMAKE_VERSION VERSION_LESS 3.1)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU")
    add_compile_options(-std=c++11)
  endif()
endif()
if(POLICY CMP0063) # visibility. since 3.3
  cmake_policy(SET CMP0063 NEW)
endif()
set(CMAKE_CXX_VISIBILITY_PRESET hidden)
set(CMAKE_VISIBILITY_INLINES_HIDDEN ON)

#SET(CMAKE_VERBOSE_MAKEFILE ON)

#find_package(Git)
#list(APPEND CMAKE_MODULE_PATH cmake)
include(cmake/tools/tools.cmake)
#check_cxx_compiler_flag(-fno-rtti HAVE_NO_RTTI)
if(MSVC)
  add_compile_options(-D_CRT_SECURE_NO_WARNINGS) #-GR- warning
else()
  add_compile_options(-fno-rtti -fno-exceptions)
endif()
if(USE_TSAN AND NOT MSVC)
  add_compile_options(-fsanitize=thread)
  set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -fsanitize=thread")
  set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -fsanitize=thread")
endif()

if(EXISTS ${${PROJECT_NAME}_SOURCE_DIR}/include)
  include_directories(${${PROJECT_NAME}_SOURCE_DIR}/include)
endif()
if(RPI)
  list(APPEND EXTRA_INCLUDE ${RPI_VC_DIR}/include)
endif()
if(EXISTS ${${PROJECT_NAME}_SOURCE_DIR}/external/lib/${OS}/${ARCH})
  list(APPEND EXTRA_LIB_DIR "${${PROJECT_NAME}_SOURCE_DIR}/external/lib/${OS}/${ARCH}")
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/external/lib/${OS}/${ARCH})
  list(APPEND EXTRA_LIB_DIR "${CMAKE_SOURCE_DIR}/external/lib/${OS}/${ARCH}")
endif()
if(EXISTS ${${PROJECT_NAME}_SOURCE_DIR}/external/include)
  list(APPEND EXTRA_INCLUDE ${${PROJECT_NAME}_SOURCE_DIR}/external/include)
endif()
if(EXISTS ${CMAKE_SOURCE_DIR}/external/include)
  list(APPEND EXTRA_INCLUDE ${CMAKE_SOURCE_DIR}/external/include)
endif()
if(APPLE)
    if(IOS)
      #set_xcode_property(myioslib IPHONEOS_DEPLOYMENT_TARGET "8.0")
    else()
      list(APPEND EXTRA_INCLUDE /usr/local/include) #macOS only
    endif()
endif()
if(EXISTS ${${PROJECT_NAME}_SOURCE_DIR}/base/capi/capi.h)
    set(HAVE_CAPI 1)
    list(APPEND EXTRA_INCLUDE ${${PROJECT_NAME}_SOURCE_DIR}/base/capi) # TODO: only files use capi.h
endif()
if(WIN32)
  if(CMAKE_C_COMPILER_ID STREQUAL "GNU") # TODO: option STATIC_LIBGCC?
    #link_libraries(-static-libgcc) cmake2.8 CMP0022
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static-libgcc")
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static-libgcc")
  endif()
endif()

#########################################################################################################
set(CMAKE_MODULE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/cmake)
option(UGS_RELOCATABLE "Build ${PROJECT_NAME} as a relocatable object instead of a shared library" OFF)

set(EXTRA_CFLAGS)
set(SDK_HEADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)
set(SRC
    PlatformSurface.cpp
    RenderLoop.cpp
    )
if(WIN32)
  list(APPEND SRC Win32Surface.cpp)
endif()
if(ANDROID)
  list(APPEND SRC AndroidSurface.cpp base/jmi/jmi.cpp)
  list(APPEND EXTRA_LIBS android log)
endif()
if(IOS)
  list(APPEND SRC UIKitSurface.mm)
  list(APPEND EXTRA_DYLIBS "-framework UIKit" "-framework Foundation") # Foundation: objc, CoreGraphics: CGRectNull
endif()
if(RPI)
  list(APPEND SRC RPiSurface.cpp)
  set_source_files_properties(RPiSurface.cpp PROPERTIES COMPILE_FLAGS -I${RPI_VC_DIR}/include)
  list(APPEND EXTRA_LIBS -L${RPI_VC_DIR}/lib bcm_host)
endif()
include(FindX11)
if(X11_FOUND)
  list(APPEND SRC X11Surface.cpp)
  set_source_files_properties(X11Surface.cpp PROPERTIES COMPILE_FLAGS -I${X11_X11_INCLUDE_PATH})
  set(EXTRA_CFLAGS "${EXTRA_CFLAGS} -DHAVE_X11=1")
  if(APPLE)
    # already marked as weak in cpp, so -weak_library is not required.
    list(APPEND EXTRA_DYLIBS -flat_namespace -Wl,-undefined,dynamic_lookup ${X11_X11_LIB})
  endif()
  list(APPEND EXTRA_DYLIBS ${X11_X11_LIB})
endif()

find_package(Wayland COMPONENTS Client Egl)
if(WAYLAND_CLIENT_FOUND AND WAYLAND_EGL_FOUND)
  set(HAVE_WAYLAND 1)
  list(APPEND SRC WaylandSurface.cpp WaylandEGLSurface.cpp)
  set(EXTRA_CFLAGS "${EXTRA_CFLAGS} -DHAVE_WAYLAND=1")
  list(APPEND EXTRA_INCLUDE ${WAYLAND_CLIENT_INCLUDE_DIR} ${WAYLAND_EGL_INCLUDE_DIR})
  list(APPEND EXTRA_LIBS ${WAYLAND_CLIENT_LIBRARIES} ${WAYLAND_EGL_LIBRARIES})
endif()

find_package(PkgConfig)
pkg_check_modules(PKG_GBM gbm)
pkg_check_modules(PKG_LIBDRM libdrm)
if(PKG_GBM_FOUND AND PKG_LIBDRM_FOUND)
  set(HAVE_GBM 1)
  set(EXTRA_CFLAGS "${EXTRA_CFLAGS} -DHAVE_GBM=1")
  list(APPEND SRC GBMSurface.cpp)
  set_source_files_properties(GBMSurface.cpp PROPERTIES COMPILE_FLAGS "-DHAVE_GBM=1 ${PKG_GBM_CFLAGS} ${PKG_LIBDRM_CFLAGS}")
  list(APPEND EXTRA_LIBS ${PKG_GBM_LIBRARIES} ${PKG_LIBDRM_LIBRARIES})
endif()

if(EXTRA_CFLAGS)
  set_source_files_properties(PlatformSurface.cpp PROPERTIES COMPILE_FLAGS ${EXTRA_CFLAGS})
endif()

add_library(${TARGET_NAME}-shared SHARED ${SRC})
add_library(${TARGET_NAME}-static STATIC ${SRC})
target_compile_definitions(${TARGET_NAME}-shared PRIVATE BUILD_UGS_LIB)
target_compile_definitions(${TARGET_NAME}-static PRIVATE BUILD_UGS_STATIC)

if(EXTRA_INCLUDE)
  list(REMOVE_DUPLICATES EXTRA_INCLUDE)
endif()
if(EXTRA_LIB_DIR)
  list(REMOVE_DUPLICATES EXTRA_LIB_DIR)
endif()

target_include_directories(${TARGET_NAME}-shared
  PRIVATE ${EXTRA_INCLUDE}
)
target_include_directories(${TARGET_NAME}-static
  PRIVATE ${EXTRA_INCLUDE}
)

set_target_properties(${TARGET_NAME}-shared PROPERTIES
    RELOCATABLE ${UGS_RELOCATABLE} # custom property. use set_relocatable_flags() to apply linker flags. TODO: patch for cmake
    MACOSX_RPATH ON
    FRAMEWORK ON
    VERSION ${PROJECT_VERSION}
    SOVERSION ${SO_VERSION}
    OUTPUT_NAME ${TARGET_NAME}
    CLEAN_DIRECT_OUTPUT 1
    )
set_relocatable_flags(${TARGET_NAME}-shared)
if(APPLE)
  add_custom_command(TARGET ${TARGET_NAME}-shared POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory Headers
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${SDK_HEADERS_DIR}/${TARGET_NAME}/ Headers
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.framework
    )
endif()

set(LINK_TO_SHARED 1) # only dso needs to link to shared libs, static libs need nothing, relocatable objects only need to link to static libs
if(UGS_RELOCATABLE)
  set(LINK_TO_SHARED 0)
else()
  set_rpath()
endif()
exclude_libs_all(${TARGET_NAME}-shared)

if(APPLE)
  if(IOS)
    if(NOT IOS_EMBEDDED_FRAMEWORK)
      set(LINK_TO_SHARED 0)
      # unexport completely including asm symbols
      #file(WRITE ${CMAKE_BINARY_DIR}/.unexport.list "*ContextEAGL*\n*egl*\n")
      #target_link_libraries(${TARGET_NAME}-shared PRIVATE -Wl,-unexported_symbols_list,${CMAKE_BINARY_DIR}/.unexport.list)
    endif()
  else()
    # macOS only
    list(APPEND EXTRA_LIB_DIR /usr/local/lib)
    #list(APPEND EXTRA_DYLIBS "-framework VideoDecodeAcceleration" "-framework IOSurface")
    if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS 10.7) # check host os?
      add_custom_command(TARGET ${TARGET_NAME}-shared POST_BUILD
        COMMAND install_name_tool -change /usr/lib/libc++.1.dylib @rpath/libc++.1.dylib $<TARGET_FILE:${TARGET_NAME}-shared>
      )
    endif()
  endif()
endif()
if(LINK_TO_SHARED)
  CHECK_LIBRARY_EXISTS(dl dlopen "" HAVE_LIBDL)
  if(HAVE_LIBDL)
    list(APPEND EXTRA_DYLIBS dl)
  endif()
  list(APPEND EXTRA_LIBS "${EXTRA_DYLIBS}")
endif()
if(EXTRA_DYLIBS)
  list(REMOVE_DUPLICATES EXTRA_DYLIBS)
endif()
if(EXTRA_LIBS)
  list(REMOVE_DUPLICATES EXTRA_LIBS)
endif()
set_target_properties(${TARGET_NAME}-shared PROPERTIES libs "${EXTRA_DYLIBS}")
set_target_properties(${TARGET_NAME}-static PROPERTIES libs "${EXTRA_LIBS}")

target_link_libraries(${TARGET_NAME}-shared LINK_PRIVATE ${EXTRA_LIBS}) 
target_link_libraries(${TARGET_NAME}-static LINK_PUBLIC  ${EXTRA_LIBS})

install(DIRECTORY ${SDK_HEADERS_DIR} DESTINATION include/)
install(TARGETS ${PROJECT_NAME}-shared ${PROJECT_NAME}-static
  EXPORT ${PROJECT_NAME}-targets
  #PUBLIC_HEADER DESTINATION ${PROJECT_NAME}
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
  FRAMEWORK DESTINATION lib
)  
install(EXPORT ${PROJECT_NAME}-targets
  DESTINATION lib/cmake/${PROJECT_NAME}
  FILE ${PROJECT_NAME}-config.cmake
)

