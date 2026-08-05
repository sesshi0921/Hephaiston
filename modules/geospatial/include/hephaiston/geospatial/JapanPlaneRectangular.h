#pragma once

#include "hephaiston/geospatial/GeoTypes.h"

#include <optional>
#include <string>
#include <vector>

namespace hephaiston::geospatial {

struct JapanPlaneRectangularZone {
    int zoneNumber = 0;
    int epsgCode = 0;
    std::string displayName;
    std::string areaDescription;
    double latitudeOfOriginDegrees = 0.0;
    double centralMeridianDegrees = 0.0;
};

[[nodiscard]] const std::vector<JapanPlaneRectangularZone>& japanPlaneRectangularZones();
[[nodiscard]] const JapanPlaneRectangularZone* findJapanPlaneRectangularZone(int zoneNumber);
[[nodiscard]] std::optional<ProjectedCoordinate> projectToJapanPlane(const GeoCoordinate& coordinate, int zoneNumber);
[[nodiscard]] std::optional<GeoCoordinate> unprojectFromJapanPlane(const ProjectedCoordinate& coordinate, int zoneNumber);
[[nodiscard]] int suggestedJapanPlaneZone(const GeoCoordinate& coordinate);

} // namespace hephaiston::geospatial
