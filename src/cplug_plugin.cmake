# REQUIRED CMAKE VARIABLES:
if (NOT DEFINED PROJECT_VERSION_MAJOR)
    message(FATAL_ERROR "Required variable \"PROJECT_VERSION_MAJOR\" is not defined. (project(name VERSION X.X.X))")
endif()
if (NOT DEFINED PROJECT_VERSION_MINOR)
    message(FATAL_ERROR "Required variable \"PROJECT_VERSION_MINOR\" is not defined. (project(name VERSION X.X.X))")
endif()
if (NOT DEFINED PROJECT_VERSION_PATCH)
    message(FATAL_ERROR "Required variable \"PROJECT_VERSION_PATCH\" is not defined. (project(name VERSION X.X.X))")
endif()
if (NOT DEFINED PROJECT_HOMEPAGE_URL)
    message(FATAL_ERROR "Required variable \"PROJECT_HOMEPAGE_URL\" is not defined. (project(name HOMEPAGE_URL XXXXX))")
endif()
if (NOT DEFINED COMPANY_NAME)
    message(FATAL_ERROR "Required variable \"COMPANY_NAME\" is not defined.")
endif()
if (NOT DEFINED COMPANY_EMAIL)
    message(FATAL_ERROR "Required variable \"COMPANY_EMAIL\" is not defined.")
endif()

# GENERATED CMAKE VARIABLES:
set(SRC_DIR ${PROJECT_SOURCE_DIR})
set(BIN_DIR ${CMAKE_BINARY_DIR})
set(PLUGIN_OPTIONS "")
set(PLUGIN_DEFINITIONS "")
set(PLUGIN_LIBRARIES "")

# Convert version string (1.0.0) > integer (65536)
math(EXPR MACOSX_BUNDLE_VERSION_INT "${PROJECT_VERSION_MAJOR} << 16 | ${PROJECT_VERSION_MINOR} << 8 | ${PROJECT_VERSION_PATCH}" OUTPUT_FORMAT DECIMAL) # cool trick bro

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE})

if (CMAKE_C_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_C_STANDARD 11)
else()
    set(CMAKE_C_STANDARD 99)
endif()

if (CMAKE_BUILD_TYPE MATCHES Release OR CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    set(CMAKE_CXX_STANDARD 20)
else()
    set(CMAKE_CXX_STANDARD 20)
endif()

if (WIN32)
    # Windows paths are complicated
    string(REPLACE "/" "\\\\" SRC_DIR ${SRC_DIR})
    string(REPLACE "/" "\\\\" SRC_DIR ${SRC_DIR})
endif()

# https://clang.llvm.org/docs/ClangCommandLineReference.html
if (CMAKE_C_COMPILER_ID STREQUAL "Clang" OR CMAKE_C_COMPILER_ID STREQUAL "AppleClang")
    list(APPEND PLUGIN_OPTIONS
        -Werror=absolute-value
        -Werror=return-type
        -Werror=shadow
        -Werror=incompatible-pointer-types
        -Werror=parentheses
        -Werror=excess-initializers
        -Werror=array-bounds
        # -Werror=implicit-fallthrough
        # -Wunused-function
        # -Wunused-variable

        -Wno-deprecated-declarations
        -Wno-deprecated
        -Wno-multichar
        -Wno-nullability-completeness
        -Wno-writable-strings
        -Wno-c2x-extensions
        -Wno-c++14-extensions
        -Wno-c++17-extensions
        -Wno-c++20-extensions
        -Wno-microsoft-enum-forward-reference
        -Wno-typedef-redefinition
        -Wno-visibility
        )
endif()


if (_WIN32)
    # Useful
    # https://gist.github.com/pervognsen/6b3c7f02c6e7db4b776bf92e0bb09d1b
    # list(APPEND PLUGIN_OPTIONS /fsanitize=address) # not working

    # list(APPEND PLUGIN_LIBRARIES clang_rt.asan-x86_64)
    # list(APPEND PLUGIN_OPTIONS "/MT")
else()
    # list(APPEND PLUGIN_OPTIONS "-fsanitize=address")
endif()

if (CMAKE_C_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    list(APPEND PLUGIN_OPTIONS /GR- /EHasc-)
    list(APPEND PLUGIN_OPTIONS "$<$<NOT:$<CONFIG:Debug>>:/Zi>")
    add_link_options("$<$<CONFIG:RELEASE>:/DEBUG>")
    if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        list(APPEND PLUGIN_OPTIONS /Od)
    endif()
else()
    list(APPEND PLUGIN_OPTIONS -fno-rtti -fno-exceptions)
    if (CMAKE_BUILD_TYPE MATCHES Debug OR CMAKE_BUILD_TYPE MATCHES RelWithDebInfo)
        list(APPEND PLUGIN_OPTIONS -fno-inline-functions)
    endif()
endif()

if (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    set(CMAKE_CXX_VISIBILITY_PRESET hidden)
    set(CMAKE_OBJC_VISIBILITY_PRESET hidden)
    set(CMAKE_VISIBILITY_INLINES_HIDDEN YES)
    # Currently getting release mode bugs on MacOS & Windows...
    # list(APPEND PLUGIN_OPTIONS -flto)
endif()

list(APPEND PLUGIN_DEFINITIONS
    CPLUG_COMPANY_NAME="${COMPANY_NAME}"
    CPLUG_COMPANY_EMAIL="${COMPANY_EMAIL}"
    CPLUG_PLUGIN_NAME="${PROJECT_NAME}"
    CPLUG_PLUGIN_URI="${PROJECT_HOMEPAGE_URL}"
    CPLUG_PLUGIN_VERSION="${PROJECT_VERSION}"

    SRC_DIR="${SRC_DIR}"
    BIN_DIR="${BIN_DIR}"
    )


if (WIN32)
    list(APPEND PLUGIN_DEFINITIONS _HAS_EXCEPTIONS=0 UNICODE _UNICODE _USE_MATH_DEFINES PW_DX11)

    # Is it time yet to force everyone to use AVX2 with haswell?
    # Windows 11 minimum requirement is Intel Coffee lake (8000 series, 2017+)
    if (CMAKE_BUILD_TYPE MATCHES Release)
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            list(APPEND PLUGIN_OPTIONS /arch:AVX)
        else()
            list(APPEND PLUGIN_OPTIONS -march=corei7-avx)
        endif()
    else()
        if (CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
            list(APPEND PLUGIN_OPTIONS /arch:AVX2)
        else()
            list(APPEND PLUGIN_OPTIONS -march=haswell)
        endif()
    endif()
    list(APPEND PLUGIN_LIBRARIES dxguid)
elseif(APPLE)
    enable_language(OBJC)
    # if (CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64")
    list(APPEND PLUGIN_OPTIONS -g)
    # endif()

    list(APPEND PLUGIN_LIBRARIES
        "-framework Quartz"
        "-framework Cocoa"
        "-framework Metal"
        "-framework MetalKit"
        "-framework AudioToolbox"
        )
    list(APPEND PLUGIN_DEFINITIONS PW_METAL)
endif()

# .plists are generated using a template
# The params @bundle_identifier@ and @bundle_type@ are used by the .plist
function(cplug_configure_info_plist target_name bundle_identifier bundle_type bundle_extension)
    configure_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/modules/CPLUG/example/Info.plist.in
        ${CMAKE_BINARY_DIR}/${bundle_type}.plist
        @ONLY
    )

    set_target_properties(${target_name} PROPERTIES
        BUNDLE True
        MACOSX_BUNDLE TRUE
        OUTPUT_NAME ${PROJECT_NAME}
        BUNDLE_EXTENSION ${bundle_extension}
        MACOSX_BUNDLE_INFO_PLIST ${CMAKE_BINARY_DIR}/${bundle_type}.plist
        CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_BUILD_TYPE}/${PROJECT_NAME}.${bundle_extension}
    )
endfunction()