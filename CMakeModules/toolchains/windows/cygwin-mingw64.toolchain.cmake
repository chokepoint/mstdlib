# Toolchain for cross-compiling to 64-bit Windows from Cygwin using mingw64.
set(CMAKE_TOOLCHAIN_PREFIX x86_64-w64-mingw32)
include("${CMAKE_CURRENT_LIST_DIR}/cygwin-mingw-common.cmake")
