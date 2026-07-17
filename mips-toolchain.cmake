set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR mips)

# Define the path to your cross-compiler
set(TOOLS "/home/tavisco/Projects/Hiby-R1-Mod/teste/mips-gcc540-glibc222-r3.3.9")
set(CMAKE_C_COMPILER "${TOOLS}/bin/mips-linux-gnu-gcc")
set(CMAKE_CXX_COMPILER "${TOOLS}/bin/mips-linux-gnu-g++")

# Set the target architecture flags to match your previous test
set(CMAKE_C_FLAGS "-march=mips32r2 -mabi=32 -mnan=legacy")
set(CMAKE_CXX_FLAGS "-march=mips32r2 -mabi=32 -mnan=legacy")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
