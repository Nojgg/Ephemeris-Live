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
    float mapZoom = 1.0f;
    Vector2 mapPan = { 0,0 };

    // Global Location
    float latitude = 46.2276f;
    float longitude = 2.2137f;

    // Text Box Buffers & Edit States
    char latBuf[32] = "46.2276";
    char lonBuf[32] = "2.2137";
    char addressBuf[128] = "";
    char telBuf[32] = "1000.0";
    char eyeBuf[32] = "25.0";
    char afovBuf[32] = "50.0";

    char journalTitle[128] = "Observation Session 01";
    char journalTarget[64] = "Jupiter";
    char journalBuffer[4096] = "Conditions d'observation :\nTransparence :\n\nNotes :\n";

    bool editLat = false, editLon = false, editAddress = false;
    bool editTel = false, editEye = false, editAfov = false;
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
    escaped << std::hex << std::uppercase << std::setfill('0');
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') escaped << c;
        else escaped << '%' << std::setw(2) << int((unsigned char)c);
    }
    return escaped.str();
}

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

// --- Geocoding ---
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
}

// --- Background Data Fetcher Thread ---
void BackendThreadFunc() {
    while (state.isRunning) {
        if (state.forceRefresh) {
            state.forceRefresh = false;
            state.isFetching = true;

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
            }

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
    if (file.is_open()) file << state.telBuf << "\n" << state.eyeBuf << "\n" << state.latBuf << "\n" << state.lonBuf << "\n" << state.afovBuf;
}
void LoadSpecs() {
    std::ifstream file("specs.txt");
    if (file.is_open()) {
        file >> state.telBuf >> state.eyeBuf >> state.latBuf >> state.lonBuf;
        if (!(file >> state.afovBuf)) strcpy(state.afovBuf, "50.0");
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

void ApplyStyle() {
    GuiSetStyle(DEFAULT, BACKGROUND_COLOR, 0x1E1E1EFF);
    GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL, 0xFFFFFFFF);
    GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL, 0x87CEEBFF);
    GuiSetStyle(DEFAULT, LINE_COLOR, 0x87CEEBFF);
    GuiSetStyle(BUTTON, BASE_COLOR_NORMAL, 0x303030FF);
}

int main() {
    InitWindow(1200, 800, "Ephemeris Live 3.0");
    SetTargetFPS(60);
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
            Rectangle mapArea = { 375,20,740,500 };
            if (CheckCollisionPointRec(GetMousePosition(), mapArea)) {
                state.mapZoom += GetMouseWheelMove() * 0.15f;
                if (state.mapZoom < 0.5f) state.mapZoom = 0.5f;
                if (state.mapZoom > 5.0f) state.mapZoom = 5.0f;
            }
            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
                Vector2 d = GetMouseDelta();
                state.mapPan.x += d.x;
                state.mapPan.y += d.y;
            }
            DrawRectangleRec(mapArea, { 20, 20, 20, 255 });

            // === MODE CISEAUX ACTIVE ===
            BeginScissorMode((int)mapArea.x, (int)mapArea.y, (int)mapArea.width, (int)mapArea.height);

            Vector2 mapCenter = { 450 + 600 / 2.0f + state.mapPan.x, 20 + 500 / 2.0f + state.mapPan.y };
            float mapRadius = 230.0f * state.mapZoom;
            DrawCircleLines((int)mapCenter.x, (int)mapCenter.y, mapRadius, { 135, 206, 235, 100 });
            DrawText("N", (int)mapCenter.x - 4, (int)(mapCenter.y - mapRadius - 15), 15, ACCENT);
            DrawText("S", (int)mapCenter.x - 4, (int)(mapCenter.y + mapRadius + 5), 15, ACCENT);
            DrawText("E", (int)(mapCenter.x + mapRadius + 5), (int)mapCenter.y - 8, 15, ACCENT);
            DrawText("W", (int)(mapCenter.x - mapRadius - 20), (int)mapCenter.y - 8, 15, ACCENT);

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
            EndScissorMode();

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

            if (state.isFetching) {
                DrawRectangleRec({ 375, 20, 740, 500 }, Fade(BLACK, 0.8f));
                int dotCount = (int)(GetTime() * 3.0) % 4;
                std::string loadingStr = "FETCHING LIVE NASA DATA";
                for (int i = 0; i < dotCount; i++) loadingStr += ".";
                int textW = MeasureText(loadingStr.c_str(), 20);
                DrawText(loadingStr.c_str(), 450 + (600 - textW) / 2, 260, 20, ACCENT);
            }
            DrawRectangleLinesEx({ 375, 20, 740, 500 }, 2, ACCENT);

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

            if (GuiButton(Rectangle{ 340, 530, 100, 30 }, "- 1 HOUR")) { state.timeOffsetSeconds -= 3600; state.isFetching = true; state.forceRefresh = true; }
            if (GuiButton(Rectangle{ 450, 530, 100, 30 }, "+ 1 HOUR")) { state.timeOffsetSeconds += 3600; state.isFetching = true; state.forceRefresh = true; }
            if (GuiButton(Rectangle{ 560, 530, 100, 30 }, "RESET NOW")) { state.timeOffsetSeconds = 0; state.isFetching = true; state.forceRefresh = true; }

            // Planet Data Panel (TFOV Intégré)
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

                            float tFl = 1000.0f, eFl = 25.0f, afov = 50.0f;
                            try { tFl = std::stof(state.telBuf); }
                            catch (...) {}
                            try { eFl = std::stof(state.eyeBuf); }
                            catch (...) {}
                            try { afov = std::stof(state.afovBuf); }
                            catch (...) {}

                            float mag = tFl / (eFl > 0 ? eFl : 1.0f);
                            float tfov_deg = afov / mag;
                            float tfov_arcsec = tfov_deg * 3600.0f;

                            Vector2 eye = { 980, 680 };
                            DrawCircleLines((int)eye.x, (int)eye.y, 70, ACCENT);

                            float pr = (float)((p.ang_size / tfov_arcsec) * 70.0f);
                            if (pr < 1.5f) pr = 1.5f;
                            if (pr > 70.0f) pr = 70.0f;

                            DrawCircleV(eye, pr, GOLD);
                            DrawText(TextFormat("Mag: %.1fx", mag), 920, 760, 15, ACCENT);
                            DrawText(TextFormat("TFOV: %.2f deg", tfov_deg), 920, 780, 15, ACCENT);
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
            if (GuiTextBox(Rectangle{ 560, 300, 150, 30 }, state.telBuf, 32, state.editTel)) { state.editTel = !state.editTel; if (!state.editTel) SaveSpecs(); }

            GuiLabel(Rectangle{ 360, 340, 200, 30 }, "Eyepiece Focal Length (mm):");
            if (GuiTextBox(Rectangle{ 560, 340, 150, 30 }, state.eyeBuf, 32, state.editEye)) { state.editEye = !state.editEye; if (!state.editEye) SaveSpecs(); }

            GuiLabel(Rectangle{ 360, 380, 200, 30 }, "Apparent FOV (deg):");
            if (GuiTextBox(Rectangle{ 560, 380, 150, 30 }, state.afovBuf, 32, state.editAfov)) { state.editAfov = !state.editAfov; if (!state.editAfov) SaveSpecs(); }

            float tFl = 1.0f; float eFl = 1.0f;
            try { if (state.telBuf[0] != '\0') tFl = std::stof(state.telBuf); }
            catch (...) {}
            try { if (state.eyeBuf[0] != '\0') eFl = std::stof(state.eyeBuf); }
            catch (...) {}
            DrawText(TextFormat("Current Magnification: %.1f x", tFl / eFl), 360, 440, 20, GOLD);
        }
        else if (state.current_tab == 2) { // JOURNAL (EDITEUR MULTILIGNE AMÉLIORÉ)
            GuiGroupBox(content, "OBSERVATORY LOGBOOK");

            // --- BARRE D'OUTILS ---
            DrawText("Formatting Tools:", 360, 40, 15, ACCENT);
            if (GuiButton(Rectangle{ 500, 35, 30, 25 }, "B")) {
                std::string temp = state.journalBuffer;
                if (temp.length() < 4000) { temp += "**gras**"; strcpy(state.journalBuffer, temp.c_str()); }
            }
            if (GuiButton(Rectangle{ 540, 35, 30, 25 }, "I")) {
                std::string temp = state.journalBuffer;
                if (temp.length() < 4000) { temp += "*italique*"; strcpy(state.journalBuffer, temp.c_str()); }
            }
            if (GuiButton(Rectangle{ 580, 35, 40, 25 }, "List")) {
                std::string temp = state.journalBuffer;
                if (temp.length() < 4000) { temp += "\n- "; strcpy(state.journalBuffer, temp.c_str()); }
            }

            // --- ZONE DE TEXTE MULTILIGNE ET COULEURS PERSONNALISÉES ---
            Rectangle journalRect = { 360, 70, 800, 400 };

            // Gestion du focus (Activation au clic de la souris)
            if (CheckCollisionPointRec(GetMousePosition(), journalRect) && IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.editJournal = true;
            }
            else if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
                state.editJournal = false;
            }

            // Capturer les touches claviers natives quand la zone est active
            if (state.editJournal) {
                // Saisie des caractères standards
                int key = GetCharPressed();
                while (key > 0) {
                    size_t len = strlen(state.journalBuffer);
                    if (len < 4090) {
                        state.journalBuffer[len] = (char)key;
                        state.journalBuffer[len + 1] = '\0';
                    }
                    key = GetCharPressed();
                }

                // Touche RETOUR ARRIÈRE (gère les caractères et les lignes)
                if (IsKeyPressed(KEY_BACKSPACE)) {
                    size_t len = strlen(state.journalBuffer);
                    if (len > 0) state.journalBuffer[len - 1] = '\0';
                }

                // Touche ENTRÉE (Vrai retour à la ligne !)
                if (IsKeyPressed(KEY_ENTER)) {
                    size_t len = strlen(state.journalBuffer);
                    if (len < 4090) {
                        state.journalBuffer[len] = '\n';
                        state.journalBuffer[len + 1] = '\0';
                    }
                }
            }

            // Dessin du fond en GRIS FONCÉ
            DrawRectangleRec(journalRect, state.editJournal ? Color{ 55, 55, 55, 255 } : Color{ 40, 40, 40, 255 });
            DrawRectangleLinesEx(journalRect, 1, state.editJournal ? WHITE : ACCENT);

            // Création de la chaîne d'affichage avec curseur clignotant
            std::string displayText = state.journalBuffer;
            if (state.editJournal && ((int)(GetTime() * 2) % 2 == 0)) displayText += "|";

            // Dessin du TEXTE EN BLANC (Supporte nativement les sauts de ligne \n)
            DrawTextEx(GetFontDefault(), displayText.c_str(), Vector2{ 375, 85 }, 16, 2, WHITE);

            DrawLine(360, 490, 1150, 490, ACCENT);

            // --- MÉTADONNÉES ---
            GuiLabel(Rectangle{ 360, 510, 80, 30 }, "Log Title:");
            if (GuiTextBox(Rectangle{ 450, 510, 400, 30 }, state.journalTitle, 128, state.editTitle)) state.editTitle = !state.editTitle;

            GuiLabel(Rectangle{ 880, 510, 80, 30 }, "Target:");
            if (GuiTextBox(Rectangle{ 940, 510, 200, 30 }, state.journalTarget, 64, state.editTarget)) state.editTarget = !state.editTarget;

            time_t now = std::time(nullptr);
            char dateStr[64]; std::strftime(dateStr, sizeof(dateStr), "%Y-%m-%d %H:%M Local", std::localtime(&now));
            DrawText(TextFormat("Date: %s", dateStr), 360, 560, 15, LIGHTGRAY);
            DrawText(TextFormat("Coordinates: Lat %s, Lon %s", state.latBuf, state.lonBuf), 880, 560, 15, LIGHTGRAY);

            if (GuiButton(Rectangle{ 360, 720, 150, 30 }, "SAVE ENTRY TO LOG")) ExportJournal();
            DrawText("Entries are appended to 'Observation_Log.txt' with Markdown tags.", 530, 725, 15, GRAY);
        }

        EndDrawing();
    }

    state.isRunning = false;
    backendThread.join();
    CloseWindow();
    return 0;
}