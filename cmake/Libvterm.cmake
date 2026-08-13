# libvterm (https://github.com/neovim/libvterm) has no CMakeLists.txt of its
# own -- it ships a hand-written Makefile whose rule is just
# "compile every src/*.c file". Its Unicode encoding tables (src/encoding/
# *.inc) are pre-generated and checked into the repo, so building it needs
# no Perl step. License is MIT (see the fetched LICENSE file), so unlike
# QCustomPlot this needs no GPL isolation -- see docs/ARCHITECTURE.md §1.1
# for that unrelated, still-standing constraint.
#
# Because there's no CMakeLists.txt to add_subdirectory(), FetchContent_
# MakeAvailable() would fail (it tries to add_subdirectory the populated
# source by default). Use FetchContent_Populate directly instead, then
# define our own minimal static library target from the sources.

include(FetchContent)

set(LIBVTERM_PINNED_COMMIT 934bc2fbf21800ac3458a499df8820ca5fb45fd3)

FetchContent_Declare(
  libvterm_src
  GIT_REPOSITORY https://github.com/neovim/libvterm.git
  GIT_TAG ${LIBVTERM_PINNED_COMMIT}
)

FetchContent_GetProperties(libvterm_src)
if(NOT libvterm_src_POPULATED)
  # CMP0169 OLD: we need the "populate without add_subdirectory" behavior
  # on purpose (libvterm has no CMakeLists.txt to add), which is exactly
  # what direct FetchContent_Populate(<name>) does. The newer replacement
  # CMake steers toward (FetchContent_MakeAvailable) assumes a
  # CMakeLists.txt exists in the fetched source, which libvterm doesn't
  # have, so it isn't a drop-in substitute here.
  if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
  endif()
  FetchContent_Populate(libvterm_src)
endif()

file(GLOB SERIALKIT_LIBVTERM_SOURCES CONFIGURE_DEPENDS "${libvterm_src_SOURCE_DIR}/src/*.c")

add_library(vterm_c STATIC ${SERIALKIT_LIBVTERM_SOURCES})
target_include_directories(vterm_c
  PUBLIC ${libvterm_src_SOURCE_DIR}/include
  PRIVATE ${libvterm_src_SOURCE_DIR}/src
)
# libvterm is plain C99, not C++; make sure it's compiled as such regardless
# of the including project's C++ standard settings.
set_target_properties(vterm_c PROPERTIES
  C_STANDARD 99
  C_STANDARD_REQUIRED ON
  LINKER_LANGUAGE C
)
