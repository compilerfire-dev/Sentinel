add_library(SentinelShared STATIC
    src/JsonDataStore.cpp
    src/NativeFileDialog.cpp
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
