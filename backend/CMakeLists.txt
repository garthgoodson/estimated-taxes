find_package(SQLite3 REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(JANSSON REQUIRED IMPORTED_TARGET jansson)

add_library(estimated_taxes_storage
  src/sqlite.cpp
  src/current_date_provider.cpp
  src/http_server.cpp
  src/api.cpp
  src/database_operations.cpp
  src/input_store.cpp
  src/projection.cpp
  src/tax_rules.cpp
  src/tax_calculation.cpp
  src/recommendations.cpp
  src/snapshot_store.cpp
)

target_compile_features(estimated_taxes_storage PUBLIC cxx_std_20)
target_include_directories(estimated_taxes_storage
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(estimated_taxes_storage PRIVATE SQLite::SQLite3 PkgConfig::JANSSON)
target_compile_options(estimated_taxes_storage
  PRIVATE
    $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>
    $<$<CXX_COMPILER_ID:MSVC>:/W4;/permissive->
)

add_executable(estimated_taxes_backend
  src/main.cpp
)
target_compile_features(estimated_taxes_backend PRIVATE cxx_std_20)
target_link_libraries(estimated_taxes_backend PRIVATE estimated_taxes_storage)

if(BUILD_TESTING)
  add_subdirectory(tests)
endif()
