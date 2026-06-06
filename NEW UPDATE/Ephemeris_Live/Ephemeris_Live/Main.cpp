

#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"


#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <array>
#include <ctime>
#include <thread>
#include <mutex>
#include <fstream>
#include <atomic>
#include <chrono>


// --- Constants ---
const float MY_PI = 3.14159265358979323846f;
const Color BG_COLOR = { 30, 30, 30, 255 };
const Color ACCENT = { 135, 206, 235, 255 }; // Sky Blue

// --- Data Structures ---
struct HorizonCoords { float alt; float az; };

struct Planet {
    std::string name;
    std::string id;
    std::string type;
    HorizonCoords coords;
    double ang_size;
    Vector2 ui_pos;
    bool isVisible;
    bool selected;
    bool dataLoaded;
};

// Application State
struct AppState {
    int current_tab = 0;

    // Global Location
    float latitude = 46.2276f;
    float longitude = 2.2137f;

    // Text Box Buffers & Edit States
    char latBuf[32] = "46.2276";
    char lonBuf[32] = "2.2137";
    char addressBuf[128] = "";
    char telBuf[32] = "1000.0";
    char eyeBuf[32] = "25.0";
    char journalTitle[128] = "Observation Session 01";
    char journalTarget[64] = "Jupiter";
    char journalBuffer[4096] = "Seeing conditions:\nTransparency:\n\nNotes:\n";

    bool editLat = false, editLon = false, editAddress = false;
    bool editTel = false, editEye = false;
    bool editTitle = false, editTarget = false, editJournal = false;

    long long timeOffsetSeconds = 0;
    std::atomic<bool> forceRefresh{ true };
    std::atomic<bool> isFetching{ true };
    std::atomic<bool> isRunning{ true };

    std::vector<Planet> planets = {
        {"Sun", "10", "star", {0,0}, 0, {0,0}, false, true, false},
        {"Mercury", "199", "planet", {0,0}, 0, {0,0}, false, false, false},
        {"Venus", "299", "planet", {0,0}, 0, {0,0}, false, false, false},
        {"Mars", "499", "planet", {0,0}, 0, {0,0}, false, false, false},
        {"Jupiter", "599", "planet", {0,0}, 0, {0,0}, false, false, false},
        {"Saturn", "699", "planet", {0,0}, 0, {0,0}, false, false, false},
        {"Uranus", "799", "planet", {0,0}, 0, {0,0}, false, false, false},
        {"Neptune", "899", "planet", {0,0}, 0, {0,0}, false, false, false},
        {"Moon", "301", "satellite", {0,0}, 0, {0,0}, false, false, false}
    };
};

AppState state;
std::mutex dataMutex;

// --- Utility Functions ---
std::string urlencode(const std::string& str) {
    std::ostringstream escaped;
    escaped << std::hex << std::uppercase << std::setfill('0'); // FIXED: setfill ensures proper %09 vs %9
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') escaped << c;
        else escaped << '%' << std::setw(2) << int((unsigned char)c);
    }
    return escaped.str();
}

// FIXED: Now properly injects your actual Latitude and Longitude into the API call
std::string build_url(const std::string& command, const std::string& start, const std::string& stop, float lat, float lon) {
    std::string site = std::to_string(lon) + "," + std::to_string(lat) + ",0";
    return "https://ssd.jpl.nasa.gov/api/horizons.api?format=text&COMMAND='" + urlencode(command) +
        "'&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='OBSERVER'&CENTER='coord%40399'&SITE_COORD='" + urlencode(site) +
        "'&START_TIME='" + urlencode(start) + "'&STOP_TIME='" + urlencode(stop) +
        "'&STEP_SIZE='1%20m'&QUANTITIES='1,13'&ANG_FORMAT='DEG'&EXTRA_PREC='YES'";
}

std::string exec(const char* cmd) {
    std::array<char, 128> buffer; std::string result;
    auto pipe = _popen(cmd, "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) result += buffer.data();
    _pclose(pipe);
    return result;
}

// --- Astronomy Math ---
float degToRad(float degrees) { return degrees * (MY_PI / 180.0f); }
float RadtoDeg(float radians) { return radians * (180.0f / MY_PI); }
float refraction(float alt_deg) {
    if (alt_deg < -0.8f) return alt_deg;
    float r = 1.02f / tan(degToRad(alt_deg + 10.3f / (alt_deg + 5.11f)));
    return alt_deg + r / 60.0f;
}
float lst_deg(float timeUTC) {
    float jd = 2440587.5f + timeUTC / 86400.0f;
    float T = (jd - 2451545.0f) / 36525.0f;
    float gst = 280.46061837f + 360.98564736629f * (jd - 2451545.0f) + T * T * (0.000387933f - T / 38710000.0f);
    return fmod(gst, 360.0f) + state.longitude;
}

HorizonCoords GetAltAz(float raDeg, float decDeg, float timeUTC) {
    float ra = degToRad(raDeg), dec = degToRad(decDeg);
    float latRad = degToRad(state.latitude);
    float lst = degToRad(lst_deg(timeUTC));
    float ha = lst - ra;
    float sin_alt = sin(dec) * sin(latRad) + cos(dec) * cos(latRad) * cos(ha);
    float alt = RadtoDeg(asin(fmaxf(-1.0f, fminf(1.0f, sin_alt))));
    float y = -cos(dec) * sin(ha);
    float x = sin(dec) * cos(latRad) - cos(dec) * cos(ha) * sin(latRad);
    float az = RadtoDeg(atan2(y, x));
    if (az < 0) az += 360.0f;
    return { refraction(alt), az };
}

bool parse_horizons(const std::string& text, double& ra, double& dec, double& ang) {
    std::istringstream stream(text); std::string line;
    while (std::getline(stream, line)) {
        if (line.find("$$SOE") != std::string::npos) {
            std::getline(stream, line); std::istringstream ls(line);
            std::vector<std::string> tokens; std::string t;
            while (ls >> t) tokens.push_back(t);
            if (tokens.size() >= 3) {
                try {
                    ra = std::stod(tokens[tokens.size() - 3]);
                    dec = std::stod(tokens[tokens.size() - 2]);
                    ang = std::stod(tokens[tokens.size() - 1]);
                    return true;
                }
                catch (...) { return false; }
            }
        }
    }
    return false;
}

// --- Geocoding (Address to Lat/Lon) ---
void GeocodeAddress(const std::string& address) {
    std::cout << "Geocoding Address: " << address << "...\n";
    std::string url = "https://nominatim.openstreetmap.org/search?q=" + urlencode(address) + "&format=json&limit=1";
    std::string cmd = "curl -s -A \"EphemerisApp/1.0\" \"" + url + "\"";
    std::string json = exec(cmd.c_str());

    size_t latPos = json.find("\"lat\":\"");
    size_t lonPos = json.find("\"lon\":\"");

    if (latPos != std::string::npos && lonPos != std::string::npos) {
        latPos += 7; lonPos += 7;
        std::string latStr = json.substr(latPos, json.find("\"", latPos) - latPos);
        std::string lonStr = json.substr(lonPos, json.find("\"", lonPos) - lonPos);

        std::lock_guard<std::mutex> lock(dataMutex);
        strcpy(state.latBuf, latStr.c_str());
        strcpy(state.lonBuf, lonStr.c_str());

        try {
            state.latitude = std::stof(latStr);
            state.longitude = std::stof(lonStr);
            state.isFetching = true;
            state.forceRefresh = true;
            std::cout << "Geocode Success: Lat " << state.latitude << ", Lon " << state.longitude << "\n";
        }
        catch (...) {}
    }
    else {
        std::cout << "Geocode Failed! Please check your spelling or internet connection.\n";
    }
}

// --- Background Data Fetcher Thread ---
void BackendThreadFunc() {
    while (state.isRunning) {
        if (state.forceRefresh) {
            state.forceRefresh = false;
            state.isFetching = true;

            // Start loading screen timer
            auto fetchStart = std::chrono::steady_clock::now();

            time_t targetTime = std::time(nullptr) + state.timeOffsetSeconds;
            char start[30], stop[30];
            std::strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", std::gmtime(&targetTime));
            time_t endTarget = targetTime + 60;
            std::strftime(stop, sizeof(stop), "%Y-%m-%d %H:%M:%S", std::gmtime(&endTarget));

            for (auto& p : state.planets) {
                if (!state.isRunning || state.forceRefresh) break;

                std::string url = build_url(p.id, start, stop, state.latitude, state.longitude);
                std::string rawData = exec(("curl -s -L \"" + url + "\"").c_str());

                double ra, dec, ang;
                if (parse_horizons(rawData, ra, dec, ang)) {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    p.coords = GetAltAz((float)ra, (float)dec, (float)targetTime);
                    p.ang_size = ang;
                    p.dataLoaded = true;
                }
                else {
                    std::cout << "\n--- NASA API ERROR: Failed to fetch " << p.name << " ---\n";
                    std::cout << "URL Sent: " << url << "\n";
                    std::cout << "API Replied: " << rawData << "\n-------------------\n";
                }
            }

            // Ensure the loading screen is visible for at least 800ms so it doesn't flash violently
            auto fetchEnd = std::chrono::steady_clock::now();
            long long duration = std::chrono::duration_cast<std::chrono::milliseconds>(fetchEnd - fetchStart).count();
            if (duration < 800) std::this_thread::sleep_for(std::chrono::milliseconds(800 - duration));

            if (!state.forceRefresh) state.isFetching = false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

// --- File I/O Helpers ---
void SaveSpecs() {
    std::ofstream file("specs.txt");
    if (file.is_open()) file << state.telBuf << "\n" << state.eyeBuf << "\n" << state.latBuf << "\n" << state.lonBuf;
}
void LoadSpecs() {
    std::ifstream file("specs.txt");
    if (file.is_open()) {
        file >> state.telBuf >> state.eyeBuf >> state.latBuf >> state.lonBuf;
        try {
            if (state.latBuf[0] != '\0') state.latitude = std::stof(state.latBuf);
            if (state.lonBuf[0] != '\0') state.longitude = std::stof(state.lonBuf);
        }
        catch (...) {}
    }
}
void ExportJournal() {
    std::ofstream file("Observation_Log.txt", std::ios::app);
    if (file.is_open()) {
        time_t now = std::time(nullptr);
        file << "==========================================\n";
        file << "TITLE : " << state.journalTitle << "\n";
        file << "TARGET: " << state.journalTarget << "\n";
        file << "DATE  : " << std::ctime(&now);
        file << "LOC   : Lat " << state.latBuf << ", Lon " << state.lonBuf << "\n";
        file << "------------------------------------------\n";
        file << state.journalBuffer << "\n\n";
    }
}

// --- GUI Helper ---
void ApplyStyle() {
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x1E1E1EFF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xFFFFFFFF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x87CEEBFF);
    GuiSetStyle(DEFAULT, LINE_COLOR, 0x87CEEBFF);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x303030FF);
}

// --- Main Application ---
int main() {

    InitWindow(1200, 800, "Ephemeris Live 3.0");
    SetTargetFPS(60);
	SetWindowIcon(LoadImage("favicon.png"));
    ApplyStyle();
    LoadSpecs();

    std::thread backendThread(BackendThreadFunc);

    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground(BG_COLOR);

        // Sidebar
        GuiGroupBox(Rectangle{ 20, 20, 300, 760 }, "CONTROL PANEL");
        if (GuiButton(Rectangle{ 40, 50, 80, 30 }, "SKY MAP")) state.current_tab = 0;
        if (GuiButton(Rectangle{ 130, 50, 80, 30 }, "SETTINGS")) state.current_tab = 1;
        if (GuiButton(Rectangle{ 220, 50, 80, 30 }, "JOURNAL")) state.current_tab = 2;

        Rectangle content = { 340, 20, 840, 760 };

        if (state.current_tab == 0) { // PLANETARIUM
            DrawRectangleRec({ 375, 20, 740, 500 }, { 20, 20, 20, 255 });

            // Polar Projection Map Background
            Vector2 mapCenter = { 450 + 600 / 2.0f, 20 + 500 / 2.0f };
            float mapRadius = 230.0f;
            DrawCircleLines((int)mapCenter.x, (int)mapCenter.y, mapRadius, { 135, 206, 235, 100 });
            DrawText("N", (int)mapCenter.x - 4, (int)(mapCenter.y - mapRadius - 15), 15, ACCENT);
            DrawText("S", (int)mapCenter.x - 4, (int)(mapCenter.y + mapRadius + 5), 15, ACCENT);
            DrawText("E", (int)(mapCenter.x + mapRadius + 5), (int)mapCenter.y - 8, 15, ACCENT);
            DrawText("W", (int)(mapCenter.x - mapRadius - 20), (int)mapCenter.y - 8, 15, ACCENT);

            // Draw Planets ALWAYS (if loaded)
            {
                std::lock_guard<std::mutex> lock(dataMutex);
                for (auto& p : state.planets) {
                    if (!p.dataLoaded) continue;
                    p.isVisible = (p.coords.alt > 0);
                    if (p.isVisible) {
                        float r = ((90.0f - p.coords.alt) / 90.0f) * mapRadius;
                        float theta = degToRad(p.coords.az - 90.0f);
                        p.ui_pos.x = mapCenter.x + r * cos(theta);
                        p.ui_pos.y = mapCenter.y + r * sin(theta);

                        DrawCircleV(p.ui_pos, 8, p.selected ? GOLD : LIGHTGRAY);
                        if (p.selected) DrawCircleLines((int)p.ui_pos.x, (int)p.ui_pos.y, 14, WHITE);
                        DrawText(p.name.c_str(), (int)p.ui_pos.x + 12, (int)p.ui_pos.y - 8, 12, WHITE);
                    }
                }
            }

            // Interaction: Map Click (Only if not fetching)
            if (!state.isFetching && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                Vector2 mouse = GetMousePosition();
                std::lock_guard<std::mutex> lock(dataMutex);
                for (auto& p : state.planets) {
                    if (p.isVisible && p.dataLoaded && CheckCollisionPointCircle(mouse, p.ui_pos, 15)) {
                        for (auto& other : state.planets) other.selected = false;
                        p.selected = true;
                        strcpy(state.journalTarget, p.name.c_str());
                    }
                }
            }

            // --- SMOOTH LOADING SCREEN OVERLAY ---
            if (state.isFetching) {
                DrawRectangleRec({ 375, 20, 740, 500 }, Fade(BLACK, 0.8f)); // Darkens the map

                int dotCount = (int)(GetTime() * 3.0) % 4;
                std::string loadingStr = "FETCHING LIVE NASA DATA";
                for (int i = 0; i < dotCount; i++) loadingStr += ".";

                int textW = MeasureText(loadingStr.c_str(), 20);
                DrawText(loadingStr.c_str(), 450 + (600 - textW) / 2, 260, 20, ACCENT);
            }

            DrawRectangleLinesEx({ 375, 20, 740, 500 }, 2, ACCENT); // Outer border

            // Target Selector Panel
            GuiGroupBox(Rectangle{ 60, 100, 220, 420 }, "SELECT TARGET");
            int btnY = 130;
            for (auto& p : state.planets) {
                bool isTarget = p.selected;
                DrawCircle(75, btnY + 15, 5, p.isVisible && p.dataLoaded ? GREEN : RED);
                GuiToggle(Rectangle{ 90, (float)btnY, 160, 30 }, p.name.c_str(), &isTarget);

                if (isTarget != p.selected) {
                    std::lock_guard<std::mutex> lock(dataMutex);
                    for (auto& other : state.planets) other.selected = false;
                    p.selected = true;
                    strcpy(state.journalTarget, p.name.c_str());
                }
                btnY += 40;
            }

            // Time Controls
            if (GuiButton(Rectangle{ 340, 530, 100, 30 }, "- 1 HOUR")) { state.timeOffsetSeconds -= 3600; state.isFetching = true; state.forceRefresh = true; }
            if (GuiButton(Rectangle{ 450, 530, 100, 30 }, "+ 1 HOUR")) { state.timeOffsetSeconds += 3600; state.isFetching = true; state.forceRefresh = true; }
            if (GuiButton(Rectangle{ 560, 530, 100, 30 }, "RESET NOW")) { state.timeOffsetSeconds = 0; state.isFetching = true; state.forceRefresh = true; }

            // Planet Data Panel
            GuiGroupBox(Rectangle{ 340, 580, 840, 200 }, "LIVE EPHEMERIS");
            std::lock_guard<std::mutex> lock(dataMutex);
            for (const auto& p : state.planets) {
                if (p.selected) {
                    DrawText(TextFormat("Target: %s", p.name.c_str()), 360, 600, 20, ACCENT);

                    if (state.isFetching) {
                        DrawText("Calculating Trajectories...", 360, 630, 20, LIGHTGRAY);
                    }
                    else {
                        DrawText(TextFormat("Status: %s", p.isVisible ? "VISIBLE" : "BELOW HORIZON"), 360, 630, 20, p.isVisible ? GREEN : RED);
                        if (p.dataLoaded) {
                            DrawText(TextFormat("Altitude: %.2f deg", p.coords.alt), 360, 660, 20, WHITE);
                            DrawText(TextFormat("Azimuth: %.2f deg", p.coords.az), 360, 690, 20, WHITE);
                            DrawText(TextFormat("Apparent Size: %.2f arcsec", p.ang_size), 600, 660, 20, WHITE);
                        }
                    }
                }
            }
        }
        else if (state.current_tab == 1) { // SETTINGS
            GuiGroupBox(content, "APPLICATION SETTINGS");

            DrawText("OBSERVATION LOCATION", 360, 50, 20, ACCENT);
            DrawLine(360, 75, 1150, 75, ACCENT);

            GuiLabel(Rectangle{ 360, 90, 200, 30 }, "Search by City/Address:");
            if (GuiTextBox(Rectangle{ 560, 90, 300, 30 }, state.addressBuf, 128, state.editAddress)) state.editAddress = !state.editAddress;
            if (GuiButton(Rectangle{ 870, 90, 100, 30 }, "GEOCODE")) { GeocodeAddress(state.addressBuf); SaveSpecs(); }

            GuiLabel(Rectangle{ 360, 140, 200, 30 }, "Manual Latitude:");
            if (GuiTextBox(Rectangle{ 560, 140, 150, 30 }, state.latBuf, 32, state.editLat)) {
                state.editLat = !state.editLat;
                if (!state.editLat) { try { state.latitude = std::stof(state.latBuf); SaveSpecs(); state.isFetching = true; state.forceRefresh = true; } catch (...) {} }
            }

            GuiLabel(Rectangle{ 360, 180, 200, 30 }, "Manual Longitude:");
            if (GuiTextBox(Rectangle{ 560, 180, 150, 30 }, state.lonBuf, 32, state.editLon)) {
                state.editLon = !state.editLon;
                if (!state.editLon) { try { state.longitude = std::stof(state.lonBuf); SaveSpecs(); state.isFetching = true; state.forceRefresh = true; } catch (...) {} }
            }

            DrawText("OPTICAL HARDWARE", 360, 260, 20, ACCENT);
            DrawLine(360, 285, 1150, 285, ACCENT);

            GuiLabel(Rectangle{ 360, 300, 200, 30 }, "Telescope Focal Length (mm):");
            if (GuiTextBox(Rectangle{ 560, 300, 150, 30 }, state.telBuf, 32, state.editTel)) {
                state.editTel = !state.editTel; if (!state.editTel) SaveSpecs();
            }

            GuiLabel(Rectangle{ 360, 340, 200, 30 }, "Eyepiece Focal Length (mm):");
            if (GuiTextBox(Rectangle{ 560, 340, 150, 30 }, state.eyeBuf, 32, state.editEye)) {
                state.editEye = !state.editEye; if (!state.editEye) SaveSpecs();
            }

            float tFl = 1.0f; float eFl = 1.0f;
            try { if (state.telBuf[0] != '\0') tFl = std::stof(state.telBuf); }
            catch (...) {}
            try { if (state.eyeBuf[0] != '\0') eFl = std::stof(state.eyeBuf); }
            catch (...) {}
            DrawText(TextFormat("Current Magnification: %.1f x", tFl / eFl), 360, 400, 20, GOLD);
        }
        else if (state.current_tab == 2) { // JOURNAL
            GuiGroupBox(content, "OBSERVATORY LOGBOOK");

            GuiLabel(Rectangle{ 360, 40, 80, 30 }, "Log Title:");
            if (GuiTextBox(Rectangle{ 450, 40, 400, 30 }, state.journalTitle, 128, state.editTitle)) state.editTitle = !state.editTitle;

            GuiLabel(Rectangle{ 880, 40, 80, 30 }, "Target:");
            if (GuiTextBox(Rectangle{ 940, 40, 200, 30 }, state.journalTarget, 64, state.editTarget)) state.editTarget = !state.editTarget;

            time_t now = std::time(nullptr);
            char dateStr[64]; std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M Local", std::localtime(&now));
            DrawText(TextFormat("Date: %s", dateStr), 360, 90, 15, LIGHTGRAY);
            DrawText(TextFormat("Coordinates: Lat %s, Lon %s", state.latBuf, state.lonBuf), 880, 90, 15, LIGHTGRAY);

            DrawLine(360, 120, 1150, 120, ACCENT);
            if (GuiTextBox(Rectangle{ 360, 140, 800, 560 }, state.journalBuffer, 4096, state.editJournal)) state.editJournal = !state.editJournal;

            if (GuiButton(Rectangle{ 360, 720, 150, 30 }, "SAVE ENTRY TO LOG")) ExportJournal();
            DrawText("Entries are appended to 'Observation_Log.txt'", 530, 725, 15, GRAY);
        }

        EndDrawing();
    }

    state.isRunning = false;
    backendThread.join();
    CloseWindow();
    return 0;
}