# CMake toolchain file for cross-compiling to Windows x86_64 using llvm-mingw.
#
# Usage:
#   cmake -B build-win -S . \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/windows-x86_64-llvm-mingw.cmake \
#     -DLLVM_MINGW_ROOT=/path/to/llvm-mingw

if(NOT DEFINED LLVM_MINGW_ROOT)
  message(FATAL_ERROR
    "LLVM_MINGW_ROOT must be set to the llvm-mingw installation directory.\n"
    "Example: cmake ... -DLLVM_MINGW_ROOT=/opt/llvm-mingw")
endif()

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_TARGET_TRIPLE x86_64-w64-mingw32)

set(CMAKE_C_COMPILER   "${LLVM_MINGW_ROOT}/bin/${_TARGET_TRIPLE}-clang")
set(CMAKE_CXX_COMPILER "${LLVM_MINGW_ROOT}/bin/${_TARGET_TRIPLE}-clang++")
set(CMAKE_RC_COMPILER  "${LLVM_MINGW_ROOT}/bin/${_TARGET_TRIPLE}-windres")
set(CMAKE_AR           "${LLVM_MINGW_ROOT}/bin/llvm-ar")
set(CMAKE_RANLIB       "${LLVM_MINGW_ROOT}/bin/llvm-ranlib")
set(CMAKE_STRIP        "${LLVM_MINGW_ROOT}/bin/llvm-strip")

# Direct the linker (lld) to produce a PDB alongside the executable.
# The empty value after = tells lld to derive the PDB name from the output name.
set(CMAKE_EXE_LINKER_FLAGS_INIT    "-Wl,--pdb=")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--pdb=")

# Emit CodeView debug information (required for PDB format).
set(CMAKE_C_FLAGS_INIT   "-g -gcodeview")
set(CMAKE_CXX_FLAGS_INIT "-g -gcodeview")

# Tell CMake where to find target headers/libraries.
set(CMAKE_SYSROOT "${LLVM_MINGW_ROOT}/${_TARGET_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH "${LLVM_MINGW_ROOT}/${_TARGET_TRIPLE}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
