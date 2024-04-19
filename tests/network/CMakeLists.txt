cmake_minimum_required(VERSION 3.14)

project(testNetworkServer LANGUAGES CXX)

set(CMAKE_INCLUDE_CURRENT_DIR ON)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Core Network WebSockets Test REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Core Network WebSockets Test REQUIRED)

include_directories(${PROJECT_SOURCE_DIR}/../..)
include_directories(${PROJECT_SOURCE_DIR}/../../logger)
include_directories(${PROJECT_SOURCE_DIR}/../../network)

qt_standard_project_setup()

qt_add_executable(${PROJECT_NAME}
  test_network.cpp
  ../../average/move.h
  ../../average/move.cpp
  ../../logger/logger.h
  ../../logger/logger.cpp
  ../../network/network_tcp_server.h
  ../../network/network_tcp_server.cpp
  ../../network/network_server.h
  ../../network/network_server.cpp
  ../../network/network_secure_mode.h
  ../../network/network_secure_mode.cpp
  ../../network/network_event.h
  ../../network/network_event.cpp
  ../../network/network_connection_type.h
  ../../network/network_connection_type.cpp
  ../../network/network_client.h
  ../../network/network_client.cpp
)

set_target_properties(${PROJECT_NAME} PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Network
    Qt${QT_VERSION_MAJOR}::WebSockets
    Qt${QT_VERSION_MAJOR}::Test
)

install(TARGETS ${PROJECT_NAME}
    BUNDLE  DESTINATION .
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

qt_generate_deploy_app_script(
    TARGET ${PROJECT_NAME}
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

install(SCRIPT ${deploy_script})
