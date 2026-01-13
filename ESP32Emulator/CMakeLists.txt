cmake_minimum_required(VERSION 3.16)

project(ESP32Emulator VERSION 0.1 LANGUAGES CXX)

# --- Автоматическая генерация MOC, UIC, RCC ---
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Найти Qt ---
find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Svg)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Svg)

# --- Исходники проекта ---
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    workerobject.h
    workerobject.cpp
    peripheralcomponent.h
    emulatorcore.cpp
    emulatorcore.h
)

# --- Создаем исполняемый файл ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(ESP32Emulator
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    add_executable(ESP32Emulator
        ${PROJECT_SOURCES}
    )
endif()

# --- Линковка Qt ---
if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    target_link_libraries(ESP32Emulator PRIVATE Qt6::Widgets Qt6::Svg)
else()
    target_link_libraries(ESP32Emulator PRIVATE Qt5::Widgets Qt5::Svg)
endif()

find_package(Qt6 COMPONENTS Core Gui Xml REQUIRED)
target_link_libraries(ESP32Emulator PRIVATE Qt6::Core Qt6::Gui Qt6::Xml)

# --- Настройки для Windows/macOS ---
set_target_properties(ESP32Emulator PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

# --- Копирование fritzing-parts в каталог сборки ---
set(FRITZING_PARTS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/fritzing-parts")
add_custom_target(copy_fritzing_parts ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${FRITZING_PARTS_DIR}"
            "$<TARGET_FILE_DIR:ESP32Emulator>/fritzing-parts"
)
add_dependencies(ESP32Emulator copy_fritzing_parts)

# --- Установка (optional) ---
include(GNUInstallDirs)
install(TARGETS ESP32Emulator
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
