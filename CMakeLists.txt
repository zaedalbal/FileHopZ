cmake_minimum_required(VERSION 3.25)
project(filehopz VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

find_package(Boost 1.82 REQUIRED COMPONENTS filesystem system thread)
find_package(OpenSSL 3.0 REQUIRED)
find_package(spdlog 1.10 REQUIRED)
find_package(CLI11 2.3 CONFIG REQUIRED)

# CONFIGURE_DEPENDS нужен чтобы cmake видел новые .cpp которые добавляются в проект
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(filehopz ${PROJECT_SOURCES})

target_compile_options(filehopz PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>: -O0 -g>
    $<$<CONFIG:Release>: -O2>
)

target_compile_definitions(filehopz PRIVATE
    $<$<CONFIG:Debug>:SPDLOG_ACTIVE_LEVEL=SPDLOG_LEVEL_TRACE>
)

target_include_directories(filehopz PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
    filehopz PRIVATE
    Boost::filesystem
    Boost::system
    Boost::thread
    OpenSSL::Crypto
    OpenSSL::SSL
    spdlog::spdlog
    CLI11::CLI11
)
