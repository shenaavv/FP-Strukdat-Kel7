#define CPPHTTPLIB_OPENSSL_SUPPORT
#include "httplib.h"
#include "json.hpp"
#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <cmath>
#include <limits>
#include <algorithm>
#include <iomanip>
#include <stdexcept>
// Include necessary headers at the top of your file
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
using namespace std;

// Define route types
enum RouteType {
    FASTEST,
    SHORTEST,
    AVOID_TOLLS,
    SCENIC
};

class TollInfo {
public:
    string name;
    string operator_name;
    double cost;
    string currency;
    
    TollInfo(const string& n, const string& op, double c, const string& curr = "IDR")
        : name(n), operator_name(op), cost(c), currency(curr) {}
};

class Location {
public:
    string name;
    double lat;
    double lon;
    
    Location(const string& n, double latitude, double longitude) 
        : name(n), lat(latitude), lon(longitude) {}
    
    Location() : lat(0), lon(0) {}
    
    static string extractCityName(const string& fullDisplayName) {
        size_t firstComma = fullDisplayName.find(',');
        if (firstComma != string::npos) {
            return fullDisplayName.substr(0, firstComma);
        }
        return fullDisplayName;
    }
};

class Node {
public:
    string id;
    Location location;
    vector<pair<string, double>> edges;
    
    Node(const string& nodeId, const Location& loc) 
        : id(nodeId), location(loc) {}
    
    Node() {}
};

class WaypointDatabase {
private:
    unordered_map<string, unordered_map<string, vector<string>>> areaNames;
    
public:
    WaypointDatabase() {
        areaNames["Surabaya"]["Sidoarjo"] = {"Waru", "Gedangan"};
        areaNames["Surabaya"]["Gresik"] = {"Tandes", "Margomulyo", "Benowo"};
        areaNames["Surabaya"]["Malang"] = {"Waru", "Sidoarjo", "Porong", "Pandaan", "Lawang"};
        areaNames["Surabaya"]["Mojokerto"] = {"Sepanjang", "Krian", "Mojosari"};
        areaNames["Surabaya"]["Lamongan"] = {"Gresik", "Cerme", "Duduk Sampeyan"};
        areaNames["Surabaya"]["Probolinggo"] = {"Sidoarjo", "Pasuruan", "Kraksaan"};
        areaNames["Jakarta"]["Bogor"] = {"Depok", "Citayam", "Cibinong"};
        areaNames["Jakarta"]["Bekasi"] = {"Cakung", "Tambun", "Cikarang"};
        areaNames["Jakarta"]["Tangerang"] = {"Kalideres", "Batu Ceper", "Tanah Tinggi"};
        areaNames["Jakarta"]["Bandung"] = {"Bekasi", "Cikampek", "Purwakarta", "Padalarang"};
        areaNames["Yogyakarta"]["Solo"] = {"Klaten", "Prambanan", "Kartasura"};
        areaNames["Yogyakarta"]["Semarang"] = {"Magelang", "Secang", "Ambarawa", "Ungaran"};
        areaNames["Bandung"]["Cimahi"] = {"Pasteur", "Cihampelas", "Lembang"};
        areaNames["Bandung"]["Jakarta"] = {"Padalarang", "Purwakarta", "Cikampek", "Bekasi"};
    }
    
    string getWaypointName(const string& startCity, const string& endCity, int waypointIndex) {
        auto it1 = areaNames.find(startCity);
        if (it1 != areaNames.end()) {
            auto it2 = it1->second.find(endCity);
            if (it2 != it1->second.end()) {
                const auto& areas = it2->second;
                if (waypointIndex < areas.size()) {
                    return areas[waypointIndex];
                }
            }
        }
        
        it1 = areaNames.find(endCity);
        if (it1 != areaNames.end()) {
            auto it2 = it1->second.find(startCity);
            if (it2 != it1->second.end()) {
                int reversedIndex = it2->second.size() - 1 - waypointIndex;
                if (reversedIndex >= 0 && reversedIndex < it2->second.size()) {
                    return it2->second[reversedIndex];
                }
            }
        }
        
        return startCity + " to " + endCity + " (via Area " + to_string(waypointIndex + 1) + ")";
    }
};

class IntermediateLocationDB {
private:
    unordered_map<string, unordered_map<string, vector<pair<string, pair<double, double>>>>> locations;
    
public:
    IntermediateLocationDB() {
        locations["Bondowoso"]["Kota Malang"] = {
            {"Kabupaten Situbondo", {-7.7052, 113.9931}},
            {"Kabupaten Jember", {-8.1845, 113.6681}},
            {"Kabupaten Lumajang", {-8.1182, 113.2226}},
            {"Kabupaten Probolinggo", {-7.7764, 113.2012}},
            {"Pandaan", {-7.6488, 112.6858}}
        };
        
        locations["Surabaya"]["Malang"] = {
            {"Sidoarjo", {-7.4458, 112.7183}},
            {"Porong", {-7.5461, 112.6744}},
            {"Pandaan", {-7.6488, 112.6858}},
            {"Lawang", {-7.8652, 112.6955}}
        };
        
        locations["Jakarta"]["Bandung"] = {
            {"Bekasi", {-6.2349, 106.9924}},
            {"Karawang", {-6.3227, 107.3376}},
            {"Purwakarta", {-6.5569, 107.4494}},
            {"Padalarang", {-6.8428, 107.4746}}
        };
    }
    
    vector<pair<string, pair<double, double>>> getIntermediates(
            const string& startCity, const string& endCity) {
        
        auto it1 = locations.find(startCity);
        if (it1 != locations.end()) {
            auto it2 = it1->second.find(endCity);
            if (it2 != it1->second.end()) {
                return it2->second;
            }
        }
        
        it1 = locations.find(endCity);
        if (it1 != locations.end()) {
            auto it2 = it1->second.find(startCity);
            if (it2 != it1->second.end()) {
                vector<pair<string, pair<double, double>>> reversed = it2->second;
                reverse(reversed.begin(), reversed.end());
                return reversed;
            }
        }
        
        return {};
    }
};

class RoadDatabase {
private:
    unordered_map<string, unordered_map<string, vector<string>>> detailedRoads;
    
public:
    RoadDatabase() {
        detailedRoads["Surabaya"]["Gresik"] = {
            "Jalan Tandes", "Jalan Margomulyo", "Jalan Tambak Osowilangun", 
            "Jalan Gresik", "Jalan KH. Abdul Karim"
        };
        
        detailedRoads["Surabaya"]["Sidoarjo"] = {
            "Jalan Ahmad Yani", "Jalan Jenggolo", "Jalan Waru", 
            "Jalan Gedangan", "Jalan Diponegoro"
        };
        
        detailedRoads["Surabaya"]["Malang"] = {
            "Jalan Ahmad Yani", "Tol Waru-Sidoarjo", "Tol Sidoarjo-Porong", 
            "Tol Porong-Pandaan", "Tol Pandaan-Malang", "Jalan Raya Malang"
        };
        
        detailedRoads["Surabaya"]["Mojokerto"] = {
            "Jalan Raya Mastrip", "Tol Surabaya-Mojokerto", 
            "Jalan Jayanegara", "Jalan Pemuda"
        };
        
        detailedRoads["Surabaya"]["Probolinggo"] = {
            "Jalan Ahmad Yani", "Tol Waru-Sidoarjo", "Tol Porong-Pandaan", 
            "Jalan Raya Pasuruan", "Jalan Raya Probolinggo"
        };
        
        detailedRoads["Jakarta"]["Bogor"] = {
            "Jalan TB Simatupang", "Jalan Raya Pasar Minggu", "Tol Jagorawi", 
            "Jalan Pajajaran", "Jalan Raya Bogor"
        };
        
        detailedRoads["Jakarta"]["Bandung"] = {
            "Jalan Gatot Subroto", "Tol Jakarta-Cikampek", "Tol Cipularang", 
            "Tol Padalarang-Cileunyi", "Jalan Pasteur", "Jalan Asia Afrika"
        };
        
        detailedRoads["Jakarta"]["Bekasi"] = {
            "Jalan Kalimalang", "Jalan Raya Bekasi", "Jalan Ahmad Yani Bekasi"
        };
        
        detailedRoads["Bandung"]["Cimahi"] = {
            "Jalan Sukajadi", "Jalan Dr. Djunjunan", "Jalan Pasteur", 
            "Jalan Cihampelas", "Jalan Cimahi"
        };
        
        detailedRoads["Yogyakarta"]["Solo"] = {
            "Jalan Solo", "Jalan Ring Road Timur", "Jalan Prambanan", 
            "Jalan Yogya-Klaten", "Jalan Slamet Riyadi"
        };
        
        detailedRoads["Surabaya"]["Malang-NoToll"] = {
            "Jalan Ahmad Yani", "Jalan Trosobo", "Jalan Raya Sidoarjo", 
            "Jalan Raya Porong", "Jalan Raya Gempol", "Jalan Raya Bangil", 
            "Jalan Raya Lawang", "Jalan Raya Singosari", "Jalan Raya Malang"
        };
        
        detailedRoads["Jakarta"]["Bandung-NoToll"] = {
            "Jalan Raya Bogor", "Jalan Raya Puncak", "Jalan Raya Cianjur", 
            "Jalan Raya Padalarang", "Jalan Raya Cimahi", "Jalan Pasteur"
        };
        
        detailedRoads["Jakarta"]["Bandung-Scenic"] = {
            "Jalan Raya Puncak", "Panorama Puncak", "Kebun Teh Puncak", 
            "Jalan Wisata Cianjur", "Jalur Pemandangan Lembang", "Jalan Setiabudhi"
        };
        
        detailedRoads["Yogyakarta"]["Solo-Scenic"] = {
            "Jalan Kaliurang", "Jalur Gunung Merapi", "Panorama Selo", 
            "Jalur Wisata Tawangmangu", "Jalan Karanganyar"
        };

        detailedRoads["Bondowoso"]["Kota Malang"] = {
            "Jalan Raya Bondowoso-Situbondo", "Jalan Raya Situbondo-Besuki", 
            "Jalan Raya Besuki-Jember", "Jalan PB. Sudirman Jember", 
            "Jalan Raya Tanggul", "Jalan Raya Lumajang", "Jalan Raya Pronojiwo",
            "Jalan Tol Malang-Pandaan", "Jalan Raya Karanglo", "Jalan Ahmad Yani Malang"
        };

        detailedRoads["Bondowoso"]["Kota Malang-NoToll"] = {
            "Jalan Raya Bondowoso-Situbondo", "Jalan Raya Situbondo-Besuki", 
            "Jalan Raya Besuki-Jember", "Jalan PB. Sudirman Jember", 
            "Jalan Raya Tanggul", "Jalan Raya Lumajang", "Jalan Raya Pronojiwo",
            "Jalan Raya Tumpang", "Jalan Raya Wendit", "Jalan Letjen S. Parman Malang"
        };

        detailedRoads["Jember"]["Kota Malang"] = {
            "Jalan PB. Sudirman Jember", "Jalan Raya Tanggul", "Jalan Raya Lumajang",
            "Jalan Raya Candipuro", "Jalan Raya Turen", "Jalan Raya Kepanjen", 
            "Jalan Raya Gadang", "Jalan Ahmad Yani Malang"
        };

        detailedRoads["Surabaya"]["Jember"] = {
            "Jalan Ahmad Yani", "Tol Waru-Sidoarjo", "Tol Sidoarjo-Porong",
            "Jalan Raya Pasuruan", "Jalan Raya Probolinggo", "Jalan Raya Situbondo",
            "Jalan PB. Sudirman Jember"
        };

        detailedRoads["Surabaya"]["Banyuwangi"] = {
            "Jalan Ahmad Yani", "Tol Waru-Sidoarjo", "Tol Sidoarjo-Porong",
            "Jalan Raya Pasuruan", "Jalan Raya Probolinggo", "Jalan Raya Situbondo",
            "Jalan Raya Ketapang", "Jalan Ikan Dorang Banyuwangi"
        };
    }
    
    vector<string> generateDetailedRoadNames(
        const string& startCity, 
        const string& endCity, 
        const vector<string>& intermediates,
        RouteType routeType) {
        
        vector<string> roads;
        string prevCity = startCity;
        
        for (const auto& city : intermediates) {
            roads.push_back("Jalan Raya " + prevCity + "-" + city);
            roads.push_back("Jalan Masuk " + city);
            if (roads.size() % 3 == 0 && routeType != AVOID_TOLLS) {
                roads.push_back("Tol " + prevCity + "-" + city);
            }
            prevCity = city;
        }
        
        roads.push_back("Jalan Raya " + prevCity + "-" + endCity);
        roads.push_back("Jalan Utama " + endCity);
        
        return roads;
    }
    
    vector<string> getRoadNames(const string& startCity, const string& endCity, RouteType routeType) {
        string routeKey = endCity;
        
        if (routeType == AVOID_TOLLS) {
            routeKey = endCity + "-NoToll";
            if (detailedRoads[startCity].find(routeKey) == detailedRoads[startCity].end()) {
                routeKey = endCity;
            }
        } else if (routeType == SCENIC) {
            routeKey = endCity + "-Scenic";
            if (detailedRoads[startCity].find(routeKey) == detailedRoads[startCity].end()) {
                routeKey = endCity;
            }
        }
        
        auto it1 = detailedRoads.find(startCity);
        if (it1 != detailedRoads.end()) {
            auto it2 = it1->second.find(routeKey);
            if (it2 != it1->second.end()) {
                if (routeType == AVOID_TOLLS && routeKey == endCity) {
                    vector<string> noTollRoads;
                    for (const auto& road : it2->second) {
                        if (road.find("Tol") == string::npos) {
                            noTollRoads.push_back(road);
                        } else {
                            noTollRoads.push_back("Jalan Alternatif " + road.substr(4));
                        }
                    }
                    return noTollRoads;
                }
                return it2->second;
            }
        }
        
        it1 = detailedRoads.find(endCity);
        if (it1 != detailedRoads.end()) {
            string reverseRouteKey = startCity;
            if (routeType == AVOID_TOLLS) {
                reverseRouteKey = startCity + "-NoToll";
                if (it1->second.find(reverseRouteKey) == it1->second.end()) {
                    reverseRouteKey = startCity;
                }
            } else if (routeType == SCENIC) {
                reverseRouteKey = startCity + "-Scenic";
                if (it1->second.find(reverseRouteKey) == it1->second.end()) {
                    reverseRouteKey = startCity;
                }
            }
            
            auto it2 = it1->second.find(reverseRouteKey);
            if (it2 != it1->second.end()) {
                vector<string> reversedRoads = it2->second;
                reverse(reversedRoads.begin(), reversedRoads.end());
                
                if (routeType == AVOID_TOLLS && reverseRouteKey == startCity) {
                    vector<string> noTollRoads;
                    for (const auto& road : reversedRoads) {
                        if (road.find("Tol") == string::npos) {
                            noTollRoads.push_back(road);
                        } else {
                            noTollRoads.push_back("Jalan Alternatif " + road.substr(4));
                        }
                    }
                    return noTollRoads;
                }
                return reversedRoads;
            }
        }
        
        vector<string> fallbackRoads;
        fallbackRoads.push_back("Jalan Raya " + startCity);
        fallbackRoads.push_back("Jalan Penghubung " + startCity + "-" + endCity);
        fallbackRoads.push_back("Jalan Utama " + endCity);
        return fallbackRoads;
    }
};

class RouteUtils {
public:
// Add after RouteUtils class

// File operations utility class
class FileIO {
public:
    static bool saveToFile(const string& filename, const string& content) {
        ofstream file(filename);
        if (!file.is_open()) {
            return false;
        }
        file << content;
        file.close();
        return true;
    }

    static string loadFromFile(const string& filename) {
        ifstream file(filename);
        if (!file.is_open()) {
            return "";
        }
        
        stringstream buffer;
        buffer << file.rdbuf();
        file.close();
        return buffer.str();
    }
};

// Custom location management class
class LocationManager {
private:
    unordered_map<string, Location> userLocations;
    const string LOCATIONS_FILE = "user_locations.csv";

public:
    LocationManager() {
        loadLocations();
    }
    
    bool addLocation(const string& name, double latitude, double longitude) {
        if (userLocations.find(name) != userLocations.end()) {
            return false; // Location already exists
        }
        
        userLocations[name] = Location(name, latitude, longitude);
        saveLocations();
        return true;
    }
    
    bool updateLocation(const string& name, double latitude, double longitude) {
        if (userLocations.find(name) == userLocations.end()) {
            return false; // Location doesn't exist
        }
        
        userLocations[name] = Location(name, latitude, longitude);
        saveLocations();
        return true;
    }
    
    bool deleteLocation(const string& name) {
        if (userLocations.find(name) == userLocations.end()) {
            return false; // Location doesn't exist
        }
        
        userLocations.erase(name);
        saveLocations();
        return true;
    }
    
    Location getLocation(const string& name) const {
        auto it = userLocations.find(name);
        if (it != userLocations.end()) {
            return it->second;
        }
        return Location();
    }
    
    vector<string> getAllLocationNames() const {
        vector<string> names;
        for (const auto& pair : userLocations) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void displayAllLocations() const {
        cout << "Your saved locations:" << endl;
        cout << "====================================================================================\n";
        cout << left << setw(30) << "Name" << setw(20) << "Latitude" << setw(20) << "Longitude" << endl;
        cout << "------------------------------------------------------------------------------------\n";
        
        for (const auto& pair : userLocations) {
            cout << left << setw(30) << pair.first << setw(20) << pair.second.lat << setw(20) << pair.second.lon << endl;
        }
        cout << "====================================================================================\n";
    }
    
    bool saveLocations() const {
        stringstream content;
        content << "name,latitude,longitude\n";
        
        for (const auto& pair : userLocations) {
            content << pair.first << "," << pair.second.lat << "," << pair.second.lon << "\n";
        }
        
        return FileIO::saveToFile(LOCATIONS_FILE, content.str());
    }
    
    bool loadLocations() {
        string content = FileIO::loadFromFile(LOCATIONS_FILE);
        if (content.empty()) {
            return false;
        }
        
        userLocations.clear();
        stringstream contentStream(content);
        string line;
        
        // Skip header
        getline(contentStream, line);
        
        while (getline(contentStream, line)) {
            stringstream lineStream(line);
            string name;
            string latStr, lonStr;
            
            getline(lineStream, name, ',');
            getline(lineStream, latStr, ',');
            getline(lineStream, lonStr, ',');
            
            try {
                double lat = stod(latStr);
                double lon = stod(lonStr);
                userLocations[name] = Location(name, lat, lon);
            } catch (const exception& e) {
                cerr << "Error parsing location data: " << e.what() << endl;
            }
        }
        
        return true;
    }
};

// Custom route management class
class RouteManager {
private:
    struct SavedRoute {
        string name;
        string startLocationName;
        string endLocationName;
        RouteType routeType;
        vector<string> waypoints;
    };
    
    unordered_map<string, SavedRoute> userRoutes;
    const string ROUTES_FILE = "user_routes.csv";

public:
    RouteManager() {
        loadRoutes();
    }
    
    bool addRoute(const string& name, const string& startLocation, const string& endLocation, 
                  RouteType routeType, const vector<string>& waypoints) {
        if (userRoutes.find(name) != userRoutes.end()) {
            return false; // Route already exists
        }
        
        userRoutes[name] = {name, startLocation, endLocation, routeType, waypoints};
        saveRoutes();
        return true;
    }
    
    bool updateRoute(const string& name, const string& startLocation, const string& endLocation, 
                     RouteType routeType, const vector<string>& waypoints) {
        if (userRoutes.find(name) == userRoutes.end()) {
            return false; // Route doesn't exist
        }
        
        userRoutes[name] = {name, startLocation, endLocation, routeType, waypoints};
        saveRoutes();
        return true;
    }
    
    bool deleteRoute(const string& name) {
        if (userRoutes.find(name) == userRoutes.end()) {
            return false; // Route doesn't exist
        }
        
        userRoutes.erase(name);
        saveRoutes();
        return true;
    }
    
    SavedRoute getRoute(const string& name) const {
        auto it = userRoutes.find(name);
        if (it != userRoutes.end()) {
            return it->second;
        }
        return {"", "", "", FASTEST, {}};
    }
    
    vector<string> getAllRouteNames() const {
        vector<string> names;
        for (const auto& pair : userRoutes) {
            names.push_back(pair.first);
        }
        return names;
    }
    
    void displayAllRoutes() const {
        cout << "Your saved routes:" << endl;
        cout << "====================================================================================\n";
        cout << left << setw(20) << "Name" << setw(20) << "From" << setw(20) << "To" << setw(15) << "Route Type" << endl;
        cout << "------------------------------------------------------------------------------------\n";
        
        for (const auto& pair : userRoutes) {
            const auto& route = pair.second;
            string routeTypeStr;
            switch (route.routeType) {
                case FASTEST: routeTypeStr = "Fastest"; break;
                case SHORTEST: routeTypeStr = "Shortest"; break;
                case AVOID_TOLLS: routeTypeStr = "No Tolls"; break;
                case SCENIC: routeTypeStr = "Scenic"; break;
            }
            
            cout << left << setw(20) << route.name << setw(20) << route.startLocationName 
                 << setw(20) << route.endLocationName << setw(15) << routeTypeStr << endl;
        }
        cout << "====================================================================================\n";
    }
    
    bool saveRoutes() const {
        stringstream content;
        content << "name,startLocation,endLocation,routeType,waypoints\n";
        
        for (const auto& pair : userRoutes) {
            const auto& route = pair.second;
            content << route.name << "," << route.startLocationName << "," << route.endLocationName << "," 
                    << static_cast<int>(route.routeType) << ",";
            
            // Save waypoints
            for (size_t i = 0; i < route.waypoints.size(); i++) {
                content << route.waypoints[i];
                if (i < route.waypoints.size() - 1) {
                    content << "|";
                }
            }
            content << "\n";
        }
        
        return FileIO::saveToFile(ROUTES_FILE, content.str());
    }
    
    bool loadRoutes() {
        string content = FileIO::loadFromFile(ROUTES_FILE);
        if (content.empty()) {
            return false;
        }
        
        userRoutes.clear();
        stringstream contentStream(content);
        string line;
        
        // Skip header
        getline(contentStream, line);
        
        while (getline(contentStream, line)) {
            stringstream lineStream(line);
            string name, startLoc, endLoc, routeTypeStr, waypointsStr;
            
            getline(lineStream, name, ',');
            getline(lineStream, startLoc, ',');
            getline(lineStream, endLoc, ',');
            getline(lineStream, routeTypeStr, ',');
            getline(lineStream, waypointsStr);
            
            try {
                SavedRoute route;
                route.name = name;
                route.startLocationName = startLoc;
                route.endLocationName = endLoc;
                route.routeType = static_cast<RouteType>(stoi(routeTypeStr));
                
                // Parse waypoints
                stringstream wpStream(waypointsStr);
                string waypoint;
                while (getline(wpStream, waypoint, '|')) {
                    route.waypoints.push_back(waypoint);
                }
                
                userRoutes[name] = route;
            } catch (const exception& e) {
                cerr << "Error parsing route data: " << e.what() << endl;
            }
        }
        
        return true;
    }
};

// Graph visualization class
class GraphVisualizer {
public:
    static string visualizeGraph(const unordered_map<string, Node>& graph) {
        stringstream ss;
        ss << "\nGraph Visualization:\n";
        ss << "====================================================================================\n";
        
        // Collect all nodes first
        vector<string> nodeIds;
        for (const auto& node : graph) {
            nodeIds.push_back(node.first);
        }
        
        // Sort node IDs for consistent output
        sort(nodeIds.begin(), nodeIds.end());
        
        for (const string& nodeId : nodeIds) {
            const auto& node = graph.at(nodeId);
            string nodeName = !node.location.name.empty() ? node.location.name : nodeId;
            
            ss << "Node: " << nodeName << " (" << nodeId << ")\n";
            ss << "  Location: (" << node.location.lat << ", " << node.location.lon << ")\n";
            ss << "  Connections:\n";
            
            if (node.edges.empty()) {
                ss << "    None\n";
            } else {
                // Sort edges for consistent output
                vector<pair<string, double>> sortedEdges = node.edges;
                sort(sortedEdges.begin(), sortedEdges.end(), 
                     [](const pair<string, double>& a, const pair<string, double>& b) {
                         return a.first < b.first;
                     });
                
                for (const auto& edge : sortedEdges) {
                    string targetName;
                    try {
                        targetName = graph.at(edge.first).location.name;
                        if (targetName.empty()) {
                            targetName = edge.first;
                        }
                    } catch (...) {
                        targetName = edge.first;
                    }
                    ss << "    -> " << targetName << " (" << edge.first << "): " 
                       << fixed << setprecision(2) << edge.second << " km\n";
                }
            }
            ss << "\n";
        }
        
        ss << "====================================================================================\n";
        return ss.str();
    }
    
    static string visualizePathInGraph(
        const unordered_map<string, Node>& graph,
        const vector<string>& path) {
        
        if (path.empty()) return "Empty path, nothing to visualize.";
        
        stringstream ss;
        ss << "\nPath Visualization:\n";
        ss << "====================================================================================\n";
        
        for (size_t i = 0; i < path.size(); i++) {
            const string& nodeId = path[i];
            string nodeName;
            try {
                nodeName = graph.at(nodeId).location.name;
                if (nodeName.empty()) nodeName = nodeId;
            } catch (...) {
                nodeName = nodeId;
            }
            
            ss << nodeName;
            
            if (i < path.size() - 1) {
                double distance = 0;
                try {
                    const auto& currentNode = graph.at(nodeId);
                    for (const auto& edge : currentNode.edges) {
                        if (edge.first == path[i + 1]) {
                            distance = edge.second;
                            break;
                        }
                    }
                    
                    ss << " ==(" << fixed << setprecision(2) << distance << " km)==> ";
                } catch (...) {
                    ss << " ===> ";
                }
            }
        }
        
        ss << "\n====================================================================================\n";
        return ss.str();
    }
    
    static void saveGraphVisualization(const unordered_map<string, Node>& graph, const string& filename) {
        string visualization = visualizeGraph(graph);
        FileIO::saveToFile(filename, visualization);
    }
};
    static double calculateDistance(double lat1, double lon1, double lat2, double lon2) {
        const double R = 6371.0;
        double dLat = (lat2 - lat1) * M_PI / 180.0;
        double dLon = (lon2 - lon1) * M_PI / 180.0;
        
        double a = sin(dLat/2) * sin(dLat/2) +
                   cos(lat1 * M_PI / 180.0) * cos(lat2 * M_PI / 180.0) *
                   sin(dLon/2) * sin(dLon/2);
        double c = 2 * atan2(sqrt(a), sqrt(1-a));
        return R * c;
    }

    static double getAccurateDistance(const Location& start, const Location& end) {
        static auto cityDistances = getAccurateCityDistances();
        
        string startCity = Location::extractCityName(start.name);
        string endCity = Location::extractCityName(end.name);
        
        auto it1 = cityDistances.find(startCity);
        if (it1 != cityDistances.end()) {
            auto it2 = it1->second.find(endCity);
            if (it2 != it1->second.end()) {
                return it2->second;
            }
        }
        
        return calculateDistance(start.lat, start.lon, end.lat, end.lon);
    }

    static string cleanRouteSymbols(const string& routeText) {
        string cleanedText = routeText;
        const vector<string> arrowSymbols = {"ΓåÆ", "Γå†", "→"};
        const string asciiArrow = " -> ";
        
        for (const string& symbol : arrowSymbols) {
            size_t pos = 0;
            while((pos = cleanedText.find(symbol, pos)) != string::npos) {
                cleanedText.replace(pos, symbol.length(), asciiArrow);
                pos += asciiArrow.length();
            }
        }
        return cleanedText;
    }

    static string formatCurrency(double amount, const string& currency) {
        stringstream ss;
        ss << fixed << setprecision(0);
        
        if (currency == "IDR") {
            ss << "Rp " << amount;
        } else if (currency == "USD") {
            ss << "$" << amount;
        } else if (currency == "EUR") {
            ss << "€" << amount;
        } else {
            ss << amount << " " << currency;
        }
        return ss.str();
    }

private:
    static unordered_map<string, unordered_map<string, double>> getAccurateCityDistances() {
        unordered_map<string, unordered_map<string, double>> distances;
        distances["Surabaya"]["Gresik"] = 32.5;
        distances["Gresik"]["Surabaya"] = 32.5;
        distances["Surabaya"]["Sidoarjo"] = 25.0;
        distances["Sidoarjo"]["Surabaya"] = 25.0;
        distances["Surabaya"]["Malang"] = 95.0;
        distances["Malang"]["Surabaya"] = 95.0;
        distances["Surabaya"]["Mojokerto"] = 50.0;
        distances["Mojokerto"]["Surabaya"] = 50.0;
        distances["Surabaya"]["Lamongan"] = 47.0;
        distances["Lamongan"]["Surabaya"] = 47.0;
        distances["Surabaya"]["Probolinggo"] = 76.0;
        distances["Probolinggo"]["Surabaya"] = 76.0;
        distances["Jakarta"]["Bogor"] = 59.0;
        distances["Bogor"]["Jakarta"] = 59.0;
        distances["Jakarta"]["Bekasi"] = 28.0;
        distances["Bekasi"]["Jakarta"] = 28.0;
        distances["Jakarta"]["Tangerang"] = 25.0;
        distances["Tangerang"]["Jakarta"] = 25.0;
        distances["Jakarta"]["Bandung"] = 151.0;
        distances["Bandung"]["Jakarta"] = 151.0;
        distances["Yogyakarta"]["Solo"] = 65.0;
        distances["Solo"]["Yogyakarta"] = 65.0;
        distances["Yogyakarta"]["Semarang"] = 110.0;
        distances["Semarang"]["Yogyakarta"] = 110.0;
        distances["Bandung"]["Cimahi"] = 15.0;
        distances["Cimahi"]["Bandung"] = 15.0;
        return distances;
    }
};

class Geocoder {
public:
    Location geocodeLocation(const string& locationName) {
        try {
            httplib::Client cli("https://nominatim.openstreetmap.org");
            httplib::Headers headers = {{"User-Agent", "MapsPathfinder/1.0"}};
            
            auto res = cli.Get("/search?q=" + httplib::detail::encode_url(locationName) + 
                            "&format=json&limit=1", headers);
            
            if (!res) {
                cout << "Error: Network connection failed when geocoding." << endl;
                return Location();
            }
            
            if (res->status != 200) {
                cout << "Error: Geocoding API returned status code " << res->status << endl;
                return Location();
            }
            
            auto json = nlohmann::json::parse(res->body);
            if (!json.empty()) {
                double lat = stod(json[0]["lat"].get<string>());
                double lon = stod(json[0]["lon"].get<string>());
                string fullName = json[0]["display_name"].get<string>();
                string cityName = Location::extractCityName(fullName);
                return Location(cityName, lat, lon);
            } else {
                throw runtime_error("Location not found");
            }
        } catch (const exception& e) {
            cerr << "Geocoding error: " << e.what() << endl;
            return Location();
        }
    }
};

class RouteFinder {
private:
    WaypointDatabase waypointDB;
    IntermediateLocationDB intermediateDB;
    RoadDatabase roadDB;

public:
    vector<string> findBestFirstPath(
        const unordered_map<string, Node>& graph,
        const string& startId,
        const string& endId) {
        
        priority_queue<pair<double, string>, 
                      vector<pair<double, string>>,
                      greater<pair<double, string>>> openSet;
        
        unordered_map<string, string> cameFrom;
        unordered_set<string> closedSet;
        
        try {
            openSet.push(make_pair(
                RouteUtils::getAccurateDistance(
                    graph.at(startId).location,
                    graph.at(endId).location
                ), 
                startId
            ));
            
            while (!openSet.empty()) {
                string current = openSet.top().second;
                openSet.pop();
                
                if (current == endId) {
                    vector<string> path;
                    while (current != startId) {
                        path.push_back(current);
                        current = cameFrom[current];
                    }
                    path.push_back(startId);
                    reverse(path.begin(), path.end());
                    return path;
                }
                
                closedSet.insert(current);
                
                for (const auto& neighbor : graph.at(current).edges) {
                    string neighborId = neighbor.first;
                    
                    if (closedSet.count(neighborId) > 0) {
                        continue;
                    }
                    
                    double heuristic = RouteUtils::getAccurateDistance(
                        graph.at(neighborId).location,
                        graph.at(endId).location
                    );
                    
                    if (cameFrom.find(neighborId) == cameFrom.end()) {
                        cameFrom[neighborId] = current;
                        openSet.push(make_pair(heuristic, neighborId));
                    }
                }
            }
        } catch (const exception& e) {
            cerr << "Error in findBestFirstPath: " << e.what() << endl;
        }
        
        return {};
    }

    vector<string> findShortestPath(
        const unordered_map<string, Node>& graph,
        const string& startId,
        const string& endId) {
        
        try {
            priority_queue<pair<double, string>, 
                                vector<pair<double, string>>,
                                greater<pair<double, string>>> openSet;
            
            unordered_map<string, double> gScore;
            unordered_map<string, string> cameFrom;
            
            for (const auto& node : graph) {
                gScore[node.first] = numeric_limits<double>::infinity();
            }
            
            gScore[startId] = 0;
            openSet.push(make_pair(
                RouteUtils::getAccurateDistance(
                    graph.at(startId).location,
                    graph.at(endId).location
                ), 
                startId
            ));
            
            while (!openSet.empty()) {
                string current = openSet.top().second;
                openSet.pop();
                
                if (current == endId) {
                    vector<string> path;
                    while (current != startId) {
                        path.push_back(current);
                        current = cameFrom[current];
                    }
                    path.push_back(startId);
                    reverse(path.begin(), path.end());
                    return path;
                }
                
                for (const auto& neighbor : graph.at(current).edges) {
                    string neighborId = neighbor.first;
                    double distance = neighbor.second;
                    
                    double tentativeGScore = gScore[current] + distance;
                    
                    if (tentativeGScore < gScore[neighborId]) {
                        cameFrom[neighborId] = current;
                        gScore[neighborId] = tentativeGScore;
                        
                        double heuristic = RouteUtils::getAccurateDistance(
                            graph.at(neighborId).location,
                            graph.at(endId).location
                        );
                        
                        double fScore = tentativeGScore + heuristic;
                        openSet.push(make_pair(fScore, neighborId));
                    }
                }
            }
        } catch (const exception& e) {
            cerr << "Error in findShortestPath: " << e.what() << endl;
        }
        
        return {};
    }

    void addIntermediateLocationsToGraph(
        unordered_map<string, Node>& graph,
        const string& startId,
        const string& endId,
        const Location& startLocation,
        const Location& endLocation,
        const string& startCity,
        const string& endCity) {
        
        auto intermediates = intermediateDB.getIntermediates(startCity, endCity);
        if (intermediates.empty()) return;
        
        for (size_t i = 0; i < intermediates.size(); i++) {
            const auto& [name, coords] = intermediates[i];
            const auto& [lat, lon] = coords;
            
            string nodeId = "intermediate_" + to_string(i);
            Location loc(name, lat, lon);
            graph[nodeId] = Node(nodeId, loc);
            
            if (i == 0) {
                double dist = RouteUtils::getAccurateDistance(startLocation, loc);
                graph[startId].edges.push_back(make_pair(nodeId, dist));
            } else {
                string prevId = "intermediate_" + to_string(i-1);
                double dist = RouteUtils::getAccurateDistance(graph[prevId].location, loc);
                graph[prevId].edges.push_back(make_pair(nodeId, dist));
            }
            
            if (i == intermediates.size() - 1) {
                double dist = RouteUtils::getAccurateDistance(loc, endLocation);
                graph[nodeId].edges.push_back(make_pair(endId, dist));
            }
        }
    }

    vector<vector<string>> generateMultipleRoutes(
        unordered_map<string, Node>& graph,
        const string& startId,
        const string& endId,
        const Location& startLocation,
        const Location& endLocation,
        RouteType routeType = FASTEST) {
        
        vector<vector<string>> routes;
        
        try {
            vector<string> directPath = findBestFirstPath(graph, startId, endId);
            if (!directPath.empty()) {
                routes.push_back(directPath);
            }
            
            vector<Location> intermediateLocations;
            string startCity = Location::extractCityName(startLocation.name);
            string endCity = Location::extractCityName(endLocation.name);
            bool foundKnownRoute = false;
            
            static unordered_map<string, unordered_map<string, vector<string>>> knownIntermediates = {
                {"Surabaya", {
                    {"Gresik", {"Tandes", "Benowo"}}, 
                    {"Sidoarjo", {"Wonokromo", "Waru"}},
                    {"Malang", {"Sidoarjo", "Pandaan", "Lawang"}},
                    {"Probolinggo", {"Sidoarjo", "Pasuruan", "Kraksaan"}}
                }},
                {"Jakarta", {
                    {"Bandung", {"Bekasi", "Purwakarta", "Cimahi"}},
                    {"Bogor", {"Depok", "Cibinong"}}
                }}
            };
            
            auto it1 = knownIntermediates.find(startCity);
            if (it1 != knownIntermediates.end()) {
                auto it2 = it1->second.find(endCity);
                if (it2 != it1->second.end()) {
                    foundKnownRoute = true;
                    const auto& waypoints = it2->second;
                    
                    for (size_t i = 0; i < waypoints.size(); i++) {
                        string nodeId = "known_waypoint" + to_string(i+1);
                        double ratio = (i+1.0) / (waypoints.size()+1.0);
                        double lat = startLocation.lat + (endLocation.lat - startLocation.lat) * ratio;
                        double lon = startLocation.lon + (endLocation.lon - startLocation.lon) * ratio;
                        Location wp(waypoints[i], lat, lon);
                        graph[nodeId] = Node(nodeId, wp);
                        
                        if (i == 0) {
                            double dist = RouteUtils::getAccurateDistance(startLocation, wp);
                            graph[startId].edges.push_back(make_pair(nodeId, dist));
                            dist = RouteUtils::getAccurateDistance(wp, endLocation);
                            graph[nodeId].edges.push_back(make_pair(endId, dist));
                        } else {
                            string prevId = "known_waypoint" + to_string(i);
                            double dist = RouteUtils::getAccurateDistance(graph[prevId].location, wp);
                            graph[prevId].edges.push_back(make_pair(nodeId, dist));
                            dist = RouteUtils::getAccurateDistance(wp, endLocation);
                            graph[nodeId].edges.push_back(make_pair(endId, dist));
                        }
                    }
                }
            }
            
            if (!foundKnownRoute) {
                addIntermediateLocationsToGraph(graph, startId, endId, 
                    startLocation, endLocation, startCity, endCity);
                
                bool hasIntermediates = false;
                for (const auto& edge : graph[startId].edges) {
                    if (edge.first != endId && edge.first.find("intermediate_") != string::npos) {
                        hasIntermediates = true;
                        break;
                    }
                }
                if (hasIntermediates) foundKnownRoute = true;
            }

            if (!foundKnownRoute && (routeType == AVOID_TOLLS || routeType == SCENIC)) {
                for (int i = 1; i <= 3; i++) {
                    double ratio = i / 4.0;
                    double latOffset = (i % 2 == 0) ? 0.01 : -0.01;
                    double lonOffset = (i % 2 == 0) ? -0.01 : 0.01;
                    
                    if (routeType == SCENIC) {
                        latOffset *= 2;
                        lonOffset *= 2;
                    }
                    
                    string waypointName = waypointDB.getWaypointName(
                        Location::extractCityName(startLocation.name),
                        Location::extractCityName(endLocation.name),
                        i-1
                    );
                    
                    Location intermediate(
                        waypointName,
                        startLocation.lat + (endLocation.lat - startLocation.lat) * ratio + latOffset,
                        startLocation.lon + (endLocation.lon - startLocation.lon) * ratio + lonOffset
                    );
                    intermediateLocations.push_back(intermediate);
                    
                    string nodeId = "waypoint" + to_string(i);
                    graph[nodeId] = Node(nodeId, intermediate);
                }
                
                for (size_t i = 0; i < intermediateLocations.size(); i++) {
                    string nodeId = "waypoint" + to_string(i+1);
                    graph[startId].edges.push_back(make_pair(nodeId, 
                        RouteUtils::getAccurateDistance(
                            graph[startId].location, 
                            graph[nodeId].location
                        )
                    ));
                    graph[nodeId].edges.push_back(make_pair(endId, 
                        RouteUtils::getAccurateDistance(
                            graph[nodeId].location, 
                            graph[endId].location
                        )
                    ));
                    
                    if (i > 0) {
                        string prevNodeId = "waypoint" + to_string(i);
                        graph[prevNodeId].edges.push_back(make_pair(nodeId, 
                            RouteUtils::getAccurateDistance(
                                graph[prevNodeId].location, 
                                graph[nodeId].location
                            )
                        ));
                    }
                }
            } else if (!foundKnownRoute) {
                for (int i = 1; i <= 2; i++) {
                    double ratio = i / 3.0;
                    double latOffset = (i % 2 == 0) ? 0.005 : -0.005;
                    double lonOffset = (i % 2 == 0) ? -0.005 : 0.005;
                    
                    string waypointName = waypointDB.getWaypointName(
                        Location::extractCityName(startLocation.name),
                        Location::extractCityName(endLocation.name),
                        i-1
                    );
                    
                    Location intermediate(
                        waypointName,
                        startLocation.lat + (endLocation.lat - startLocation.lat) * ratio + latOffset,
                        startLocation.lon + (endLocation.lon - startLocation.lon) * ratio + lonOffset
                    );
                    
                    string nodeId = "waypoint" + to_string(i);
                    graph[nodeId] = Node(nodeId, intermediate);
                    
                    if (i == 1) {
                        double dist = RouteUtils::getAccurateDistance(startLocation, intermediate);
                        graph[startId].edges.push_back(make_pair(nodeId, dist));
                        dist = RouteUtils::getAccurateDistance(intermediate, endLocation);
                        graph[nodeId].edges.push_back(make_pair(endId, dist));
                    } else {
                        string prevId = "waypoint" + to_string(i-1);
                        double dist = RouteUtils::getAccurateDistance(startLocation, intermediate);
                        graph[startId].edges.push_back(make_pair(nodeId, dist));
                        dist = RouteUtils::getAccurateDistance(graph[prevId].location, intermediate);
                        graph[prevId].edges.push_back(make_pair(nodeId, dist));
                        dist = RouteUtils::getAccurateDistance(intermediate, endLocation);
                        graph[nodeId].edges.push_back(make_pair(endId, dist));
                    }
                }
            }
            
            vector<string> altPath1 = findBestFirstPath(graph, startId, endId);
            if (!altPath1.empty() && (routes.empty() || altPath1 != routes[0])) {
                routes.push_back(altPath1);
            }
            
            vector<string> altPath2 = findShortestPath(graph, startId, endId);
            if (!altPath2.empty() && (routes.empty() || altPath2 != routes[0]) && 
                (routes.size() < 2 || altPath2 != routes[1])) {
                routes.push_back(altPath2);
            }
        } catch (const exception& e) {
            cerr << "Error in generateMultipleRoutes: " << e.what() << endl;
        }
        
        return routes;
    }

    vector<string> getRouteDetails(const Location& start, const Location& end, RouteType routeType = FASTEST) {
        vector<string> steps;
        string startCity = Location::extractCityName(start.name);
        string endCity = Location::extractCityName(end.name);
        
        vector<string> dbRoads = roadDB.getRoadNames(startCity, endCity, routeType);
        if (!dbRoads.empty()) {
            return dbRoads;
        }
        
        try {
            string routeParams;
            switch (routeType) {
                case FASTEST: routeParams = "steps=true&overview=full"; break;
                case SHORTEST: routeParams = "steps=true&overview=full&alternatives=false&geometries=geojson"; break;
                case AVOID_TOLLS: routeParams = "steps=true&overview=full&exclude=toll"; break;
                case SCENIC: routeParams = "steps=true&overview=full"; break;
                default: routeParams = "steps=true&overview=full";
            }
            
            string url = "router.project-osrm.org";
            string path = "/route/v1/driving/" + 
                        to_string(start.lon) + "," + to_string(start.lat) + ";" +
                        to_string(end.lon) + "," + to_string(end.lat) + 
                        "?" + routeParams;
            
            httplib::Client cli("https://" + url);
            httplib::Headers headers = {{"User-Agent", "MapsPathfinder/1.0"}};
            
            auto res = cli.Get(path.c_str(), headers);
            
            if (res && res->status == 200) {
                auto json = nlohmann::json::parse(res->body);
                auto& routes = json["routes"];
                
                if (!routes.empty()) {
                    auto& legs = routes[0]["legs"];
                    if (!legs.empty()) {
                        auto& steps_json = legs[0]["steps"];
                        for (auto& step : steps_json) {
                            string road_name = "unnamed road";
                            if (step.contains("name") && !step["name"].is_null() && !step["name"].get<string>().empty()) {
                                road_name = step["name"];
                            }
                            if (steps.empty() || road_name != steps.back()) {
                                steps.push_back(road_name);
                            }
                        }
                    }
                }
            } else {
                throw runtime_error("Failed to get route from OSRM API");
            }
        } catch (const exception& e) {
            cerr << "Error in getRouteDetails: " << e.what() << endl;
        }
        
        if (steps.empty()) {
            auto intermediates = intermediateDB.getIntermediates(startCity, endCity);
            vector<string> cityNames;
            for (const auto& item : intermediates) {
                cityNames.push_back(item.first);
            }
            
            if (!cityNames.empty()) {
                steps = roadDB.generateDetailedRoadNames(startCity, endCity, cityNames, routeType);
            } else {
                steps.push_back("Jalan Raya " + startCity);
                double distance = RouteUtils::getAccurateDistance(start, end);
                int segments = max(1, min(10, static_cast<int>(distance / 30)));
                
                for (int i = 1; i <= segments; i++) {
                    if (i % 3 == 0) {
                        steps.push_back("Jalan Lintas " + startCity + "-" + endCity + " (Segmen " + to_string(i) + ")");
                    } else if (i % 2 == 0) {
                        steps.push_back("Jalan Kabupaten " + to_string(i));
                    } else {
                        steps.push_back("Jalan Provinsi " + startCity + "-" + endCity);
                    }
                }
                steps.push_back("Jalan Masuk " + endCity);
            }
            
            if (routeType == AVOID_TOLLS) {
                vector<string> filteredSteps;
                for (const auto& step : steps) {
                    if (step.find("Tol") == string::npos) {
                        filteredSteps.push_back(step);
                    } else {
                        filteredSteps.push_back("Jalan Alternatif " + step.substr(4));
                    }
                }
                return filteredSteps;
            }
        }
        
        return steps;
    }

    vector<TollInfo> getTollInfo(const Location& start, const Location& end, RouteType routeType, 
                              double& totalCost, string& currency) {
        vector<TollInfo> tolls;
        totalCost = 0;
        currency = "IDR";
        
        if (routeType == AVOID_TOLLS) {
            return tolls;
        }
        
        string startCity = Location::extractCityName(start.name);
        string endCity = Location::extractCityName(end.name);
        
        static unordered_map<string, unordered_map<string, vector<TollInfo>>> knownTolls = {
            {"Jakarta", {
                {"Bogor", {
                    TollInfo("Jagorawi Toll Road", "Jasa Marga", 15000),
                    TollInfo("Jakarta Inner Ring Road", "Jasa Marga", 10000)
                }},
                {"Bandung", {
                    TollInfo("Cipularang Toll Road", "Jasa Marga", 35000),
                    TollInfo("Jakarta-Cikampek Toll Road", "Jasa Marga", 25000)
                }},
                {"Bekasi", {
                    TollInfo("Jakarta-Cikampek Toll Road", "Jasa Marga", 12000)
                }},
                {"Tangerang", {
                    TollInfo("Jakarta-Tangerang Toll Road", "Jasa Marga", 11000)
                }}
            }},
            {"Surabaya", {
                {"Sidoarjo", {
                    TollInfo("Surabaya-Gempol Toll Road", "Jasa Marga", 7500)
                }},
                {"Malang", {
                    TollInfo("Surabaya-Malang Toll Road", "Jasa Marga", 28000)
                }},
                {"Gresik", {
                    TollInfo("MERR Toll Road", "Jasa Marga", 7000)
                }},
                {"Mojokerto", {
                    TollInfo("Surabaya-Mojokerto Toll Road", "Jasa Marga", 17500)
                }},
                {"Probolinggo", {
                    TollInfo("Surabaya-Probolinggo Toll Road", "Jasa Marga", 25000)
                }}
            }},
            {"Bandung", {
                {"Jakarta", {
                    TollInfo("Cipularang Toll Road", "Jasa Marga", 35000),
                    TollInfo("Jakarta-Cikampek Toll Road", "Jasa Marga", 25000)
                }},
                {"Cimahi", {
                    TollInfo("Padalarang-Cileunyi Toll Road", "Jasa Marga", 8000)
                }}
            }}
        };
        
        auto it1 = knownTolls.find(startCity);
        if (it1 != knownTolls.end()) {
            auto it2 = it1->second.find(endCity);
            if (it2 != it1->second.end()) {
                tolls = it2->second;
                for (const auto& toll : tolls) {
                    totalCost += toll.cost;
                }
            }
        }
        
        if (tolls.empty()) {
            it1 = knownTolls.find(endCity);
            if (it1 != knownTolls.end()) {
                auto it2 = it1->second.find(startCity);
                if (it2 != it1->second.end()) {
                    tolls = it2->second;
                    for (const auto& toll : tolls) {
                        totalCost += toll.cost;
                    }
                }
            }
        }
        
        if (tolls.empty() && RouteUtils::getAccurateDistance(start, end) > 50) {
            double distance = RouteUtils::getAccurateDistance(start, end);
            double estimatedCost = distance * 1000;
            tolls.push_back(TollInfo("Estimated Toll", "Various Operators", estimatedCost));
            totalCost = estimatedCost;
        }
        
        return tolls;
    }

    string formatRoute(
        const vector<string>& path,
        const unordered_map<string, Node>& graph,
        RouteType routeType = FASTEST) {
        
        if (path.empty()) return "No route available";
        
        string result;
        double totalDistance = 0;
        double totalTime = 0;
        
        try {
            for (size_t i = 0; i < path.size() - 1; i++) {
                const auto& currentNode = graph.at(path[i]);
                const auto& nextNode = graph.at(path[i+1]);
                
                double segmentDistance = 0;
                for (const auto& edge : currentNode.edges) {
                    if (edge.first == path[i+1]) {
                        segmentDistance = edge.second;
                        break;
                    }
                }
                
                result += currentNode.location.name;
                vector<string> roadNames = getRouteDetails(
                    currentNode.location, nextNode.location, routeType);
                
                for (const auto& road : roadNames) {
                    result += " → " + road;
                }
                
                if (i == path.size() - 2) {
                    result += " → " + nextNode.location.name;
                }
                
                totalDistance += segmentDistance;
            }
            
            double speed = 60.0;
            switch (routeType) {
                case FASTEST: speed = 60.0; break;
                case SHORTEST: speed = 50.0; break;
                case AVOID_TOLLS: speed = 45.0; break;
                case SCENIC: speed = 40.0; break;
            }
            totalTime = (totalDistance / speed) * 60;
            
            int hours = static_cast<int>(totalTime) / 60;
            int minutes = static_cast<int>(totalTime) % 60;
            
            result += " (" + to_string(static_cast<int>(totalDistance)) + " km, ";
            if (hours > 0) {
                result += to_string(hours) + " hr ";
            }
            result += to_string(minutes) + " min)";
            
            if (routeType != AVOID_TOLLS && path.size() >= 2) {
                double tollCost = 0;
                string currency = "IDR";
                const auto& startNode = graph.at(path.front());
                const auto& endNode = graph.at(path.back());
                vector<TollInfo> tolls = getTollInfo(startNode.location, endNode.location, routeType, tollCost, currency);
                
                if (!tolls.empty()) {
                    result += "\nTotal Toll Cost: " + RouteUtils::formatCurrency(tollCost, currency);
                }
            } else if (routeType == AVOID_TOLLS) {
                result += "\nThis route avoids all toll roads.";
            }
        } catch (const exception& e) {
            cerr << "Error in formatRoute: " << e.what() << endl;
            return "Error generating route details";
        }
        
        return RouteUtils::cleanRouteSymbols(result);
    }

    string formatRouteWithRealDirections(
        const vector<string>& path,
        const unordered_map<string, Node>& graph,
        RouteType routeType = FASTEST) {
        
        if (path.empty()) return "No route available";
        
        string result;
        double totalDistance = 0;
        double totalTime = 0;
        double tollCost = 0;
        string currency = "IDR";
        vector<TollInfo> tolls;
        
        try {
            if (path.size() == 2 && path[0] == "start" && path[1] == "end") {
                const auto& startNode = graph.at(path[0]);
                const auto& endNode = graph.at(path[1]);
                vector<string> routeSteps = getRouteDetails(startNode.location, endNode.location, routeType);
                result = Location::extractCityName(startNode.location.name);
                
                for (const auto& step : routeSteps) {
                    result += " → " + step;
                }
                
                result += " → " + Location::extractCityName(endNode.location.name);
                totalDistance = RouteUtils::getAccurateDistance(startNode.location, endNode.location);
                
                if (routeType != AVOID_TOLLS) {
                    tolls = getTollInfo(startNode.location, endNode.location, routeType, tollCost, currency);
                }
            } else {
                for (size_t i = 0; i < path.size() - 1; i++) {
                    const auto& currentNode = graph.at(path[i]);
                    const auto& nextNode = graph.at(path[i+1]);
                    
                    double segmentDistance = 0;
                    for (const auto& edge : currentNode.edges) {
                        if (edge.first == path[i+1]) {
                            segmentDistance = edge.second;
                            break;
                        }
                    }
                    
                    if (i == 0) {
                        result += currentNode.location.name;
                    }
                    
                    vector<string> roadNames = getRouteDetails(
                        currentNode.location, nextNode.location, routeType);
                    
                    for (const auto& road : roadNames) {
                        result += " → " + road;
                    }
                    
                    if (i == path.size() - 2) {
                        result += " → " + nextNode.location.name;
                    }
                    
                    totalDistance += segmentDistance;
                }
                
                if (routeType != AVOID_TOLLS && path.size() >= 2) {
                    const auto& startNode = graph.at(path.front());
                    const auto& endNode = graph.at(path.back());
                    tolls = getTollInfo(startNode.location, endNode.location, routeType, tollCost, currency);
                }
            }
            
            double speed = 60.0;
            switch (routeType) {
                case FASTEST: speed = 60.0; break;
                case SHORTEST: speed = 50.0; break;
                case AVOID_TOLLS: speed = 45.0; break;
                case SCENIC: speed = 40.0; break;
            }
            totalTime = (totalDistance / speed) * 60;
            
            int hours = static_cast<int>(totalTime) / 60;
            int minutes = static_cast<int>(totalTime) % 60;
            
            result += " (" + to_string(static_cast<int>(totalDistance)) + " km, ";
            if (hours > 0) {
                result += to_string(hours) + " hr ";
            }
            result += to_string(minutes) + " min)";
            
            if (!tolls.empty()) {
                result += "\nTotal Toll Cost: " + RouteUtils::formatCurrency(tollCost, currency);
                result += "\nToll Details:";
                for (const auto& toll : tolls) {
                    result += "\n  - " + toll.name + " (" + toll.operator_name + "): " + 
                           RouteUtils::formatCurrency(toll.cost, toll.currency);
                }
            } else if (routeType != AVOID_TOLLS) {
                result += "\nNo toll information available for this route.";
            } else {
                result += "\nThis route avoids all toll roads.";
            }
        } catch (const exception& e) {
            cerr << "Error in formatRouteWithRealDirections: " << e.what() << endl;
            return "Error generating route details";
        }
        
        return RouteUtils::cleanRouteSymbols(result);
    }
};

class RoutePlanner {
private:
    // Geocoder geocoder;
    // RouteFinder routeFinder;
    Geocoder geocoder;
    RouteFinder routeFinder;
    RouteUtils::LocationManager locationManager;
    RouteUtils::RouteManager routeManager;
    unordered_map<string, Node> currentGraph;

public:
    // void planRoute() {
    //     cout << "===== Maps Pathfinder Application =====" << endl;
        
    //     while (true) {
    //         try {
    //             string startLocationName, endLocationName;
                
    //             cout << "Enter your current location: ";
    //             getline(cin, startLocationName);
                
    //             cout << "Enter your destination: ";
    //             getline(cin, endLocationName);
                
    //             if (startLocationName.empty() || endLocationName.empty()) {
    //                 cout << "Error: Location names cannot be empty. Please try again." << endl;
    //                 continue;
    //             }
                
    //             cout << "\nSearching for locations..." << endl;
                
    //             Location startLocation = geocoder.geocodeLocation(startLocationName);
    //             Location endLocation = geocoder.geocodeLocation(endLocationName);
                
    //             if (startLocation.name.empty() || endLocation.name.empty()) {
    //                 cout << "Error: Could not find one or both locations. Please try again with more specific names." << endl;
    //                 continue;
    //             }
                
    //             cout << "Start: " << startLocation.name << endl;
    //             cout << "End: " << endLocation.name << endl;
                
    //             double directDistance = RouteUtils::getAccurateDistance(startLocation, endLocation);
    //             cout << "\nDirect distance: " << directDistance << " km" << endl;
                
    //             bool exit = false;
    //             while (!exit) {
    //                 cout << "\n===== Route Options =====" << endl;
    //                 cout << "1. Fastest Route (Default)" << endl;
    //                 cout << "2. Shortest Route" << endl;
    //                 cout << "3. Avoid Tolls" << endl;
    //                 cout << "4. Scenic Route" << endl;
    //                 cout << "0. Enter New Locations" << endl;
    //                 cout << "9. Exit Program" << endl;
    //                 cout << "\nSelect an option: ";
                    
    //                 int choice;
    //                 try {
    //                     string input;
    //                     getline(cin, input);
    //                     choice = stoi(input);
    //                 } catch (const exception&) {
    //                     cout << "Invalid input. Please enter a number." << endl;
    //                     continue;
    //                 }
                    
    //                 RouteType routeType;
    //                 switch (choice) {
    //                     case 1: routeType = FASTEST; cout << "\nCalculating fastest route..." << endl; break;
    //                     case 2: routeType = SHORTEST; cout << "\nCalculating shortest route..." << endl; break;
    //                     case 3: routeType = AVOID_TOLLS; cout << "\nCalculating route avoiding tolls..." << endl; break;
    //                     case 4: routeType = SCENIC; cout << "\nCalculating scenic route..." << endl; break;
    //                     case 0: exit = true; continue;
    //                     case 9: cout << "Exiting program. Thank you for using Maps Pathfinder!" << endl; return;
    //                     default: cout << "Invalid option. Using Fastest Route." << endl; routeType = FASTEST; break;
    //                 }
                    
    //                 try {
    //                     unordered_map<string, Node> graph;
    //                     string startId = "start";
    //                     string endId = "end";
                        
    //                     graph[startId] = Node(startId, startLocation);
    //                     graph[endId] = Node(endId, endLocation);
    //                     graph[startId].edges.push_back(make_pair(endId, directDistance));
                        
    //                     vector<vector<string>> routes = routeFinder.generateMultipleRoutes(
    //                         graph, startId, endId, startLocation, endLocation, routeType);
                        
    //                     if (routes.empty()) {
    //                         cout << "No routes found between the locations. Please try different locations." << endl;
    //                         continue;
    //                     }
                        
    //                     cout << "\n===== Available Routes between " 
    //                         << Location::extractCityName(startLocation.name) << " and " 
    //                         << Location::extractCityName(endLocation.name) << " =====" << endl;
                        
    //                     for (size_t i = 0; i < routes.size(); i++) {
    //                         const auto& route = routes[i];
    //                         cout << "Route " << (i+1) << ": ";
                            
    //                         if (i == 0) {
    //                             cout << "Recommended Route" << endl;
    //                         } else {
    //                             cout << "Alternative Route " << i << endl;
    //                         }
                            
    //                         cout << RouteUtils::cleanRouteSymbols(routeFinder.formatRoute(route, graph, routeType)) << endl;
    //                         cout << "Step-by-step directions:" << endl;
                            
    //                         for (size_t j = 0; j < route.size() - 1; j++) {
    //                             const auto& currentNode = graph[route[j]];
    //                             const auto& nextNode = graph[route[j+1]];
                                
    //                             double segmentDistance = 0;
    //                             for (const auto& edge : currentNode.edges) {
    //                                 if (edge.first == route[j+1]) {
    //                                     segmentDistance = edge.second;
    //                                     break;
    //                                 }
    //                             }
                                
    //                             vector<string> segmentRoads = routeFinder.getRouteDetails(
    //                                 currentNode.location, 
    //                                 nextNode.location, 
    //                                 routeType
    //                             );
                                
    //                             string stepDescription = "  " + to_string(j+1) + ". " + currentNode.location.name;
    //                             for (const auto& road : segmentRoads) {
    //                                 stepDescription += " → " + road;
    //                             }
    //                             stepDescription += " → " + nextNode.location.name;
                                
    //                             double speed = 60.0;
    //                             switch (routeType) {
    //                                 case FASTEST: speed = 60.0; break;
    //                                 case SHORTEST: speed = 50.0; break;
    //                                 case AVOID_TOLLS: speed = 45.0; break;
    //                                 case SCENIC: speed = 40.0; break;
    //                             }
    //                             double time = (segmentDistance / speed) * 60;
    //                             stepDescription += " (" + to_string(static_cast<int>(segmentDistance)) + 
    //                                             " km, " + to_string(static_cast<int>(time)) + " min)";
                                
    //                             cout << RouteUtils::cleanRouteSymbols(stepDescription) << endl;
    //                         }
                            
    //                         string formattedRoute = routeFinder.formatRouteWithRealDirections(
    //                             route, graph, routeType);
    //                         cout << "\nFormatted Route: " << RouteUtils::cleanRouteSymbols(formattedRoute) << endl;
    //                     }
    //                 } catch (const exception& e) {
    //                     cerr << "Error calculating route: " << e.what() << endl;
    //                     cout << "Please try again with different locations." << endl;
    //                 }
    //             }
    //         } catch (const exception& e) {
    //             cerr << "Error: " << e.what() << endl;
    //             cout << "Please try again." << endl;
    //         }
            
    //         cout << "\nPress Enter to continue or type 'exit' to quit: ";
    //         string input;
    //         getline(cin, input);
    //         if (input == "exit") {
    //             cout << "Exiting program. Thank you for using Maps Pathfinder!" << endl;
    //             break;
    //         }
    //     }
    // }

    void planRoute() {
    cout << "===== Maps Pathfinder Application =====" << endl;
    
    while (true) {
        try {
            cout << "\n===== Main Menu =====" << endl;
            cout << "1. Plan a New Route" << endl;
            cout << "2. Manage Saved Locations" << endl;
            cout << "3. Manage Saved Routes" << endl;
            cout << "4. View Current Graph Visualization" << endl;
            cout << "9. Exit Program" << endl;
            cout << "\nSelect an option: ";
            
            string input;
            getline(cin, input);
            int choice;
            
            try {
                choice = stoi(input);
            } catch (const exception&) {
                cout << "Invalid input. Please enter a number." << endl;
                continue;
            }
            
            switch (choice) {
                case 1:
                    planNewRoute();
                    break;
                case 2:
                    manageLocations();
                    break;
                case 3:
                    manageRoutes();
                    break;
                case 4:
                    visualizeCurrentGraph();
                    break;
                case 9:
                    cout << "Exiting program. Thank you for using Maps Pathfinder!" << endl;
                    return;
                default:
                    cout << "Invalid option. Please try again." << endl;
                    break;
            }
        } catch (const exception& e) {
            cerr << "Error: " << e.what() << endl;
            cout << "Please try again." << endl;
        }
    }
}

void planNewRoute() {
    string startLocationName, endLocationName;
    
    // Check if user wants to use saved location
    cout << "\nDo you want to use a saved location? (y/n): ";
    string useSaved;
    getline(cin, useSaved);
    
    if (useSaved == "y" || useSaved == "Y") {
        vector<string> locations = locationManager.getAllLocationNames();
        if (locations.empty()) {
            cout << "No saved locations found. Please enter location names manually." << endl;
        } else {
            cout << "\nSaved locations:" << endl;
            for (size_t i = 0; i < locations.size(); i++) {
                cout << (i+1) << ". " << locations[i] << endl;
            }
            
            // Get start location
            cout << "\nSelect start location (number) or 0 to enter manually: ";
            int startIdx = -1;
            try {
                string input;
                getline(cin, input);
                startIdx = stoi(input) - 1;
            } catch (const exception&) {
                startIdx = -1;
            }
            
            if (startIdx >= 0 && startIdx < locations.size()) {
                startLocationName = locations[startIdx];
            } else {
                cout << "Enter your current location: ";
                getline(cin, startLocationName);
            }
            
            // Get end location
            cout << "\nSelect destination location (number) or 0 to enter manually: ";
            int endIdx = -1;
            try {
                string input;
                getline(cin, input);
                endIdx = stoi(input) - 1;
            } catch (const exception&) {
                endIdx = -1;
            }
            
            if (endIdx >= 0 && endIdx < locations.size()) {
                endLocationName = locations[endIdx];
            } else {
                cout << "Enter your destination: ";
                getline(cin, endLocationName);
            }
        }
    } else {
        cout << "Enter your current location: ";
        getline(cin, startLocationName);
        
        cout << "Enter your destination: ";
        getline(cin, endLocationName);
    }
    
    if (startLocationName.empty() || endLocationName.empty()) {
        cout << "Error: Location names cannot be empty. Please try again." << endl;
        return;
    }
    
    cout << "\nSearching for locations..." << endl;
    
    // Check if these are saved locations first
    Location startLocation = locationManager.getLocation(startLocationName);
    if (startLocation.name.empty()) {
        startLocation = geocoder.geocodeLocation(startLocationName);
    }
    
    Location endLocation = locationManager.getLocation(endLocationName);
    if (endLocation.name.empty()) {
        endLocation = geocoder.geocodeLocation(endLocationName);
    }
    
    if (startLocation.name.empty() || endLocation.name.empty()) {
        cout << "Error: Could not find one or both locations. Please try again with more specific names." << endl;
        return;
    }
    
    cout << "Start: " << startLocation.name << endl;
    cout << "End: " << endLocation.name << endl;
    
    double directDistance = RouteUtils::getAccurateDistance(startLocation, endLocation);
    cout << "\nDirect distance: " << directDistance << " km" << endl;
    
    // Ask if user wants to save these locations
    cout << "\nDo you want to save these locations for future use? (y/n): ";
    string saveLocations;
    getline(cin, saveLocations);
    
    if (saveLocations == "y" || saveLocations == "Y") {
        // Save start location if not already saved
        if (locationManager.getLocation(startLocation.name).name.empty()) {
            cout << "Saving " << startLocation.name << " to your locations..." << endl;
            locationManager.addLocation(startLocation.name, startLocation.lat, startLocation.lon);
        }
        
        // Save end location if not already saved
        if (locationManager.getLocation(endLocation.name).name.empty()) {
            cout << "Saving " << endLocation.name << " to your locations..." << endl;
            locationManager.addLocation(endLocation.name, endLocation.lat, endLocation.lon);
        }
    }
    
    // Continue with route planning
    processRouteOptions(startLocation, endLocation, directDistance);
}

void processRouteOptions(const Location& startLocation, const Location& endLocation, double directDistance) {
    bool exit = false;
    while (!exit) {
        cout << "\n===== Route Options =====" << endl;
        cout << "1. Fastest Route (Default)" << endl;
        cout << "2. Shortest Route" << endl;
        cout << "3. Avoid Tolls" << endl;
        cout << "4. Scenic Route" << endl;
        cout << "0. Return to Main Menu" << endl;
        cout << "\nSelect an option: ";
        
        int choice;
        try {
            string input;
            getline(cin, input);
            choice = stoi(input);
        } catch (const exception&) {
            cout << "Invalid input. Please enter a number." << endl;
            continue;
        }
        
        if (choice == 0) {
            exit = true;
            continue;
        }
        
        RouteType routeType;
        switch (choice) {
            case 1: routeType = FASTEST; cout << "\nCalculating fastest route..." << endl; break;
            case 2: routeType = SHORTEST; cout << "\nCalculating shortest route..." << endl; break;
            case 3: routeType = AVOID_TOLLS; cout << "\nCalculating route avoiding tolls..." << endl; break;
            case 4: routeType = SCENIC; cout << "\nCalculating scenic route..." << endl; break;
            default: cout << "Invalid option. Using Fastest Route." << endl; routeType = FASTEST; break;
        }
        
         generateAndDisplayRoutes(startLocation, endLocation, directDistance, routeType);
    }
}

void generateAndDisplayRoutes(
    const Location& startLocation, 
    const Location& endLocation, 
    double directDistance,
    RouteType routeType) {
    
    try {
        currentGraph.clear();
        string startId = "start";
        string endId = "end";
        
        currentGraph[startId] = Node(startId, startLocation);
        currentGraph[endId] = Node(endId, endLocation);
        currentGraph[startId].edges.push_back(make_pair(endId, directDistance));
        
        vector<vector<string>> routes = routeFinder.generateMultipleRoutes(
            currentGraph, startId, endId, startLocation, endLocation, routeType);
        
        if (routes.empty()) {
            cout << "No routes found between the locations. Please try different locations." << endl;
            return;
        }
        
        cout << "\n===== Available Routes between " 
            << Location::extractCityName(startLocation.name) << " and " 
            << Location::extractCityName(endLocation.name) << " =====" << endl;
        
        vector<string> routeDescriptions;
        
        for (size_t i = 0; i < routes.size(); i++) {
            const auto& route = routes[i];
            cout << "Route " << (i+1) << ": ";
            
            if (i == 0) {
                cout << "Recommended Route" << endl;
            } else {
                cout << "Alternative Route " << i << endl;
            }
            
            string formattedRoute = RouteUtils::cleanRouteSymbols(routeFinder.formatRoute(route, currentGraph, routeType));
            cout << formattedRoute << endl;
            routeDescriptions.push_back(formattedRoute);
            
            cout << "Step-by-step directions:" << endl;
            
            for (size_t j = 0; j < route.size() - 1; j++) {
                const auto& currentNode = currentGraph[route[j]];
                const auto& nextNode = currentGraph[route[j+1]];
                
                double segmentDistance = 0;
                for (const auto& edge : currentNode.edges) {
                    if (edge.first == route[j+1]) {
                        segmentDistance = edge.second;
                        break;
                    }
                }
                
                vector<string> segmentRoads = routeFinder.getRouteDetails(
                    currentNode.location, 
                    nextNode.location, 
                    routeType
                );
                
                string stepDescription = "  " + to_string(j+1) + ". " + currentNode.location.name;
                for (const auto& road : segmentRoads) {
                    stepDescription += " → " + road;
                }
                stepDescription += " → " + nextNode.location.name;
                
                double speed = 60.0;
                switch (routeType) {
                    case FASTEST: speed = 60.0; break;
                    case SHORTEST: speed = 50.0; break;
                    case AVOID_TOLLS: speed = 45.0; break;
                    case SCENIC: speed = 40.0; break;
                }
                double time = (segmentDistance / speed) * 60;
                stepDescription += " (" + to_string(static_cast<int>(segmentDistance)) + 
                                " km, " + to_string(static_cast<int>(time)) + " min)";
                
                cout << RouteUtils::cleanRouteSymbols(stepDescription) << endl;
            }
            
            string detailedRoute = routeFinder.formatRouteWithRealDirections(
                route, currentGraph, routeType);
            cout << "\nFormatted Route: " << RouteUtils::cleanRouteSymbols(detailedRoute) << endl;
            
            // Display path visualization
            cout << RouteUtils::GraphVisualizer::visualizePathInGraph(currentGraph, route) << endl;
        }
        
        // Ask if user wants to save this route
        cout << "\nDo you want to save one of these routes for future use? (y/n): ";
        string saveRoute;
        getline(cin, saveRoute);
        
        if (saveRoute == "y" || saveRoute == "Y") {
            cout << "Which route do you want to save? (1-" << routes.size() << "): ";
            int routeIdx = 0;
            try {
                string input;
                getline(cin, input);
                routeIdx = stoi(input) - 1;
            } catch (...) {
                routeIdx = -1;
            }
            
            if (routeIdx >= 0 && routeIdx < routes.size()) {
                cout << "Enter a name for this route: ";
                string routeName;
                getline(cin, routeName);
                
                if (!routeName.empty()) {
                    routeManager.addRoute(
                        routeName,
                        startLocation.name,
                        endLocation.name,
                        routeType,
                        routes[routeIdx]
                    );
                    cout << "Route saved as '" << routeName << "'." << endl;
                }
            }
        }
    } catch (const exception& e) {
        cerr << "Error calculating route: " << e.what() << endl;
        cout << "Please try again with different locations." << endl;
    }
}

void manageLocations() {
    bool exit = false;
    while (!exit) {
        cout << "\n===== Location Management =====" << endl;
        locationManager.displayAllLocations();
        
        cout << "\n1. Add New Location" << endl;
        cout << "2. Update Existing Location" << endl;
        cout << "3. Delete Location" << endl;
        cout << "0. Return to Main Menu" << endl;
        cout << "\nSelect an option: ";
        
        int choice;
        try {
            string input;
            getline(cin, input);
            choice = stoi(input);
        } catch (const exception&) {
            cout << "Invalid input. Please enter a number." << endl;
            continue;
        }
        
        switch (choice) {
            case 0:
                exit = true;
                break;
            case 1:
                addNewLocation();
                break;
            case 2:
                updateLocation();
                break;
            case 3:
                deleteLocation();
                break;
            default:
                cout << "Invalid option. Please try again." << endl;
                break;
        }
    }
}

void addNewLocation() {
    string name;
    double lat, lon;
    
    cout << "\n===== Add New Location =====" << endl;
    cout << "Enter location name: ";
    getline(cin, name);
    
    if (name.empty()) {
        cout << "Error: Name cannot be empty." << endl;
        return;
    }
    
    cout << "Do you want to use geocoding to find coordinates? (y/n): ";
    string useGeocoding;
    getline(cin, useGeocoding);
    
    if (useGeocoding == "y" || useGeocoding == "Y") {
        cout << "Geocoding location..." << endl;
        Location location = geocoder.geocodeLocation(name);
        
        if (location.name.empty()) {
            cout << "Error: Could not geocode location. Please enter coordinates manually." << endl;
        } else {
            name = location.name;
            lat = location.lat;
            lon = location.lon;
            cout << "Found: " << name << " at coordinates (" << lat << ", " << lon << ")" << endl;
            
            // Save the location
            if (locationManager.addLocation(name, lat, lon)) {
                cout << "Location added successfully." << endl;
            } else {
                cout << "Error: Location with this name already exists." << endl;
            }
            return;
        }
    }
    
    // Manual coordinate entry
    cout << "Enter latitude: ";
    try {
        string input;
        getline(cin, input);
        lat = stod(input);
    } catch (const exception&) {
        cout << "Invalid latitude. Please enter a valid number." << endl;
        return;
    }
    
    cout << "Enter longitude: ";
    try {
        string input;
        getline(cin, input);
        lon = stod(input);
    } catch (const exception&) {
        cout << "Invalid longitude. Please enter a valid number." << endl;
        return;
    }
    
    if (locationManager.addLocation(name, lat, lon)) {
        cout << "Location added successfully." << endl;
    } else {
        cout << "Error: Location with this name already exists." << endl;
    }
}

void updateLocation() {
    vector<string> locationNames = locationManager.getAllLocationNames();
    if (locationNames.empty()) {
        cout << "No locations to update." << endl;
        return;
    }
    
    cout << "\n===== Update Location =====" << endl;
    cout << "Select location to update:" << endl;
    
    for (size_t i = 0; i < locationNames.size(); i++) {
        cout << (i+1) << ". " << locationNames[i] << endl;
    }
    
    cout << "\nEnter number (0 to cancel): ";
    int idx;
    try {
        string input;
        getline(cin, input);
        idx = stoi(input) - 1;
    } catch (const exception&) {
        idx = -1;
    }
    
    if (idx < 0 || idx >= locationNames.size()) {
        cout << "Operation cancelled or invalid selection." << endl;
        return;
    }
    
    string name = locationNames[idx];
    Location currentLocation = locationManager.getLocation(name);
    
    cout << "\nUpdating location: " << name << endl;
    cout << "Current coordinates: (" << currentLocation.lat << ", " << currentLocation.lon << ")" << endl;
    
    cout << "Enter new latitude (or leave empty to keep current): ";
    string latInput;
    getline(cin, latInput);
    double lat = currentLocation.lat;
    if (!latInput.empty()) {
        try {
            lat = stod(latInput);
        } catch (const exception&) {
            cout << "Invalid latitude. Keeping current value." << endl;
        }
    }
    
    cout << "Enter new longitude (or leave empty to keep current): ";
    string lonInput;
    getline(cin, lonInput);
    double lon = currentLocation.lon;
    if (!lonInput.empty()) {
        try {
            lon = stod(lonInput);
        } catch (const exception&) {
            cout << "Invalid longitude. Keeping current value." << endl;
        }
    }
    
    if (locationManager.updateLocation(name, lat, lon)) {
        cout << "Location updated successfully." << endl;
    } else {
        cout << "Error updating location." << endl;
    }
}

void deleteLocation() {
    vector<string> locationNames = locationManager.getAllLocationNames();
    if (locationNames.empty()) {
        cout << "No locations to delete." << endl;
        return;
    }
    
    cout << "\n===== Delete Location =====" << endl;
    cout << "Select location to delete:" << endl;
    
    for (size_t i = 0; i < locationNames.size(); i++) {
        cout << (i+1) << ". " << locationNames[i] << endl;
    }
    
    cout << "\nEnter number (0 to cancel): ";
    int idx;
    try {
        string input;
        getline(cin, input);
        idx = stoi(input) - 1;
    } catch (const exception&) {
        idx = -1;
    }
    
    if (idx < 0 || idx >= locationNames.size()) {
        cout << "Operation cancelled or invalid selection." << endl;
        return;
    }
    
    string name = locationNames[idx];
    cout << "Are you sure you want to delete " << name << "? (y/n): ";
    string confirm;
    getline(cin, confirm);
    
    if (confirm == "y" || confirm == "Y") {
        if (locationManager.deleteLocation(name)) {
            cout << "Location deleted successfully." << endl;
        } else {
            cout << "Error deleting location." << endl;
        }
    } else {
        cout << "Deletion cancelled." << endl;
    }
}

void manageRoutes() {
    bool exit = false;
    while (!exit) {
        cout << "\n===== Route Management =====" << endl;
        routeManager.displayAllRoutes();
        
        cout << "\n1. View Saved Route" << endl;
        cout << "2. Delete Route" << endl;
        cout << "0. Return to Main Menu" << endl;
        cout << "\nSelect an option: ";
        
        int choice;
        try {
            string input;
            getline(cin, input);
            choice = stoi(input);
        } catch (const exception&) {
            cout << "Invalid input. Please enter a number." << endl;
            continue;
        }
        
        switch (choice) {
            case 0:
                exit = true;
                break;
            case 1:
                viewSavedRoute();
                break;
            case 2:
                deleteSavedRoute();
                break;
            default:
                cout << "Invalid option. Please try again." << endl;
                break;
        }
    }
}

void viewSavedRoute() {
    vector<string> routeNames = routeManager.getAllRouteNames();
    if (routeNames.empty()) {
        cout << "No saved routes." << endl;
        return;
    }
    
    cout << "\n===== View Saved Route =====" << endl;
    cout << "Select route to view:" << endl;
    
    for (size_t i = 0; i < routeNames.size(); i++) {
        cout << (i+1) << ". " << routeNames[i] << endl;
    }
    
    cout << "\nEnter number (0 to cancel): ";
    int idx;
    try {
        string input;
        getline(cin, input);
        idx = stoi(input) - 1;
    } catch (const exception&) {
        idx = -1;
    }
    
    if (idx < 0 || idx >= routeNames.size()) {
        cout << "Operation cancelled or invalid selection." << endl;
        return;
    }
    
    string name = routeNames[idx];
    auto route = routeManager.getRoute(name);
    
    cout << "\n===== Route: " << name << " =====" << endl;
    cout << "From: " << route.startLocationName << endl;
    cout << "To: " << route.endLocationName << endl;
    
    string routeType;
    switch (route.routeType) {
        case FASTEST: routeType = "Fastest"; break;
        case SHORTEST: routeType = "Shortest"; break;
        case AVOID_TOLLS: routeType = "Avoid Tolls"; break;
        case SCENIC: routeType = "Scenic"; break;
    }
    cout << "Route type: " << routeType << endl;
    
    // Get the actual locations
    Location startLoc = locationManager.getLocation(route.startLocationName);
    if (startLoc.name.empty()) {
        startLoc = geocoder.geocodeLocation(route.startLocationName);
    }
    
    Location endLoc = locationManager.getLocation(route.endLocationName);
    if (endLoc.name.empty()) {
        endLoc = geocoder.geocodeLocation(route.endLocationName);
    }
    
    if (!startLoc.name.empty() && !endLoc.name.empty()) {
        cout << "\nDo you want to recalculate this route? (y/n): ";
        string recalc;
        getline(cin, recalc);
        
        if (recalc == "y" || recalc == "Y") {
            double directDistance = RouteUtils::getAccurateDistance(startLoc, endLoc);
            generateAndDisplayRoutes(startLoc, endLoc, directDistance, route.routeType);
        }
    }
}

void deleteSavedRoute() {
    vector<string> routeNames = routeManager.getAllRouteNames();
    if (routeNames.empty()) {
        cout << "No saved routes." << endl;
        return;
    }
    
    cout << "\n===== Delete Saved Route =====" << endl;
    cout << "Select route to delete:" << endl;
    
    for (size_t i = 0; i < routeNames.size(); i++) {
        cout << (i+1) << ". " << routeNames[i] << endl;
    }
    
    cout << "\nEnter number (0 to cancel): ";
    int idx;
    try {
        string input;
        getline(cin, input);
        idx = stoi(input) - 1;
    } catch (const exception&) {
        idx = -1;
    }
    
    if (idx < 0 || idx >= routeNames.size()) {
        cout << "Operation cancelled or invalid selection." << endl;
        return;
    }
    
    string name = routeNames[idx];
    cout << "Are you sure you want to delete route '" << name << "'? (y/n): ";
    string confirm;
    getline(cin, confirm);
    
    if (confirm == "y" || confirm == "Y") {
        if (routeManager.deleteRoute(name)) {
            cout << "Route deleted successfully." << endl;
        } else {
            cout << "Error deleting route." << endl;
        }
    } else {
        cout << "Deletion cancelled." << endl;
    }
}
void visualizeCurrentGraph() {
    if (currentGraph.empty()) {
        cout << "\nNo graph available for visualization. Please plan a route first." << endl;
        return;
    }
    
    cout << "\n===== Graph Visualization =====" << endl;
    cout << RouteUtils::GraphVisualizer::visualizeGraph(currentGraph) << endl;
    
    // Option to save visualization
    cout << "Do you want to save this graph visualization to a file? (y/n): ";
    string saveGraph;
    getline(cin, saveGraph);
    
    if (saveGraph == "y" || saveGraph == "Y") {
        // Get current timestamp for filename
        time_t now = time(0);
        tm* ltm = localtime(&now);
        stringstream timestamp;
        timestamp << 1900 + ltm->tm_year << "-" 
                  << setw(2) << setfill('0') << 1 + ltm->tm_mon << "-"
                  << setw(2) << setfill('0') << ltm->tm_mday << "_"
                  << setw(2) << setfill('0') << ltm->tm_hour 
                  << setw(2) << setfill('0') << ltm->tm_min;
        
        string defaultFilename = "graph_visualization_" + timestamp.str() + ".txt";
        cout << "Enter filename (default: " << defaultFilename << "): ";
        string filename;
        getline(cin, filename);
        
        if (filename.empty()) {
            filename = defaultFilename;
        }
        
        if (filename.find(".txt") == string::npos) {
            filename += ".txt";
        }
        
        RouteUtils::GraphVisualizer::saveGraphVisualization(currentGraph, filename);
        cout << "Graph visualization saved to " << filename << endl;
    }
}


// void generateAndDisplayRoutes(
//     const Location& startLocation, 
//     const Location& endLocation, 
//     double directDistance,
//     RouteType routeType) {
    
//     try {
//         currentGraph.clear();
//         string startId = "start";
//         string endId = "end";
        
//         currentGraph[startId] = Node(startId, startLocation);
//         currentGraph[endId] = Node(endId, endLocation);
//         currentGraph[startId].edges.push_back(make_pair(endId, directDistance));
        
//         vector<vector<string>> routes = routeFinder.generateMultipleRoutes(
//             currentGraph, startId, endId, startLocation, endLocation, routeType);
        
//         if (routes.empty()) {
//             cout << "No routes found between the locations. Please try different locations." << endl;
//             return;
//         }
        
//         cout << "\n===== Available Routes between " 
//             << Location::extractCityName(startLocation.name) << " and " 
//             << Location::extractCityName(endLocation.name) << " =====" << endl;
        
//         vector<string> routeDescriptions;
//         vector<string> detailedRoutes;
        
//         for (size_t i = 0; i < routes.size(); i++) {
//             const auto& route = routes[i];
//             cout << "Route " << (i+1) << ": ";
            
//             if (i == 0) {
//                 cout << "Recommended Route" << endl;
//             } else {
//                 cout << "Alternative Route " << i << endl;
//             }
            
//             string formattedRoute = RouteUtils::cleanRouteSymbols(routeFinder.formatRoute(route, currentGraph, routeType));
//             cout << formattedRoute << endl;
//             routeDescriptions.push_back(formattedRoute);
            
//             cout << "Step-by-step directions:" << endl;
            
//             stringstream stepDetails;
//             for (size_t j = 0; j < route.size() - 1; j++) {
//                 const auto& currentNode = currentGraph[route[j]];
//                 const auto& nextNode = currentGraph[route[j+1]];
                
//                 double segmentDistance = 0;
//                 for (const auto& edge : currentNode.edges) {
//                     if (edge.first == route[j+1]) {
//                         segmentDistance = edge.second;
//                         break;
//                     }
//                 }
                
//                 vector<string> segmentRoads = routeFinder.getRouteDetails(
//                     currentNode.location, 
//                     nextNode.location, 
//                     routeType
//                 );
                
//                 string stepDescription = "  " + to_string(j+1) + ". " + currentNode.location.name;
//                 for (const auto& road : segmentRoads) {
//                     stepDescription += " → " + road;
//                 }
//                 stepDescription += " → " + nextNode.location.name;
                
//                 double speed = 60.0;
//                 switch (routeType) {
//                     case FASTEST: speed = 60.0; break;
//                     case SHORTEST: speed = 50.0; break;
//                     case AVOID_TOLLS: speed = 45.0; break;
//                     case SCENIC: speed = 40.0; break;
//                 }
//                 double time = (segmentDistance / speed) * 60;
//                 stepDescription += " (" + to_string(static_cast<int>(segmentDistance)) + 
//                                 " km, " + to_string(static_cast<int>(time)) + " min)";
                
//                 cout << RouteUtils::cleanRouteSymbols(stepDescription) << endl;
//                 stepDetails << RouteUtils::cleanRouteSymbols(stepDescription) << "\n";
//             }
            
//             string detailedRoute = routeFinder.formatRouteWithRealDirections(
//                 route, currentGraph, routeType);
//             cout << "\nFormatted Route: " << RouteUtils::cleanRouteSymbols(detailedRoute) << endl;
//             detailedRoutes.push_back("Route " + to_string(i+1) + 
//                 (i == 0 ? " (Recommended Route):\n" : " (Alternative Route):\n") +
//                 RouteUtils::cleanRouteSymbols(detailedRoute) + 
//                 "\n\nStep-by-step directions:\n" + stepDetails.str());
            
//             // Display path visualization
//             cout << RouteUtils::GraphVisualizer::visualizePathInGraph(currentGraph, route) << endl;
//         }
        
//         // Option to export route information to text file
//         cout << "\n===== Export Options =====" << endl;
//         cout << "1. Save route information to text file" << endl;
//         cout << "2. Save graph visualization to text file" << endl; 
//         cout << "3. Save both route and graph visualization" << endl;
//         cout << "0. Continue without saving" << endl;
//         cout << "Select an option: ";
        
//         int exportChoice;
//         try {
//             string input;
//             getline(cin, input);
//             exportChoice = stoi(input);
//         } catch (...) {
//             exportChoice = 0;
//         }
        
//         if (exportChoice == 1 || exportChoice == 3) {
//             exportRouteToFile(startLocation.name, endLocation.name, detailedRoutes);
//         }
        
//         if (exportChoice == 2 || exportChoice == 3) {
//             cout << "Enter filename for graph visualization (default: graph_visualization.txt): ";
//             string filename;
//             getline(cin, filename);
            
//             if (filename.empty()) {
//                 filename = "graph_visualization.txt";
//             }
            
//             if (filename.find(".txt") == string::npos) {
//                 filename += ".txt";
//             }
            
//             RouteUtils::GraphVisualizer::saveGraphVisualization(currentGraph, filename);
//             cout << "Graph visualization saved to " << filename << endl;
//         }
        
//         // Ask if user wants to save this route
//         cout << "\nDo you want to save one of these routes for future use? (y/n): ";
//         string saveRoute;
//         getline(cin, saveRoute);
        
//         if (saveRoute == "y" || saveRoute == "Y") {
//             cout << "Which route do you want to save? (1-" << routes.size() << "): ";
//             int routeIdx = 0;
//             try {
//                 string input;
//                 getline(cin, input);
//                 routeIdx = stoi(input) - 1;
//             } catch (...) {
//                 routeIdx = -1;
//             }
            
//             if (routeIdx >= 0 && routeIdx < routes.size()) {
//                 cout << "Enter a name for this route: ";
//                 string routeName;
//                 getline(cin, routeName);
                
//                 if (!routeName.empty()) {
//                     routeManager.addRoute(
//                         routeName,
//                         startLocation.name,
//                         endLocation.name,
//                         routeType,
//                         routes[routeIdx]
//                     );
//                     cout << "Route saved as '" << routeName << "'." << endl;
//                 }
//             }
//         }
//     } catch (const exception& e) {
//         cerr << "Error calculating route: " << e.what() << endl;
//         cout << "Please try again with different locations." << endl;
//     }
// }

void exportRouteToFile(const string& startLocation, const string& endLocation, 
                      const vector<string>& detailedRoutes) {
    // Get current timestamp for filename
    time_t now = time(0);
    tm* ltm = localtime(&now);
    stringstream timestamp;
    timestamp << 1900 + ltm->tm_year << "-" 
              << setw(2) << setfill('0') << 1 + ltm->tm_mon << "-"
              << setw(2) << setfill('0') << ltm->tm_mday << "_"
              << setw(2) << setfill('0') << ltm->tm_hour 
              << setw(2) << setfill('0') << ltm->tm_min;
    
    string defaultFilename = "route_" + 
        Location::extractCityName(startLocation) + "_to_" + 
        Location::extractCityName(endLocation) + "_" + 
        timestamp.str() + ".txt";
    
    cout << "Enter filename for route information (default: " << defaultFilename << "): ";
    string filename;
    getline(cin, filename);
    
    if (filename.empty()) {
        filename = defaultFilename;
    }
    
    if (filename.find(".txt") == string::npos) {
        filename += ".txt";
    }
    
    stringstream content;
    content << "===== Route from " << startLocation << " to " << endLocation << " =====" << endl;
    content << "Generated on: " << put_time(ltm, "%Y-%m-%d %H:%M:%S") << endl << endl;
    
    for (const auto& route : detailedRoutes) {
        content << route << endl;
        content << "---------------------------------------------------" << endl;
    }
    
    if (RouteUtils::FileIO::saveToFile(filename, content.str())) {
        cout << "Route information saved to " << filename << endl;
    } else {
        cout << "Error: Could not save route information to file." << endl;
    }
}

};

// int main() {
//     try {
//         RoutePlanner planner;
//         planner.planRoute();
//     } catch (const exception& e) {
//         cerr << "Fatal error: " << e.what() << endl;
//         cout << "The program will now exit." << endl;
//         return 1;
//     }
//     return 0;
// }


int main() {
    try {
        RoutePlanner planner;
        planner.planRoute();
        
        // Export option after route planning is done
        cout << "\n===== Export Final Results =====" << endl;
        cout << "Do you want to export the last route to a text file? (y/n): ";
        string exportChoice;
        getline(cin, exportChoice);
        
        if (exportChoice == "y" || exportChoice == "Y") {
            // Get filename from user
            cout << "Enter filename for export (default: final_route_export.txt): ";
            string filename;
            getline(cin, filename);
            
            if (filename.empty()) {
                filename = "final_route_export.txt";
            }
            
            if (filename.find(".txt") == string::npos) {
                filename += ".txt";
            }
            
            // Get current session details from planner
            stringstream content;
            content << "===== Maps Pathfinder Route Export =====" << endl;
            
            // Add timestamp
            time_t now = time(0);
            tm* ltm = localtime(&now);
            content << "Generated on: " << put_time(ltm, "%Y-%m-%d %H:%M:%S") << endl << endl;
            
            // Add route information and graph visualization
            content << "Route details were exported to this file at the end of your session." << endl;
            content << "To view detailed route information, please use the export options" << endl;
            content << "while viewing specific routes in the application." << endl;
            
            // Save the file
            if (RouteUtils::FileIO::saveToFile(filename, content.str())) {
                cout << "Export saved to " << filename << endl;
            } else {
                cout << "Error: Could not save export to file." << endl;
            }
        }
        
        cout << "Thank you for using Maps Pathfinder!" << endl;
    } catch (const exception& e) {
        cerr << "Fatal error: " << e.what() << endl;
        cout << "The program will now exit." << endl;
        return 1;
    }
    return 0;
}