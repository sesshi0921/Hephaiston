hephaiston_add_plugin(geo_polygon
    src/GeoPolygonPlugin.cpp
)
target_link_libraries(geo_polygon PRIVATE
    hephaiston_earth
    hephaiston_planar_map
    hephaiston_geometry
    hephaiston_geospatial
    hephaiston_native_dialog
)
target_include_directories(geo_polygon PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(geo_polygon PRIVATE HEPHAISTON_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
