cmake_minimum_required(VERSION 2.8)
project(chan)

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

  set(CMAKE_THREAD_PREFER_PTHREAD 0)
  find_package(Threads)

macro(show VAR)
#message("** ${VAR} is ${${VAR}}")
endmacro()

  set(USE_PTHREAD       ${CMAKE_USE_PTHREADS_INIT})
  set(USE_WIN32_THREADS ${CMAKE_USE_WIN32_THREADS_INIT})

  show(CMAKE_THREAD_LIBS_INIT)
  show(CMAKE_USE_SPROC_INIT)
  show(CMAKE_USE_WIN32_THREADS_INIT)
  show(CMAKE_USE_PTHREADS_INIT)
  show(CMAPE_HP_PTHREADS_INIT)

###############################################################################
#  Targets
###############################################################################

  file(GLOB SOURCES
    src/*.h
    src/*.c
    config.h.in
    )
  set(SOURCES ${SOURCES} "${PROJECT_BINARY_DIR}/config.h")

  add_executable(cv app/cv2.c ${SOURCES})
  add_executable(egchan app/egchan.c ${SOURCES})

###############################################################################
#  Config
###############################################################################

  include(CheckFunctionExists)
  include(CheckIncludeFile)
  include(CheckCSourceCompiles)
  include(CheckTypeSize)
  check_type_size(size_t SIZET_BYTES)
  check_function_exists(usleep HAVE_USLEEP)
  check_c_source_compiles("main(){int a=0; return __sync_add_and_fetch(&a,10);}" HAVE_ATOMIC_INTRINSICS_GCC)
  if(WIN32)
    set(HAVE_ATOMIC_INTRINSICS_MSVC TRUE)
  else()
    set(HAVE_ATOMIC_INTRINSICS_MSVC FALSE)
  endif()
  check_include_file(unistd.h HAVE_UNISTD)
  check_include_file(stdint.h HAVE_STDINT)
  configure_file ("${PROJECT_SOURCE_DIR}/config.h.in"
      "${PROJECT_BINARY_DIR}/config.h" )
  include_directories("${PROJECT_BINARY_DIR}")

###############################################################################
#  Test  
###############################################################################
enable_testing(true)
add_subdirectory(test)
