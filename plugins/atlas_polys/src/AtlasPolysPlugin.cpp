#include "PluginAPI.h"
#include "hephaiston/atlas_polys/AtlasPolysExporter.h"
#include "hephaiston/earth/EarthModule.h"
#include "hephaiston/geometry/PolygonValidation.h"
#include "hephaiston/geospatial/JapanPlaneRectangular.h"
#include "hephaiston/native_dialog/NativeFolderDialog.h"
#include "hephaiston/planar_map/PlanarMapModule.h"

#include <imgui.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <sstream>
#include <utility>

namespace hephaiston::atlas_polys {
using geospatial::GeoCoordinate;
using geospatial::GeoPolygon;
using geometry::Point2d;

enum class GeoViewMode { Earth, PlanarMap };
enum class PolygonToolState { Inactive, Capturing, ReadyToComplete, ValidationError };

struct GeoViewTransitionSettings {
    double switchToPlanarAltitudeMeters = 50000.0;
    double switchToEarthZoomThreshold = 5.0;
    bool automaticTransitionEnabled = true;
};

struct State {
    EditorRegistry* registry=nullptr; ViewportStatus* viewportStatus=nullptr; ViewportInputState* input=nullptr; ViewportVisibleRect* visibleRect=nullptr; ViewMode* viewMode=nullptr; SelectionManager* selection=nullptr; IEditorLogger* logger=nullptr;
    earth::EarthModule earth; planar_map::PlanarMapModule map; planar_map::PlanarMapCamera mapCamera; GeoViewMode geoView=GeoViewMode::Earth;
    PolygonToolState toolState=PolygonToolState::Inactive; std::vector<GeoCoordinate> draft; std::optional<GeoCoordinate> preview; std::vector<GeoPolygon> polygons; std::uint64_t nextId=1;
    // Standard map is the reliable default. It is the familiar Tokyo/Saitama
    // map layer and is available over the full GSI standard zoom range.
    std::string mapLayer="gsi_standard"; bool showPhotoPlaceNames=true; std::string message; std::filesystem::path exportFolder; ExportCoordinateMode exportMode=ExportCoordinateMode::Geographic; int exportZone=9; bool exportKml=true,exportCsv=true; bool displayProjected=false;
    std::string renameId; char renameBuffer[128]{};
    GeoViewTransitionSettings transitionSettings;

    std::filesystem::path settingsPath() const { return std::filesystem::current_path() / "hephaiston_atlas_polys.ini"; }
    std::filesystem::path legacySettingsPath() const { return std::filesystem::current_path() / "hephaiston_geo_polygon.ini"; }
    void loadSettings() { std::ifstream in(settingsPath()); bool importedLegacy=false; if (!in) { in.open(legacySettingsPath()); importedLegacy=bool(in); } if (!in) { logger->debug("[AtlasPolys] No persisted plugin settings found; using defaults."); return; } std::string key; std::string value; while (in >> key) { if (key == "exportFolder") { in >> std::quoted(value); if (std::filesystem::is_directory(value)) exportFolder = value; else logger->warning("[AtlasPolys] Ignored unavailable persisted export folder: " + value); } else if (key == "exportZone") { in >> value; try { exportZone = std::clamp(std::stoi(value), 1, 19); } catch (...) { logger->warning("[AtlasPolys] Invalid persisted export zone; using default zone IX."); } } } logger->debug(importedLegacy ? "[AtlasPolys] Imported legacy Geo Polygon settings." : "[AtlasPolys] Plugin settings loaded."); }
    void saveSettings() const { std::ofstream out(settingsPath()); if (!out) { logger->error("[AtlasPolys] Failed to write plugin settings: " + settingsPath().string()); return; } out << "exportFolder " << std::quoted(exportFolder.string()) << '\n' << "exportZone " << exportZone << '\n'; logger->debug("[AtlasPolys] Plugin settings saved."); }

    bool capturing() const { return toolState==PolygonToolState::Capturing || toolState==PolygonToolState::ReadyToComplete || toolState==PolygonToolState::ValidationError; }
    earth::EarthCameraState earthCamera() const { return {viewportStatus->orbitYawDegrees,viewportStatus->orbitPitchDegrees,viewportStatus->orbitDistanceMeters}; }
    earth::EarthImageryLayer earthImageryLayer() const {
        if (mapLayer == "gsi_photo") return earth::EarthImageryLayer::Photo;
        if (mapLayer == "gsi_relief") return earth::EarthImageryLayer::Relief;
        return earth::EarthImageryLayer::Standard;
    }
    void setMapLayer(std::string layer) {
        if (mapLayer == layer) return;
        logger->info("[AtlasPolys] Map imagery layer changed: " + mapLayer + " -> " + layer);
        mapLayer = std::move(layer);
    }
    // The Core FBO spans the area behind both side panels. Its projection is
    // therefore based on the full framebuffer width, not visibleRect.width.
    // Picking must use exactly that same coordinate system or vertices drift.
    planar_map::PlanarMapViewport mapViewport() const { return {ImGui::GetIO().DisplaySize.x, visibleRect->max.y-visibleRect->min.y}; }
    std::optional<GeoCoordinate> pick() const {
        const double x=input->mousePos.x, y=input->mousePos.y-visibleRect->min.y;
        const auto viewport = mapViewport();
        return geoView==GeoViewMode::Earth ? earth.pickEarthSurface({x,y},{viewport.widthPixels,viewport.heightPixels},earthCamera()) : map.screenToGeo(x,y,viewport,mapCamera);
    }
    std::vector<Point2d> projected(const std::vector<GeoCoordinate>& coordinates) const {
        std::vector<Point2d> result; result.reserve(coordinates.size());
        int zone=exportZone; if (!coordinates.empty()) zone=geospatial::suggestedJapanPlaneZone(coordinates.front());
        for(const auto& c:coordinates) { auto p=geospatial::projectToJapanPlane(c,zone); if(!p) return {}; result.push_back({p->eastingMeters,p->northingMeters}); }
        return result;
    }
    void constrainEarthFocusToJapan() {
        // Keep the globe workflow focused on Japanese land. These bounds
        // include the main islands, Okinawa/Yonaguni and Minamitorishima.
        viewportStatus->orbitYawDegrees = std::clamp(viewportStatus->orbitYawDegrees, 122.0f, 154.0f);
        viewportStatus->orbitPitchDegrees = std::clamp(viewportStatus->orbitPitchDegrees, 20.0f, 46.0f);
    }
    void switchView(GeoViewMode mode) {
        const bool leavingPlanar = geoView == GeoViewMode::PlanarMap && mode == GeoViewMode::Earth;
        geoView=mode;
        if(mode==GeoViewMode::Earth){ *viewMode=ViewMode::Mode3D; if(leavingPlanar){const double mpp=std::max(0.01,viewportStatus->metersPerPixel);const double altitude=std::max(1000.0,mpp*mapViewport().heightPixels/(2.0*std::tan(25.0*3.141592653589793/180.0)));viewportStatus->orbitDistanceMeters=static_cast<float>(std::max(1.007,1.0+altitude/earth::EarthModule::earthRadiusMeters));viewportStatus->orbitYawDegrees=static_cast<float>(mapCamera.center.longitudeDegrees);viewportStatus->orbitPitchDegrees=static_cast<float>(mapCamera.center.latitudeDegrees);}else{viewportStatus->orbitDistanceMeters=3.0f;viewportStatus->orbitYawDegrees=140.0f;viewportStatus->orbitPitchDegrees=35.0f;}viewportStatus->targetX=viewportStatus->targetY=viewportStatus->targetZ=0.0f; }
        else { *viewMode=ViewMode::Mode2D; const double mpp=map.metersPerPixel(mapCamera); viewportStatus->metersPerPixel=mpp;viewportStatus->zoom=1.0/mpp;viewportStatus->panMeters={0,0}; }
        registry->viewportRenderSettings().showHorizontalGrid=false; registry->viewportRenderSettings().showOriginAxes=false;
        registry->viewportRenderSettings().showScaleBar = mode == GeoViewMode::PlanarMap;
        if (mode == GeoViewMode::Earth) constrainEarthFocusToJapan();
        logger->info(std::string("[AtlasPolys] Switched to ") + (mode == GeoViewMode::Earth ? "Earth" : "Planar Map") + " view; layer=" + mapLayer);
    }
    void transitionToPlanarMap() {
        if (geoView != GeoViewMode::Earth) return;
        const double mpp = std::max(0.01, earth.surfaceDistanceMeters(earthCamera()) /
            std::max(1.0, mapViewport().heightPixels * 0.93));
        mapCamera.center = {viewportStatus->orbitYawDegrees, viewportStatus->orbitPitchDegrees, std::nullopt};
        mapCamera.zoomLevel = planarMapZoomForMetersPerPixel(mpp, mapCamera.center.latitudeDegrees);
        switchView(GeoViewMode::PlanarMap);
        viewportStatus->metersPerPixel = mpp;
        viewportStatus->zoom = 1.0 / mpp;
        message = "Switched to Planar Map at the matching Earth scale.";
        logger->info("[AtlasPolys] Automatic Earth-to-Planar Map transition completed at matching scale.");
    }
    void transitionToEarth() {
        if (geoView != GeoViewMode::PlanarMap) return;
        switchView(GeoViewMode::Earth);
        message = "Switched to Earth at the matching planar-map scale.";
        logger->info("[AtlasPolys] Automatic Planar Map-to-Earth transition completed at matching scale.");
    }
    static double planarMapZoomForMetersPerPixel(double mpp, double latitudeDegrees) {
        constexpr double kEarthRadiusMeters = 6378137.0;
        constexpr double kPi = 3.141592653589793;
        const double latitudeRadians = latitudeDegrees * kPi / 180.0;
        return std::clamp(std::log2(2.0 * kPi * kEarthRadiusMeters * std::cos(latitudeRadians) /
            (256.0 * std::max(0.01, mpp))), 0.0, 18.0);
    }
    void startCapture(){draft.clear();preview.reset();toolState=PolygonToolState::Capturing;message="Polygon Mode: click the globe or map to add vertices.";logger->info("[AtlasPolys] Polygon capture started.");}
    void cancelCapture(){const auto discarded=draft.size();draft.clear();preview.reset();toolState=PolygonToolState::Inactive;message="Polygon Mode cancelled.";logger->info("[AtlasPolys] Polygon capture cancelled; discarded " + std::to_string(discarded) + " draft vertices.");}
    void undoDraft(){if(draft.empty()){logger->warning("[AtlasPolys] Undo requested with no draft vertices.");return;}draft.pop_back();toolState=draft.size()>=3?PolygonToolState::ReadyToComplete:PolygonToolState::Capturing;logger->debug("[AtlasPolys] Removed last draft vertex; remaining=" + std::to_string(draft.size()));}
    void addVertex(const GeoCoordinate& c){
        if(!geospatial::isValid(c)){message="Point rejected: invalid geographic coordinate.";toolState=PolygonToolState::ValidationError;logger->error("[AtlasPolys] Rejected vertex: invalid geographic coordinate.");return;}
        auto pts=projected(draft); if(pts.size()!=draft.size()){message="Point rejected: projection failed.";toolState=PolygonToolState::ValidationError;logger->error("[AtlasPolys] Rejected vertex: draft coordinate projection failed.");return;}
        auto p=geospatial::projectToJapanPlane(c,draft.empty()?exportZone:geospatial::suggestedJapanPlaneZone(draft.front())); if(!p){message="Point rejected: projection failed.";toolState=PolygonToolState::ValidationError;logger->error("[AtlasPolys] Rejected vertex: candidate coordinate projection failed.");return;}
        const auto check=geometry::validateOpenRingCandidate(pts,{p->eastingMeters,p->northingMeters}); if(!check.valid()){message=check.message;toolState=PolygonToolState::ValidationError;logger->warning("[AtlasPolys] Rejected vertex: " + check.message);return;}
        draft.push_back(c);toolState=draft.size()>=3?PolygonToolState::ReadyToComplete:PolygonToolState::Capturing;message="Vertex added.";logger->info("[AtlasPolys] Vertex accepted; count=" + std::to_string(draft.size()) + ", lon=" + std::to_string(c.longitudeDegrees) + ", lat=" + std::to_string(c.latitudeDegrees));
    }
    void complete(){
        auto pts=projected(draft); const auto check=geometry::validateClosedRing(pts); if(!check.valid()){message=check.message;toolState=PolygonToolState::ValidationError;logger->warning("[AtlasPolys] Polygon completion rejected: " + check.message);return;}
        geometry::normalizeCounterClockwise(draft,pts);
        GeoPolygon polygon{nextId++,"Site Polygon ",draft,true}; polygon.name += (polygon.id<10?"00":polygon.id<100?"0":"")+std::to_string(polygon.id); logger->info("[AtlasPolys] Polygon completed: id=" + std::to_string(polygon.id) + ", vertices=" + std::to_string(polygon.vertices.size()) + ", normalized=CCW."); polygons.push_back(std::move(polygon)); draft.clear();preview.reset();toolState=PolygonToolState::Inactive;message="Polygon completed and normalized counter-clockwise.";
    }
    bool validateAllForExport(std::string& error) const { for (const auto& polygon : polygons) { const auto points=projected(polygon.vertices); const auto check=geometry::validateClosedRing(points); if (!check.valid()) { error="Polygon \""+polygon.name+"\" is invalid: "+check.message; return false; } } return true; }
    GeoPolygon* polygonById(const std::string& id){constexpr std::string_view prefix="atlas_polys:";if(!id.starts_with(prefix))return nullptr; const auto number=id.substr(prefix.size());try{auto value=std::stoull(number);for(auto&p:polygons)if(p.id==value)return&p;}catch(...){}return nullptr;}
    std::string serializeJson() const { std::ostringstream out;out<<"{\"polygons\":[";for(std::size_t i=0;i<polygons.size();++i){const auto&p=polygons[i];if(i)out<<',';out<<"{\"id\":"<<p.id<<",\"name\":\""<<p.name<<"\",\"visible\":"<<(p.visible?"true":"false")<<",\"vertices\":[";for(std::size_t n=0;n<p.vertices.size();++n){if(n)out<<',';out<<"["<<p.vertices[n].longitudeDegrees<<","<<p.vertices[n].latitudeDegrees<<"]";}out<<"]}";}out<<"]}";return out.str(); }
};

std::string escapeCsv(std::string text){std::string out="\"";for(char c:text){if(c=='\"')out+="\"\"";else out+=c;}return out+'\"';}
std::string escapeXml(const std::string& text){std::string out;for(char c:text){if(c=='&')out+="&amp;";else if(c=='<')out+="&lt;";else if(c=='>')out+="&gt;";else out+=c;}return out;}

class KmlExporter final : public IAtlasPolysExporter {
public: std::string_view id()const override{return "kml";} std::string_view displayName()const override{return "KML";}
ExportResult exportPolygons(const ExportRequest&r) override {if(!r.polygons)return {false,{},"No polygons supplied."};const auto file=r.folder/"site_polygons.kml";std::ofstream out(file);if(!out)return{false,{},"Unable to write KML."};out<<std::setprecision(12)<<"<?xml version=\"1.0\" encoding=\"UTF-8\"?><kml xmlns=\"http://www.opengis.net/kml/2.2\"><Document>";for(const auto&p:*r.polygons){out<<"<Placemark><name>"<<escapeXml(p.name)<<"</name><Polygon><outerBoundaryIs><LinearRing><coordinates>";for(const auto&v:p.vertices)out<<v.longitudeDegrees<<","<<v.latitudeDegrees<<",0 ";if(!p.vertices.empty())out<<p.vertices.front().longitudeDegrees<<","<<p.vertices.front().latitudeDegrees<<",0";out<<"</coordinates></LinearRing></outerBoundaryIs></Polygon></Placemark>";}out<<"</Document></kml>";return out?ExportResult{true,{file},"KML exported in EPSG:4326."}:ExportResult{false,{},"KML write failed."};}
};
class CsvExporter final : public IAtlasPolysExporter {
public:std::string_view id()const override{return "coordinate_csv";}std::string_view displayName()const override{return "Coordinate CSV";}
ExportResult exportPolygons(const ExportRequest&r)override{if(!r.polygons)return{false,{},"No polygons supplied."};const auto file=r.folder/"site_polygon_coordinates.csv";std::ofstream out(file);if(!out)return{false,{},"Unable to write CSV."};out<<std::setprecision(12);if(r.coordinateMode==ExportCoordinateMode::Geographic){out<<"polygon_id,polygon_name,vertex_index,epsg,longitude_deg,latitude_deg\n";for(const auto&p:*r.polygons)for(std::size_t i=0;i<p.vertices.size();++i)out<<p.id<<','<<escapeCsv(p.name)<<','<<i+1<<",4326,"<<p.vertices[i].longitudeDegrees<<','<<p.vertices[i].latitudeDegrees<<'\n';}else{const auto*z=geospatial::findJapanPlaneRectangularZone(r.japanPlaneZone);if(!z)return{false,{},"Invalid Japan Plane Rectangular zone."};out<<"polygon_id,polygon_name,vertex_index,epsg,zone,northing_m,easting_m\n";for(const auto&p:*r.polygons)for(std::size_t i=0;i<p.vertices.size();++i){auto c=geospatial::projectToJapanPlane(p.vertices[i],r.japanPlaneZone);if(!c)return{false,{},"Coordinate projection failed."};out<<p.id<<','<<escapeCsv(p.name)<<','<<i+1<<','<<z->epsgCode<<','<<z->zoneNumber<<','<<c->northingMeters<<','<<c->eastingMeters<<'\n';}}return out?ExportResult{true,{file},"CSV exported."}:ExportResult{false,{},"CSV write failed."};}
};

class GeoPanel final : public IMainMenuPanel { std::shared_ptr<State>s_; public:explicit GeoPanel(std::shared_ptr<State>s):s_(std::move(s)){} void draw()override{
 ImGui::TextUnformatted("Atlas Polys");ImGui::SeparatorText("View");if(ImGui::Button("Earth",{100,0}))s_->switchView(GeoViewMode::Earth);ImGui::SameLine();if(ImGui::Button("Planar Map",{100,0}))s_->switchView(GeoViewMode::PlanarMap);
 if(s_->geoView==GeoViewMode::PlanarMap){ImGui::SeparatorText("Layer");const char* current=s_->mapLayer.c_str();if(ImGui::BeginCombo("Map layer",current)){for(const auto&source:s_->map.tileSources()){if(ImGui::Selectable(source.displayName.c_str(),source.id==s_->mapLayer))s_->setMapLayer(source.id);}ImGui::EndCombo();}if(s_->mapLayer=="gsi_photo")ImGui::Checkbox("Place names and roads",&s_->showPhotoPlaceNames);ImGui::TextWrapped("%s",s_->map.attribution(s_->mapLayer).c_str());if(!s_->map.lastTileError().empty())ImGui::TextDisabled("%s",s_->map.lastTileError().c_str());}
 ImGui::SeparatorText("Polygon");if(!s_->capturing()){if(ImGui::Button("Start Polygon Mode",{-1,0}))s_->startCapture();}else{ImGui::TextColored(ImVec4(.3f,.9f,.5f,1),"Polygon Mode: ACTIVE");ImGui::Text("Vertices: %d",int(s_->draft.size()));const bool possible=s_->draft.size()>=3;if(!possible)ImGui::BeginDisabled();if(ImGui::Button("Complete Polygon",{-1,0}))s_->complete();if(!possible)ImGui::EndDisabled();if(ImGui::Button("Undo Last Point",{-1,0}))s_->undoDraft();if(ImGui::Button("Cancel",{-1,0}))s_->cancelCapture();}if(!s_->message.empty())ImGui::TextWrapped("%s",s_->message.c_str());
 ImGui::SeparatorText("Export");int mode=int(s_->exportMode);ImGui::RadioButton("Geographic Coordinates",&mode,0);ImGui::RadioButton("Japan Plane Rectangular CS",&mode,1);s_->exportMode=ExportCoordinateMode(mode);if(s_->exportMode==ExportCoordinateMode::Geographic){ImGui::TextDisabled("CRS: WGS 84 / EPSG:4326 / Longitude, Latitude");}else{const auto*z=geospatial::findJapanPlaneRectangularZone(s_->exportZone);if(ImGui::BeginCombo("Zone",z?z->displayName.c_str():"Invalid")){for(const auto&zone:geospatial::japanPlaneRectangularZones())if(ImGui::Selectable(zone.displayName.c_str(),zone.zoneNumber==s_->exportZone))s_->exportZone=zone.zoneNumber;ImGui::EndCombo();}}
 ImGui::Checkbox("KML",&s_->exportKml);ImGui::Checkbox("Coordinate CSV",&s_->exportCsv);ImGui::TextWrapped("Folder: %s",s_->exportFolder.empty()?"(not selected)":s_->exportFolder.string().c_str());if(ImGui::Button("Select Folder...",{-1,0})){const auto result=native_dialog::selectFolder(s_->exportFolder);if(result.path){s_->exportFolder=*result.path;s_->saveSettings();}else if(!result.error.empty())s_->message=result.error;}const bool canExport=!s_->exportFolder.empty()&&!s_->polygons.empty()&&(s_->exportKml||s_->exportCsv);if(!canExport)ImGui::BeginDisabled();if(ImGui::Button("Export",{-1,0})){std::string validationError;if(!s_->validateAllForExport(validationError)){s_->message="Export failed: "+validationError;return;}std::error_code ec;const auto probe=s_->exportFolder/".hephaiston_write_probe";{std::ofstream probeFile(probe);if(!probeFile){s_->message="Export failed: output folder is not writable.";return;}}std::filesystem::remove(probe,ec);ExportRequest r{s_->exportFolder,s_->exportMode,s_->exportZone,s_->exportKml,s_->exportCsv,&s_->polygons};std::vector<std::filesystem::path>files;if(s_->exportKml){auto result=KmlExporter{}.exportPolygons(r);if(!result.success){s_->message="Export failed: "+result.message;return;}files.insert(files.end(),result.files.begin(),result.files.end());}if(s_->exportCsv){auto result=CsvExporter{}.exportPolygons(r);if(!result.success){s_->message="Export failed: "+result.message;return;}files.insert(files.end(),result.files.begin(),result.files.end());}s_->message="Export completed: "+std::to_string(s_->polygons.size())+" polygons";for(const auto&f:files)s_->message+="\n"+f.string();}if(!canExport)ImGui::EndDisabled();ImGui::TextDisabled("KML always stores coordinates in EPSG:4326. The selected projected CRS is used for coordinate CSV export."); }};

class GeoLayer final : public IViewportSceneLayer {
public:
    explicit GeoLayer(std::shared_ptr<State> state) : state_(std::move(state)) {}
    const char* id() const override { return "atlas_polys.layer"; }
    const char* displayName() const override { return "Atlas Polys View"; }

    void collectViewportTexturedMeshes(std::vector<ViewportTexturedMesh>& out) override {
        if (state_->geoView == GeoViewMode::PlanarMap) {
            const auto appendTiles = [&out](const std::vector<planar_map::MapTile>& tiles, std::string_view idPrefix, float z, bool transparentLightPixels) {
                for (const auto& tile : tiles) {
                ViewportTexturedMesh mesh;
                mesh.id = "atlas_polys." + std::string(idPrefix) + "." + std::to_string(tile.zoom) + "." + std::to_string(tile.x) + "." + std::to_string(tile.y);
                mesh.texturePath = tile.imagePath.string();
                mesh.vertices = {
                    {float(tile.minX), float(tile.minY), z, tile.uLeft, tile.vBottom},
                    {float(tile.maxX), float(tile.minY), z, tile.uRight, tile.vBottom},
                    {float(tile.minX), float(tile.maxY), z, tile.uLeft, tile.vTop},
                    {float(tile.maxX), float(tile.maxY), z, tile.uRight, tile.vTop},
                };
                mesh.indices = {0, 1, 2, 1, 3, 2};
                mesh.makeLightPixelsTransparent = transparentLightPixels;
                out.push_back(std::move(mesh));
            }
            };
            appendTiles(state_->map.visibleTiles(state_->mapCamera, state_->mapViewport(), state_->mapLayer), "tile", -0.02f, false);
            if (state_->mapLayer == "gsi_photo" && state_->showPhotoPlaceNames) {
                appendTiles(state_->map.visibleTiles(state_->mapCamera, state_->mapViewport(), "gsi_standard"), "photo_labels", 0.01f, true);
            }
            return;
        }
        const auto sphere = state_->earth.texturedGlobeMesh();
        ViewportTexturedMesh mesh;
        mesh.id = "atlas_polys.nasa_bluemarble";
        mesh.texturePath = std::string(HEPHAISTON_SOURCE_DIR) + "/assets/nasa_bluemarble_2048.png";
        mesh.vertices.reserve(sphere.vertices.size());
        for (const auto& vertex : sphere.vertices) mesh.vertices.push_back({vertex.x, vertex.y, vertex.z, vertex.u, vertex.v});
        mesh.indices = sphere.indices;
        out.push_back(std::move(mesh));
        for (const auto& tile : state_->earth.visibleDetailTiles(state_->earthCamera(), {state_->mapViewport().widthPixels, state_->mapViewport().heightPixels}, state_->earthImageryLayer())) {
            ViewportTexturedMesh detail;
            detail.id = tile.id;
            detail.texturePath = tile.imagePath.string();
            detail.vertices.reserve(tile.mesh.vertices.size());
            for (const auto& vertex : tile.mesh.vertices) {
                detail.vertices.push_back({vertex.x, vertex.y, vertex.z, vertex.u, vertex.v});
            }
            detail.indices = tile.mesh.indices;
            out.push_back(std::move(detail));
        }
    }

    void collectViewportLines(std::vector<ViewportLine>& out) override {
        auto add = [&out](const auto& lines) { for (const auto& line : lines) out.push_back({line.x1,line.y1,line.z1,line.x2,line.y2,line.z2,{line.r,line.g,line.b,line.a}}); };
        const bool earthView = state_->geoView == GeoViewMode::Earth;
        if (earthView) {
            add(state_->earth.fallbackGlobeLines());
        } else if (state_->map.visibleTiles(state_->mapCamera, state_->mapViewport(), state_->mapLayer).empty()) {
            add(state_->map.fallbackMapLines(state_->mapCamera,state_->mapViewport()));
        }
        for (const auto& polygon : state_->polygons) if (polygon.visible) {
            const bool selected = state_->selection->contains("atlas_polys:" + std::to_string(polygon.id));
            if (earthView) add(state_->earth.polygonLines(polygon.vertices,true,selected)); else add(state_->map.polygonLines(polygon.vertices,state_->mapCamera,true,selected));
        }
        if (state_->capturing()) { auto line=state_->draft; if (state_->preview) line.push_back(*state_->preview); if (earthView) add(state_->earth.polygonLines(line,false,false)); else add(state_->map.polygonLines(line,state_->mapCamera,false,false)); }
    }

    void collectViewportPoints(std::vector<ViewportPoint>& out) override {
        if (state_->geoView != GeoViewMode::PlanarMap) return;
        auto add = [&](const std::vector<GeoCoordinate>& vertices, ImVec4 color) { for (const auto& vertex : vertices) { auto point=state_->map.geoToLocalMeters(vertex,state_->mapCamera); if (point) out.push_back({float(point->first),float(point->second),0.1f,7.0f,color}); } };
        for (const auto& polygon : state_->polygons) if (polygon.visible) add(polygon.vertices,state_->selection->contains("atlas_polys:"+std::to_string(polygon.id))?ImVec4(1,.72f,.18f,1):ImVec4(.25f,.85f,1,1));
        if (state_->capturing()) add(state_->draft,ImVec4(1,.85f,.2f,1));
    }
private:
    std::shared_ptr<State> state_;
};

class GeoTool final : public IViewportTool {
public:
    explicit GeoTool(std::shared_ptr<State> state) : state_(std::move(state)) {}
    const char* id() const override { return "atlas_polys.tool"; }
    const char* displayName() const override { return "Atlas Polys"; }
    bool handlesViewportNavigation(EditorContext&) const override {
        return state_->geoView == GeoViewMode::Earth && !state_->capturing();
    }
    bool blocksDefaultViewportNavigation(EditorContext&) const override { return state_->capturing(); }

    void onViewportInput(EditorContext&) override {
        if (state_->geoView == GeoViewMode::Earth && !state_->capturing()) updateEarthCamera();
        if (state_->geoView == GeoViewMode::Earth) state_->constrainEarthFocusToJapan();
        if (state_->geoView == GeoViewMode::PlanarMap) updatePlanarCamera();
        if (!state_->capturing() && state_->transitionSettings.automaticTransitionEnabled) {
            if (state_->geoView == GeoViewMode::Earth &&
                state_->earth.surfaceDistanceMeters(state_->earthCamera()) <= state_->transitionSettings.switchToPlanarAltitudeMeters) {
                state_->transitionToPlanarMap();
            } else if (state_->geoView == GeoViewMode::PlanarMap &&
                state_->mapCamera.zoomLevel <= state_->transitionSettings.switchToEarthZoomThreshold) {
                state_->transitionToEarth();
            }
        }
        if (!state_->capturing()) return;
        if (state_->input->hovered) state_->preview = state_->pick();
        if (ImGui::IsKeyPressed(ImGuiKey_Escape)) state_->cancelCapture();
        else if (ImGui::IsKeyPressed(ImGuiKey_Backspace) || (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z))) state_->undoDraft();
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter)) state_->complete();
        else if (state_->input->clicked && state_->preview) state_->addVertex(*state_->preview);
        else if (state_->input->hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) state_->complete();
    }
private:
    float earthOrbitResponse() const {
        // Earth uses radius-normalised orbit distance, so use surface altitude
        // rather than the generic CAD distance response. Near the surface the
        // response drops smoothly, preventing small drags from jumping far.
        const double altitude = std::max(1000.0, state_->earth.surfaceDistanceMeters(state_->earthCamera()));
        return std::clamp(static_cast<float>(std::pow(altitude / 1000000.0, 0.35)), 0.18f, 1.25f);
    }
    void dragEarthCamera(const ImVec2& delta, const ViewportNavigationSettings& navigation) {
        constexpr double kPi = 3.14159265358979323846;
        constexpr double kMetersPerLatitudeDegree = 110574.0;
        constexpr double kMetersPerLongitudeDegreeAtEquator = 111320.0;
        constexpr double kVerticalFovRadians = 50.0 * kPi / 180.0;
        const double altitude = std::max(1000.0, state_->earth.surfaceDistanceMeters(state_->earthCamera()));
        const double metersPerPixel = 2.0 * altitude * std::tan(kVerticalFovRadians * 0.5) /
            std::max(1.0, state_->mapViewport().heightPixels);
        const double latitudeRadians = state_->viewportStatus->orbitPitchDegrees * kPi / 180.0;
        const double longitudeDegreesPerPixel = metersPerPixel /
            (kMetersPerLongitudeDegreeAtEquator * std::max(0.10, std::cos(latitudeRadians)));
        const double latitudeDegreesPerPixel = metersPerPixel / kMetersPerLatitudeDegree;
        // At a close altitude, drag now follows the approximate ground scale
        // of one screen pixel instead of applying a fixed angular orbit step.
        constexpr double kDragResponse = 2.0;
        const double sensitivity = navigation.effectiveMoveSensitivity() * kDragResponse;
        state_->viewportStatus->orbitYawDegrees -= static_cast<float>(delta.x * longitudeDegreesPerPixel * sensitivity);
        state_->viewportStatus->orbitPitchDegrees = std::clamp(
            state_->viewportStatus->orbitPitchDegrees + static_cast<float>(delta.y * latitudeDegreesPerPixel * sensitivity),
            -82.0f, 82.0f);
    }
    void updateEarthCamera() {
        if (!state_->input->hovered) return;
        ImGuiIO& io = ImGui::GetIO();
        const auto& navigation = state_->registry->viewportNavigationSettings();
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left) && !io.KeyShift) {
            const ImVec2 delta = io.MouseDelta;
            // Align Earth orbit with the flat-map drag convention while
            // scaling it to the current ground resolution.
            dragEarthCamera(delta, navigation);
        }
        if (std::abs(state_->input->wheel) > 0.0f) {
            const double factor = std::pow(1.0 / 1.12, static_cast<double>(state_->input->wheel) *
                navigation.effectiveZoomSensitivity() * earthOrbitResponse());
            // Stay just above the surface so the camera cannot enter the globe
            // or hit the near clip plane; close Earth views move to Planar Map.
            state_->viewportStatus->orbitDistanceMeters = std::clamp(static_cast<float>(state_->viewportStatus->orbitDistanceMeters * factor), 1.007f, 10000000.0f);
            state_->viewportStatus->zoom = 1.0 / state_->viewportStatus->orbitDistanceMeters;
            state_->viewportStatus->metersPerPixel = state_->earth.surfaceDistanceMeters(state_->earthCamera()) / std::max(1.0, state_->mapViewport().heightPixels * 0.93);
        }
    }
    void updatePlanarCamera() {
        const auto viewport = state_->mapViewport();
        const double mpp = std::max(0.01, state_->viewportStatus->metersPerPixel);
        if (std::abs(state_->viewportStatus->panMeters.x) > 0.001f || std::abs(state_->viewportStatus->panMeters.y) > 0.001f) {
            const auto center = state_->map.screenToGeo(viewport.widthPixels*.5 + state_->viewportStatus->panMeters.x/mpp, viewport.heightPixels*.5 - state_->viewportStatus->panMeters.y/mpp, viewport, state_->mapCamera);
            if (center) state_->mapCamera.center = *center;
            state_->viewportStatus->panMeters = {0,0};
        }
        int minZoom = 0;
        int maxZoom = 18;
        for (const auto& source : state_->map.tileSources()) {
            if (source.id == state_->mapLayer) {
                minZoom = source.minZoom;
                maxZoom = source.maxZoom;
                break;
            }
        }
        state_->mapCamera.zoomLevel = std::clamp(
            State::planarMapZoomForMetersPerPixel(mpp, state_->mapCamera.center.latitudeDegrees),
            static_cast<double>(minZoom), static_cast<double>(maxZoom));

        // The FBO renderer, tile geometry and picking must use precisely the
        // same scale. Without writing the clamped tile scale back here, zooming
        // past a source's max level made the rendered view and click ray drift.
        const double realizedMpp = state_->map.metersPerPixel(state_->mapCamera);
        state_->viewportStatus->metersPerPixel = realizedMpp;
        state_->viewportStatus->zoom = 1.0 / realizedMpp;
    }
    std::shared_ptr<State> state_;
};

class AtlasPolysHierarchy final : public IHierarchyProvider {std::shared_ptr<State>s_;public:explicit AtlasPolysHierarchy(std::shared_ptr<State>s):s_(std::move(s)){}const char*id()const override{return"atlas_polys.hierarchy";}void collectHierarchy(std::vector<EditorHierarchyItem>&out)override{EditorHierarchyItem root{"atlas_polys","Atlas Polys",{}};for(const auto&p:s_->polygons){EditorHierarchyItem poly{"atlas_polys:"+std::to_string(p.id),p.name,{}};for(std::size_t i=0;i<p.vertices.size();++i){std::ostringstream label;label<<"Vertex "<<i+1<<": ";if(s_->displayProjected){auto c=geospatial::projectToJapanPlane(p.vertices[i],s_->exportZone);if(c)label<<"N "<<std::fixed<<std::setprecision(2)<<c->northingMeters<<" m / E "<<c->eastingMeters<<" m";}else label<<std::fixed<<std::setprecision(6)<<p.vertices[i].longitudeDegrees<<"°, "<<p.vertices[i].latitudeDegrees<<"°";poly.children.push_back({"atlas_polys_vertex:"+std::to_string(p.id)+":"+std::to_string(i),label.str(),{}});}root.children.push_back(std::move(poly));}out.push_back(std::move(root));}};

class AtlasPolysProperties final : public IPropertiesPanel {std::shared_ptr<State>s_;public:explicit AtlasPolysProperties(std::shared_ptr<State>s):s_(std::move(s)){}const char*id()const override{return"atlas_polys.properties";}bool canInspect(const SelectionItem&i)const override{return i.id.starts_with("atlas_polys:");}void draw(EditorContext&,const SelectionItem&i)override{auto*p=s_->polygonById(i.id);if(!p)return;ImGui::Text("ID: %llu",static_cast<unsigned long long>(p->id));if(s_->renameId!=i.id){if(ImGui::Button("Rename")){s_->renameId=i.id;std::snprintf(s_->renameBuffer,sizeof(s_->renameBuffer),"%s",p->name.c_str());}}else{ImGui::SetNextItemWidth(-1);ImGui::InputText("Name",s_->renameBuffer,sizeof(s_->renameBuffer));if(ImGui::IsKeyPressed(ImGuiKey_Enter)&&s_->renameBuffer[0]){p->name=s_->renameBuffer;s_->renameId.clear();}ImGui::SameLine();if(ImGui::Button("Cancel"))s_->renameId.clear();}ImGui::Checkbox("Visible",&p->visible);ImGui::Checkbox("Display projected coordinates",&s_->displayProjected);}};

class AtlasPolysContextMenu final : public IContextMenuProvider {std::shared_ptr<State>s_;public:explicit AtlasPolysContextMenu(std::shared_ptr<State>s):s_(std::move(s)){}const char*id()const override{return"atlas_polys.context";}void drawHierarchyContextMenu(EditorContext&,const std::string&id)override{auto*p=s_->polygonById(id);if(!p)return;ImGui::Separator();if(ImGui::MenuItem(p->visible?"Hide polygon":"Show polygon"))p->visible=!p->visible;if(ImGui::MenuItem("Delete polygon")){const auto pid=p->id;s_->polygons.erase(std::remove_if(s_->polygons.begin(),s_->polygons.end(),[pid](const auto&q){return q.id==pid;}),s_->polygons.end());s_->selection->clear();}}};

class AtlasPolysStatus final : public IStatusBarWidget {std::shared_ptr<State>s_;public:explicit AtlasPolysStatus(std::shared_ptr<State>s):s_(std::move(s)){}void draw()override{ImGui::TextUnformatted(s_->geoView==GeoViewMode::Earth?"Atlas: Earth":"Atlas: Planar Map");ImGui::SameLine();ImGui::TextUnformatted(s_->map.attribution(s_->mapLayer).c_str());}};

class AtlasPolysPlugin final : public IPlugin {
public:
    PluginDescriptor descriptor() const override { return {"atlas_polys", "Atlas Polys", "Hephaiston", "0.1.0", kPluginApiVersion}; }

    bool onLoad(EditorContext& context) override {
        context.logger().info("[AtlasPolys] Loading Atlas Polys plugin.");
        state_ = std::make_shared<State>();
        state_->registry = &context.registry();
        state_->viewportStatus = &context.viewportStatus();
        state_->input = &context.viewportInput();
        state_->visibleRect = &context.visibleRect();
        state_->viewMode = &context.viewMode();
        state_->selection = &context.selection();
        state_->logger = &context.logger();
        state_->loadSettings();
        state_->switchView(GeoViewMode::Earth);

        auto& registry = context.registry();
        registry.viewportRenderSettings().showViewModeToggle = false;
        registry.viewportNavigationSettings().trackpadZoomGestureMode = TrackpadZoomGestureMode::Pinch;
        registry.registerMainMenuPanel("atlas_polys.panel", "Atlas Polys", std::make_unique<GeoPanel>(state_));
        registry.registerViewportSceneLayer(std::make_unique<GeoLayer>(state_));
        registry.registerViewportTool(std::make_unique<GeoTool>(state_));
        registry.registerHierarchyProvider(std::make_unique<AtlasPolysHierarchy>(state_));
        registry.registerPropertiesPanel(std::make_unique<AtlasPolysProperties>(state_));
        registry.registerContextMenuProvider(std::make_unique<AtlasPolysContextMenu>(state_));
        registry.registerStatusBarWidget("atlas_polys.status", "Atlas Polys Status", std::make_unique<AtlasPolysStatus>(state_));
        registry.registerMenuItem({"atlas_polys.menu", "Plugins", "Atlas Polys", {}, true, true, [] {}});
        context.logger().info("[AtlasPolys] Atlas Polys plugin loaded; Earth view and pinch navigation are active.");
        return true;
    }

    void onUnload(EditorContext& context) override {
        context.logger().info("[AtlasPolys] Unloading Atlas Polys plugin.");
        if (state_ && state_->capturing()) state_->cancelCapture();
        if (state_) state_->saveSettings();
        context.registry().viewportRenderSettings().showHorizontalGrid = true;
        context.registry().viewportRenderSettings().showOriginAxes = true;
        context.registry().viewportRenderSettings().showScaleBar = true;
        context.registry().viewportRenderSettings().showViewModeToggle = true;
        context.registry().viewportNavigationSettings().trackpadZoomGestureMode = TrackpadZoomGestureMode::TwoFingerScroll;
        state_.reset();
        context.logger().info("[AtlasPolys] Atlas Polys plugin unloaded.");
    }

private:
    std::shared_ptr<State> state_;
};
} // namespace hephaiston::atlas_polys

HEPHAISTON_DECLARE_PLUGIN(hephaiston::atlas_polys::AtlasPolysPlugin)
