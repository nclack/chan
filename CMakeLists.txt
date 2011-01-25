cmake_minimum_required(VERSION 2.8)
project(threads)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)
if(WIN32)
  set(ROOT_3RDPARTY_PATH "${CMAKE_SOURCE_DIR}/../3rdParty/")
endif()

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)
#set(CMAKE_VERBOSE_MAKEFILE 1) ## Useful for debuging the build

include_directories(src)

  set(CMAKE_THREAD_PREFER_PTHREAD 1)
  find_package(Threads)

###############################################################################
#  Targets
###############################################################################

  add_executable(hw src/hw.c)
  target_link_libraries(hw
      ${CMAKE_THREAD_LIBS_INIT}
      )

  add_executable(cv src/cv.c)
  target_link_libraries(cv
      ${CMAKE_THREAD_LIBS_INIT}
      )

  add_executable(cv2 src/cv2.c)
  target_link_libraries(cv2
      ${CMAKE_THREAD_LIBS_INIT}
      )

###############################################################################
#  Config
###############################################################################

  include(CheckFunctionExists)
  include(CheckIncludeFile)
  include(CheckCSourceCompiles)
  check_function_exists(usleep HAVE_USLEEP)
  check_c_source_compiles("main(){int a=0; return __sync_add_and_fetch(&a,10);}" HAVE_ATOMIC_INTRINSICS_GCC)
  check_function_exists(InterlockedIncrement HAVE_ATOMIC_INTRINSICS_MSVC)
  check_include_file(unistd.h HAVE_UNISTD)
  configure_file ("${PROJECT_SOURCE_DIR}/config.h.in"
      "${PROJECT_BINARY_DIR}/config.h" )
  include_directories("${PROJECT_BINARY_DIR}")

