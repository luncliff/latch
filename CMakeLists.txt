cmake_minimum_required(VERSION 3.12)
project(latch VERSION 1.0 LANGUAGES CXX)

string(TOLOWER ${CMAKE_SYSTEM_NAME} SYSTEM_NAME)
message(STATUS "using system: ${SYSTEM_NAME}")

add_library(latch
    latch.h
    latch_${SYSTEM_NAME}.cpp
)

set_target_properties(latch
PROPERTIES
    CXX_STANDARD 20
)

if(APPLE)
    set_target_properties(latch
    PROPERTIES
        PUBLIC_HEADER latch_darwin.h
    )
else()
    set_target_properties(latch
    PROPERTIES
        PUBLIC_HEADER latch.h
    )
endif()

target_include_directories(latch
PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    if(WIN32)
        # clang-cl is not considered for now ...
    else()
        target_compile_options(latch
        PUBLIC
            -stdlib=libc++
        )
        target_link_libraries(latch
        PUBLIC
            c++
        )
    endif()
elseif(${CMAKE_CXX_COMPILER_ID} MATCHES MSVC)
    target_compile_options(latch
    PUBLIC
        /W4
    )
endif()

if(WIN32)
    set_target_properties(latch
    PROPERTIES
        WINDOWS_EXPORT_ALL_SYMBOLS True
    )

    target_link_libraries(latch
    PUBLIC
        kernel32 synchronization
    )
elseif(APPLE)
    message(STATUS "using sysroot: ${CMAKE_OSX_SYSROOT}")
    # target_include_directories(latch
    # PRIVATE
    #     ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/Kernel.framework/Headers
    # )
    # target_link_directories(latch
    # PRIVATE
    #     ${CMAKE_OSX_SYSROOT}/System/Library/Frameworks/Kernel.framework
    # )
elseif(LINUX)
    target_link_libraries(latch
    PUBLIC
        stdc++
    )
endif()

# the header is always installed
install(FILES   latch.h
        # TYPE        INCLUDE
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)
install(TARGETS         latch
        EXPORT          ${PROJECT_NAME}-config
        RUNTIME  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        PUBLIC_HEADER 
                 DESTINATION ${CMAKE_INSTALL_PREFIX}/include
)

install(EXPORT      ${PROJECT_NAME}-config
        DESTINATION ${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}
)

include(CMakePackageConfigHelpers)
set(VERSION_FILE_PATH   ${CMAKE_BINARY_DIR}/cmake/${PROJECT_NAME}-config-version.cmake)
write_basic_package_version_file(${VERSION_FILE_PATH}
    VERSION             ${PROJECT_VERSION}
    COMPATIBILITY       SameMajorVersion
)

install(FILES           ${VERSION_FILE_PATH} 
        DESTINATION     ${CMAKE_INSTALL_PREFIX}/share/${PROJECT_NAME}
)

enable_testing()
find_package(Threads REQUIRED)

add_executable(latch_test
    latch_test.cpp
)

set_target_properties(latch_test
PROPERTIES
    CXX_STANDARD 20
)

target_link_libraries(latch_test
PRIVATE
    Threads::Threads latch
)

if(EXISTS ${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    include(${CMAKE_BINARY_DIR}/conanbuildinfo.cmake)
    conan_basic_setup()
    message(STATUS "  include\t: ${CONAN_INCLUDE_DIRS}")
    message(STATUS "  dir    \t: ${CONAN_LIB_DIRS}")
    message(STATUS "  libs   \t: ${CONAN_LIBS}")
    message(STATUS "  defines\t: ${CONAN_DEFINES}")

    target_include_directories(latch_test
    PRIVATE
        ${CONAN_INCLUDE_DIRS}
    )
    target_link_directories(latch_test
    PRIVATE
        ${CONAN_LIB_DIRS}
    )
    target_link_libraries(latch_test
    PRIVATE
        ${CONAN_LIBS}
    )
    target_compile_options(latch_test
    PRIVATE
        ${CONAN_DEFINES}
    )
else()
    # use 'external/catch2.zip' if not found
    find_package(Catch2 2.10 CONFIG REQUIRED
        PATHS "${PROJECT_SOURCE_DIR}/external/share/Catch2"
    )
    get_target_property(Catch2_INCLUDE_DIR Catch2::Catch2
        INTERFACE_INCLUDE_DIRECTORIES
    )
    message(STATUS "using Catch2: ${Catch2_INCLUDE_DIR}")
    target_link_libraries(latch_test
    PRIVATE
        Catch2::Catch2
    )
endif()

if(${CMAKE_SYSTEM_NAME} MATCHES Linux)
    target_link_libraries(latch_test
    PRIVATE
        m
    )
endif()

add_test(NAME test_ctor_z   COMMAND latch_test "latch ctor zero")
add_test(NAME test_ctor_n   COMMAND latch_test "latch ctor negative")
add_test(NAME test_ctor_p   COMMAND latch_test "latch ctor positive")
add_test(NAME test_wait     COMMAND latch_test "latch wait")
add_test(NAME test_try_wait COMMAND latch_test "latch try_wait")
add_test(NAME test_awake    COMMAND latch_test "latch awake")
