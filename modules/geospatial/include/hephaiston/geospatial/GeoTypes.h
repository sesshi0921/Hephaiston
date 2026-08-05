#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace hephaiston::geospatial {

struct GeoCoordinate {
    double longitudeDegrees = 0.0;
    double latitudeDegrees = 0.0;
    std::optional<double> altitudeMeters;
};

using GeoPolygonId = std::uint64_t;

struct GeoPolygon {
    GeoPolygonId id = 0;
    std::string name;
    std::vector<GeoCoordinate> vertices;
    bool visible = true;
};

struct ProjectedCoordinate {
    // Explicit GIS axis names. Do not treat these as ambiguous X/Y values.
    double northingMeters = 0.0;
    double eastingMeters = 0.0;
};

[[nodiscard]] bool isValid(const GeoCoordinate& coordinate);

} // namespace hephaiston::geospatial
