cmake_minimum_required(VERSION 3.12.0)
enable_testing()
project(mqtt-sample C CXX)

find_program(CCACHE_FOUND ccache)
if(CCACHE_FOUND)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_COMPILE ccache)
    set_property(GLOBAL PROPERTY RULE_LAUNCH_LINK ccache)
endif(CCACHE_FOUND)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_EXE_LINKER_FLAGS "-static-libgcc -static-libstdc++")

find_package(OpenSSL REQUIRED)
# find_package(eclipse-paho-mqtt-c REQUIRED)

include_directories(include)

add_executable(mqtt-client-app
    src/main.cpp
    #src/main.c
    #src/pubsub_opts.c
)

# target_link_libraries(mqtt-client-app eclipse-paho-mqtt-c::paho-mqtt3cs-static m)
target_link_libraries(mqtt-client-app mosquitto_static m ssl crypto) 
