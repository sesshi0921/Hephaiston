#include "hephaiston/planar_map/PlanarMapModule.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <curl/curl.h>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <sstream>
#include <thread>
#include <unordered_set>

namespace hephaiston::planar_map {
namespace {
constexpr double kR=6378137.0,kPi=3.1415926535897932384626433832795,kDeg=kPi/180.0;
const std::vector<TileSourceDescriptor> kSources {
    {"gsi_relief","GSI Color Relief (no labels)","https://cyberjapandata.gsi.go.jp/xyz/relief/{z}/{x}/{y}.png",5,15,"© Geospatial Information Authority of Japan",true},
    {"gsi_standard","GSI Standard Map","https://cyberjapandata.gsi.go.jp/xyz/std/{z}/{x}/{y}.png",0,18,"© Geospatial Information Authority of Japan",true},
    {"gsi_photo","GSI Seamless Photo","https://cyberjapandata.gsi.go.jp/xyz/seamlessphoto/{z}/{x}/{y}.jpg",2,18,"© Geospatial Information Authority of Japan",false},
    {"offline","Offline fallback","",0,22,"Offline latitude/longitude grid",true},
};
double mercY(double lat) { const double clamped=std::clamp(lat,-85.05112878,85.05112878)*kDeg; return kR*std::log(std::tan(kPi/4.0+clamped/2.0)); }
double invMercY(double y) { return (2.0*std::atan(std::exp(y/kR))-kPi/2.0)/kDeg; }
MapLine line(double x1,double y1,double x2,double y2,float r,float g,float b,float a) {return {float(x1),float(y1),0,float(x2),float(y2),0,r,g,b,a};}
constexpr int kTileSize = 256;
double normalizedTileY(double latitudeDegrees) { const double lat=std::clamp(latitudeDegrees,-85.05112878,85.05112878)*kDeg; return (1.0-std::log(std::tan(lat)+1.0/std::cos(lat))/kPi)*0.5; }
double tileYToMercatorY(double tileY, int zoom) { const double n=std::pow(2.0,zoom); const double latitude=std::atan(std::sinh(kPi*(1.0-2.0*tileY/n))); return kR*std::log(std::tan(kPi/4.0+latitude/2.0)); }
std::string replaceToken(std::string value, const char* token, int number) { const std::string replacement=std::to_string(number); std::size_t offset=0; while((offset=value.find(token,offset))!=std::string::npos){value.replace(offset,std::strlen(token),replacement);offset+=replacement.size();}return value; }
size_t writeFile(void* data,size_t size,size_t count,void* user) { return std::fwrite(data,size,count,static_cast<FILE*>(user)); }
bool cachedTileExists(const std::filesystem::path& path) {
    std::error_code error;
    return std::filesystem::exists(path, error) && !error && std::filesystem::file_size(path, error) > 0 && !error;
}
} // namespace
const std::vector<TileSourceDescriptor>& PlanarMapModule::tileSources() const{return kSources;}
double PlanarMapModule::metersPerPixel(const PlanarMapCamera& c) const { return std::cos(c.center.latitudeDegrees*kDeg)*2.0*kPi*kR/(256.0*std::pow(2.0,c.zoomLevel)); }
std::optional<geospatial::GeoCoordinate> PlanarMapModule::screenToGeo(double x,double y,const PlanarMapViewport& v,const PlanarMapCamera& c) const {
 if(v.widthPixels<=0||v.heightPixels<=0) return std::nullopt; const double mpp=metersPerPixel(c); const double mx=c.center.longitudeDegrees*kDeg*kR+(x-v.widthPixels*0.5)*mpp; const double my=mercY(c.center.latitudeDegrees)-(y-v.heightPixels*0.5)*mpp; const double longitude=std::remainder(mx/kR/kDeg,360.0); geospatial::GeoCoordinate result{longitude,invMercY(my),{}}; return geospatial::isValid(result)?std::optional<geospatial::GeoCoordinate>(result):std::nullopt; }
std::optional<std::pair<double,double>> PlanarMapModule::geoToLocalMeters(const geospatial::GeoCoordinate& g,const PlanarMapCamera& c) const { if(!geospatial::isValid(g))return std::nullopt; const double deltaLongitude=std::remainder(g.longitudeDegrees-c.center.longitudeDegrees,360.0); return std::pair<double,double>{kR*deltaLongitude*kDeg,mercY(g.latitudeDegrees)-mercY(c.center.latitudeDegrees)}; }
std::vector<MapLine> PlanarMapModule::fallbackMapLines(const PlanarMapCamera& c,const PlanarMapViewport& v) const { std::vector<MapLine> out; const double mpp=metersPerPixel(c), halfW=v.widthPixels*mpp*.5,halfH=v.heightPixels*mpp*.5; const double extent=std::max(halfW,halfH), raw=extent/5.0,p=std::pow(10.0,std::floor(std::log10(std::max(1.0,raw)))),step=(raw/p<2?1:(raw/p<5?2:5))*p; for(double n=-std::ceil(halfW/step)*step;n<=halfW;n+=step)out.push_back(line(n,-halfH,n,halfH,.18f,.27f,.31f,1)); for(double n=-std::ceil(halfH/step)*step;n<=halfH;n+=step)out.push_back(line(-halfW,n,halfW,n,.18f,.27f,.31f,1)); out.push_back(line(-halfW,0,halfW,0,.34f,.48f,.57f,1));out.push_back(line(0,-halfH,0,halfH,.34f,.48f,.57f,1));return out; }
std::vector<MapLine> PlanarMapModule::polygonLines(const std::vector<geospatial::GeoCoordinate>& vs,const PlanarMapCamera& c,bool closed,bool selected) const {std::vector<MapLine> out;if(vs.size()<2)return out;const auto col=selected?std::array<float,4>{1,.72f,.18f,1}:std::array<float,4>{.25f,.85f,1,1};auto a=geoToLocalMeters(vs[0],c);for(std::size_t i=1;i<vs.size();++i){auto b=geoToLocalMeters(vs[i],c);if(a&&b)out.push_back(line(a->first,a->second,b->first,b->second,col[0],col[1],col[2],col[3]));a=b;}if(closed){auto first=geoToLocalMeters(vs.front(),c);auto last=geoToLocalMeters(vs.back(),c);if(first&&last)out.push_back(line(last->first,last->second,first->first,first->second,col[0],col[1],col[2],col[3]));}return out;}
std::string PlanarMapModule::attribution(const std::string& id) const{for(const auto&s:kSources)if(s.id==id)return s.attribution;return "Offline fallback";}

PlanarMapModule::PlanarMapModule() : tileWorkerThread_(&PlanarMapModule::tileWorker, this) {}

PlanarMapModule::~PlanarMapModule() {
    {
        std::lock_guard lock(tileMutex_);
        stopTileWorker_ = true;
    }
    tileCondition_.notify_one();
    if (tileWorkerThread_.joinable()) tileWorkerThread_.join();
}

std::string PlanarMapModule::lastTileError() const {
    std::lock_guard lock(tileMutex_);
    return lastTileError_;
}

std::filesystem::path PlanarMapModule::cachePathFor(const TileSourceDescriptor& source,int zoom,int x,int y) const {
    const auto extension=source.id=="gsi_photo"?".jpg":".png";
    return std::filesystem::current_path()/".hephaiston_tile_cache"/source.id/std::to_string(zoom)/std::to_string(x)/(std::to_string(y)+extension);
}

void PlanarMapModule::scheduleTile(const TileSourceDescriptor& source, int zoom, int x, int y, const std::filesystem::path& path, bool priority) {
    if (cachedTileExists(path)) return;
    const std::string key = source.id + ":" + std::to_string(zoom) + ":" + std::to_string(x) + ":" + std::to_string(y);
    {
        std::lock_guard lock(tileMutex_);
        if (failedTileKeys_.contains(key) || scheduledTileKeys_.contains(key)) return;
        scheduledTileKeys_.insert(key);
        if (priority) pendingTiles_.push_front({source, zoom, x, y, path, key});
        else pendingTiles_.push_back({source, zoom, x, y, path, key});
    }
    tileCondition_.notify_one();
}

void PlanarMapModule::schedulePhotoPrefetch(const TileSourceDescriptor& source, const PlanarMapCamera& camera, int zoom) {
    if (source.id != "gsi_photo") return;
    for (int delta : {-2, -1, 1, 2}) {
        const int prefetchZoom = std::clamp(zoom + delta, source.minZoom, source.maxZoom);
        if (prefetchZoom == zoom) continue;
        const int count = 1 << prefetchZoom;
        const int centerX = static_cast<int>(std::floor((camera.center.longitudeDegrees + 180.0) / 360.0 * count));
        const int centerY = static_cast<int>(std::floor(normalizedTileY(camera.center.latitudeDegrees) * count));
        for (int y = centerY - 1; y <= centerY + 1; ++y) {
            if (y < 0 || y >= count) continue;
            for (int rawX = centerX - 1; rawX <= centerX + 1; ++rawX) {
                const int x = ((rawX % count) + count) % count;
                scheduleTile(source, prefetchZoom, x, y, cachePathFor(source, prefetchZoom, x, y));
            }
        }
    }
}

void PlanarMapModule::tileWorker() {
    for (;;) {
        TileRequest request;
        {
            std::unique_lock lock(tileMutex_);
            tileCondition_.wait(lock, [this] { return stopTileWorker_ || !pendingTiles_.empty(); });
            if (stopTileWorker_ && pendingTiles_.empty()) return;
            request = std::move(pendingTiles_.front());
            pendingTiles_.pop_front();
        }
        const bool success = downloadTile(request);
        std::lock_guard lock(tileMutex_);
        scheduledTileKeys_.erase(request.key);
        if (!success) {
            failedTileKeys_.insert(request.key);
            lastTileError_ = "Tile download failed; using offline fallback.";
        }
    }
}

bool PlanarMapModule::downloadTile(const TileRequest& request) {
    if (cachedTileExists(request.path)) return true;
    std::error_code error;
    std::filesystem::create_directories(request.path.parent_path(), error);
    if (error) return false;
    const auto temporaryPath = request.path.string() + ".download";
    FILE* file = std::fopen(temporaryPath.c_str(), "wb");
    if (!file) return false;

    static std::once_flag curlInit;
    static CURLcode curlInitResult = CURLE_FAILED_INIT;
    std::call_once(curlInit, [] { curlInitResult = curl_global_init(CURL_GLOBAL_DEFAULT); });
    const std::string url = replaceToken(replaceToken(replaceToken(request.source.urlTemplate, "{z}", request.zoom), "{x}", request.x), "{y}", request.y);
    CURL* curl = curlInitResult == CURLE_OK ? curl_easy_init() : nullptr;
    if (!curl) {
        std::fclose(file);
        std::filesystem::remove(temporaryPath, error);
        return false;
    }
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeFile);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, file);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS, 1500L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, 4000L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Hephaiston/0.1 Planar Map");
    const CURLcode code = curl_easy_perform(curl);
    long http = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http);
    curl_easy_cleanup(curl);
    std::fclose(file);
    if (code != CURLE_OK || http < 200 || http >= 300 || !cachedTileExists(temporaryPath)) {
        std::filesystem::remove(temporaryPath, error);
        return false;
    }
    std::filesystem::rename(temporaryPath, request.path, error);
    if (error) {
        std::filesystem::remove(temporaryPath, error);
        return false;
    }
    return true;
}

std::vector<MapTile> PlanarMapModule::visibleTiles(const PlanarMapCamera& camera,const PlanarMapViewport& viewport,const std::string& sourceId) {
    const auto sourceIt=std::find_if(kSources.begin(),kSources.end(),[&](const auto&source){return source.id==sourceId;});
    if(sourceIt==kSources.end() || sourceIt->urlTemplate.empty()) return {};
    const int zoom=std::clamp(static_cast<int>(std::floor(camera.zoomLevel)),sourceIt->minZoom,sourceIt->maxZoom);
    const int count=1<<zoom;
    const double mpp=metersPerPixel(camera);
    const double centerMercX=camera.center.longitudeDegrees*kDeg*kR, centerMercY=mercY(camera.center.latitudeDegrees);
    const double halfMercX=viewport.widthPixels*mpp*.5, halfMercY=viewport.heightPixels*mpp*.5;
    const double worldWidth=2.0*kPi*kR;
    const auto mercatorToTileY = [count](double mercatorY) {
        return normalizedTileY(invMercY(mercatorY)) * count;
    };
    // X is intentionally unbounded. The mesh remains continuous across the
    // dateline, while only the remote XYZ request is wrapped below.
    const int minX=static_cast<int>(std::floor((centerMercX-halfMercX)/worldWidth*count+count*.5))-1;
    const int maxX=static_cast<int>(std::floor((centerMercX+halfMercX)/worldWidth*count+count*.5))+1;
    const int minY=std::max(0,static_cast<int>(std::floor(mercatorToTileY(centerMercY+halfMercY)))-1);
    const int maxY=std::min(count-1,static_cast<int>(std::floor(mercatorToTileY(centerMercY-halfMercY)))+1);
    std::vector<MapTile> out;
    for(int y=minY;y<=maxY;++y) for(int x=minX;x<=maxX;++x) {
        const int wrappedX=((x%count)+count)%count;
        const auto path=cachePathFor(*sourceIt,zoom,wrappedX,y);
        std::filesystem::path texturePath = path;
        float uLeft=0.0f, vTop=0.0f, uRight=1.0f, vBottom=1.0f;
        if (!cachedTileExists(path)) {
            // Keep the map continuous during refinement by drawing the best
            // cached parent tile, cropped to this child's geographic extent.
            bool foundParent = false;
            for (int levels=1; levels<=std::min(zoom-sourceIt->minZoom, 8); ++levels) {
                const int parentZoom=zoom-levels;
                const int divisor=1<<levels;
                const int parentX=wrappedX/divisor;
                const int parentY=y/divisor;
                const auto parentPath=cachePathFor(*sourceIt,parentZoom,parentX,parentY);
                if (!cachedTileExists(parentPath)) continue;
                texturePath=parentPath;
                const int childX=wrappedX%divisor;
                const int childY=y%divisor;
                uLeft=float(childX)/float(divisor);
                uRight=float(childX+1)/float(divisor);
                vTop=float(childY)/float(divisor);
                vBottom=float(childY+1)/float(divisor);
                foundParent=true;
                break;
            }
            scheduleTile(*sourceIt, zoom, wrappedX, y, path, true);
            if (!foundParent) continue;
        }
        const double minMercX=(static_cast<double>(x)/count*360.0-180.0)*kDeg*kR;
        const double maxMercX=(static_cast<double>(x+1)/count*360.0-180.0)*kDeg*kR;
        const double maxMercY=tileYToMercatorY(y,zoom);
        const double minMercY=tileYToMercatorY(y+1,zoom);
        out.push_back({x,y,zoom,minMercX-centerMercX,minMercY-centerMercY,maxMercX-centerMercX,maxMercY-centerMercY,uLeft,vTop,uRight,vBottom,texturePath});
    }
    schedulePhotoPrefetch(*sourceIt, camera, zoom);
    return out;
}
} // namespace hephaiston::planar_map
