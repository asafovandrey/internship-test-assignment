cmake_minimum_required(VERSION 3.16)

project(internship_test_assignment
    VERSION 1.0.0
    DESCRIPTION "Библиотека записи сообщений в журнал"
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(LIBLOG_BUILD_TESTS "Собирать юнит-тесты" ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_library(liblog_warnings INTERFACE)
target_compile_options(liblog_warnings INTERFACE -Wall -Wextra -Wpedantic)

# Требование 1: два варианта сборки, статический и динамический
set(LIBLOG_SOURCES
    liblog/logger.cpp
    liblog/file_worker.cpp
    liblog/uds_worker.cpp
    liblog/prepare_record.cpp)

function(liblog_setup target)
    # Корень репозитория — для #include <liblog/liblog.h> у потребителей,
    # каталог liblog/ — для #include "liblog.h" внутри самой библиотеки.
    target_include_directories(${target}
        PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
                $<INSTALL_INTERFACE:include>
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/liblog)

    target_compile_features(${target} PUBLIC cxx_std_17)
    target_link_libraries(${target} PRIVATE liblog_warnings)

    set_target_properties(${target} PROPERTIES
        OUTPUT_NAME log
        POSITION_INDEPENDENT_CODE ON)
endfunction()

add_library(liblog_static STATIC ${LIBLOG_SOURCES})
add_library(liblog::static ALIAS liblog_static)
liblog_setup(liblog_static)

add_library(liblog_shared SHARED ${LIBLOG_SOURCES})
add_library(liblog::shared ALIAS liblog_shared)
liblog_setup(liblog_shared)
set_target_properties(liblog_shared PROPERTIES
    VERSION   ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR})

# Тесты: каждый — отдельная программа, CTest судит по коду возврата.
# Сторонние библиотеки не используются.
if(LIBLOG_BUILD_TESTS)
    enable_testing()

    set(LIBLOG_TESTS
        test_logger
        test_file_worker
        test_uds_worker)

    foreach(test_name ${LIBLOG_TESTS})
        add_executable(${test_name} liblog/tests/${test_name}.cpp)
        target_link_libraries(${test_name} PRIVATE liblog::static liblog_warnings)
        target_include_directories(${test_name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/liblog/tests)
        add_test(NAME ${test_name} COMMAND ${test_name})
    endforeach()

endif()

include(GNUInstallDirs)
install(TARGETS liblog_static liblog_shared
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES liblog/liblog.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/liblog)
