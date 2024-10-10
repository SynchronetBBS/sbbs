# the name of the target operating system
set(CMAKE_SYSTEM_NAME Windows)

# Add search path
list(APPEND CMAKE_PROGRAM_PATH /home/admin/mingw-w32/bin)

# which compilers to use for C and C++
set(CMAKE_C_COMPILER   /home/admin/mingw-w32/bin/i686-w64-mingw32-gcc)
set(CMAKE_CXX_COMPILER /home/admin/mingw-w32/bin/i686-w64-mingw32-g++)
set(CMAKE_AR           /home/admin/mingw-w32/bin/i686-w64-mingw32-ar)

# where is the target environment located
set(CMAKE_FIND_ROOT_PATH /home/admin/mingw-w32)

# adjust the default behavior of the FIND_XXX() commands:
# search programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)

# search headers and libraries in the target environment
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

