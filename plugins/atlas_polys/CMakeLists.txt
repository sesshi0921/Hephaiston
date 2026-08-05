hephaiston_add_plugin(atlas_polys
    src/AtlasPolysPlugin.cpp
)
target_link_libraries(atlas_polys PRIVATE
    hephaiston_earth
    hephaiston_planar_map
    hephaiston_geometry
    hephaiston_geospatial
    hephaiston_native_dialog
)
target_include_directories(atlas_polys PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)
target_compile_definitions(atlas_polys PRIVATE HEPHAISTON_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
