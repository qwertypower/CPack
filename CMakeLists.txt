cmake_minimum_required(VERSION 2.8)
project(CPack)

OPTION( BUILD_SHARED_LIBS
  "Build package with shared libraries."
  ON
)
OPTION( USE_MMAN
  "For using memory mapping."
  ON
)
set(KEY_LENGHT "64" CACHE STRING "Max key string lenght")

FILE(GLOB HEADERS *.h*)
FILE(GLOB SOURCES *.c*)

IF(CMAKE_HAS_SORT)
  LIST(SORT HEADERS)
  LIST(SORT SOURCES)
ENDIF(CMAKE_HAS_SORT)

IF(USE_MMAN)
  add_definitions(-DUSE_MMAN=1)
  message(STATUS "Compile using mman.")
ELSE(NOT USE_MMAN)
  add_definitions(-DUSE_MMAN=0)
  message(STATUS "Compile using stdio file reader.")
ENDIF(USE_MMAN)


INCLUDE_DIRECTORIES(
  ${CMAKE_CURRENT_SOURCE_DIR}
)
add_definitions("-Dstringsize=${KEY_LENGHT}")
IF(BUILD_SHARED_LIBS)
  message(STATUS "Compile shared library.")
  add_definitions(-D__DLL__)
  ADD_LIBRARY(CPack SHARED ${SOURCES})
ELSE(NOT BUILD_SHARED_LIBS)
  message(STATUS "Compile static library.")
  ADD_LIBRARY(CPack STATIC ${SOURCES})
ENDIF(BUILD_SHARED_LIBS)