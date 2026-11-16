# Custom FindThreads module for cross-compiling platforms like Xbox (NXDK)
# which provide threading via WinAPI instead of POSIX pthreads and where
# CMAKE_SYSTEM_NAME is not "Windows".

if(XBOX)
    set(Threads_FOUND TRUE)
    set(CMAKE_THREAD_LIBS_INIT "")
    set(CMAKE_HAVE_THREADS_LIBRARY 1)
    set(CMAKE_USE_WIN32_THREADS_INIT 1)
    set(CMAKE_USE_PTHREADS_INIT 0)
    set(CMAKE_HAVE_LIBC_PTHREAD 1)

    if(NOT TARGET Threads::Threads)
        add_library(Threads::Threads INTERFACE IMPORTED GLOBAL)
    endif()

    include(FindPackageHandleStandardArgs)
    find_package_handle_standard_args(Threads DEFAULT_MSG Threads_FOUND)
else()
    # Forward to CMake's built-in FindThreads module
    include("${CMAKE_ROOT}/Modules/FindThreads.cmake")
endif()
