# Final Project Struktur Data Kelompok 7

## Member

| No  | Nama                                | NRP        |
| --- | ----------------------------------- | ---------- |
| 1   | Kanafira Vanesha Putri              | 5027241010 |
| 2   | Clarissa Aydin Rahmazea             | 5027241014 |
| 3   | Muhammad Fatihul Qolbi Ash Shiddiqi | 5027241023 |
| 4   | Mochammad Atha Tajuddin             | 5027241093 |
| 5   | Muhammad Ahsani Taqwiim Rakhman     | 5027241099 |
| 6   | Danar Bagus Rasendriya              | 5027231055 |

Link Youtube : https://www.youtube.com/playlist?list=PLDfnCUqcAIJcOvA6aiDSNwZI91b6fTBE8

🌍 Project Overview
Maps Pathfinder is a C++ application that allows users to plan and visualize routes between different locations. It uses algorithms to find optimal paths based on different route types (fastest, shortest, avoid tolls, scenic) and provides detailed directions with estimated travel times, distances, and toll information.

✨ Key Features
Route Planning: Generate multiple route options between any two locations
Multiple Routing Options: Choose between fastest, shortest, toll-free, or scenic routes
Detailed Directions: Step-by-step navigation instructions with distance and time estimates
Toll Information: Cost estimates for toll roads when applicable
Location Management: Save and manage frequently used locations
Route Management: Save favorite routes for quick access later
Graph Visualization: Visual representation of route networks
Export Functionality: Save routes and graph visualizations to text files
📋 Requirements
C++ Compiler: C++14 or newer (g++, MSVC, Clang)
Dependencies:
httplib - HTTP requests
nlohmann/json - JSON parsing
OpenSSL - For secure HTTPS connections
Build System: CMake 3.10+ recommended
🛠️ Installation
Prerequisites
Install a C++ compiler (g++, Visual Studio with C++ tools, or Clang)
Install OpenSSL development libraries:
Windows: Install using vcpkg or download from OpenSSL website
Linux: sudo apt-get install libssl-dev (Debian/Ubuntu)
macOS: brew install openssl
Building from Source

# Clone the repository
git clone https://github.com/shenaavv/FP-Strukdat-Kel7.git
cd FP-Strukdat-Kel7

# Create build directory
mkdir build && cd build

# Configure and build
cmake ..
cmake --build .


🚀 Running the Application
Navigate to the build directory or where your executable was created
Run the application:
Windows: maps_pathfinder.exe
Linux/macOS: ./maps_pathfinder
Follow the on-screen instructions to use the application
📖 Usage Guide
Main Menu
When you start the application, you'll see the main menu with these options:

Plan a New Route: Find routes between locations
Manage Saved Locations: Add, update, or delete saved locations
Manage Saved Routes: View or delete saved routes
View Current Graph Visualization: Visualize the current route graph
Exit Program: Close the application
Planning a Route
Select 1 from the main menu
Choose whether to use a saved location or enter a new one
Enter or select your starting location
Enter or select your destination
Choose a route type:
Fastest Route
Shortest Route
Avoid Tolls
Scenic Route
View the generated routes with step-by-step directions
Optionally save or export the route
Managing Locations
Select 2 from the main menu
View your saved locations
Choose to add, update, or delete locations
Managing Routes
Select 3 from the main menu
View your saved routes
Choose to view details or delete a route
Exporting Data
When viewing routes, you'll have options to:

Save route information to a text file
Save graph visualization to a text file
Save both route and visualization
💾 Data Files
The application creates and uses these files:

user_locations.csv: Saved user locations
user_routes.csv: Saved user routes
Generated export files: Route details and graph visualizations
🧮 Data Structures and Algorithms
Key Data Structures
Location Class

Stores location information (name, latitude, longitude)
Node Class

Represents a graph vertex with location and connecting edges
Graph Representation

Implemented using an unordered_map for fast node lookup
Uses adjacency lists (vector of pairs) for edges
Priority Queue

Used in pathfinding algorithms for efficient node selection
Route Representation

Vector of node IDs representing the path
Associated metadata (distance, time, toll information)
Algorithms
A Pathfinding Algorithm*

Implementation in findShortestPath method
Uses heuristic distance estimation for optimization
Time Complexity: O(E log V) where E is edges and V is vertices
Greedy Best-First Search

Implementation in findBestFirstPath method
Greedy algorithm using straight-line distance heuristic
Time Complexity: O(E log V)
Haversine Formula

Used in calculateDistance for accurate Earth-surface distance calculations
Route Generation

Multi-path generation with waypoint insertion
Creates alternative routes through different intermediate points
Graph Visualization Algorithms

Custom ASCII visualization of nodes and connections
🔍 Implementation Details
1.Route Finding Process
The application first geocodes the starting and ending locations
2.It creates a graph with nodes for the start, end, and potential waypoints
3.Applies pathfinding algorithms (A*, Best-First Search) to find routes
4.Generates detailed directions for each route segment
5.Calculates metrics (distance, time, toll costs)
6.Formats and displays the results

Route Types

Fastest: Prioritizes highways and high-speed roads
Shortest: Minimizes total distance regardless of road type
Avoid Tolls: Routes around toll roads even if path is longer
Scenic: Includes interesting waypoints even at cost of efficiency


🙏 Acknowledgments
httplib for HTTP client functionality
nlohmann/json for JSON processing
OpenStreetMap Nominatim API for geocoding services
OSRM API for route calculation assistance
