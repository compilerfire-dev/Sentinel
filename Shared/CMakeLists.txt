add_library(SentinelShared STATIC
    src/JsonDataStore.cpp
    src/NativeFileDialog.cpp
    src/SharedTaskMerge.cpp
    src/SharedTaskWorld.cpp
)

target_include_directories(SentinelShared PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(SentinelShared PUBLIC
    PkgConfig::GTK3
)

target_compile_options(SentinelShared PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

if(BUILD_TESTING)
    add_executable(SentinelSharedConcurrencyTest
        tests/JsonDataStoreConcurrencyTest.cpp
    )
    target_link_libraries(SentinelSharedConcurrencyTest PRIVATE SentinelShared)
    add_test(NAME SentinelShared.ConcurrentJsonTransactions
             COMMAND SentinelSharedConcurrencyTest)

    add_executable(SentinelSharedTaskWorldMigrationTest
        tests/SharedTaskWorldMigrationTest.cpp
    )
    target_link_libraries(SentinelSharedTaskWorldMigrationTest PRIVATE SentinelShared)
    add_test(NAME SentinelShared.SharedTaskWorldMigration
             COMMAND SentinelSharedTaskWorldMigrationTest)

    add_executable(SentinelSharedTaskMergeTest
        tests/SharedTaskMergeTest.cpp
    )
    target_link_libraries(SentinelSharedTaskMergeTest PRIVATE SentinelShared)
    add_test(NAME SentinelShared.SharedTaskMerge
             COMMAND SentinelSharedTaskMergeTest)
endif()
