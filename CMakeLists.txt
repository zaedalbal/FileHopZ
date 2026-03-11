cmake_minimum_required(VERSION 3.25)
project(filehopz)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Boost REQUIRED COMPONENTS filesystem system thread)
find_package(OpenSSL REQUIRED)

# CONFIGURE_DEPENDS нужен чтобы cmake видел новые .cpp которые добавляются в проект
file(GLOB_RECURSE PROJECT_SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)

add_executable(filehopz ${PROJECT_SOURCES})

target_sources(filehopz PRIVATE ${PROJECT_SOURCES})

target_include_directories(filehopz PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

target_link_libraries(
    filehopz PRIVATE
    Boost::filesystem
    Boost::system
    Boost::thread
    OpenSSL::Crypto
    OpenSSL::SSL
)
