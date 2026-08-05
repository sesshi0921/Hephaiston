# Runtime assets deliberately live outside the source tree.  This keeps large,
# redistributable imagery out of Git while still making a normal CMake configure
# sufficient for a usable Atlas Polys installation.

option(HEPHAISTON_DOWNLOAD_EARTH_TEXTURE
       "Download the NASA Blue Marble fallback texture during CMake configure"
       ON)

set(HEPHAISTON_EARTH_TEXTURE_URL
    "https://svs.gsfc.nasa.gov/vis/a000000/a002900/a002915/bluemarble-2048.png"
    CACHE STRING "NASA Blue Marble fallback texture URL")
set(HEPHAISTON_EARTH_TEXTURE_SHA256
    "ae6214b078ed0864c96f74bcb10ae3021f6eb116f8059797efe0fa9ea8b89d35"
    CACHE STRING "Expected SHA-256 of the downloaded NASA Blue Marble texture")
set(HEPHAISTON_EARTH_TEXTURE_PATH
    "${CMAKE_BINARY_DIR}/assets/nasa_bluemarble_2048.png"
    CACHE FILEPATH "Path to the Atlas Polys Earth fallback texture")

function(hephaiston_prepare_runtime_assets)
    if(EXISTS "${HEPHAISTON_EARTH_TEXTURE_PATH}")
        message(STATUS "Earth fallback texture: ${HEPHAISTON_EARTH_TEXTURE_PATH}")
        return()
    endif()

    if(NOT HEPHAISTON_DOWNLOAD_EARTH_TEXTURE)
        message(STATUS "Earth fallback texture download disabled; Atlas Polys will use its grid fallback.")
        return()
    endif()

    get_filename_component(_earth_texture_directory "${HEPHAISTON_EARTH_TEXTURE_PATH}" DIRECTORY)
    file(MAKE_DIRECTORY "${_earth_texture_directory}")
    set(_temporary_path "${HEPHAISTON_EARTH_TEXTURE_PATH}.download")

    file(DOWNLOAD "${HEPHAISTON_EARTH_TEXTURE_URL}" "${_temporary_path}"
         STATUS _download_status
         LOG _download_log
         EXPECTED_HASH "SHA256=${HEPHAISTON_EARTH_TEXTURE_SHA256}"
         TLS_VERIFY ON
         INACTIVITY_TIMEOUT 15
         TIMEOUT 90
         SHOW_PROGRESS)
    list(GET _download_status 0 _download_code)
    list(GET _download_status 1 _download_message)
    if(_download_code EQUAL 0)
        file(RENAME "${_temporary_path}" "${HEPHAISTON_EARTH_TEXTURE_PATH}")
        message(STATUS "Downloaded Earth fallback texture: ${HEPHAISTON_EARTH_TEXTURE_PATH}")
    else()
        file(REMOVE "${_temporary_path}")
        message(WARNING
            "Could not download the NASA Blue Marble fallback texture (${_download_message}). "
            "Atlas Polys remains usable with its grid fallback.  Retry configure while online, "
            "or provide -DHEPHAISTON_EARTH_TEXTURE_PATH=/absolute/path/to/bluemarble.png.")
    endif()
endfunction()
