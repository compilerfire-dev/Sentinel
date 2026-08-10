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
    SentinelShared
)

target_compile_options(SentinelStats PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(BUILD_TESTING)
    add_executable(SentinelStatsDataTest
        tests/StatisticsDataTest.cpp
        src/StatisticsData.cpp
    )
    target_include_directories(SentinelStatsDataTest PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/include
    )
    target_link_libraries(SentinelStatsDataTest PRIVATE SentinelShared)
    target_compile_options(SentinelStatsDataTest PRIVATE
        -Wall
        -Wextra
        -Wpedantic
    )
    add_test(NAME SentinelStats.DataAggregation COMMAND SentinelStatsDataTest)
endif()
