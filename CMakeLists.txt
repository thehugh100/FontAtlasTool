cmake_minimum_required(VERSION 3.10)
project(fontAtlasTool)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PNG REQUIRED) 
find_package(BZIP2 REQUIRED)
find_package(ZLIB REQUIRED)

add_executable(
    fontAtlasTool
    main.cpp
    FontAtlas.cpp
)

target_link_directories(fontAtlasTool PUBLIC deps/freetype/build)
target_include_directories(fontAtlasTool PUBLIC include deps/json/include deps/freetype/include)

target_link_libraries(fontAtlasTool PUBLIC freetype ${PNG_LIBRARY} ${ZLIB_LIBRARY} ${BZIP2_LIBRARY})

target_compile_definitions(fontAtlasTool PUBLIC GL_SILENCE_DEPRECATION)