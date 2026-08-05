# Optional plugin workspace.
#
# Put plugin subdirectories under plugins/ and add them here, for example:
#
#   add_subdirectory(site_editor)
#
# Plugin targets can use short API includes after linking hephaiston_plugin_api:
#
#   target_link_libraries(my_plugin PRIVATE hephaiston_plugin_api)
#
# Then plugin source can include:
#
#   #include "EditorRegistry.h"
#   #include "ViewportRenderSettings.h"

add_subdirectory(geo_polygon)
