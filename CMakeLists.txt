cmake_minimum_required(VERSION 3.10)
project(discord_cpp_bot)

set(CMAKE_CXX_STANDARD 17)

find_package(DPP REQUIRED)
find_package(mongocxx REQUIRED)
find_package(bsoncxx REQUIRED)

include_directories(${DPP_INCLUDE_DIRS} ${MONGOCXX_INCLUDE_DIRS} ${BSONCXX_INCLUDE_DIRS} include)

add_executable(discord_cpp_bot src/main.cpp)
 
target_link_libraries(discord_cpp_bot
    dpp
    mongocxx
    bsoncxx
)
