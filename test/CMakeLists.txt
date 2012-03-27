cmake_minimum_required(VERSION 2.8)
#
# INPUT VARIABLES
# ---------------
#
# ROOT_3RDPARTY_PATH
#

#Some stuff to let this CMakeLists.txt stand alone, if needed
enable_testing(true)

#find gtest
set(GTEST_ROOT "${ROOT_3RDPARTY_PATH}/gtest")
set(GTEST_MSVC_SEARCH MD)
find_package(GTest)
if(GTEST_FOUND)
  include_directories(${GTEST_INCLUDE_DIRS})

##
# Config
#

  find_package(OpenMP)

##
# Add tests
#
  include_directories(
      ${CMAKE_SOURCE_DIR}/src
      ${CMAKE_SOURCE_DIR}/test
      ${CMAKE_BUILD_DIR}
      )
  file(GLOB PRIMARY_TEST_SOURCES 
      *.cpp)
  file(GLOB OTHER_SOURCES 
      ../src/*.c
      ../src/*.h
      ../src/*.cpp
      )
  add_executable(alltests 
    ${PRIMARY_TEST_SOURCES} 
    ${OTHER_SOURCES}
  )
  target_link_libraries(alltests
  ${GTEST_BOTH_LIBRARIES}
  )
  if(${OpenMP_FOUND})
    set_target_properties(
      alltests
      PROPERTIES
        COMPILE_FLAGS ${OpenMP_C_FLAGS}
        LINK_FLAGS    ${OpenMP_C_FLAGS}
    )
  endif()
  gtest_add_tests(alltests " " ${PRIMARY_TEST_SOURCES})
endif(GTEST_FOUND)
