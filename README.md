# shmCpp
A C++ interface for using POSIX shared memory

This is a header-only C++ library for creating and manipulating POSIX shared memory objects.

`shmCpp` should work with any C++ standard from C++03 onward.
However, the tests require C++11 or later.


## Including shmCpp in your Project

As this library is header-only, very little installation is required.
Simply clone the repository to a sensible location in your project, and `#include` the header file `shmCpp.hpp`.

When compiling, you will need to use `-I <shmCpp-root-dir>/include` so the compiler can find the header file.
You will also need to link the `librt` library. With GCC, this can be accomplished by adding `-lrt` to your compile command(s).

### CMake-based projects

Simply add:
```
add_subdirectory(<shmCpp-root-dir>)
include_directories(<shmCpp-root-dir>/include)
target_link_libraries(<your-compile-target> rt)
```
to your `CMakeLists.txt` file.
