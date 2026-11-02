# =============================================================================
# СЭДМ — программная система экспертной дегустации мёда
# Корневой файл сборки (CMake). Курсовая работа по дисциплине ТРПС,
# МГТУ им. Н.Э. Баумана, кафедра ИУ6, гр. ИУ6-73Б, 2026.
#
# Состав проекта:
#   3rdparty/      — сторонние библиотеки (QXlsx, crypt_blowfish);
#   src/core/      — статическая библиотека sedm_core: предметная область,
#                    доступ к данным, прикладные сервисы, импорт/экспорт;
#   src/widgets/   — статическая библиотека sedm_widgets: общие элементы
#                    интерфейса (тема, ползунок-сота, справка, логотип);
#   src/admin/     — программа администратора sedm_admin.exe;
#   src/taster/    — программа дегустатора sedm_taster.exe;
#   tools/demo/    — утилита демонстрационных данных sedm_demo;
#   tests/         — модульные и интеграционные тесты (Qt Test).
# =============================================================================
cmake_minimum_required(VERSION 3.16)

project(SEDM
    VERSION 1.0.0
    DESCRIPTION "Программная система экспертной дегустации мёда"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Тип сборки" FORCE)
endif()

find_package(Qt5 5.15 REQUIRED COMPONENTS Core Gui Widgets Sql Svg PrintSupport Test)

# Единый каталог для исполняемых файлов (удобно для запуска и развёртывания)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(SEDM_BUILD_TESTS "Собирать тесты" ON)

# Строгие предупреждения для собственного кода (сторонний код — без них)
set(SEDM_WARNINGS -Wall -Wextra -Wpedantic)

add_subdirectory(3rdparty)
add_subdirectory(src/core)
# Подсистемы интерфейса подключаются по мере их появления в проекте
foreach(_part src/widgets src/admin src/taster)
    if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${_part}/CMakeLists.txt)
        add_subdirectory(${_part})
    endif()
endforeach()

# Вспомогательная утилита демонстрационных данных (показ программы, испытания)
option(SEDM_BUILD_TOOLS "Собирать утилиту демонстрационных данных sedm_demo" ON)
if(SEDM_BUILD_TOOLS)
    add_subdirectory(tools/demo)
endif()

if(SEDM_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
