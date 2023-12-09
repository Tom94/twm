cmake_minimum_required(VERSION 3.13)

project(
    twm
    VERSION 0.1
	DESCRIPTION "tiny tiling window manager for Windows"
    LANGUAGES C CXX
)
set(TWM_VERSION "${CMAKE_PROJECT_VERSION}")

if (NOT TWM_DEPLOY)
    set(TWM_VERSION "${TWM_VERSION}dev")
endif()

if (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(TWM_VERSION_ARCH "${TWM_VERSION} (64 bit)")
elseif (CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(TWM_VERSION_ARCH "${TWM_VERSION} (32 bit)")
endif()

# Set ourselves as the startup project in visual studio.
# Not available until cmake 3.6, but doesn't break older versions.
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT twm)

# Set a default configuration if none was specified
if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    message(STATUS "No release type specified. Setting to 'Release'.")
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS "Debug" "Release" "RelWithDebInfo")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if (MSVC)
    # Disable annoying secure CRT warnings
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_CRT_SECURE_NO_WARNINGS")

    # Parallel build
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /MP24")

    # Static build
    set(CompilerFlags
        CMAKE_CXX_FLAGS CMAKE_CXX_FLAGS_DEBUG CMAKE_CXX_FLAGS_RELEASE
        CMAKE_CXX_FLAGS_MINSIZEREL CMAKE_CXX_FLAGS_RELWITHDEBINFO
        CMAKE_C_FLAGS CMAKE_C_FLAGS_DEBUG CMAKE_C_FLAGS_RELEASE
        CMAKE_C_FLAGS_MINSIZEREL CMAKE_C_FLAGS_RELWITHDEBINFO
    )

    foreach (CompilerFlag ${CompilerFlags})
        string(REPLACE "/MD" "/MT" ${CompilerFlag} "${${CompilerFlag}}")
    endforeach()

	# Enable more warnings
    if (CMAKE_CXX_FLAGS MATCHES "/W[0-4]")
        string(REGEX REPLACE "/W[0-4]" "/W4" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
    else()
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /W4")
    endif()

    # twm uses UTF-8 chars. Specify UTF-8 charset explicitly.
    # "/utf-8" is equivalent to "/source-charset:utf-8 /execution-charset:utf-8".
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /utf-8")

    # To allow for wildcards in command-line path arguments on windows,
    # we need to link to wsetargv.obj
    # http://msdn.microsoft.com/en-us/library/8bch7bkk.aspx
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} wsetargv.obj")
elseif (CMAKE_CXX_COMPILER_ID MATCHES "Clang" OR CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    if (${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s USE_ZLIB=1 -s DISABLE_EXCEPTION_CATCHING=0")
    endif()

    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wno-unused-parameter")
    if (CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-gnu-anonymous-struct -Wno-c99-extensions -Wno-nested-anon-types -Wno-deprecated-register -Wno-deprecated-anon-enum-enum-conversion")
    endif()
    if (CMAKE_CXX_COMPILER_ID MATCHES "GNU")
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-misleading-indentation -Wno-deprecated-declarations")
    endif()
endif()

# Prefer libc++ in conjunction with Clang
include(CheckCXXSourceRuns)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

macro(CHECK_CXX_COMPILER_AND_LINKER_FLAGS _RESULT _CXX_FLAGS _LINKER_FLAGS)
    set(CMAKE_REQUIRED_FLAGS ${_CXX_FLAGS})
    set(CMAKE_REQUIRED_LIBRARIES ${_LINKER_FLAGS})
    set(CMAKE_REQUIRED_QUIET TRUE)
    check_cxx_source_runs("#include <iostream>\nint main(int argc, char **argv) { std::cout << \"test\"; return 0; }" ${_RESULT})
    set(CMAKE_REQUIRED_FLAGS "")
    set(CMAKE_REQUIRED_LIBRARIES "")
endmacro()

if (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
    CHECK_CXX_COMPILER_AND_LINKER_FLAGS(HAS_LIBCPP "-stdlib=libc++" "-stdlib=libc++")
    if (APPLE OR HAS_LIBCPP)
        set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -D_LIBCPP_VERSION")
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++")
        set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++")
        message(STATUS "twm: using libc++.")
    else()
        CHECK_CXX_COMPILER_AND_LINKER_FLAGS(HAS_LIBCPP_AND_CPPABI "-stdlib=libc++" "-stdlib=libc++ -lc++abi")
        if (HAS_LIBCPP_AND_CPPABI)
            set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -stdlib=libc++ -D_LIBCPP_VERSION")
            set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
            set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -stdlib=libc++ -lc++abi")
            message(STATUS "twm: using libc++ and libc++abi.")
        else()
            message(FATAL_ERROR
                "When Clang is used to compile twm, libc++ "
                "must be available -- GCC's libstdc++ is not supported! (please install "
                "the libc++ development headers, provided e.g. by the packages "
                "'libc++-dev' and 'libc++abi-dev' on Debian/Ubuntu)."
            )
        endif()
    endif()
endif()

set(
	TWM_SOURCES
	src/main.cpp
	src/common.cpp include/twm/common.h
	src/config.cpp include/twm/config.h
	src/hotkey.cpp include/twm/hotkey.h
	src/logging.cpp include/twm/logging.h
	src/math.cpp include/twm/math.h
	src/platform.cpp include/twm/platform.h
)

# set(TWM_SOURCES ${TWM_SOURCES} resources/icon.rc)
# set(TWM_SOURCES ${TWM_SOURCES} resources/twm.manifest)

set(TWM_LIBS dwmapi)

add_executable(twm ${TWM_SOURCES})
target_link_libraries(twm PUBLIC ${TWM_LIBS})
target_include_directories(twm PUBLIC
	"${CMAKE_CURRENT_SOURCE_DIR}/include"
	"${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tinylogger"
	"${CMAKE_CURRENT_SOURCE_DIR}/dependencies/tomlplusplus/include"
)

set(TWM_DEFINITIONS -DTWM_VERSION="${TWM_VERSION_ARCH}")
target_compile_definitions(twm PUBLIC ${TWM_DEFINITIONS})

set(CPACK_PACKAGE_VENDOR "Tom94 (Thomas Müller)")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Image viewer and comparison tool for graphics people.")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://tom94.net")
set(CPACK_PACKAGE_INSTALL_DIRECTORY "${PROJECT_NAME}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE.txt")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")

# Installation via WIX
install(TARGETS twm RUNTIME DESTINATION "bin")
set(CPACK_GENERATOR "WIX")
set(CPACK_PACKAGE_FILE_NAME "${PROJECT_NAME}-installer")

# set(CPACK_PACKAGE_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.png")
set(CPACK_PACKAGE_EXECUTABLES "twm" "twm")

set(CPACK_WIX_PATCH_FILE "${CMAKE_CURRENT_SOURCE_DIR}/resources/patch.wxs")
# set(CPACK_WIX_UPGRADE_GUID "F3EC0D95-B2DC-4451-975C-CF591871E162")
# set(CPACK_WIX_PRODUCT_GUID "F7478E0D-33A4-4F00-829B-45CE27B1D0E2")
# set(CPACK_WIX_PRODUCT_ICON "${CMAKE_CURRENT_SOURCE_DIR}/resources/icon.ico")

configure_file(scripts/configure-winget.cmake scripts/configure-winget.cmake @ONLY)
configure_file(resources/winget.yaml resources/winget.yaml COPYONLY)
set(CPACK_POST_BUILD_SCRIPTS "${CMAKE_BINARY_DIR}/scripts/configure-winget.cmake")

include(CPack)
