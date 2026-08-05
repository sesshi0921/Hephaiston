#pragma once

#include "hephaiston/geospatial/GeoTypes.h"

#include <optional>
#include <filesystem>
#include <condition_variable>
#include <deque>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace hephaiston::earth {

struct ViewportRect { double width = 1.0; double height = 1.0; };
struct ScreenPoint { double x = 0.0; double y = 0.0; };
struct EarthCameraState {
    double yawDegrees = 140.0;
    double pitchDegrees = 35.0;
    double distanceEarthRadii = 3.0;
};
struct EarthLine { float x1, y1, z1, x2, y2, z2; float r, g, b, a; };
struct EarthSurfaceVertex { float x, y, z, u, v; };
struct EarthSurfaceMesh { std::vector<EarthSurfaceVertex> vertices; std::vector<unsigned int> indices; };
struct EarthDetailTile { std::string id; std::filesystem::path imagePath; EarthSurfaceMesh mesh; };

// The caller selects imagery; the Earth module only resolves it to low-level
// XYZ requests and never knows about plugin UI or map-layer names.
enum class EarthImageryLayer { Standard, Photo, Relief };

class EarthModule {
public:
    EarthModule();
    ~EarthModule();
    EarthModule(const EarthModule&) = delete;
    EarthModule& operator=(const EarthModule&) = delete;
    static constexpr double earthRadiusMeters = 6378137.0;
    [[nodiscard]] std::optional<geospatial::GeoCoordinate> pickEarthSurface(ScreenPoint screen, ViewportRect viewport, const EarthCameraState& camera) const;
    [[nodiscard]] std::vector<EarthLine> fallbackGlobeLines() const;
    [[nodiscard]] EarthSurfaceMesh texturedGlobeMesh(int longitudeSegments = 128, int latitudeSegments = 64) const;
    [[nodiscard]] std::vector<EarthLine> polygonLines(const std::vector<geospatial::GeoCoordinate>& vertices, bool closed, bool selected) const;
    [[nodiscard]] double surfaceDistanceMeters(const EarthCameraState& camera) const;
    [[nodiscard]] geospatial::GeoCoordinate viewCenterCoordinate(const EarthCameraState& camera) const;
    [[nodiscard]] std::vector<EarthDetailTile> visibleDetailTiles(const EarthCameraState& camera, ViewportRect viewport, EarthImageryLayer imagery = EarthImageryLayer::Standard);

private:
    struct TileRequest { int zoom = 0; int x = 0; int y = 0; EarthImageryLayer imagery = EarthImageryLayer::Standard; std::filesystem::path path; std::string key; };
    void scheduleDetailTile(int zoom, int x, int y, EarthImageryLayer imagery, const std::filesystem::path& path);
    void tileWorker();
    bool downloadDetailTile(const TileRequest& request);
    std::thread tileWorkerThread_;
    std::mutex tileMutex_;
    std::condition_variable tileCondition_;
    std::deque<TileRequest> pendingTiles_;
    std::unordered_set<std::string> scheduledTileKeys_;
    std::unordered_set<std::string> failedDetailTileKeys_;
    bool stopTileWorker_ = false;
};

} // namespace hephaiston::earth
