# The toolchain the libraries are built with: Unreal Engine's own, for its
# Linux x86_64 target — its clang, its headers, its libc++ — and the flags its
# build tool compiles a module with. vcpkg loads this file through the triplet
# beside it (scripts/libs/triplets/x64-linux-ue.cmake); scripts/build-libs.sh
# sets the two environment variables it reads.
#
# The flags are set here, as CMAKE_<LANG>_FLAGS_INIT, because vcpkg does not
# apply a triplet's VCPKG_C_FLAGS and VCPKG_CXX_FLAGS when a toolchain file is
# chained in: the port's CMakeCache.txt shows CMAKE_CXX_FLAGS empty otherwise.

set(UE_SDK "$ENV{UE_LINUX_SDK}")              # .../HostLinux/Linux_x64/v<version>/x86_64-unknown-linux-gnu
set(UE_TARGET x86_64-unknown-linux-gnu)

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER "${UE_SDK}/bin/clang")
set(CMAKE_CXX_COMPILER "${UE_SDK}/bin/clang++")
set(CMAKE_C_COMPILER_TARGET "${UE_TARGET}")
set(CMAKE_CXX_COMPILER_TARGET "${UE_TARGET}")
set(CMAKE_SYSROOT "${UE_SDK}")

set(CMAKE_FIND_ROOT_PATH "${UE_SDK}")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# As the build tool compiles a module (RpgWire.Build.cs: no exceptions, no RTTI),
# and hidden, so that what the module links is the module's own. The C++
# headers are named as the build tool names them; the C headers come from the
# sysroot, which is why there is no -nostdinc here.
set(CMAKE_C_FLAGS_INIT "-fPIC -fvisibility=hidden")
set(CMAKE_CXX_FLAGS_INIT "-fPIC -fvisibility=hidden -fno-rtti -fno-exceptions -DGOOGLE_PROTOBUF_NO_RTTI -nostdinc++ -isystem ${UE_SDK}/include -isystem ${UE_SDK}/include/c++/v1")
set(CMAKE_EXE_LINKER_FLAGS_INIT "-stdlib=libc++ -fuse-ld=lld -static-libstdc++")

# What vcpkg has already built for this target is found before the sysroot
# serves the rest. A root-path rule of ONLY alone hides every earlier package
# from the next one's find_package, and the second package fails to configure.
if(DEFINED ENV{UE_VCPKG_INSTALL_ROOT})
  list(INSERT CMAKE_FIND_ROOT_PATH 0 "$ENV{UE_VCPKG_INSTALL_ROOT}/${VCPKG_TARGET_TRIPLET}")
endif()
