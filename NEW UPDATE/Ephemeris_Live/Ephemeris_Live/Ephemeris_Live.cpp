#define _CRT_SECURE_NO_WARNINGS
#include <iostream>
#include <string>
#include <vector>
#include <cmath>
#include <sstream>
#include <iomanip>
#include <array>
#include <memory>
#include <ctime>

// --- Bibliothèques réseau ---
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")
#include "httplib.h" 

// --- Configuration ---
float LATITUDE = 46.2276f;
float LONGITUDE = 2.2137f;
const float PI = 3.14159265358979323846f;
const std::string CENTER = "coord@399";
const std::string STEP_SIZE = "1 m";

struct PlanetData {
    std::string name;
    std::string id;
    std::string type;
};

std::vector<PlanetData> planets = {
    {"Sun", "10", "star"}, {"Mercury", "199", "planet"},
    {"Venus", "299", "planet"}, {"Mars", "499", "planet"},
    {"Jupiter", "599", "planet"}, {"Saturn", "699", "planet"},
    {"Uranus", "799", "planet"}, {"Neptune", "899", "planet"},
    {"Pluto", "999", "planet"}, {"Moon", "301", "satellite"}
};

// --- Math & Astronomy Helpers (Defined first so they can be called) ---
float degToRad(float degrees) { return degrees * (PI / 180.0f); }
float RadtoDeg(float radians) { return radians * (180.0f / PI); }

float refraction(float alt_deg) {
    if (alt_deg < -0.8f) return alt_deg;
    float r = 1.02f / tan(degToRad(alt_deg + 10.3f / (alt_deg + 5.11f)));
    return alt_deg + r / 60.0f;
}

float lst_deg(float timeUTC) {
    float jd = 2440587.5f + timeUTC / 86400.0f;
    float T = (jd - 2451545.0f) / 36525.0f;
    float gst = 280.46061837f + 360.98564736629f * (jd - 2451545.0f) + T * T * (0.000387933f - T / 38710000.0f);
    return fmod(gst, 360.0f);
}

// Struct for Return values
struct HorizonCoords {
    float alt;
    float az;
};

// Now GetAltAz works because the helpers are already defined above
HorizonCoords GetAltAz(float raDeg, float decDeg, float timeUTC) {
    float ra = degToRad(raDeg), dec = degToRad(decDeg);
    float latRad = degToRad(LATITUDE);
    float lst = degToRad(lst_deg(timeUTC));
    float ha = lst - ra;

    // Altitude Formula
    float sin_alt = sin(dec) * sin(latRad) + cos(dec) * cos(latRad) * cos(ha);
    float alt = RadtoDeg(asin(fmaxf(-1.0f, fminf(1.0f, sin_alt))));

    // Azimuth Formula
    float y = -cos(dec) * sin(ha);
    float x = sin(dec) * cos(latRad) - cos(dec) * cos(ha) * sin(latRad);
    float az = RadtoDeg(atan2(y, x));

    // Normalize to 0-360
    if (az < 0) az += 360.0f;

    return { refraction(alt), az };
}

// --- NASA API Logic ---
std::string urlencode(const std::string& str) {
    std::ostringstream escaped;
    escaped << std::hex;
    for (char c : str) {
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') escaped << c;
        else escaped << '%' << std::setw(2) << int((unsigned char)c);
    }
    return escaped.str();
}

std::string build_url(const std::string& command, const std::string& start, const std::string& stop) {
    return "https://ssd.jpl.nasa.gov/api/horizons.api?format=text&COMMAND='" + urlencode(command) +
        "'&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='OBSERVER'&CENTER='" + urlencode(CENTER) +
        "'&START_TIME='" + urlencode(start) + "'&STOP_TIME='" + urlencode(stop) +
        "'&STEP_SIZE='" + urlencode(STEP_SIZE) + "'&QUANTITIES='1,13'&ANG_FORMAT='DEG'&EXTRA_PREC='YES'";
}

std::string exec(const char* cmd) {
    std::array<char, 128> buffer;
    std::string result;
    auto pipe = _popen(cmd, "r");
    if (!pipe) return "";
    while (fgets(buffer.data(), buffer.size(), pipe) != nullptr) result += buffer.data();
    _pclose(pipe);
    return result;
}

bool parse_horizons(const std::string& text, double& ra, double& dec, double& ang) {
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (line.find("$$SOE") != std::string::npos) {
            std::getline(stream, line);
            std::istringstream ls(line);
            std::vector<std::string> tokens;
            std::string t;
            while (ls >> t) tokens.push_back(t);
            if (tokens.size() >= 3) {
                ra = std::stod(tokens[tokens.size() - 3]);
                dec = std::stod(tokens[tokens.size() - 2]);
                ang = std::stod(tokens[tokens.size() - 1]);
                return true;
            }
        }
    }
    return false;
}

int main() {
    httplib::Server svr;

    svr.set_default_headers({ {"Access-Control-Allow-Origin", "*"} });

    svr.Get("/api", [&](const httplib::Request& req, httplib::Response& res) {
        std::string pName = req.get_param_value("planet");
        std::string pID = "10";
        for (auto& p : planets) if (p.name == pName) pID = p.id;

        time_t now = std::time(nullptr);
        char start[30], stop[30];
        std::strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", std::gmtime(&now));
        time_t end = now + 60;
        std::strftime(stop, sizeof(stop), "%Y-%m-%d %H:%M:%S", std::gmtime(&end));

        std::string rawData = exec(("curl -s -L \"" + build_url(pID, start, stop) + "\"").c_str());
        double ra, dec, ang;

        if (parse_horizons(rawData, ra, dec, ang)) {
            HorizonCoords coords = GetAltAz((float)ra, (float)dec, (float)now);

            std::ostringstream json;
            json << "{\"planet\":\"" << pName << "\",\"alt\":" << coords.alt
                << ",\"az\":" << coords.az
                << ",\"ang_size\":" << ang << "}";

            res.set_content(json.str(), "application/json");
        }
        else {
            res.set_content("{\"error\":\"Data retrieval failed\"}", "application/json");
        }
        });

    std::cout << "Server running on http://localhost:8080. Call /api?planet=Sun" << std::endl;
    svr.listen("0.0.0.0", 8080);
    return 0;
}