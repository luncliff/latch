cmake_minimum_required(VERSION 3.12)
project(latch VERSION 1.0 LANGUAGES CXX)

add_library(latch
    latch.hpp
    latch_windows.cpp
)

set_target_properties(latch
PROPERTIES
    CXX_STANDARD 20
    PUBLIC_HEADER latch.hpp
)

target_include_directories(latch
PUBLIC
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}>
)

if(${CMAKE_CXX_COMPILER_ID} MATCHES Clang)
    if(WIN32)
        # clang-cl is not considered for now ...
    else()
        target_compile_options(latch
        PUBLIC
            -stdlib=libc++
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
        kernel32
    )
endif()


install(FILES   latch.hpp
        DESTINATION ${CMAKE_INSTALL_PREFIX}/include/${PROJECT_NAME}
)

install(TARGETS         latch
        EXPORT          ${PROJECT_NAME}-config
        RUNTIME  DESTINATION ${CMAKE_INSTALL_PREFIX}/bin
        LIBRARY  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        ARCHIVE  DESTINATION ${CMAKE_INSTALL_PREFIX}/lib
        INCLUDES DESTINATION ${CMAKE_INSTALL_PREFIX}/include
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
find_package(Catch2 CONFIG REQUIRED)

add_executable(latch_test
    latch_test.cpp
)
target_link_libraries(latch_test
PRIVATE
    Catch2::Catch2 latch
)
add_test(NAME test_1    COMMAND latch_test)

