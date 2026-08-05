#include "hephaiston/geometry/PolygonValidation.h"

#include <algorithm>
#include <cmath>

namespace hephaiston::geometry {
namespace {
double cross(Point2d a, Point2d b, Point2d c) { return (b.x - a.x) * (c.y - a.y) - (b.y - a.y) * (c.x - a.x); }
double distanceSquared(Point2d a, Point2d b) { const double dx = a.x - b.x; const double dy = a.y - b.y; return dx * dx + dy * dy; }
bool onSegment(Point2d a, Point2d b, Point2d p, double epsilon) {
    return std::abs(cross(a, b, p)) <= epsilon && p.x >= std::min(a.x, b.x) - epsilon && p.x <= std::max(a.x, b.x) + epsilon && p.y >= std::min(a.y, b.y) - epsilon && p.y <= std::max(a.y, b.y) + epsilon;
}
PolygonValidationResult error(PolygonValidationError code, const char* message) { return {code, message}; }
} // namespace

bool segmentsIntersect(Point2d a, Point2d b, Point2d c, Point2d d, double epsilon) {
    const double abC = cross(a, b, c), abD = cross(a, b, d), cdA = cross(c, d, a), cdB = cross(c, d, b);
    if (((abC > epsilon && abD < -epsilon) || (abC < -epsilon && abD > epsilon)) &&
        ((cdA > epsilon && cdB < -epsilon) || (cdA < -epsilon && cdB > epsilon))) return true;
    return onSegment(a, b, c, epsilon) || onSegment(a, b, d, epsilon) || onSegment(c, d, a, epsilon) || onSegment(c, d, b, epsilon);
}

double signedArea(const std::vector<Point2d>& points) {
    if (points.size() < 3) return 0.0;
    double area = 0.0;
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point2d& a = points[i]; const Point2d& b = points[(i + 1) % points.size()];
        area += a.x * b.y - b.x * a.y;
    }
    return area * 0.5;
}

bool isCounterClockwise(const std::vector<Point2d>& points) { return signedArea(points) > 0.0; }

void normalizeCounterClockwise(std::vector<hephaiston::geospatial::GeoCoordinate>& coordinates, const std::vector<Point2d>& projected) {
    if (coordinates.size() == projected.size() && !isCounterClockwise(projected)) std::reverse(coordinates.begin(), coordinates.end());
}

PolygonValidationResult validateOpenRingCandidate(const std::vector<Point2d>& points, Point2d candidate, double duplicateToleranceMeters, double minimumEdgeMeters) {
    if (!std::isfinite(candidate.x) || !std::isfinite(candidate.y)) return error(PolygonValidationError::InvalidCoordinate, "Picked coordinate is invalid.");
    const double duplicateTolerance2 = duplicateToleranceMeters * duplicateToleranceMeters;
    for (const auto& point : points) if (distanceSquared(point, candidate) <= duplicateTolerance2) return error(PolygonValidationError::DuplicateVertex, "Point rejected: it duplicates an existing vertex.");
    if (!points.empty() && distanceSquared(points.back(), candidate) < minimumEdgeMeters * minimumEdgeMeters) return error(PolygonValidationError::EdgeTooShort, "Point rejected: the new edge is too short.");
    if (points.size() >= 3) {
        const Point2d a = points.back();
        for (std::size_t i = 0; i + 1 < points.size() - 1; ++i) {
            if (segmentsIntersect(a, candidate, points[i], points[i + 1])) return error(PolygonValidationError::SelfIntersection, "Point rejected: the new edge intersects an existing edge.");
        }
    }
    return {};
}

PolygonValidationResult validateClosedRing(const std::vector<Point2d>& points, double minimumAreaSquareMeters, double minimumEdgeMeters) {
    if (points.size() < 3) return error(PolygonValidationError::TooFewVertices, "A polygon requires at least three vertices.");
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point2d& a = points[i]; const Point2d& b = points[(i + 1) % points.size()];
        if (distanceSquared(a, b) < minimumEdgeMeters * minimumEdgeMeters) return error(PolygonValidationError::EdgeTooShort, "Polygon contains an extremely short edge.");
    }
    for (std::size_t i = 0; i < points.size(); ++i) {
        const Point2d a = points[i], b = points[(i + 1) % points.size()];
        for (std::size_t j = i + 1; j < points.size(); ++j) {
            const std::size_t ni = (i + 1) % points.size(), nj = (j + 1) % points.size();
            if (i == j || i == nj || ni == j) continue;
            if (segmentsIntersect(a, b, points[j], points[nj])) return error(PolygonValidationError::SelfIntersection, "Polygon closing edge intersects an existing edge.");
        }
    }
    if (std::abs(signedArea(points)) < minimumAreaSquareMeters) return error(PolygonValidationError::ZeroArea, "Polygon area is zero or too small.");
    return {};
}

} // namespace hephaiston::geometry
