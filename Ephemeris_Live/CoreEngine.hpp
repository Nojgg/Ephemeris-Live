#pragma once
#include "AstroMath.hpp"
#include "raylib.h"
#include <string>
#include <vector>




struct CelestialObject {
    std::string name;
    std::string type;
    int internalId;
    float ra;
    float dec;
    float magnitude;
    float angularSize;
    AstroMath::HorizonCoords currentCoords;
    Vector2 projectedScreenPos;
    bool isVisible;
    bool isSelected;
};

struct ConstellationLine {
    std::string star1;
    std::string star2;
};

class AstronomicalCatalog {
private:
    std::vector<CelestialObject> items;
    std::vector<ConstellationLine> asterisms;


public:
    AstronomicalCatalog() {
        LoadRealDatabase();
    }

    void LoadRealDatabase() {
        items.clear();
        asterisms.clear();

        // Système Solaire héliocentrique complet projeté sur la Terre
        items.push_back({ "Sun", "planet", 0, 0.0f, 0.0f, -26.74f, 1920.0f, {0,0}, {0,0}, false, true });
        items.push_back({ "Moon", "planet", 1, 0.0f, 0.0f, -12.74f, 1800.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Mercury", "planet", 2, 0.0f, 0.0f, 0.50f, 7.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Venus", "planet", 3, 0.0f, 0.0f, -4.40f, 30.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Mars", "planet", 4, 0.0f, 0.0f, 0.70f, 10.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Jupiter", "planet", 5, 0.0f, 0.0f, -2.20f, 45.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Saturn", "planet", 6, 0.0f, 0.0f, 0.46f, 18.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Uranus", "planet", 7, 0.0f, 0.0f, 5.70f, 3.6f, {0,0}, {0,0}, false, false });
        items.push_back({ "Neptune", "planet", 8, 0.0f, 0.0f, 7.80f, 2.3f, {0,0}, {0,0}, false, false });
        items.push_back({ "Pluto", "planet", 9, 0.0f, 0.0f, 13.60f, 0.1f, {0,0}, {0,0}, false, false });

        // Étoiles de référence (Bright Star)
        items.push_back({ "Sirius (Alpha CMa)", "star", -1, 101.287f, -16.716f, -1.46f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Canopus (Alpha Car)", "star", -1, 95.987f, -52.695f, -0.74f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Rigel Kentaurus (Alpha Cen)", "star", -1, 219.902f, -60.833f, -0.27f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Arcturus (Alpha Boo)", "star", -1, 213.915f, 19.182f, -0.05f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Vega (Alpha Lyr)", "star", -1, 279.234f, 38.783f, 0.03f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Capella (Alpha Aur)", "star", -1, 79.172f, 45.997f, 0.08f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Rigel (Beta Ori)", "star", -1, 78.634f, -8.201f, 0.13f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Procyon (Alpha CMi)", "star", -1, 114.825f, 5.224f, 0.34f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Betelgeuse (Alpha Ori)", "star", -1, 88.792f, 7.407f, 0.42f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Altair (Alpha Aql)", "star", -1, 297.695f, 8.868f, 0.76f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Aldebaran (Alpha Tau)", "star", -1, 68.980f, 16.509f, 0.87f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Antares (Alpha Sco)", "star", -1, 247.351f, -26.431f, 1.05f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Spica (Alpha Vir)", "star", -1, 201.298f, -11.161f, 0.98f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Pollux (Beta Gem)", "star", -1, 116.328f, 28.026f, 1.14f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Deneb (Alpha Cyg)", "star", -1, 310.357f, 45.280f, 1.25f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Regulus (Alpha Leo)", "star", -1, 152.092f, 11.967f, 1.36f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Castor (Alpha Gem)", "star", -1, 113.649f, 31.888f, 1.58f, 0.0f, {0,0}, {0,0}, false, false });
        items.push_back({ "Polaris (Alpha UMi)", "star", -1, 37.954f, 89.264f, 1.97f, 0.0f, {0,0}, {0,0}, false, false });

        // Constellations
        asterisms.push_back({ "Vega (Alpha Lyr)", "Altair (Alpha Aql)" });
        asterisms.push_back({ "Altair (Alpha Aql)", "Deneb (Alpha Cyg)" });
        asterisms.push_back({ "Deneb (Alpha Cyg)", "Vega (Alpha Lyr)" });
        asterisms.push_back({ "Sirius (Alpha CMa)", "Procyon (Alpha CMi)" });
        asterisms.push_back({ "Procyon (Alpha CMi)", "Betelgeuse (Alpha Ori)" });
        asterisms.push_back({ "Betelgeuse (Alpha Ori)", "Sirius (Alpha CMa)" });
        asterisms.push_back({ "Betelgeuse (Alpha Ori)", "Rigel (Beta Ori)" });
        asterisms.push_back({ "Castor (Alpha Gem)", "Pollux (Beta Gem)" });
    }

    void UpdatePositions(long long timeUTC, float latitude, float longitude) {
        double julianDate = AstroMath::GetJulianDate(timeUTC);
        float currentLst = AstroMath::GetLocalSiderealTime(timeUTC, longitude);

        for (auto& obj : items) {
            if (obj.type == "planet") {
                AstroMath::ComputeAnalyticalPlanet(obj.internalId, julianDate, obj.ra, obj.dec, obj.angularSize);
            }
            obj.currentCoords = AstroMath::EquatorialToHorizontal(obj.ra, obj.dec, latitude, currentLst);
            obj.isVisible = (obj.currentCoords.alt >= 0.0f);
        }
    }

    std::vector<CelestialObject>& GetObjects() { return items; }
    std::vector<ConstellationLine>& GetAsterisms() { return asterisms; }

    CelestialObject* GetSelectedObject() {
        for (auto& obj : items) {
            if (obj.isSelected) return &obj;
        }
        return nullptr;
    }

    void SelectObjectByName(const std::string& name) {
        for (auto& obj : items) {
            obj.isSelected = (obj.name == name);
        }
    }
};

