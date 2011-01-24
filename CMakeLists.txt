cmake_minimum_required(VERSION 2.8)
project(threads)

IF(NOT CMAKE_BUILD_TYPE)
  SET(CMAKE_BUILD_TYPE Debug CACHE STRING
      "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
      FORCE)
ENDIF(NOT CMAKE_BUILD_TYPE)

  set(CMAKE_THREAD_PREFER_PTHREAD 1)
  find_package(Threads)

  add_executable(hw src/hw.c)
  target_link_libraries(hw
      ${CMAKE_THREAD_LIBS_INIT}
      )
