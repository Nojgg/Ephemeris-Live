#define _CRT_SECURE_NO_WARNINGS
#include "raylib.h"
#include "raymath.h"
#define RAYGUI_IMPLEMENTATION
#include "raygui.h"



#include "AstroMath.hpp"
#include "CoreEngine.hpp"
#include <unordered_map>

#include <iostream>
#include <string>
#include <vector>
#include <ctime>
#include <fstream>
#include <algorithm>
#include <sstream>

const Color MODE_JOUR_BG = { 30, 30, 30, 255 };
const Color MODE_JOUR_ACCENT = { 135, 206, 235, 255 }; 
const Color MODE_NUIT_BG = { 12, 0, 0, 255 };
const Color MODE_NUIT_ACCENT = { 220, 0, 0, 255 };

struct HardwareConfig {
    float telescopeFocalLength = 1000.0f;
    float eyepieceFocalLength = 25.0f;
    float eyepieceAfov = 50.0f;
    float latitude = 46.2276f;
    float longitude = 2.2137f;
};

struct CityGeocode {
    std::string name; std::string country; float lat; float lon;
};

class AstronomyApplication {



private:
    std::unordered_map<std::string, std::vector<CityGeocode*>> prefixIndex;

    void BuildPrefixIndex();
    std::vector<CityGeocode*> GetAutocomplete(const std::string& inputRaw);

    int activeTab = 0;
    bool isNightVisionActive = false;

    float viewZoom = 1.0f;
    Vector2 viewPan = { 0.0f, 0.0f };

    bool filterShowPlanets = true;
    bool filterShowStars = true;
    bool mapShowPlanets = true;
    bool mapShowStars = true;
    bool mapShowConstellations = true;
    bool mapShowTrajectories = true;
    bool mapShowCardinals = true;
    bool simulateAtmosphere = false;

    int activeTextBoxIndex = -1;

    char textBufferCitySearch[64] = "Paris";
    char textBufferLat[32] = "48.8566";
    char textBufferLon[32] = "2.3522";

    char textBufferTelescope[32] = "1000.0";
    char textBufferEyepiece[32] = "25.0";
    char textBufferAfov[32] = "50.0";

    Vector2 targetListScrollOffset = { 0, 0 };

    std::vector<std::string> journalLines;
    size_t cursorX = 0;
    size_t cursorY = 0;

    char journalTitleBuffer[128] = "Observation Session 01";
    char journalTargetBuffer[64] = "Jupiter";
    bool isEditingJournalText = false;

    long long simulationTimeOffset = 0;
    HardwareConfig hardware;
    AstronomicalCatalog catalog;
    std::vector<CityGeocode> cityDatabase;

 
public:
    AstronomyApplication() {
        InitCityDatabase();
        BuildPrefixIndex();
        PerformGeocodeLookup();

        journalLines.push_back("Conditions d'observation :");
        journalLines.push_back("Transparence : 4/5");
        journalLines.push_back("Seeing (Turbulence) : Calme");
        journalLines.push_back("");
        journalLines.push_back("Notes :");

        cursorY = journalLines.size() - 1;
        cursorX = journalLines[cursorY].length();
    }

    void Execute() {
        InitWindow(1200, 800, "StellarCore Engine Pro v6.5");
        SetTargetFPS(60);
        SyncInterfaceStyles();

        while (!WindowShouldClose()) {
            ProcessInputs();
            UpdateDataPhysics();
            RenderGraphicsPipeline();
        }
        CloseWindow();
    }

private:
    void InitCityDatabase()
    {
        std::ifstream file("cities.csv");
        std::string line;

        if (!file.is_open())
        {
            std::cout << "cities.csv not found!" << std::endl;
            return;
        }

        std::getline(file, line); 

        while (std::getline(file, line))
        {
            std::stringstream ss(line);
            std::string name, country, lat, lon;

            std::getline(ss, name, ',');
            std::getline(ss, country, ',');
            std::getline(ss, lat, ',');
            std::getline(ss, lon, ',');
            try
            {
                float latitude = std::stof(lat);
                float longitude = std::stof(lon);

                cityDatabase.push_back({
                    name,
                    country,
                    latitude,
                    longitude
                    });
            }
            catch (const std::exception& e)
            {
                std::cout << "CSV invalide: " << line << std::endl;
                std::cout << e.what() << std::endl;
                continue;
            }
        }
    }

    std::string Normalize(std::string s)
    {
        std::string out;
        out.reserve(s.size());

        for (unsigned char c : s)
        {
            c = std::tolower(c);

            
            if (c >= 192) continue; 

            if (c == ' ' || c == '-' || c == '\'' || c == '.')
                continue;

            out += c;
        }

        return out;
    }

    void PerformGeocodeLookup() {
        std::string search = Normalize(textBufferCitySearch);

        bool found = false;

        for (const auto& city : cityDatabase) {
            std::string currentCity = Normalize(city.name);

            if (currentCity.find(search) != std::string::npos) {
                hardware.latitude = city.lat;
                hardware.longitude = city.lon;

                sprintf(textBufferLat, "%.4f", city.lat);
                sprintf(textBufferLon, "%.4f", city.lon);

                found = true;
                break;
            }
        }
    }

    void SyncInterfaceStyles()
    {
        if (isNightVisionActive)
        {
            // ===== NIGHT MODE =====

            GuiSetStyle(DEFAULT, BACKGROUND_COLOR,
                ColorToInt(Color{ 10, 0, 0, 255 }));

            GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,
                ColorToInt(Color{ 255, 0, 0, 255 }));

            GuiSetStyle(DEFAULT, LINE_COLOR,
                ColorToInt(Color{ 255, 0, 0, 255 }));

            GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,
                ColorToInt(Color{ 255, 0, 0, 255 }));


            // Boutons
            GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,
                ColorToInt(Color{ 60, 0, 0, 255 }));

            GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED,
                ColorToInt(Color{ 90, 0, 0, 255 }));

            GuiSetStyle(BUTTON, BASE_COLOR_PRESSED,
                ColorToInt(Color{ 120, 0, 0, 255 }));

            GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,
                ColorToInt(RED));


            // Toggles (liste des objets)
            GuiSetStyle(TOGGLE, BASE_COLOR_NORMAL,
                ColorToInt(Color{ 50, 0, 0, 255 }));

            GuiSetStyle(TOGGLE, BASE_COLOR_FOCUSED,
                ColorToInt(Color{ 80, 0, 0, 255 }));

            GuiSetStyle(TOGGLE, BASE_COLOR_PRESSED,
                ColorToInt(Color{ 120, 0, 0, 255 }));

            GuiSetStyle(TOGGLE, TEXT_COLOR_NORMAL,
                ColorToInt(RED));
        }
        else
        {
            // ===== DAY MODE =====

            GuiSetStyle(DEFAULT, BACKGROUND_COLOR,
                ColorToInt(Color{ 30, 35, 45, 255 }));

            GuiSetStyle(DEFAULT, BORDER_COLOR_NORMAL,
                ColorToInt(Color{ 100, 170, 255, 255 }));

            GuiSetStyle(DEFAULT, LINE_COLOR,
                ColorToInt(Color{ 100, 170, 255, 255 }));

            GuiSetStyle(DEFAULT, TEXT_COLOR_NORMAL,
                ColorToInt(WHITE));


            // Boutons
            GuiSetStyle(BUTTON, BASE_COLOR_NORMAL,
                ColorToInt(Color{ 70, 75, 85, 255 }));

            GuiSetStyle(BUTTON, BASE_COLOR_FOCUSED,
                ColorToInt(Color{ 100, 110, 130, 255 }));

            GuiSetStyle(BUTTON, BASE_COLOR_PRESSED,
                ColorToInt(Color{ 50, 55, 65, 255 }));

            GuiSetStyle(BUTTON, TEXT_COLOR_NORMAL,
                ColorToInt(WHITE));


            // Toggles
            GuiSetStyle(TOGGLE, BASE_COLOR_NORMAL,
                ColorToInt(Color{ 60, 65, 75, 255 }));

            GuiSetStyle(TOGGLE, BASE_COLOR_FOCUSED,
                ColorToInt(Color{ 90, 100, 120, 255 }));

            GuiSetStyle(TOGGLE, BASE_COLOR_PRESSED,
                ColorToInt(Color{ 50, 55, 65, 255 }));

            GuiSetStyle(TOGGLE, TEXT_COLOR_NORMAL,
                ColorToInt(WHITE));
        }
    }

    void ProcessInputs() {
        if (IsKeyPressed(KEY_N) && activeTextBoxIndex == -1 && !isEditingJournalText) {
            isNightVisionActive = !isNightVisionActive;
            SyncInterfaceStyles();
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON)) {
            Rectangle journalRect = { 360, 70, 800, 400 };
            if (activeTab == 2 && CheckCollisionPointRec(GetMousePosition(), journalRect)) {
                isEditingJournalText = true;
                activeTextBoxIndex = -1;
            }
            else {
                isEditingJournalText = false;
            }
        }

        if (isEditingJournalText && activeTab == 2) {
            int key = GetCharPressed();
            while (key > 0) {
                if ((key >= 32) && (key <= 125)) {
                    journalLines[cursorY].insert(cursorX, 1, (char)key);
                    cursorX++;
                }
                key = GetCharPressed();
            }

            if (IsKeyPressed(KEY_ENTER)) {
                std::string currentLineRemainder = journalLines[cursorY].substr(cursorX);
                journalLines[cursorY] = journalLines[cursorY].substr(0, cursorX);
                journalLines.insert(journalLines.begin() + cursorY + 1, currentLineRemainder);
                cursorY++;
                cursorX = 0;
            }

            if (IsKeyPressed(KEY_BACKSPACE)) {
                if (cursorX > 0) {
                    journalLines[cursorY].erase(cursorX - 1, 1);
                    cursorX--;
                }
                else if (cursorY > 0) {
                    cursorX = journalLines[cursorY - 1].length();
                    journalLines[cursorY - 1] += journalLines[cursorY];
                    journalLines.erase(journalLines.begin() + cursorY);
                    cursorY--;
                }
            }

            if (IsKeyPressed(KEY_LEFT)) {
                if (cursorX > 0) cursorX--;
                else if (cursorY > 0) { cursorY--; cursorX = journalLines[cursorY].length(); }
            }
            if (IsKeyPressed(KEY_RIGHT)) {
                if (cursorX < journalLines[cursorY].length()) cursorX++;
                else if (cursorY < journalLines.size() - 1) { cursorY++; cursorX = 0; }
            }
            if (IsKeyPressed(KEY_UP) && cursorY > 0) {
                cursorY--;
                if (cursorX > journalLines[cursorY].length()) cursorX = journalLines[cursorY].length();
            }
            if (IsKeyPressed(KEY_DOWN) && cursorY < journalLines.size() - 1) {
                cursorY++;
                if (cursorX > journalLines[cursorY].length()) cursorX = journalLines[cursorY].length();
            }
        }
    }

    void UpdateDataPhysics() {
        // --- SYNCHRONISATION UTC STRICTE POUR SÉCURISER L'HORIZON ---
        // Les formules analytiques astronomiques exigent le temps universel de Greenwich (UTC).
        // Travailler sur std::time(nullptr) brut mélangeait le temps local machine et créait un décalage.
        std::time_t localTime = std::time(nullptr);
        std::tm* utcTimeStruct = std::gmtime(&localTime);
        std::time_t utcTimeSeconds = std::mktime(utcTimeStruct);

        long long finalCalculatedUTC = static_cast<long long>(utcTimeSeconds) + simulationTimeOffset;

        try {
            hardware.latitude = std::stof(textBufferLat);
            hardware.longitude = std::stof(textBufferLon);
            hardware.telescopeFocalLength = std::stof(textBufferTelescope);
            hardware.eyepieceFocalLength = std::stof(textBufferEyepiece);
            hardware.eyepieceAfov = std::stof(textBufferAfov);
        }
        catch (...) {}

        catalog.UpdatePositions(finalCalculatedUTC, hardware.latitude, hardware.longitude);
    }

    void DrawExclusiveTextBox(Rectangle bounds, char* text, int textSize, int boxIndex) {
        bool isFocused = (activeTextBoxIndex == boxIndex);
        if (GuiTextBox(bounds, text, textSize, isFocused)) {
            activeTextBoxIndex = isFocused ? -1 : boxIndex;
        }
    }

    void RenderGraphicsPipeline() {
        Color backgroundClr = isNightVisionActive ? MODE_NUIT_BG : MODE_JOUR_BG;
        Color themeAccent = isNightVisionActive ? MODE_NUIT_ACCENT : MODE_JOUR_ACCENT;

        BeginDrawing();
        ClearBackground(backgroundClr);

        GuiGroupBox(Rectangle{ 20, 20, 300, 760 }, "CONTROL PANEL");
        if (GuiButton(Rectangle{ 40, 50, 80, 30 }, "SKY MAP")) activeTab = 0;
        if (GuiButton(Rectangle{ 130, 50, 80, 30 }, "SETTINGS")) activeTab = 1;
        if (GuiButton(Rectangle{ 220, 50, 80, 30 }, "JOURNAL")) activeTab = 2;

        if (activeTab == 0) {
            GuiCheckBox(Rectangle{ 40, 95, 15, 15 }, "Planets", &filterShowPlanets);
            GuiCheckBox(Rectangle{ 140, 95, 15, 15 }, "Stars", &filterShowStars);

            auto& database = catalog.GetObjects();

            int filteredCount = 0;
            for (auto& obj : database) {
                if (obj.type == "planet" && !filterShowPlanets) continue;
                if (obj.type == "star" && !filterShowStars) continue;
                filteredCount++;
            }

            Rectangle scrollBoxBounds = { 40, 125, 260, 545 };
            Rectangle contentGridBounds = { 0, 0, 242, static_cast<float>(filteredCount * 36 + 10) };
            Rectangle viewScrollArea = { 0, 0, 0, 0 };

            GuiScrollPanel(scrollBoxBounds, NULL, contentGridBounds, &targetListScrollOffset, &viewScrollArea);

            BeginScissorMode((int)viewScrollArea.x, (int)viewScrollArea.y, (int)viewScrollArea.width, (int)viewScrollArea.height);
            int internalBtnY = (int)(scrollBoxBounds.y + targetListScrollOffset.y + 10);

            for (auto& obj : database) {
                if (obj.type == "planet" && !filterShowPlanets) continue;
                if (obj.type == "star" && !filterShowStars) continue;

                Color statusColor = obj.isVisible ? (isNightVisionActive ? RED : GREEN) : (isNightVisionActive ? Color{ 70, 0, 0, 255 } : RED);
                bool isTarget = obj.isSelected;

                DrawCircle(60, internalBtnY + 15, 5, statusColor);
                GuiToggle(Rectangle{ 75, (float)internalBtnY, 165, 30 }, obj.name.c_str(), &isTarget);

                if (isTarget != obj.isSelected) {
                    catalog.SelectObjectByName(obj.name);
                    strcpy(journalTargetBuffer, obj.name.c_str());
                }
                internalBtnY += 36;
            }
            EndScissorMode();
            DrawRectangleLinesEx(scrollBoxBounds, 1, themeAccent);

            if (GuiButton(Rectangle{ 40, 685, 80, 30 }, "- 1 HOUR")) { simulationTimeOffset -= 3600; }
            if (GuiButton(Rectangle{ 130, 685, 80, 30 }, "+ 1 HOUR")) { simulationTimeOffset += 3600; }
            if (GuiButton(Rectangle{ 220, 685, 80, 30 }, "RESET")) { simulationTimeOffset = 0; }

            RenderSkyMapTab(themeAccent);
        }
        else if (activeTab == 1) { RenderSettingsTab(themeAccent); }
        else if (activeTab == 2) { RenderClassicLogbookTab(themeAccent); }

        bool stateBefore = isNightVisionActive;
        GuiToggle(Rectangle{ 40, 735, 260, 35 }, "NIGHT VISION MODE [N]", &isNightVisionActive);
        if (isNightVisionActive != stateBefore) SyncInterfaceStyles();

        EndDrawing();
    }

    void RenderSkyMapTab(Color themeColor) {
        GuiCheckBox(Rectangle{ 340, 20, 15, 15 }, "Stars", &mapShowStars);
        GuiCheckBox(Rectangle{ 420, 20, 15, 15 }, "Planets", &mapShowPlanets);
        GuiCheckBox(Rectangle{ 510, 20, 15, 15 }, "Lines", &mapShowConstellations);
        GuiCheckBox(Rectangle{ 590, 20, 15, 15 }, "Predict", &mapShowTrajectories);
        GuiCheckBox(Rectangle{ 680, 20, 15, 15 }, "Degree Grid", &mapShowCardinals);

        Rectangle mapViewport = { 340, 50, 840, 470 };
        DrawRectangleRec(mapViewport, isNightVisionActive ? Color{ 5, 0, 0, 255 } : Color{ 15, 15, 20, 255 });

        if (CheckCollisionPointRec(GetMousePosition(), mapViewport)) {
            viewZoom += GetMouseWheelMove() * 0.15f;
            viewZoom = std::clamp(viewZoom, 0.5f, 6.0f);

            if (IsMouseButtonDown(MOUSE_RIGHT_BUTTON)) {
                Vector2 localDrag = GetMouseDelta();
                viewPan.x += localDrag.x;
                viewPan.y += localDrag.y;
            }
        }

        BeginScissorMode((int)mapViewport.x, (int)mapViewport.y, (int)mapViewport.width, (int)mapViewport.height);

        Vector2 horizonCenter = { mapViewport.x + mapViewport.width / 2.0f + viewPan.x,
                                  mapViewport.y + mapViewport.height / 2.0f + viewPan.y };
        float calculatedRadius = 220.0f * viewZoom;

        DrawCircleLines((int)horizonCenter.x, (int)horizonCenter.y, calculatedRadius, Fade(themeColor, 0.3f));

        if (mapShowCardinals) {
            DrawText("N (0°)", (int)horizonCenter.x - 15, (int)(horizonCenter.y - calculatedRadius - 20), 12, themeColor);
            DrawText("S (180°)", (int)horizonCenter.x - 20, (int)(horizonCenter.y + calculatedRadius + 8), 12, themeColor);
            DrawText("E (90°)", (int)(horizonCenter.x + calculatedRadius + 8), (int)horizonCenter.y - 6, 12, themeColor);
            DrawText("W (270°)", (int)(horizonCenter.x - calculatedRadius - 45), (int)horizonCenter.y - 6, 12, themeColor);
        }

        auto& database = catalog.GetObjects();
        Vector2 mouseCoordinates = GetMousePosition();
        CelestialObject* closestObject = nullptr;
        CelestialObject* selection = catalog.GetSelectedObject();
        float minimumDistancePixels = 20.0f;

        if (mapShowConstellations && mapShowStars) {
            for (auto& ast : catalog.GetAsterisms()) {
                CelestialObject* s1 = nullptr;
                CelestialObject* s2 = nullptr;
                for (auto& obj : database) {
                    if (obj.name == ast.star1) s1 = &obj;
                    if (obj.name == ast.star2) s2 = &obj;
                }
                if (s1 && s2 && s1->isVisible && s2->isVisible) {
                    DrawLineEx(s1->projectedScreenPos, s2->projectedScreenPos, 1.5f, Fade(themeColor, 0.3f));
                }
            }
        }


        if (mapShowTrajectories && selection) {
            std::time_t lT = std::time(nullptr);
            std::tm* uTS = std::gmtime(&lT);
            std::time_t uTSec = std::mktime(uTS);
            long long baseTrajectoryUTC = static_cast<long long>(uTSec) + simulationTimeOffset;

            for (int h = -12; h <= 12; h++) {
                float t_lst = AstroMath::GetLocalSiderealTime(baseTrajectoryUTC + h * 3600, hardware.longitude);
                auto t_coords = AstroMath::EquatorialToHorizontal(selection->ra, selection->dec, hardware.latitude, t_lst);

                if (t_coords.alt >= 0.0f) {
                    float lR = ((90.0f - t_coords.alt) / 90.0f) * calculatedRadius;
                    float cT = AstroMath::DegToRad(t_coords.az - 90.0f);
                    Vector2 trajPos = { horizonCenter.x + lR * std::cos(cT), horizonCenter.y + lR * std::sin(cT) };

                    if (CheckCollisionPointRec(trajPos, mapViewport)) {
                        DrawCircleV(trajPos, 1.2f, Fade(themeColor, 0.5f));
                    }
                }
            }
        }


        for (auto& obj : database) {
            if (!obj.isVisible) continue; 
            if (obj.type == "planet" && !mapShowPlanets) continue;
            if (obj.type == "star" && !mapShowStars) continue;


            float linearRadialFactor = ((90.0f - obj.currentCoords.alt) / 90.0f) * calculatedRadius;
            float computationalTheta = AstroMath::DegToRad(obj.currentCoords.az - 90.0f);

            obj.projectedScreenPos.x = horizonCenter.x + linearRadialFactor * std::cos(computationalTheta);
            obj.projectedScreenPos.y = horizonCenter.y + linearRadialFactor * std::sin(computationalTheta);

            if (CheckCollisionPointRec(obj.projectedScreenPos, mapViewport)) {
                float computedSize = std::clamp(6.0f - obj.magnitude, 1.5f, 8.0f);
                Color drawingColor = (obj.type == "planet") ? GOLD : (isNightVisionActive ? RED : WHITE);

                DrawCircleV(obj.projectedScreenPos, computedSize, drawingColor);

                if (obj.isSelected) {
                    DrawCircleLines((int)obj.projectedScreenPos.x, (int)obj.projectedScreenPos.y, (int)computedSize + 5, themeColor);
                    DrawText(obj.name.c_str(), (int)obj.projectedScreenPos.x + 12, (int)obj.projectedScreenPos.y - 6, 12, drawingColor);
                }

                float currentDistance = Vector2Distance(mouseCoordinates, obj.projectedScreenPos);
                if (currentDistance < minimumDistancePixels) {
                    minimumDistancePixels = currentDistance;
                    closestObject = &obj;
                }
            }
        }

        if (IsMouseButtonPressed(MOUSE_LEFT_BUTTON) && CheckCollisionPointRec(mouseCoordinates, mapViewport)) {
            if (closestObject != nullptr) {
                catalog.SelectObjectByName(closestObject->name);
                strcpy(journalTargetBuffer, closestObject->name.c_str());
            }
        }

        EndScissorMode();
        DrawRectangleLinesEx(mapViewport, 2, themeColor);

        GuiGroupBox(Rectangle{ 340, 540, 840, 240 }, "EYEPIECE SIMULATION");

        GuiCheckBox(Rectangle{ 360, 720, 15, 15 }, "Simulate Atmosphere (Seeing)", &simulateAtmosphere);

        if (selection) {
            float magnification = hardware.telescopeFocalLength / (hardware.eyepieceFocalLength > 0.0f ? hardware.eyepieceFocalLength : 1.0f);
            float trueFieldOfViewDeg = hardware.eyepieceAfov / magnification;
            float trueFieldOfViewArcsec = trueFieldOfViewDeg * 3600.0f;

            DrawText(TextFormat("Target: %s", selection->name.c_str()), 360, 565, 20, themeColor);
            DrawText(TextFormat("Status: %s", selection->isVisible ? "VISIBLE" : "BELOW HORIZON"), 360, 595, 16, selection->isVisible ? GREEN : RED);
            DrawText(TextFormat("Altitude: %.2f deg", selection->currentCoords.alt), 360, 625, 16, WHITE);
            DrawText(TextFormat("Azimuth: %.2f deg", selection->currentCoords.az), 360, 655, 16, WHITE);

            Vector2 eyepieceCenter = { 1020, 640 };
            DrawCircleLines((int)eyepieceCenter.x, (int)eyepieceCenter.y, 80, themeColor);

            Vector2 jitter = { 0, 0 };
            if (simulateAtmosphere && selection->currentCoords.alt > 0) {
                float turbulence = std::clamp((90.0f - selection->currentCoords.alt) / 30.0f, 0.2f, 2.0f);
                jitter.x = ((rand() % 100) / 50.0f - 1.0f) * turbulence;
                jitter.y = ((rand() % 100) / 50.0f - 1.0f) * turbulence;
            }

            Vector2 drawPos = { eyepieceCenter.x + jitter.x, eyepieceCenter.y + jitter.y };

            float calculatedPixelRadius = (selection->angularSize / trueFieldOfViewArcsec) * 80.0f;
            calculatedPixelRadius = std::clamp(calculatedPixelRadius, 2.0f, 78.0f);

            DrawCircleV(drawPos, calculatedPixelRadius, isNightVisionActive ? RED : GOLD);

            // Rendu des satellites et des ombres de phases
            long long t = std::time(nullptr) + simulationTimeOffset;
            if (selection->name == "Jupiter") {
                for (int m = 1; m <= 4; m++) {
                    float offset = sin(t / (10000.0f * m)) * (calculatedPixelRadius + 5.0f + m * 8.0f);
                    DrawCircleV({ drawPos.x + offset, drawPos.y }, 1.5f, WHITE);
                }
            }
            else if (selection->name == "Saturn") {
                float titanOffset = sin(t / 80000.0f) * (calculatedRadius + 25.0f);
                DrawCircleV({ drawPos.x + titanOffset, drawPos.y + titanOffset * 0.2f }, 1.5f, WHITE);
                DrawEllipseLines((int)drawPos.x, (int)drawPos.y, (float)calculatedPixelRadius * 2.2f, (float)calculatedPixelRadius * 0.8f, GOLD);
            }
            else if (selection->name == "Mars") {
                float phobos = sin(t / 2000.0f) * (calculatedPixelRadius + 6.0f);
                float deimos = cos(t / 5000.0f) * (calculatedPixelRadius + 14.0f);
                DrawCircleV({ drawPos.x + phobos, drawPos.y - 2 }, 1.0f, LIGHTGRAY);
                DrawCircleV({ drawPos.x + deimos, drawPos.y + 3 }, 1.0f, LIGHTGRAY);
            }
            else if (selection->name == "Neptune") {
                float triton = sin(t / 15000.0f) * (calculatedPixelRadius + 12.0f);
                DrawCircleV({ drawPos.x + triton, drawPos.y + triton * 0.4f }, 1.0f, LIGHTGRAY);
            }
            else if (selection->name == "Moon") {
                CelestialObject* sun = nullptr;
                for (auto& o : database) if (o.name == "Sun") sun = &o;
                if (sun) {
                    float elongation = selection->ra - sun->ra;
                    float phaseShift = sin(AstroMath::DegToRad(elongation)) * calculatedPixelRadius;
                    Color shadow = isNightVisionActive ? Color{ 5, 0, 0, 255 } : Color{ 15, 15, 20, 255 };
                    DrawCircleV({ drawPos.x + phaseShift, drawPos.y }, calculatedPixelRadius * 0.95f, shadow);
                }
            }

            DrawText(TextFormat("Mag: %.1fx", magnification), 920, 745, 14, themeColor);
            DrawText(TextFormat("TFOV: %.2f deg", trueFieldOfViewDeg), 920, 763, 14, themeColor);
        }
    }

    void RenderSettingsTab(Color themeColor) {
        Rectangle contentArea = { 340, 20, 840, 760 };
        GuiGroupBox(contentArea, "APPLICATION SETTINGS");

        DrawText("OBSERVATION LOCATION", 360, 50, 20, themeColor);
        DrawLine(360, 75, 1150, 75, themeColor);

        GuiLabel(Rectangle{ 360, 90, 200, 30 }, "Search by City/Address:");
        DrawExclusiveTextBox(Rectangle{ 560, 90, 300, 30 }, textBufferCitySearch, 64, 1);
        if (GuiButton(Rectangle{ 880, 90, 100, 30 }, "GEOCODE")) PerformGeocodeLookup();

        GuiLabel(Rectangle{ 360, 140, 200, 30 }, "Manual Latitude:");
        DrawExclusiveTextBox(Rectangle{ 560, 140, 150, 30 }, textBufferLat, 32, 2);

        GuiLabel(Rectangle{ 360, 180, 200, 30 }, "Manual Longitude:");
        DrawExclusiveTextBox(Rectangle{ 550, 180, 150, 30 }, textBufferLon, 32, 3);

        DrawText("OPTICAL HARDWARE", 360, 260, 20, themeColor);
        DrawLine(360, 285, 1150, 285, themeColor);

        GuiLabel(Rectangle{ 360, 300, 200, 30 }, "Telescope Focal Length (mm):");
        DrawExclusiveTextBox(Rectangle{ 560, 300, 150, 30 }, textBufferTelescope, 32, 4);

        GuiLabel(Rectangle{ 370, 340, 200, 30 }, "Eyepiece Focal Length (mm):");
        DrawExclusiveTextBox(Rectangle{ 560, 340, 150, 30 }, textBufferEyepiece, 32, 5);

        GuiLabel(Rectangle{ 360, 380, 200, 30 }, "Apparent FOV (deg):");
        DrawExclusiveTextBox(Rectangle{ 560, 380, 150, 30 }, textBufferAfov, 32, 6);
    }

    void RenderClassicLogbookTab(Color themeColor) {
        Rectangle contentArea = { 340, 20, 840, 760 };
        GuiGroupBox(contentArea, "OBSERVATORY LOGBOOK");

        Rectangle journalRect = { 360, 70, 800, 400 };

        DrawRectangleRec(journalRect, isEditingJournalText ? Color{ 55, 55, 55, 255 } : Color{ 40, 40, 40, 255 });
        DrawRectangleLinesEx(journalRect, 1, isEditingJournalText ? WHITE : themeColor);

        int startX = 375, startY = 85, lineSpacing = 22;

        for (size_t i = 0; i < journalLines.size(); ++i) {
            int currentLineY = startY + (int)i * lineSpacing;
            DrawTextEx(GetFontDefault(), journalLines[i].c_str(), Vector2{ (float)startX, (float)currentLineY }, 16, 2, WHITE);

            if (isEditingJournalText && i == cursorY) {
                std::string textBeforeCursorOnLine = journalLines[i].substr(0, cursorX);
                float widthBeforeCursor = MeasureTextEx(GetFontDefault(), textBeforeCursorOnLine.c_str(), 16, 2).x;

                int cursorVisualX = startX + (int)widthBeforeCursor;
                if ((int)(GetTime() * 3) % 2 == 0) {
                    DrawRectangle(cursorVisualX, currentLineY, 2, 16, isNightVisionActive ? RED : MODE_JOUR_ACCENT);
                }
            }
        }

        DrawLine(360, 490, 1150, 490, themeColor);

        GuiLabel(Rectangle{ 360, 510, 80, 30 }, "Log Title:");
        DrawExclusiveTextBox(Rectangle{ 450, 510, 400, 30 }, journalTitleBuffer, 128, 7);

        GuiLabel(Rectangle{ 880, 510, 80, 30 }, "Target:");
        DrawExclusiveTextBox(Rectangle{ 940, 510, 200, 30 }, journalTargetBuffer, 64, 8);

        if (GuiButton(Rectangle{ 360, 720, 150, 30 }, "SAVE ENTRY TO LOG")) {
            std::ofstream logFile("Observation_Log.txt", std::ios::app);
            if (logFile.is_open()) {
                std::time_t rawNow = std::time(nullptr);
                logFile << "Title: " << journalTitleBuffer << " - " << std::ctime(&rawNow);
                logFile << "Target Asset: " << journalTargetBuffer << "\nNotes:\n";
                for (const auto& line : journalLines) logFile << line << "\n";
                logFile << "-------------------------------------------\n\n";
                logFile.close();
            }
        }
    }
};

void AstronomyApplication::BuildPrefixIndex()
{
    prefixIndex.clear();

    for (auto& city : cityDatabase)
    {
        std::string key = Normalize(city.name);

        for (size_t i = 1; i <= key.size(); i++)
        {
            std::string prefix = key.substr(0, i);
            prefixIndex[prefix].push_back(&city);
        }
    }
}

std::vector<CityGeocode*> AstronomyApplication::GetAutocomplete(const std::string& inputRaw)
{
    std::string key = Normalize(inputRaw);

    if (prefixIndex.find(key) != prefixIndex.end())
        return prefixIndex[key];

    return {};
}

int main() {


    AstronomyApplication app;
    app.Execute();
    return 0;
}