if(DEFINED ENV{PSPDEV})
    set(PSPDEV "$ENV{PSPDEV}" CACHE PATH "Root of the PSP toolchain")
else()
    execute_process(
        COMMAND psp-config --pspdev-path
        OUTPUT_VARIABLE PSPDEV
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    set(PSPDEV "${PSPDEV}" CACHE PATH "Root of the PSP toolchain")
endif()

if(NOT PSPDEV OR NOT EXISTS "${PSPDEV}")
    message(FATAL_ERROR
        "Cannot locate PSPDEV.\n"
        "Either set the PSPDEV environment variable or make sure psp-config is in PATH.")
endif()

execute_process(
    COMMAND psp-config --pspsdk-path
    OUTPUT_VARIABLE PSPSDK
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
set(PSPSDK "${PSPSDK}" CACHE PATH "Path to PSPSDK")
set(CMAKE_SYSTEM_NAME      Generic)
set(CMAKE_SYSTEM_PROCESSOR mips)

set(PSP_PREFIX "${PSPDEV}/bin/psp-")

set(CMAKE_C_COMPILER        "${PSP_PREFIX}gcc")
set(CMAKE_CXX_COMPILER      "${PSP_PREFIX}g++")
set(CMAKE_ASM_COMPILER      "${PSP_PREFIX}gcc")
set(CMAKE_AR                "${PSP_PREFIX}gcc-ar"   CACHE FILEPATH "")
set(CMAKE_RANLIB            "${PSP_PREFIX}gcc-ranlib" CACHE FILEPATH "")
set(CMAKE_LINKER            "${PSP_PREFIX}gcc")
set(CMAKE_STRIP             "${PSP_PREFIX}strip")
set(CMAKE_OBJCOPY           "${PSP_PREFIX}objcopy")
set(CMAKE_OBJDUMP           "${PSP_PREFIX}objdump")
set(CMAKE_SIZE              "${PSP_PREFIX}size")
set(CMAKE_SYSROOT           "${PSPDEV}/psp")
set(CMAKE_FIND_ROOT_PATH    "${PSPDEV}/psp" "${PSPSDK}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER) 
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(PSP_COMMON_FLAGS "-D_PSP_FW_VERSION=600 -G0")

set(CMAKE_C_FLAGS_INIT   "${PSP_COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_INIT "${PSP_COMMON_FLAGS}")
set(CMAKE_ASM_FLAGS_INIT "${PSP_COMMON_FLAGS}")

set(ENV{PSPDEV}  "${PSPDEV}")
set(ENV{PSPSDK} "${PSPSDK}")