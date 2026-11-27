# Libraries for Unreal Engine's Linux target, built with the engine's own
# toolchain (../ue-linux.toolchain.cmake). Release only: RpgWire.Build.cs
# links the Release libraries for every configuration but Debug.
set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Linux)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../ue-linux.toolchain.cmake")
set(VCPKG_BUILD_TYPE release)
