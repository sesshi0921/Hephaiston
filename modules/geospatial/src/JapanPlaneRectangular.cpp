#include "hephaiston/geospatial/JapanPlaneRectangular.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace hephaiston::geospatial {
namespace {
constexpr double kPi = 3.1415926535897932384626433832795;
constexpr double kDegToRad = kPi / 180.0;
constexpr double kRadToDeg = 180.0 / kPi;
constexpr double kA = 6378137.0;                 // GRS80
constexpr double kInvF = 298.257222101;
constexpr double kF = 1.0 / kInvF;
constexpr double kE2 = 2.0 * kF - kF * kF;
constexpr double kEp2 = kE2 / (1.0 - kE2);
constexpr double kScale = 0.9999;

const std::vector<JapanPlaneRectangularZone> kZones {
    {1, 6669, "Zone I", "Nagano / Yamanashi / Shizuoka", 33.0, 129.5},
    {2, 6670, "Zone II", "Fukuoka / Saga / Kumamoto", 33.0, 131.0},
    {3, 6671, "Zone III", "Yamaguchi / Hiroshima / Shimane", 36.0, 132.1666666666667},
    {4, 6672, "Zone IV", "Kagawa / Ehime / Tokushima", 33.0, 133.5},
    {5, 6673, "Zone V", "Hyogo / Okayama / Tottori", 36.0, 134.3333333333333},
    {6, 6674, "Zone VI", "Kyoto / Osaka / Wakayama", 36.0, 136.0},
    {7, 6675, "Zone VII", "Fukui / Gifu / Aichi", 36.0, 137.1666666666667},
    {8, 6676, "Zone VIII", "Niigata / Nagano / Toyama", 36.0, 138.5},
    {9, 6677, "Zone IX", "Tokyo / Kanagawa / Saitama", 36.0, 139.8333333333333},
    {10, 6678, "Zone X", "Aomori / Akita / Iwate", 40.0, 140.8333333333333},
    {11, 6679, "Zone XI", "Hokkaido south", 44.0, 140.25},
    {12, 6680, "Zone XII", "Hokkaido central", 44.0, 142.25},
    {13, 6681, "Zone XIII", "Hokkaido east", 44.0, 144.25},
    {14, 6682, "Zone XIV", "Okinawa", 26.0, 142.0},
    {15, 6683, "Zone XV", "Okinawa west", 26.0, 127.5},
    {16, 6684, "Zone XVI", "Okinawa southwest", 26.0, 124.0},
    {17, 6685, "Zone XVII", "Ogasawara", 26.0, 131.0},
    {18, 6686, "Zone XVIII", "Ogasawara east", 20.0, 136.0},
    {19, 6687, "Zone XIX", "Okinotorishima", 26.0, 154.0},
};

double meridionalArc(double latitude) {
    const double e4 = kE2 * kE2;
    const double e6 = e4 * kE2;
    return kA * ((1.0 - kE2 / 4.0 - 3.0 * e4 / 64.0 - 5.0 * e6 / 256.0) * latitude
        - (3.0 * kE2 / 8.0 + 3.0 * e4 / 32.0 + 45.0 * e6 / 1024.0) * std::sin(2.0 * latitude)
        + (15.0 * e4 / 256.0 + 45.0 * e6 / 1024.0) * std::sin(4.0 * latitude)
        - (35.0 * e6 / 3072.0) * std::sin(6.0 * latitude));
}
} // namespace

bool isValid(const GeoCoordinate& c) {
    return std::isfinite(c.longitudeDegrees) && std::isfinite(c.latitudeDegrees) &&
           c.longitudeDegrees >= -180.0 && c.longitudeDegrees <= 180.0 &&
           c.latitudeDegrees >= -90.0 && c.latitudeDegrees <= 90.0;
}

const std::vector<JapanPlaneRectangularZone>& japanPlaneRectangularZones() { return kZones; }

const JapanPlaneRectangularZone* findJapanPlaneRectangularZone(int zoneNumber) {
    const auto it = std::find_if(kZones.begin(), kZones.end(), [zoneNumber](const auto& zone) { return zone.zoneNumber == zoneNumber; });
    return it == kZones.end() ? nullptr : &*it;
}

std::optional<ProjectedCoordinate> projectToJapanPlane(const GeoCoordinate& coordinate, int zoneNumber) {
    const auto* zone = findJapanPlaneRectangularZone(zoneNumber);
    if (!zone || !isValid(coordinate)) return std::nullopt;
    const double lat = coordinate.latitudeDegrees * kDegToRad;
    const double lon = coordinate.longitudeDegrees * kDegToRad;
    const double lon0 = zone->centralMeridianDegrees * kDegToRad;
    const double lat0 = zone->latitudeOfOriginDegrees * kDegToRad;
    const double sinLat = std::sin(lat);
    const double cosLat = std::cos(lat);
    const double tanLat = std::tan(lat);
    const double n = kA / std::sqrt(1.0 - kE2 * sinLat * sinLat);
    const double t = tanLat * tanLat;
    const double c = kEp2 * cosLat * cosLat;
    const double a = cosLat * (lon - lon0);
    const double m = meridionalArc(lat);
    const double m0 = meridionalArc(lat0);
    ProjectedCoordinate result;
    result.eastingMeters = kScale * n * (a + (1.0 - t + c) * std::pow(a, 3) / 6.0 +
        (5.0 - 18.0 * t + t * t + 72.0 * c - 58.0 * kEp2) * std::pow(a, 5) / 120.0);
    result.northingMeters = kScale * (m - m0 + n * tanLat * (a * a / 2.0 +
        (5.0 - t + 9.0 * c + 4.0 * c * c) * std::pow(a, 4) / 24.0 +
        (61.0 - 58.0 * t + t * t + 600.0 * c - 330.0 * kEp2) * std::pow(a, 6) / 720.0));
    return result;
}

std::optional<GeoCoordinate> unprojectFromJapanPlane(const ProjectedCoordinate& coordinate, int zoneNumber) {
    const auto* zone = findJapanPlaneRectangularZone(zoneNumber);
    if (!zone || !std::isfinite(coordinate.northingMeters) || !std::isfinite(coordinate.eastingMeters)) return std::nullopt;
    const double lat0 = zone->latitudeOfOriginDegrees * kDegToRad;
    const double m = meridionalArc(lat0) + coordinate.northingMeters / kScale;
    const double e1 = (1.0 - std::sqrt(1.0 - kE2)) / (1.0 + std::sqrt(1.0 - kE2));
    const double mu = m / (kA * (1.0 - kE2 / 4.0 - 3.0 * kE2 * kE2 / 64.0 - 5.0 * kE2 * kE2 * kE2 / 256.0));
    const double phi1 = mu + (3.0 * e1 / 2.0 - 27.0 * std::pow(e1, 3) / 32.0) * std::sin(2.0 * mu)
        + (21.0 * e1 * e1 / 16.0 - 55.0 * std::pow(e1, 4) / 32.0) * std::sin(4.0 * mu)
        + (151.0 * std::pow(e1, 3) / 96.0) * std::sin(6.0 * mu)
        + (1097.0 * std::pow(e1, 4) / 512.0) * std::sin(8.0 * mu);
    const double sin1 = std::sin(phi1);
    const double cos1 = std::cos(phi1);
    const double t1 = std::tan(phi1) * std::tan(phi1);
    const double c1 = kEp2 * cos1 * cos1;
    const double n1 = kA / std::sqrt(1.0 - kE2 * sin1 * sin1);
    const double r1 = kA * (1.0 - kE2) / std::pow(1.0 - kE2 * sin1 * sin1, 1.5);
    const double d = coordinate.eastingMeters / (n1 * kScale);
    const double lat = phi1 - (n1 * std::tan(phi1) / r1) * (d * d / 2.0 -
        (5.0 + 3.0 * t1 + 10.0 * c1 - 4.0 * c1 * c1 - 9.0 * kEp2) * std::pow(d, 4) / 24.0 +
        (61.0 + 90.0 * t1 + 298.0 * c1 + 45.0 * t1 * t1 - 252.0 * kEp2 - 3.0 * c1 * c1) * std::pow(d, 6) / 720.0);
    const double lon = zone->centralMeridianDegrees * kDegToRad + (d - (1.0 + 2.0 * t1 + c1) * std::pow(d, 3) / 6.0 +
        (5.0 - 2.0 * c1 + 28.0 * t1 - 3.0 * c1 * c1 + 8.0 * kEp2 + 24.0 * t1 * t1) * std::pow(d, 5) / 120.0) / cos1;
    GeoCoordinate result {lon * kRadToDeg, lat * kRadToDeg, std::nullopt};
    return isValid(result) ? std::optional<GeoCoordinate>(result) : std::nullopt;
}

int suggestedJapanPlaneZone(const GeoCoordinate& coordinate) {
    if (!isValid(coordinate)) return 9;
    int best = 9;
    double bestDistance = std::numeric_limits<double>::max();
    for (const auto& zone : kZones) {
        const double dLat = coordinate.latitudeDegrees - zone.latitudeOfOriginDegrees;
        const double dLon = coordinate.longitudeDegrees - zone.centralMeridianDegrees;
        const double distance = dLat * dLat + dLon * dLon;
        if (distance < bestDistance) { bestDistance = distance; best = zone.zoneNumber; }
    }
    return best;
}

} // namespace hephaiston::geospatial
