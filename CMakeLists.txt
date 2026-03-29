cmake_minimum_required(VERSION 3.16)

if(NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    message(FATAL_ERROR
        "No toolchain file specified.\n"
        "Configure with:\n"
        "  cmake -DCMAKE_TOOLCHAIN_FILE=/path/to/psp.cmake ..")
endif()

project(MinecraftPSP CXX C ASM)

if(NOT PSPSDK OR NOT PSPDEV)
    execute_process(
        COMMAND psp-config --pspsdk-path
        OUTPUT_VARIABLE PSPSDK
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    execute_process(
        COMMAND psp-config --pspdev-path
        OUTPUT_VARIABLE PSPDEV
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
endif()

if(NOT PSPSDK)
    message(FATAL_ERROR "psp-config not found — make sure the PSP toolchain is in PATH.")
endif()

message(STATUS "PSPDEV  = ${PSPDEV}")
message(STATUS "PSPSDK  = ${PSPSDK}")

set(PSP_FW_VERSION 600 CACHE STRING "PSP firmware version target (e.g. 600)")

set(PSP_LARGE_MEMORY "" CACHE STRING "Large-memory mode: 0=off 1=on 2=extra (empty=auto)")

set(EXPAND_MEMORY 0)
if(PSP_FW_VERSION GREATER 390)
    set(EXPAND_MEMORY 1)
    if(PSP_LARGE_MEMORY STREQUAL "0")
        set(EXPAND_MEMORY 0)
    elseif(PSP_LARGE_MEMORY STREQUAL "2")
        set(EXPAND_MEMORY 2)
    endif()
else()
    if(PSP_LARGE_MEMORY STREQUAL "1")
        set(EXPAND_MEMORY 1)
    elseif(PSP_LARGE_MEMORY STREQUAL "2")
        set(EXPAND_MEMORY 2)
    endif()
endif()

message(STATUS "EXPAND_MEMORY = ${EXPAND_MEMORY}")

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/world/*.cpp
    src/render/*.cpp
    src/math/*.cpp
    src/input/*.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES})

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_11)

target_compile_options(${PROJECT_NAME} PRIVATE
    -O2
    -G0
    -Wall
    -fno-exceptions
    -fno-rtti
    -Wno-misleading-indentation
    -Wno-unused-function
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    PSP
    _PSP_FW_VERSION=${PSP_FW_VERSION}
)

target_include_directories(${PROJECT_NAME} PRIVATE
    src
    ${PSPSDK}/include
    ${PSPDEV}/psp/include
)

target_link_options(${PROJECT_NAME} PRIVATE
    -L${PSPSDK}/lib
    -L${PSPDEV}/psp/lib
    -Wl,-zmax-page-size=128
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    stdc++
    pspgum
    pspgu
    pspaudiolib
    pspaudio
    psprtc
    psppower
    m

    pspdebug
    pspdisplay
    pspge
    pspctrl
    pspnet
    pspnet_apctl
)

find_program(PSP_FIXUP psp-fixup-imports REQUIRED)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${PSP_FIXUP} $<TARGET_FILE:${PROJECT_NAME}>
    COMMENT "Running psp-fixup-imports"
)

find_program(MKSFOEX mksfoex REQUIRED)

set(PSP_EBOOT_TITLE "Minecraft PSP")
set(PSP_EBOOT_SFO   "${CMAKE_CURRENT_BINARY_DIR}/PARAM.SFO")

add_custom_command(
    OUTPUT  ${PSP_EBOOT_SFO}
    COMMAND ${MKSFOEX} -d MEMSIZE=${EXPAND_MEMORY} "${PSP_EBOOT_TITLE}" ${PSP_EBOOT_SFO}
    COMMENT "Generating PARAM.SFO"
    VERBATIM
)
add_custom_target(param_sfo ALL DEPENDS ${PSP_EBOOT_SFO})

find_program(PACK_PBP  pack-pbp  REQUIRED)
find_program(PSP_STRIP psp-strip REQUIRED)

set(PSP_EBOOT_ICON  NULL)
set(PSP_EBOOT_ICON1 NULL)
set(PSP_EBOOT_PIC0  NULL)
set(PSP_EBOOT_PIC1  NULL)
set(PSP_EBOOT_SND0  NULL)
set(PSP_EBOOT_PSAR  NULL)

set(STRIPPED_ELF  "${CMAKE_CURRENT_BINARY_DIR}/${PROJECT_NAME}_strip.elf")
set(PSP_EBOOT_PBP "${CMAKE_CURRENT_BINARY_DIR}/EBOOT.PBP")

add_custom_command(
    OUTPUT  ${PSP_EBOOT_PBP}
    COMMAND ${PSP_STRIP} $<TARGET_FILE:${PROJECT_NAME}> -o ${STRIPPED_ELF}
    COMMAND ${PACK_PBP}
                ${PSP_EBOOT_PBP}
                ${PSP_EBOOT_SFO}
                ${PSP_EBOOT_ICON}
                ${PSP_EBOOT_ICON1}
                ${PSP_EBOOT_PIC0}
                ${PSP_EBOOT_PIC1}
                ${PSP_EBOOT_SND0}
                ${STRIPPED_ELF}
                ${PSP_EBOOT_PSAR}
    COMMAND ${CMAKE_COMMAND} -E remove -f ${STRIPPED_ELF}
    DEPENDS ${PROJECT_NAME} param_sfo
    COMMENT "Packing EBOOT.PBP"
    VERBATIM
)
add_custom_target(eboot ALL DEPENDS ${PSP_EBOOT_PBP})