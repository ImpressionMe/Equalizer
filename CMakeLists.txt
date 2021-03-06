
# Copyright (c) 2010 Daniel Pfeifer <daniel@pfeifer-mail.de>
#               2010-2015 Stefan Eilemann <eile@eyescale.ch>

cmake_minimum_required(VERSION 2.8 FATAL_ERROR)
project(Equalizer)

list(APPEND CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/CMake
  ${CMAKE_SOURCE_DIR}/CMake/common)

include(GitExternal)
include(SubProject)

set(VERSION_MAJOR "1")
set(VERSION_MINOR "8")
set(VERSION_PATCH "0")
set(VERSION_ABI 180)
set(PROJECT_INCLUDE_NAME eq)

include(Common)

option(EQUALIZER_USE_OSG "Test for OpenSceneGraph support" ON)
option(EQUALIZER_USE_HWLOC "Test for hwloc support (experimental)" OFF)
option(EQUALIZER_INSTALL_SERVER_HEADERS "Install Equalizer server headers" OFF)
option(EQUALIZER_BUILD_2_0_API
  "Enable for pure 2.0 API (breaks compatibility with 1.x API)" OFF)

mark_as_advanced(EQUALIZER_INSTALL_SERVER_HEADERS)

set(DPUT_HOST ppa:bbp/ppa)

set(COMMON_PROJECT_DOMAIN ch.eyescale)
set(EQUALIZER_DESCRIPTION "Parallel rendering framework for the creation of
                          parallel and scalable OpenGL and GPGPU applications.")

if(EQUALIZER_BUILD_2_0_API)
  list(APPEND FIND_PACKAGES_DEFINES EQ_2_0_API)
else()
  list(APPEND FIND_PACKAGES_DEFINES EQ_1_0_API)
endif()

include(configure)

list(APPEND CPPCHECK_EXTRA_ARGS -DEQUALIZERFABRIC_STATIC=)

if(CMAKE_COMPILER_IS_CLANG)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-overloaded-virtual")
endif()

set(EQ_FEATURES)
if(MSVC)
  set(EQ_FEATURES "${EQ_FEATURES} WGL")
  include(UseGnuWin32)
else()
  set(PTHREAD_LIBRARIES pthread)
endif()

if(LUNCHBOX_USE_OPENMP)
  find_package(OpenMP REQUIRED)
  set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${OpenMP_C_FLAGS}")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${OpenMP_CXX_FLAGS}")
endif()

if(APPLE)
  add_definitions(-DDarwin)
  set(CMAKE_INCLUDE_SYSTEM_FLAG_CXX "-isystem ") # Disabled by GNU.cmake!?
endif(APPLE)

if(EQ_GLX_USED)
  include_directories(SYSTEM ${X11_INCLUDE_DIR})
  set(EQ_FEATURES "${EQ_FEATURES} GLX")
endif()
if(EQ_AGL_USED)
  set(EQ_FEATURES "${EQ_FEATURES} AGL(32bit)")
endif()
if(EQ_QT_USED)
  set(EQ_FEATURES "${EQ_FEATURES} Qt")
endif()

list(APPEND EQ_DEFINITIONS -DEQ_BUILD_DIR="${CMAKE_BINARY_DIR}/")

if(EQUALIZER_BUILD_2_0_API)
  list(APPEND EQ_DEFINITIONS -DEQ_2_0_API)
  set(EQ_FEATURES "${EQ_FEATURES} APIv2")
else()
  set(EQ_FEATURES "${EQ_FEATURES} APIv1")
endif()
add_definitions(${EQ_DEFINITIONS})

include(GLEW/CMakeLists.txt)
add_subdirectory(tools) # before examples, they depend on it
add_subdirectory(examples)
if(EXISTS "${Equalizer_SOURCE_DIR}/tests/CMakeLists.txt" )
  add_subdirectory(tests)
endif()

add_subdirectory(eq)
add_subdirectory(seq)

if(EXISTS "${CMAKE_SOURCE_DIR}/auxiliary" AND
   IS_DIRECTORY "${CMAKE_SOURCE_DIR}/auxiliary")
  add_subdirectory(auxiliary)
endif()
add_subdirectory(doc)

set(EQUALIZER_DEPENDENT_LIBRARIES vmmlib Lunchbox Collage)
if(GLEW_MX_FOUND)
  list(APPEND EQUALIZER_DEPENDENT_LIBRARIES GLEW_MX)
endif()

common_install(RELATIVE
  FILES ${CMAKE_SOURCE_DIR}/CMake/common/FindGLEW_MX.cmake
    CMake/EqGLLibraries.cmake
  DESTINATION ${CMAKE_MODULE_INSTALL_PATH}
  COMPONENT dev)
install(FILES ${DEFINES_FILE} DESTINATION include/eq COMPONENT dev)

if(HWLOC_GL_FOUND)
  set(EQ_FEATURES "${EQ_FEATURES} HWLOC")
endif()
if(LUNCHBOX_USE_DNSSD)
  set(EQ_FEATURES "${EQ_FEATURES} zeroconf")
endif()
if(HWSD_FOUND)
  set(EQ_FEATURES "${EQ_FEATURES} HWSD")
endif()
if(CUDA_FOUND)
  set(EQ_FEATURES "${EQ_FEATURES} CUDA")
endif()
if(DEFLECT_FOUND)
  set(EQ_FEATURES "${EQ_FEATURES} Deflect")
endif()
if(OpenCV_FOUND)
  set(EQ_FEATURES "${EQ_FEATURES} OpenCV")
endif()
if(VRPN_FOUND)
  set(EQ_FEATURES "${EQ_FEATURES} VRPN")
endif()
if(MAGELLAN_FOUND)
  set(EQ_FEATURES "${EQ_FEATURES} SpaceMouse")
endif()

if(MSVC)
  message(STATUS "Configured with${EQ_FEATURES}")
else()
  message(STATUS "Configured with ${CMAKE_BUILD_TYPE}${EQ_FEATURES}")
endif()

set(DOXYGEN_EXTRA_INPUT "${CMAKE_INSTALL_PREFIX}/include/seq ${CMAKE_INSTALL_PREFIX}/share/Equalizer/examples")
set(DOXYGEN_EXTRA_EXCLUDE "${CMAKE_INSTALL_PREFIX}/include/eq/admin {CMAKE_INSTALL_PREFIX}/include/GL")

include(CPackConfig)
include(PackageConfig)
include(DoxygenRule) # must be after all targets

set(CTEST_CUSTOM_WARNING_EXCEPTION ${CTEST_CUSTOM_WARNING_EXCEPTION}
  # http://public.kitware.com/Bug/view.php?id=10179
  "ld: warning: duplicate dylib")
include(CTest)
