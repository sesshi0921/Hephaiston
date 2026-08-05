#include "hephaiston/earth/EarthModule.h"

#include <algorithm>
#include <cmath>
#include <curl/curl.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <mutex>

namespace hephaiston::earth {
namespace {
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kDeg = kPi / 180.0;
struct V { double x, y, z; };
V add(V a, V b) { return {a.x+b.x,a.y+b.y,a.z+b.z}; }
V sub(V a, V b) { return {a.x-b.x,a.y-b.y,a.z-b.z}; }
V mul(V a, double s) { return {a.x*s,a.y*s,a.z*s}; }
double dot(V a,V b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
V cross(V a,V b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
V normalized(V v) { const double d=std::sqrt(std::max(1e-20,dot(v,v))); return mul(v,1.0/d); }
V sphere(const geospatial::GeoCoordinate& c, double radius=1.004) { const double lat=c.latitudeDegrees*kDeg, lon=c.longitudeDegrees*kDeg; return {radius*std::cos(lat)*std::cos(lon),radius*std::cos(lat)*std::sin(lon),radius*std::sin(lat)}; }
EarthLine line(V a,V b, float r,float g,float bl,float alpha) { return {float(a.x),float(a.y),float(a.z),float(b.x),float(b.y),float(b.z),r,g,bl,alpha}; }
constexpr int kTileSize = 256;
double normalizedTileY(double latitudeDegrees) { const double latitude=std::clamp(latitudeDegrees,-85.05112878,85.05112878)*kDeg; return (1.0-std::log(std::tan(latitude)+1.0/std::cos(latitude))/kPi)*0.5; }
double tileYToLatitude(int tileY,int zoom) { const double count=std::pow(2.0,zoom); return std::atan(std::sinh(kPi*(1.0-2.0*tileY/count)))/kDeg; }
std::string replaceToken(std::string value,const char* token,int number) { const std::string replacement=std::to_string(number); std::size_t offset=0; while((offset=value.find(token,offset))!=std::string::npos){value.replace(offset,std::strlen(token),replacement);offset+=replacement.size();} return value; }
size_t writeFile(void* data,size_t size,size_t count,void* user) { return std::fwrite(data,size,count,static_cast<FILE*>(user)); }
const char* imageryId(EarthImageryLayer imagery) {
    switch (imagery) {
    case EarthImageryLayer::Photo: return "photo";
    case EarthImageryLayer::Relief: return "relief";
    case EarthImageryLayer::Standard: return "std";
    }
    return "std";
}
const char* imageryExtension(EarthImageryLayer imagery) {
    return imagery == EarthImageryLayer::Photo ? ".jpg" : ".png";
}
const char* imageryUrlTemplate(EarthImageryLayer imagery) {
    switch (imagery) {
    case EarthImageryLayer::Photo: return "https://cyberjapandata.gsi.go.jp/xyz/seamlessphoto/{z}/{x}/{y}.jpg";
    case EarthImageryLayer::Relief: return "https://cyberjapandata.gsi.go.jp/xyz/relief/{z}/{x}/{y}.png";
    case EarthImageryLayer::Standard: return "https://cyberjapandata.gsi.go.jp/xyz/std/{z}/{x}/{y}.png";
    }
    return "https://cyberjapandata.gsi.go.jp/xyz/std/{z}/{x}/{y}.png";
}
} // namespace

std::optional<geospatial::GeoCoordinate> EarthModule::pickEarthSurface(ScreenPoint screen, ViewportRect viewport, const EarthCameraState& camera) const {
    if (viewport.width <= 0.0 || viewport.height <= 0.0) return std::nullopt;
    const double yaw=camera.yawDegrees*kDeg, pitch=camera.pitchDegrees*kDeg;
    const V eye {camera.distanceEarthRadii*std::cos(pitch)*std::cos(yaw), camera.distanceEarthRadii*std::cos(pitch)*std::sin(yaw), camera.distanceEarthRadii*std::sin(pitch)};
    const V forward=normalized(mul(eye,-1.0));
    const V right=normalized(cross(forward,{0,0,1}));
    const V up=cross(right,forward);
    const double aspect=viewport.width/viewport.height;
    const double tanHalf=std::tan(50.0*kDeg*0.5);
    const double nx=(2.0*screen.x/viewport.width-1.0)*aspect*tanHalf;
    const double ny=(1.0-2.0*screen.y/viewport.height)*tanHalf;
    const V ray=normalized(add(forward,add(mul(right,nx),mul(up,ny))));
    const double b=2.0*dot(eye,ray), c=dot(eye,eye)-1.0, d=b*b-4.0*c;
    if (d < 0.0) return std::nullopt;
    const double t=(-b-std::sqrt(d))*0.5;
    if (t < 0.0) return std::nullopt;
    const V p=normalized(add(eye,mul(ray,t)));
    return geospatial::GeoCoordinate {std::atan2(p.y,p.x)/kDeg, std::asin(p.z)/kDeg, std::nullopt};
}

std::vector<EarthLine> EarthModule::fallbackGlobeLines() const {
    std::vector<EarthLine> out;
    constexpr int segments=96;
    for (int lat=-75; lat<=75; lat+=15) for (int i=0;i<segments;++i) {
        const double a=-180.0+360.0*i/segments,b=-180.0+360.0*(i+1)/segments;
        out.push_back(line(sphere({a,double(lat),{}}),sphere({b,double(lat),{}}),0.20f,0.42f,0.56f,1.0f));
    }
    for (int lon=-180;lon<180;lon+=15) for(int i=0;i<segments/2;++i) {
        const double a=-90.0+180.0*i/(segments/2),b=-90.0+180.0*(i+1)/(segments/2);
        out.push_back(line(sphere({double(lon),a,{}}),sphere({double(lon),b,{}}),0.16f,0.34f,0.46f,1.0f));
    }
    // Equator and prime meridian provide an offline, data-free reference.
    return out;
}

EarthSurfaceMesh EarthModule::texturedGlobeMesh(int longitudeSegments, int latitudeSegments) const {
    EarthSurfaceMesh mesh;
    longitudeSegments = std::max(8, longitudeSegments);
    latitudeSegments = std::max(4, latitudeSegments);
    mesh.vertices.reserve(static_cast<std::size_t>(longitudeSegments + 1) * (latitudeSegments + 1));
    mesh.indices.reserve(static_cast<std::size_t>(longitudeSegments) * latitudeSegments * 6);
    for (int y = 0; y <= latitudeSegments; ++y) {
        const double v = static_cast<double>(y) / latitudeSegments;
        const double latitude = 90.0 - v * 180.0;
        for (int x = 0; x <= longitudeSegments; ++x) {
            const double u = static_cast<double>(x) / longitudeSegments;
            const V p = sphere({-180.0 + u * 360.0, latitude, {0.0}}, 1.0);
            mesh.vertices.push_back({static_cast<float>(p.x), static_cast<float>(p.y), static_cast<float>(p.z), static_cast<float>(u), static_cast<float>(v)});
        }
    }
    const unsigned int row = static_cast<unsigned int>(longitudeSegments + 1);
    for (int y = 0; y < latitudeSegments; ++y) for (int x = 0; x < longitudeSegments; ++x) {
        const unsigned int a = static_cast<unsigned int>(y) * row + static_cast<unsigned int>(x);
        mesh.indices.insert(mesh.indices.end(), {a, a + row, a + 1, a + 1, a + row, a + row + 1});
    }
    return mesh;
}

std::vector<EarthLine> EarthModule::polygonLines(const std::vector<geospatial::GeoCoordinate>& vertices, bool closed, bool selected) const {
    std::vector<EarthLine> out; if (vertices.size()<2) return out;
    const auto color=selected ? std::array<float,4>{1.0f,0.72f,0.18f,1.0f} : std::array<float,4>{0.25f,0.85f,1.0f,1.0f};
    for(std::size_t i=1;i<vertices.size();++i) out.push_back(line(sphere(vertices[i-1]),sphere(vertices[i]),color[0],color[1],color[2],color[3]));
    if(closed) out.push_back(line(sphere(vertices.back()),sphere(vertices.front()),color[0],color[1],color[2],color[3]));
    return out;
}

double EarthModule::surfaceDistanceMeters(const EarthCameraState& camera) const { return std::max(0.0, camera.distanceEarthRadii-1.0)*earthRadiusMeters; }
geospatial::GeoCoordinate EarthModule::viewCenterCoordinate(const EarthCameraState& camera) const {
    const double yaw = camera.yawDegrees * kDeg;
    return {std::atan2(std::sin(yaw), std::cos(yaw)) / kDeg, camera.pitchDegrees, std::nullopt};
}

EarthModule::EarthModule() : tileWorkerThread_(&EarthModule::tileWorker, this) {}
EarthModule::~EarthModule() { { std::lock_guard lock(tileMutex_); stopTileWorker_=true; } tileCondition_.notify_one(); if(tileWorkerThread_.joinable()) tileWorkerThread_.join(); }

void EarthModule::scheduleDetailTile(int zoom,int x,int y,EarthImageryLayer imagery,const std::filesystem::path& path) {
    if (std::filesystem::exists(path) && std::filesystem::file_size(path) > 0) return;
    const std::string key=std::string(imageryId(imagery))+":"+std::to_string(zoom)+":"+std::to_string(x)+":"+std::to_string(y);
    { std::lock_guard lock(tileMutex_); if(failedDetailTileKeys_.contains(key)||scheduledTileKeys_.contains(key)) return; scheduledTileKeys_.insert(key); pendingTiles_.push_back({zoom,x,y,imagery,path,key}); }
    tileCondition_.notify_one();
}

void EarthModule::tileWorker() { for (;;) { TileRequest request; { std::unique_lock lock(tileMutex_); tileCondition_.wait(lock,[this]{return stopTileWorker_||!pendingTiles_.empty();}); if(stopTileWorker_&&pendingTiles_.empty()) return; request=std::move(pendingTiles_.front()); pendingTiles_.pop_front(); } const bool ok=downloadDetailTile(request); std::lock_guard lock(tileMutex_); scheduledTileKeys_.erase(request.key); if(!ok) failedDetailTileKeys_.insert(request.key); } }

bool EarthModule::downloadDetailTile(const TileRequest& request) {
    std::error_code error; std::filesystem::create_directories(request.path.parent_path(),error);
    FILE* file=std::fopen(request.path.string().c_str(),"wb");
    if (error || !file) return false;
    static std::once_flag curlInit; static CURLcode initResult=CURLE_FAILED_INIT;
    std::call_once(curlInit,[] { initResult=curl_global_init(CURL_GLOBAL_DEFAULT); });
    const std::string base=imageryUrlTemplate(request.imagery);
    const std::string url=replaceToken(replaceToken(replaceToken(base,"{z}",request.zoom),"{x}",request.x),"{y}",request.y);
    CURL* curl=initResult==CURLE_OK ? curl_easy_init() : nullptr;
    if (!curl) { std::fclose(file); std::filesystem::remove(request.path,error); return false; }
    curl_easy_setopt(curl,CURLOPT_URL,url.c_str()); curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,writeFile); curl_easy_setopt(curl,CURLOPT_WRITEDATA,file); curl_easy_setopt(curl,CURLOPT_FOLLOWLOCATION,1L); curl_easy_setopt(curl,CURLOPT_CONNECTTIMEOUT_MS,1000L); curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,3000L); curl_easy_setopt(curl,CURLOPT_USERAGENT,"Hephaiston/0.1 Earth imagery");
    const CURLcode result=curl_easy_perform(curl); long http=0; curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&http); curl_easy_cleanup(curl); std::fclose(file);
    if (result!=CURLE_OK || http<200 || http>=300 || !std::filesystem::exists(request.path) || std::filesystem::file_size(request.path)==0) { std::filesystem::remove(request.path,error); return false; }
    return true;
}

std::vector<EarthDetailTile> EarthModule::visibleDetailTiles(const EarthCameraState& camera, ViewportRect viewport, EarthImageryLayer imagery) {
    const double altitude=surfaceDistanceMeters(camera);
    if (altitude > 2500000.0 || viewport.width<=0.0 || viewport.height<=0.0) return {};
    const geospatial::GeoCoordinate center=viewCenterCoordinate(camera);
    const double mpp=std::max(1.0,2.0*std::max(1000.0,altitude)*std::tan(25.0*kDeg)/viewport.height);
    const int maxTileZoom = imagery == EarthImageryLayer::Relief ? 15 : 17;
    const int zoom=std::clamp(static_cast<int>(std::floor(std::log2(2.0*kPi*earthRadiusMeters*std::max(0.2,std::cos(center.latitudeDegrees*kDeg))/(kTileSize*mpp)))),4,maxTileZoom);
    const int count=1<<zoom;
    const double halfLatitude=std::clamp(std::atan((altitude/earthRadiusMeters)*std::tan(25.0*kDeg))*viewport.height/viewport.width/kDeg+1.0,1.0,45.0);
    const double halfLongitude=std::clamp(halfLatitude*viewport.width/viewport.height/std::max(0.2,std::cos(center.latitudeDegrees*kDeg))+1.0,1.0,90.0);
    const int minY=std::max(0,static_cast<int>(std::floor(normalizedTileY(center.latitudeDegrees+halfLatitude)*count))-1);
    const int maxY=std::min(count-1,static_cast<int>(std::floor(normalizedTileY(center.latitudeDegrees-halfLatitude)*count))+1);
    const int minX=static_cast<int>(std::floor((center.longitudeDegrees-halfLongitude+180.0)/360.0*count))-1;
    const int maxX=static_cast<int>(std::floor((center.longitudeDegrees+halfLongitude+180.0)/360.0*count))+1;
    std::vector<EarthDetailTile> out;
    for (int y=minY;y<=maxY;++y) for (int rawX=minX;rawX<=maxX;++rawX) {
        const int x=((rawX%count)+count)%count;
        const auto path=std::filesystem::current_path()/".hephaiston_earth_tile_cache"/imageryId(imagery)/std::to_string(zoom)/std::to_string(x)/(std::to_string(y)+imageryExtension(imagery));
        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) == 0) {
            scheduleDetailTile(zoom,x,y,imagery,path);
            continue;
        }
        const double lonMin=static_cast<double>(rawX)/count*360.0-180.0, lonMax=static_cast<double>(rawX+1)/count*360.0-180.0, latMax=tileYToLatitude(y,zoom), latMin=tileYToLatitude(y+1,zoom);
        EarthSurfaceMesh mesh; constexpr int divisions=6; const unsigned row=divisions+1;
        for(int iy=0;iy<=divisions;++iy) for(int ix=0;ix<=divisions;++ix) { const double v=static_cast<double>(iy)/divisions,u=static_cast<double>(ix)/divisions; const V point=sphere({lonMin+(lonMax-lonMin)*u,latMax+(latMin-latMax)*v,{}},1.0015); mesh.vertices.push_back({float(point.x),float(point.y),float(point.z),float(u),float(v)}); }
        for(unsigned iy=0;iy<divisions;++iy) for(unsigned ix=0;ix<divisions;++ix) { const unsigned a=iy*row+ix; mesh.indices.insert(mesh.indices.end(),{a,a+1,a+row,a+1,a+row+1,a+row}); }
        out.push_back({"earth.detail."+std::to_string(zoom)+"."+std::to_string(rawX)+"."+std::to_string(y),path,std::move(mesh)});
    }
    for (int delta : {-2, -1, 1, 2}) {
        const int prefetchZoom=std::clamp(zoom+delta,4,maxTileZoom);
        const int prefetchCount=1<<prefetchZoom;
        const int centerX=static_cast<int>(std::floor((center.longitudeDegrees+180.0)/360.0*prefetchCount));
        const int centerY=static_cast<int>(std::floor(normalizedTileY(center.latitudeDegrees)*prefetchCount));
        for(int y=centerY-1;y<=centerY+1;++y) for(int rawX=centerX-1;rawX<=centerX+1;++rawX) {
            if(y<0||y>=prefetchCount) continue;
            const int x=((rawX%prefetchCount)+prefetchCount)%prefetchCount;
            const auto path=std::filesystem::current_path()/".hephaiston_earth_tile_cache"/imageryId(imagery)/std::to_string(prefetchZoom)/std::to_string(x)/(std::to_string(y)+imageryExtension(imagery));
            scheduleDetailTile(prefetchZoom,x,y,imagery,path);
        }
    }
    return out;
}
} // namespace hephaiston::earth
