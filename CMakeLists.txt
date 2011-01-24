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

  set(CMAKE_THREAD_PREFER_PTHREAD 1)
  find_package(Threads)

  add_executable(hw src/hw.c)
  target_link_libraries(hw
      ${CMAKE_THREAD_LIBS_INIT}
      )
include_directories(src)

###############################################################################
#  Config
###############################################################################

  include(CheckFunctionExists)
  include(CheckIncludeFile)
  check_function_exists(usleep HAVE_USLEEP)
  check_include_file(unistd.h HAVE_UNISTD)
  configure_file ("${PROJECT_SOURCE_DIR}/config.h.in"
      "${PROJECT_BINARY_DIR}/config.h" )
  include_directories("${PROJECT_BINARY_DIR}")

