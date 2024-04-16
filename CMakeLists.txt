cmake_minimum_required(VERSION 3.14)

project(mqttBroker
    VERSION 1.0.1.0
    DESCRIPTION "mqtt broker"
    LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(${PROJECT_NAME})

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network WebSockets REQUIRED)

list(APPEND CMAKE_MODULE_PATH ${CMAKE_CURRENT_LIST_DIR})
include(build_options)
set_build_options("${CMAKE_CURRENT_LIST_DIR}")

add_compile_definitions(HIGH_PERFORMANCE NOSTACKTRACE _CRT_SECURE_NO_WARNINGS)

FILE(GLOB SOURCES *.cpp)
FILE(GLOB HEADERS *.h)

FILE(GLOB NWSOURCES network/*.cpp)
FILE(GLOB NWHEADERS network/*.h)
include_directories(${PROJECT_SOURCE_DIR}/network)

FILE(GLOB LOGSOURCES logger/*.cpp)
FILE(GLOB LOGHEADERS logger/*.h)
include_directories(${PROJECT_SOURCE_DIR}/logger)

FILE(GLOB AVGSOURCES average/*.cpp)
FILE(GLOB AVGHEADERS average/*.h)
include_directories(${PROJECT_SOURCE_DIR}/average)

list(FILTER SOURCES EXCLUDE REGEX ".*main\\.cpp$")
list(APPEND SOURCES main_broker.cpp)

target_sources(${PROJECT_NAME} PRIVATE ${SOURCES} ${HEADERS} ${AVGSOURCES} ${AVGHEADERS} ${NWSOURCES} ${NWHEADERS} ${LOGSOURCES} ${LOGHEADERS})

if (WIN32)
    target_sources(${PROJECT_NAME} PRIVATE resource.rc)
    target_link_libraries(${PROJECT_NAME} PRIVATE ws2_32)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::WebSockets
)
