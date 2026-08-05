#pragma once

#include "hephaiston/geospatial/GeoTypes.h"

#include <string>
#include <vector>

namespace hephaiston::geometry {

struct Point2d { double x = 0.0; double y = 0.0; };

enum class PolygonValidationError {
    None,
    TooFewVertices,
    InvalidCoordinate,
    DuplicateVertex,
    EdgeTooShort,
    SelfIntersection,
    ZeroArea,
};

struct PolygonValidationResult {
    PolygonValidationError error = PolygonValidationError::None;
    std::string message;
    [[nodiscard]] bool valid() const { return error == PolygonValidationError::None; }
};

[[nodiscard]] bool segmentsIntersect(Point2d a, Point2d b, Point2d c, Point2d d, double epsilon = 1e-9);
[[nodiscard]] double signedArea(const std::vector<Point2d>& points);
[[nodiscard]] bool isCounterClockwise(const std::vector<Point2d>& points);
void normalizeCounterClockwise(std::vector<hephaiston::geospatial::GeoCoordinate>& coordinates, const std::vector<Point2d>& projected);
[[nodiscard]] PolygonValidationResult validateOpenRingCandidate(const std::vector<Point2d>& points, Point2d candidate, double duplicateToleranceMeters = 0.05, double minimumEdgeMeters = 0.10);
[[nodiscard]] PolygonValidationResult validateClosedRing(const std::vector<Point2d>& points, double minimumAreaSquareMeters = 0.01, double minimumEdgeMeters = 0.10);

} // namespace hephaiston::geometry
