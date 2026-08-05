#pragma once

#include "hephaiston/geospatial/GeoTypes.h"

#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace hephaiston::atlas_polys {
enum class ExportCoordinateMode { Geographic, JapanPlaneRectangular };
struct ExportRequest { std::filesystem::path folder; ExportCoordinateMode coordinateMode=ExportCoordinateMode::Geographic; int japanPlaneZone=9; bool kml=true; bool csv=true; const std::vector<geospatial::GeoPolygon>* polygons=nullptr; };
struct ExportResult { bool success=false; std::vector<std::filesystem::path> files; std::string message; };
class IAtlasPolysExporter { public: virtual ~IAtlasPolysExporter()=default; virtual std::string_view id() const=0; virtual std::string_view displayName() const=0; virtual ExportResult exportPolygons(const ExportRequest& request)=0; };
} // namespace hephaiston::atlas_polys
