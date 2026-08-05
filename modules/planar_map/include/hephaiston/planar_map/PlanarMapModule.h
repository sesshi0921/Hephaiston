#pragma once

#include "hephaiston/geospatial/GeoTypes.h"

#include <optional>
#include <condition_variable>
#include <deque>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

namespace hephaiston::planar_map {

struct TileSourceDescriptor { std::string id; std::string displayName; std::string urlTemplate; int minZoom=0; int maxZoom=18; std::string attribution; bool enabledByDefault=false; };
struct PlanarMapCamera { geospatial::GeoCoordinate center {139.767125,35.681236,{}}; double zoomLevel=9.0; };
struct PlanarMapViewport { double widthPixels=1.0; double heightPixels=1.0; };
struct MapLine { float x1,y1,z1,x2,y2,z2,r,g,b,a; };
struct MapTile {
    int x = 0;
    int y = 0;
    int zoom = 0;
    double minX = 0.0;
    double minY = 0.0;
    double maxX = 0.0;
    double maxY = 0.0;
    // Texture region. Parent-tile fallback uses a cropped area while a full
    // resolution tile keeps the default [0, 1] range.
    float uLeft = 0.0f;
    float vTop = 0.0f;
    float uRight = 1.0f;
    float vBottom = 1.0f;
    std::filesystem::path imagePath;
};

class PlanarMapModule {
public:
    PlanarMapModule();
    ~PlanarMapModule();
    PlanarMapModule(const PlanarMapModule&) = delete;
    PlanarMapModule& operator=(const PlanarMapModule&) = delete;
    [[nodiscard]] const std::vector<TileSourceDescriptor>& tileSources() const;
    [[nodiscard]] double metersPerPixel(const PlanarMapCamera& camera) const;
    [[nodiscard]] std::optional<geospatial::GeoCoordinate> screenToGeo(double x, double y, const PlanarMapViewport& viewport, const PlanarMapCamera& camera) const;
    [[nodiscard]] std::optional<std::pair<double,double>> geoToLocalMeters(const geospatial::GeoCoordinate& coordinate, const PlanarMapCamera& camera) const;
    [[nodiscard]] std::vector<MapLine> fallbackMapLines(const PlanarMapCamera& camera, const PlanarMapViewport& viewport) const;
    [[nodiscard]] std::vector<MapLine> polygonLines(const std::vector<geospatial::GeoCoordinate>& vertices, const PlanarMapCamera& camera, bool closed, bool selected) const;
    [[nodiscard]] std::string attribution(const std::string& sourceId) const;
    // Returns cached visible XYZ tiles and queues missing tiles for background
    // download. gsi_photo also prefetches the two lower and two higher zoom
    // levels around the current camera without blocking the UI thread.
    [[nodiscard]] std::vector<MapTile> visibleTiles(const PlanarMapCamera& camera, const PlanarMapViewport& viewport, const std::string& sourceId);
    [[nodiscard]] std::string lastTileError() const;

private:
    struct TileRequest {
        TileSourceDescriptor source;
        int zoom = 0;
        int x = 0;
        int y = 0;
        std::filesystem::path path;
        std::string key;
    };
    [[nodiscard]] std::filesystem::path cachePathFor(const TileSourceDescriptor& source, int zoom, int x, int y) const;
    void scheduleTile(const TileSourceDescriptor& source, int zoom, int x, int y, const std::filesystem::path& path, bool priority = false);
    void schedulePhotoPrefetch(const TileSourceDescriptor& source, const PlanarMapCamera& camera, int zoom);
    void tileWorker();
    bool downloadTile(const TileRequest& request);

    std::thread tileWorkerThread_;
    mutable std::mutex tileMutex_;
    std::condition_variable tileCondition_;
    std::deque<TileRequest> pendingTiles_;
    std::unordered_set<std::string> scheduledTileKeys_;
    std::unordered_set<std::string> failedTileKeys_;
    bool stopTileWorker_ = false;
    std::string lastTileError_;
};

} // namespace hephaiston::planar_map
