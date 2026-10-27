set(SE_DEFAULT_OUTPUT_NAME "scratch-xbox")

# Apparently setting these flags like this makes both xemu and real hardware work...
set(SE_RENDERER_VALID_OPTIONS "xbox" "headless")
set(SE_WINDOWING_VALID_OPTIONS "sdl3" "headless")
set(SE_AUDIO_ENGINE_VALID_OPTIONS "sdl3" "headless")

set(SE_DEPS_VALID_OPTIONS "fallback" "system")
set(SE_ZIP_BACKEND_VALID_OPTIONS "miniz")
set(SE_LUA_BACKEND_VALID_OPTIONS "fallback" "lua51")

set(SE_DECTALK_DEFAULT ON)
set(SE_CACHING_DEFAULT ON)
set(SE_BUNDLE_DEFAULT ON) # DO NOT TOUCH; Xbox relies on using bundle entirely.
set(SE_ALLOW_BUNDLE ON)
set(SE_ALLOW_CLOUDVARS OFF) # TODO: Revisit cloud vars once Mist++ is integrated into SE!
set(SE_ALLOW_DOWNLOAD ON)

set(SE_HAS_TOUCH FALSE)
set(SE_HAS_MOUSE FALSE)
set(SE_HAS_KEYBOARD FALSE)
set(SE_HAS_CONTROLLER TRUE)

set(SE_HAS_THREADS ON)

# While the runtime already adds any compatible project.sb/sb2/sb3 file into the executable,
# you can set this flag to ON if you want to place whatever projects you have in the romfs
# folder into the ISO image. This is helpful if you have a buggy Xemu FTP implementation.
set(COPY_PROJECTS_TO_XBOX OFF)

# - On 64MB retail consoles, SE_LWMEM_HEAP_SIZE_MB sets the baseline heap size.
# - On 128MB expanded consoles, the runtime dynamically scales the heap to
#   prioritize the runtime over the GPU.
# - It is recommended to leave this enabled for the Xbox.
set(SE_USE_LWMEM_DEFAULT ON)
set(SE_LWMEM_HEAP_SIZE_MB 32 CACHE STRING "Baseline size in MB of the lwmem heap")
set(SE_LWMEM_GPU_RESERVE_MB 32 CACHE STRING "Size in MB reserved for GPU and system buffers")

set(SE_PLATFORM_DEFINITIONS "__XBOX__")
set(SE_PLATFORM "xbox")

option(SE_XBOX_FORCE_480P "Use 640x480 instead of automatic Xbox HD mode selection" OFF)
option(SE_XBOX_ALLOW_EXPANDED_RAM "Clear the XBE's 64MB memory limit for expanded consoles" ON)
option(SE_XBOX_TRACE_GAME_STATE "Log stage variables when opening the runtime pause menu" OFF)
set(SE_XBOX_TRACE_LOG_HOST "" CACHE STRING "Optional unicast IPv4 destination for diagnostic UDP logs")
set(SE_XBOX_TRACE_LOG_PORT 18194 CACHE STRING "UDP port for optional Xbox diagnostic logs")
if(NOT SE_XBOX_TRACE_LOG_HOST STREQUAL "")
    if(NOT SE_XBOX_TRACE_LOG_HOST MATCHES "^[0-9]+\\.[0-9]+\\.[0-9]+\\.[0-9]+$")
        message(FATAL_ERROR "SE_XBOX_TRACE_LOG_HOST must be a unicast IPv4 literal")
    endif()
    string(REPLACE "." ";" trace_log_octets "${SE_XBOX_TRACE_LOG_HOST}")
    list(LENGTH trace_log_octets trace_log_octet_count)
    if(NOT trace_log_octet_count EQUAL 4)
        message(FATAL_ERROR "SE_XBOX_TRACE_LOG_HOST must be a unicast IPv4 literal")
    endif()
    foreach(octet IN LISTS trace_log_octets)
        if(NOT octet MATCHES "^[0-9][0-9]?[0-9]?$" OR octet MATCHES "^0[0-9]" OR octet GREATER 255)
            message(FATAL_ERROR "SE_XBOX_TRACE_LOG_HOST must be a unicast IPv4 literal")
        endif()
    endforeach()
    list(GET trace_log_octets 0 trace_log_first_octet)
    if(trace_log_first_octet EQUAL 0 OR trace_log_first_octet GREATER_EQUAL 224)
        message(FATAL_ERROR "SE_XBOX_TRACE_LOG_HOST must be a unicast IPv4 literal")
    endif()
    if(NOT SE_XBOX_TRACE_LOG_PORT MATCHES "^[0-9]+$" OR SE_XBOX_TRACE_LOG_PORT MATCHES "^0[0-9]" OR SE_XBOX_TRACE_LOG_PORT LESS 1 OR SE_XBOX_TRACE_LOG_PORT GREATER 65535)
        message(FATAL_ERROR "SE_XBOX_TRACE_LOG_PORT must be between 1 and 65535")
    endif()
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/source/log.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/platforms/xbox/os.cpp"
        APPEND PROPERTY COMPILE_DEFINITIONS
        "SE_XBOX_TRACE_LOG_HOST=\"${SE_XBOX_TRACE_LOG_HOST}\""
        "SE_XBOX_TRACE_LOG_PORT=${SE_XBOX_TRACE_LOG_PORT}")
endif()
set(SE_XBOX_TRACE_WATCH_VARIABLES "" CACHE STRING "Optional comma-separated stage variables or sprite fields for live diagnostic samples")
if(NOT SE_XBOX_TRACE_WATCH_VARIABLES STREQUAL "")
    if(SE_XBOX_TRACE_LOG_HOST STREQUAL "")
        message(FATAL_ERROR "SE_XBOX_TRACE_WATCH_VARIABLES requires SE_XBOX_TRACE_LOG_HOST")
    endif()
    string(LENGTH "${SE_XBOX_TRACE_WATCH_VARIABLES}" trace_watch_length)
    if(trace_watch_length GREATER 512 OR NOT SE_XBOX_TRACE_WATCH_VARIABLES MATCHES "^[-A-Za-z0-9_ #:,]+$")
        message(FATAL_ERROR "Watch names must use ASCII letters, digits, spaces, _, #, :, or -; separate names with commas")
    endif()
    string(REPLACE "," ";" trace_watch_names "${SE_XBOX_TRACE_WATCH_VARIABLES}")
    list(LENGTH trace_watch_names trace_watch_count)
    if(trace_watch_count GREATER 16)
        message(FATAL_ERROR "At most 16 fields may be watched")
    endif()
    set(trace_watch_normalized)
    foreach(name IN LISTS trace_watch_names)
        string(STRIP "${name}" name)
        string(LENGTH "${name}" name_length)
        if(name_length LESS 1 OR name_length GREATER 48)
            message(FATAL_ERROR "Each watched variable name must contain 1 to 48 characters")
        endif()
        list(APPEND trace_watch_normalized "${name}")
    endforeach()
    list(JOIN trace_watch_normalized "," trace_watch_definition)
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/cmake/xbox_variable_watch_config.hpp.in"
        "${CMAKE_CURRENT_BINARY_DIR}/generated/xbox_variable_watch_config.hpp" @ONLY)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/runtime_loop.cpp"
        APPEND PROPERTY COMPILE_DEFINITIONS SE_XBOX_TRACE_WATCH_VARIABLES_ENABLED)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/runtime_loop.cpp"
        APPEND PROPERTY INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}/generated")
endif()
option(SE_XBOX_TRACE_MENU_INPUT "Log bounded Xbox menu handoff input snapshots" OFF)
if(SE_XBOX_TRACE_MENU_INPUT)
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/source/menus/projectMenu.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/menus/pauseMenu.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/windowing/sdl3/input_sdl3.cpp"
        APPEND PROPERTY COMPILE_DEFINITIONS SE_XBOX_TRACE_MENU_INPUT)
endif()
option(SE_XBOX_TRACE_RENDERER "Log GPU pixel probes and render-cache statistics" OFF)
if(SE_XBOX_TRACE_RENDERER)
    set_property(SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/source/renderers/xbox/renderer_xbox.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/source/windowing/sdl3/window_sdl3.cpp"
        APPEND PROPERTY COMPILE_DEFINITIONS SE_XBOX_TRACE_RENDERER)
endif()
option(SE_XBOX_DEBUG_SYMBOLS "Keep DWARF symbols for xemu guest debugging" OFF)
if(SE_XBOX_TRACE_GAME_STATE)
    set_property(SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/source/menus/pauseMenu.cpp" APPEND PROPERTY
        COMPILE_DEFINITIONS SE_XBOX_TRACE_GAME_STATE)
endif()
if(SE_XBOX_FORCE_480P)
    add_compile_definitions(SE_XBOX_FORCE_480P)
endif()

include_directories(BEFORE ${CMAKE_CURRENT_SOURCE_DIR}/include/platforms/xbox)
include_directories(
    ${NXDK_DIR}/lib/net/lwip/src/include
    ${NXDK_DIR}/lib/net/nforceif/include
    ${NXDK_DIR}/lib/net/nvnetdrv
)

# HACK: Force CMake's internal system probes to see the NXDK network stack
# This should help libcurl detect sockets, recv, send, and struct sizes
list(APPEND CMAKE_REQUIRED_INCLUDES
    ${NXDK_DIR}/lib/net/lwip/src/include
    ${NXDK_DIR}/lib/net/lwip/src/include/posix
    ${NXDK_DIR}/lib/net/nforceif/include
    ${NXDK_DIR}/lib/net/nvnetdrv
)

set(CMAKE_REQUIRED_FLAGS "-D__XBOX__ -DUSE_LWIPSOCK")

# HACK: A few manual overrides
set(CURL_SIZEOF_CURL_SOCKET_T 4 CACHE INTERNAL "Forced size for NXDK")
set(CURL_SIZEOF_CURL_OFF_T 8 CACHE INTERNAL "Forced size for NXDK")
set(HAVE_POSIX_STRERROR_R 1 CACHE INTERNAL "Forced for NXDK")

add_compile_options(
    $<$<COMPILE_LANGUAGE:CXX>:-include>
    $<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/include/platforms/xbox/xbox_iostream_injector.hpp>
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
    $<$<COMPILE_LANGUAGE:C>:-U_MSC_VER>
    -march=pentium3
    -mtune=pentium3
    -D__XBOX__
    -DSE_NO_OPENGL
    -DSTBI_NO_SIMD
    -DHAVE_UNISTD_H
    -DHAVE_DIRENT_H
    -DHAVE_UTIME_H
    -DHAVE_FCNTL_H
    -DHAVE_IO_H
    -DUSE_LWIPSOCK
    -DLWIP_POSIX_SOCKETS_IO_NAMES=0
    -DLWIP_TIMEVAL_PRIVATE=0
    -DMINIZ_NO_TIME=1
    -Drestrict=__restrict
    -DXBOXRT_RESTRICT=__restrict
    -Dalloca=__builtin_alloca
    -Dftello=ftell
    -Dfseeko=fseek
    -DUNLEN=256
    -fms-extensions

    # Miscellaneous Xbox performance tuning
    -O1                  # Baseline optimizations, further ones are below
    -fomit-frame-pointer # Frees up the EBP register
    -msse                # Enable SSE instructions
    -mfpmath=sse         # Use SSE for floating point math instead of x87 FPU
    -fno-math-errno      # Tells the compiler math functions won't set errno (allows better vectorization)

    # Aligning loops and functions to the CPU's cache line boundary
    -falign-functions=16
    -falign-loops=16
)

# Optimize the filesystem, components of the runtime, renderers, and windowing to run with -O3 optimizations.
file(GLOB_RECURSE SE_MAX_OPTIMIZE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/filesystem.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/blockExecutor.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/math.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/time.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/value.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/renderers/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/windowing/*.cpp")
set_source_files_properties(${SE_MAX_OPTIMIZE_SOURCES} PROPERTIES COMPILE_OPTIONS "-O3;-ffast-math;-fno-finite-math-only")

# Optimize menus to run with -O2 optimizations.
file(GLOB_RECURSE SE_OPTIMIZE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/menus/*.cpp")
set_source_files_properties(${SE_OPTIMIZE_SOURCES} PROPERTIES COMPILE_OPTIONS "-O2;-ffast-math;-fno-finite-math-only")

# Everything else gets -O1 optimizations except for collisions.
file(GLOB_RECURSE SE_NO_OPTIMIZE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/runtime/collision.cpp")
file(GLOB_RECURSE SE_BASIC_OPTIMIZE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp")
list(REMOVE_ITEM SE_BASIC_OPTIMIZE_SOURCES ${SE_MAX_OPTIMIZE_SOURCES} ${SE_OPTIMIZE_SOURCES} ${SE_NO_OPTIMIZE_SOURCES})
set_source_files_properties(${SE_BASIC_OPTIMIZE_SOURCES} PROPERTIES COMPILE_OPTIONS "-O1;-ffast-math;-fno-finite-math-only")

# Collisions can only work accurately with tweaks and no other optimization flags
set_source_files_properties(${SE_NO_OPTIMIZE_SOURCES} PROPERTIES COMPILE_OPTIONS "-O0;-ffp-contract=off;-fno-strict-aliasing")

# HACK: Force CMake to skip failing network checks for libcurl on Xbox
set(HAVE_SYS_SOCKET_H 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_SOCKET 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_RECV 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_SEND 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_SELECT 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_GETADDRINFO 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_FREEADDRINFO 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_STRUCT_SOCKADDR_STORAGE 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_GETPEERNAME 1 CACHE INTERNAL "Forced for NXDK")
set(HAVE_GETSOCKNAME 1 CACHE INTERNAL "Forced for NXDK")

# HACK: Force type size checks to bypass CMake's failing test compilations
set(HAVE_SIZEOF_SSIZE_T 1 CACHE INTERNAL "Forced for NXDK")
set(SIZEOF_SSIZE_T 4 CACHE INTERNAL "Forced for NXDK")
set(HAVE_SIZEOF_TIME_T 1 CACHE INTERNAL "Forced for NXDK")
set(SIZEOF_TIME_T 4 CACHE INTERNAL "Forced for NXDK")
set(HAVE_SIZEOF_SUSECONDS_T 0 CACHE INTERNAL "Forced for NXDK")
unset(SIZEOF_SUSECONDS_T CACHE)
set(HAVE_SIZEOF_OFF_T 1 CACHE INTERNAL "Forced for NXDK")
set(SIZEOF_OFF_T 4 CACHE INTERNAL "Forced for NXDK")
set(HAVE_SIZEOF_SA_FAMILY_T 1 CACHE INTERNAL "Forced for NXDK")
set(SIZEOF_SA_FAMILY_T 2 CACHE INTERNAL "Forced for NXDK")
set(HAVE_SIZEOF_STRUCT_SOCKADDR_STORAGE 1 CACHE INTERNAL "Forced for NXDK")
set(SIZEOF_STRUCT_SOCKADDR_STORAGE 128 CACHE INTERNAL "Forced for NXDK")
set(HAVE_CURL_SIZEOF_CURL_SOCKET_T 1 CACHE INTERNAL "Forced for NXDK")
set(CURL_SIZEOF_CURL_SOCKET_T 4 CACHE INTERNAL "Forced for NXDK")
set(HAVE_CURL_SIZEOF_CURL_OFF_T 1 CACHE INTERNAL "Forced for NXDK")
set(CURL_SIZEOF_CURL_OFF_T 8 CACHE INTERNAL "Forced for NXDK")

# FIXME: Force mistpp to resolve via source on Xbox and apply the compatibility patch;
# remove when mistpp gets updated to support this target without patches
#set(CL_mistpp_STAGES source)
#set(CL_mistpp_PATCHES "${CMAKE_CURRENT_SOURCE_DIR}/cmake/patches/mistpp.patch")

macro(package_platform)
    if(SE_XBOX_DEBUG_SYMBOLS)
        target_compile_options(scratch-everywhere PRIVATE -g -gdwarf-4)
        target_link_options(scratch-everywhere PRIVATE -debug:dwarf -opt:ref -opt:icf)
        find_program(SE_XBOX_OBJCOPY NAMES i386-elf-objcopy i686-elf-objcopy
            x86_64-elf-objcopy gobjcopy objcopy
            HINTS /opt/homebrew/opt/binutils/bin /usr/local/opt/binutils/bin REQUIRED)
        add_custom_command(TARGET scratch-everywhere POST_BUILD
            COMMAND "${SE_XBOX_OBJCOPY}" -O elf32-i386
                "$<TARGET_FILE:scratch-everywhere>" "${CMAKE_CURRENT_BINARY_DIR}/scratch-xbox-symbols.elf"
            COMMAND "${SE_XBOX_OBJCOPY}" --strip-debug "$<TARGET_FILE:scratch-everywhere>"
            COMMENT "Saving xemu/GDB guest symbols"
        )
    endif()
    target_link_libraries(scratch-everywhere PUBLIC
        ${NXDK_DIR}/lib/libnxdk_automount_d.lib
        ${NXDK_DIR}/lib/libnxdk_net.lib
    )
    target_link_options(scratch-everywhere PRIVATE 
        "-safeseh:no"                  # REQUIRED FOR BUNDLE
        "-include:_automount_d_drive"  # TODO: Do we even need D: anymore if automount is on?
    )

    find_program(CXBE_BIN cxbe PATHS "${NXDK_DIR}/bin" "${NXDK_DIR}/tools/cxbe" "$ENV{NXDK_DIR}/bin" "$ENV{NXDK_DIR}/tools/cxbe")
    if(NOT CXBE_BIN)
        set(CXBE_BIN "${NXDK_DIR}/bin/cxbe")
    endif()

    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND ${CXBE_BIN}
        -OUT:${CMAKE_CURRENT_BINARY_DIR}/default.xbe
        -TITLE:"${SE_APP_NAME}"
        $<TARGET_FILE:scratch-everywhere>
        COMMENT "Making default.xbe"
    )

    if(SE_XBOX_ALLOW_EXPANDED_RAM)
        find_program(SE_XBOX_PYTHON NAMES python3 python REQUIRED)
        add_custom_command(TARGET scratch-everywhere POST_BUILD
            COMMAND "${SE_XBOX_PYTHON}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/configure-xbox-memory.py"
                "${CMAKE_CURRENT_BINARY_DIR}/default.xbe"
            COMMENT "Allowing expanded Xbox RAM"
        )
    endif()

    find_program(EXTRACT_XISO_BIN extract-xiso PATHS "${NXDK_DIR}/bin" "${NXDK_DIR}/tools/extract-xiso/build" "$ENV{NXDK_DIR}/bin" "$ENV{NXDK_DIR}/tools/extract-xiso/build")
    if(EXTRACT_XISO_BIN)
    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/iso_root"
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_BINARY_DIR}/default.xbe" "${CMAKE_CURRENT_BINARY_DIR}/iso_root/default.xbe"
        COMMENT "Preparing ISO root directory..."
    )
    if(COPY_PROJECTS_TO_XBOX)
    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy "${CMAKE_CURRENT_SOURCE_DIR}/romfs/*.sb*" "${CMAKE_CURRENT_BINARY_DIR}/iso_root"
        COMMENT "Copying projects in romfs to ISO..."
    )
    endif()
    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND "${EXTRACT_XISO_BIN}" -c "${CMAKE_CURRENT_BINARY_DIR}/iso_root" "${CMAKE_CURRENT_BINARY_DIR}/scratch-xbox.iso"
        COMMENT "Building scratch-xbox.iso"
    )
    add_custom_command(TARGET scratch-everywhere POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E remove_directory "${CMAKE_CURRENT_BINARY_DIR}/iso_root"
        COMMAND "${CMAKE_COMMAND}" -E remove "${CMAKE_CURRENT_BINARY_DIR}/scratch-xbox.exe"
        COMMENT "Cleaning up"
    )
endif()
endmacro()
