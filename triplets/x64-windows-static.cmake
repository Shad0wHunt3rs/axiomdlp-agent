set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Windows)

# Suppress deprecated warnings treated as errors in gRPC/Abseil
set(VCPKG_CXX_FLAGS "/wd4996")
set(VCPKG_C_FLAGS "/wd4996")