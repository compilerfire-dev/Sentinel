find_package(PkgConfig REQUIRED)
pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)

add_executable(SentinelStats
    main.cpp
    src/Application.cpp
    src/StatisticsData.cpp
)

target_include_directories(SentinelStats PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(SentinelStats PRIVATE
    PkgConfig::GTK3
)

target_compile_options(SentinelStats PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)
