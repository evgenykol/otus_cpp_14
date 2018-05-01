cmake_minimum_required(VERSION 3.2)

if(NOT DEFINED ENV{TRAVIS_BUILD_NUMBER})
    message("TRAVIS_BUILD_NUMBER environment variable set to default")
    set(ENV{TRAVIS_BUILD_NUMBER} "1")
endif()

project(yamr VERSION 0.0.$ENV{TRAVIS_BUILD_NUMBER})

# libboost-system-dev libboost-doc
find_package(Boost COMPONENTS system REQUIRED)
find_package(Threads)

configure_file(version.h.in version.h)

add_executable(yamr main.cpp)

set_target_properties(yamr
        PROPERTIES
                CXX_STANDARD 14
                CXX_STANDARD_REQUIRED ON
                CMAKE_CXX_EXTENSIONS OFF
                COMPILE_OPTIONS -Wpedantic -Wall -Wextra
)

target_link_libraries(yamr
    ${Boost_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

install(TARGETS yamr RUNTIME DESTINATION bin)

set(CPACK_GENERATOR DEB)

set(CPACK_PACKAGE_VERSION_MAJOR "${PROJECT_VERSION_MAJOR}")
set(CPACK_PACKAGE_VERSION_MINOR "${PROJECT_VERSION_MINOR}")
set(CPACK_PACKAGE_VERSION_PATCH "${PROJECT_VERSION_PATCH}")

set(CPACK_PACKAGE_CONTACT edkolotilov@gmail.com)

include (CPack)
