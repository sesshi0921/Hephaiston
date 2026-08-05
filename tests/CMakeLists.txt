add_executable(geo_module_tests GeoModuleTests.cpp)
target_compile_features(geo_module_tests PRIVATE cxx_std_20)
target_link_libraries(geo_module_tests PRIVATE hephaiston_geometry hephaiston_geospatial hephaiston_planar_map)
add_test(NAME geo_module_tests COMMAND geo_module_tests)

add_executable(editor_logger_tests
    EditorLoggerTests.cpp
    ${CMAKE_SOURCE_DIR}/src/EditorLogger.cpp
)
target_compile_features(editor_logger_tests PRIVATE cxx_std_20)
target_include_directories(editor_logger_tests PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(editor_logger_tests PRIVATE cpp_logger::cpp_logger)
target_compile_definitions(editor_logger_tests PRIVATE HEPHAISTON_LOG_DIRECTORY="${CMAKE_BINARY_DIR}/logs")
add_test(NAME editor_logger_tests COMMAND editor_logger_tests)
