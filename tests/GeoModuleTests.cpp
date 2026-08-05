#include "hephaiston/geometry/PolygonValidation.h"
#include "hephaiston/geospatial/JapanPlaneRectangular.h"
#include "hephaiston/planar_map/PlanarMapModule.h"

#include <cmath>
#include <cstdlib>
#include <iostream>

using namespace hephaiston;
using namespace hephaiston::geometry;
using namespace hephaiston::geospatial;

namespace {
void require(bool condition, const char* message) { if (!condition) { std::cerr << "FAILED: " << message << '\n'; std::exit(1); } }
}

int main() {
    const std::vector<Point2d> triangle {{0,0},{10,0},{0,10}};
    require(validateClosedRing(triangle).valid(), "valid triangle");
    const std::vector<Point2d> square {{0,0},{10,0},{10,10},{0,10}};
    require(validateClosedRing(square).valid(), "valid quadrilateral");
    std::vector<GeoCoordinate> clockwise {{0,0,{}},{0,1,{}},{1,0,{}}};
    normalizeCounterClockwise(clockwise, {{0,0},{0,1},{1,0}});
    require(clockwise[0].longitudeDegrees == 1.0, "clockwise coordinates are reversed");
    require(!validateOpenRingCandidate({{0,0},{10,10},{0,10}}, {10,0}).valid(), "self crossing insertion rejected");
    require(!validateClosedRing({{0,0},{10,10},{0,10},{10,0}}).valid(), "bow tie rejected");
    require(!validateOpenRingCandidate({{0,0}}, {0.01,0.01}).valid(), "short or duplicate edge rejected");
    require(!validateClosedRing({{0,0},{1,0}}).valid(), "two vertices rejected");
    require(!validateClosedRing({{0,0},{10,0},{20,0}}).valid(), "zero area rejected");

    const GeoCoordinate tokyo {139.767125,35.681236,{}};
    const auto projected = projectToJapanPlane(tokyo, 9);
    require(projected.has_value(), "Tokyo projects to zone IX");
    const auto restored = unprojectFromJapanPlane(*projected, 9);
    require(restored.has_value() && std::abs(restored->longitudeDegrees-tokyo.longitudeDegrees)<1e-7 && std::abs(restored->latitudeDegrees-tokyo.latitudeDegrees)<1e-7, "Japan plane inverse projection round trip");
    const auto& zones = japanPlaneRectangularZones();
    require(zones.size()==19 && zones.front().epsgCode==6669 && zones.back().epsgCode==6687, "all JGD2011 I-XIX EPSG zones");

    planar_map::PlanarMapModule map;
    planar_map::PlanarMapCamera camera; camera.center=tokyo; camera.zoomLevel=13;
    const auto picked=map.screenToGeo(400,300,{800,600},camera);
    require(picked.has_value() && std::abs(picked->longitudeDegrees-tokyo.longitudeDegrees)<1e-8, "planar center picking");
    require(!map.tileSources().empty() && !map.attribution("gsi_standard").empty(), "tile source metadata and attribution");
    std::cout << "geo module tests passed\n";
}
