cmake_minimum_required(VERSION 3.10)
project(AushadhalayamLegacy CXX C)

# ─────────────────────────────────────────────────────────────────────────────
# TOOLCHAIN GUARD
# This project MUST be built with MinGW-w64 targeting Win32 (i686 or x86_64).
# MSVC and Clang-CL are explicitly unsupported.
# ─────────────────────────────────────────────────────────────────────────────
if(NOT MINGW)
    message(FATAL_ERROR "This target requires MinGW-w64. Use -DCMAKE_TOOLCHAIN_FILE or"
            " configure with the MinGW generator.")
endif()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ─────────────────────────────────────────────────────────────────────────────
# OUTPUT
# ─────────────────────────────────────────────────────────────────────────────
set(EXE_NAME aushadhalayam)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# ─────────────────────────────────────────────────────────────────────────────
# WINDOWS SUBSYSTEM & XP SP3 COMPATIBILITY
#
# The PE header minimum-OS version is set to 5.01 (Windows XP SP3 x86).
# For 64-bit XP (very rare), use 5.02 instead.
#
# -Wl,--subsystem,windows:5.01   → windowed app, min OS = XP SP3
# -mwindows                      → suppress the console window
# ─────────────────────────────────────────────────────────────────────────────
set(XP_SUBSYSTEM_FLAG "-Wl,--subsystem,windows:5.01")

# ─────────────────────────────────────────────────────────────────────────────
# STATIC RUNTIME LINKING
#
# Bundling libgcc, libstdc++, and libwinpthread into the EXE ensures no external
# DLL dependencies — critical for XP where Visual C++ runtimes are absent and
# MinGW runtimes may not be installed.
# ─────────────────────────────────────────────────────────────────────────────
set(STATIC_LINK_FLAGS "-static -static-libgcc -static-libstdc++")

# ─────────────────────────────────────────────────────────────────────────────
# COMPILER FLAGS
# ─────────────────────────────────────────────────────────────────────────────
set(COMMON_WARN "-Wall -Wextra -Wpedantic -Wno-deprecated-declarations")
set(OPTIMISE    "-O2 -ffunction-sections -fdata-sections")
set(DEAD_STRIP  "-Wl,--gc-sections")

set(CMAKE_CXX_FLAGS         "${CMAKE_CXX_FLAGS} ${COMMON_WARN}")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} ${OPTIMISE}")
set(CMAKE_EXE_LINKER_FLAGS  "${CMAKE_EXE_LINKER_FLAGS} ${STATIC_LINK_FLAGS} ${DEAD_STRIP} ${XP_SUBSYSTEM_FLAG} -mwindows")

# ─────────────────────────────────────────────────────────────────────────────
# SQLITE3 AMALGAMATION (bundled — never link the system sqlite3)
#
# Download sqlite-amalgamation-XXXXXXXX.zip from https://sqlite.org/download.html
# and unpack sqlite3.c + sqlite3.h into vendor/sqlite3/.
#
# SQLITE_THREADSAFE=1  → serialized mode; safe for one connection per thread.
# SQLITE_DEFAULT_WAL_SYNCHRONOUS=1 → NORMAL synchronous in WAL mode.
# SQLITE_OMIT_DEPRECATED is intentionally NOT set so wxSQLite3 compat is kept.
# ─────────────────────────────────────────────────────────────────────────────
add_library(sqlite3_static STATIC
    vendor/sqlite3/sqlite3.c
)
target_compile_definitions(sqlite3_static PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_DEFAULT_JOURNAL_MODE=WAL
    SQLITE_DEFAULT_WAL_SYNCHRONOUS=1
    SQLITE_DEFAULT_CACHE_SIZE=-8000
    SQLITE_ENABLE_JSON1
    SQLITE_ENABLE_FTS5
)
target_include_directories(sqlite3_static PUBLIC vendor/sqlite3)
# SQLite's amalgamation intentionally uses C (not C++) — compile as C
set_source_files_properties(vendor/sqlite3/sqlite3.c PROPERTIES LANGUAGE C)

# ─────────────────────────────────────────────────────────────────────────────
# wxWidgets (STATIC build required)
#
# Build instructions for static wxWidgets with MinGW:
#   cd wxWidgets-3.2.x/build/msw
#   mingw32-make -f makefile.gcc BUILD=release SHARED=0 UNICODE=1 MONOLITHIC=0
#
# Then point wx-config to your static build or set wxWidgets_ROOT_DIR.
# ─────────────────────────────────────────────────────────────────────────────
set(wxWidgets_USE_STATIC ON)
set(wxWidgets_USE_UNICODE ON)

find_package(wxWidgets REQUIRED
    COMPONENTS core base adv
)

if(NOT wxWidgets_FOUND)
    message(FATAL_ERROR "wxWidgets static libraries not found.\n"
        "Build wxWidgets 3.2.x with SHARED=0 and set wxWidgets_ROOT_DIR.")
endif()

include(${wxWidgets_USE_FILE})

# ─────────────────────────────────────────────────────────────────────────────
# APPLICATION SOURCES
# ─────────────────────────────────────────────────────────────────────────────
set(APP_SOURCES
    src/main.cpp
    src/MainFrame.cpp
    src/WorkerThread.cpp
    src/BillingGrid.cpp
    src/PrinterManager.cpp
    src/SidebarPanel.cpp
    src/ChartPanel.cpp
    src/DashboardPage.cpp
    src/BillingPage.cpp
    src/InventoryPage.cpp
    src/MedicinesPage.cpp
    src/PurchasePage.cpp
    src/SalesPage.cpp
    src/CustomersPage.cpp
    src/SettingsPage.cpp
)
# DatabaseManager.{h,cpp} is kept as a utility reference but is intentionally
# excluded from the build — WorkerThread.cpp uses the raw SQLite C API directly.

# Windows resource file — create src/resources.rc to add a version manifest
# and application icon.  Omitted here; uncomment when the file exists.
# if(WIN32)
#     list(APPEND APP_SOURCES src/resources.rc)
# endif()

add_executable(${EXE_NAME} WIN32 ${APP_SOURCES})

target_include_directories(${EXE_NAME} PRIVATE src vendor/sqlite3)

target_link_libraries(${EXE_NAME} PRIVATE
    sqlite3_static
    ${wxWidgets_LIBRARIES}
    # Win32 system libraries required by wxWidgets static build
    -lcomctl32
    -lole32
    -lgdi32
    -lcomdlg32
    -lwinspool
    -loleaut32
    -luuid
    -lrpcrt4
    -ladvapi32
    -lshell32
    -lwinmm
)

# ─────────────────────────────────────────────────────────────────────────────
# BUILD SIZE REPORT (Release only)
# ─────────────────────────────────────────────────────────────────────────────
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_custom_command(TARGET ${EXE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "=== Build complete ==="
        COMMAND ${CMAKE_SIZE} $<TARGET_FILE:${EXE_NAME}>
    )
endif()

# ─────────────────────────────────────────────────────────────────────────────
# QUICK-START BUILD COMMANDS
# ─────────────────────────────────────────────────────────────────────────────
#
# mkdir build && cd build
# cmake .. -G "MinGW Makefiles" \
#           -DCMAKE_BUILD_TYPE=Release \
#           -DwxWidgets_ROOT_DIR=/path/to/wxWidgets-3.2.x
# mingw32-make -j4
#
# The resulting bin/aushadhalayam.exe has ZERO external DLL dependencies and
# runs on Windows XP SP3 through Windows 11.
# ─────────────────────────────────────────────────────────────────────────────
