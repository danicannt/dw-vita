# 32-bit ARM Linux (ARMv7-A, hard float): stands in for the Vita's Cortex-A9
# when VitaSDK is not at hand. Tests run under qemu-arm.
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc)
set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++)
set(CMAKE_C_FLAGS_INIT "-march=armv7-a -mtune=cortex-a9 -mfpu=neon")
set(CMAKE_CXX_FLAGS_INIT "-march=armv7-a -mtune=cortex-a9 -mfpu=neon")
set(CMAKE_FIND_ROOT_PATH /usr/arm-linux-gnueabihf)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_CROSSCOMPILING_EMULATOR qemu-arm;-L;/usr/arm-linux-gnueabihf)
