/*TODO : Finish backend (calculations, parser, etc.)
		 Implement frontend (UI, rendering, etc.)
		 Add error handling and edge cases
		 Optimize performance if necessary
		 Test with various celestial objects and time inputs*/

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

		 // --- Bibliothèques réseau indispensables pour Windows ---
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "crypt32.lib")

#include "httplib.h" // Inclure httplib.h (que tu as déjà)

// --- Configuration des paramètres ---
float LATITUDE = 46.2276f;
float LONGITUDE = 2.2137f;
const float PI = 3.14159265358979323846f; // Redéfinition de PI car Raylib a été retiré
const std::string CENTER = "coord@399";
const std::string STEP_SIZE = "1 m";

struct PlanetData {
	std::string name;
	std::string id;
	std::string type;
};

// Plus besoin des couleurs ici, le Web (CSS/JS) s'en chargera !
std::vector<PlanetData> planets = {
	{"Sun", "10", "star"}, {"Mercury", "199", "planet"},
	{"Venus", "299", "planet"}, {"Mars", "499", "planet"},
	{"Jupiter", "599", "planet"}, {"Saturn", "699", "planet"},
	{"Uranus", "799", "planet"}, {"Neptune", "899", "planet"},
	{"Pluto", "999", "planet"}, {"Moon", "301", "satellite"}
};

// --- Mathématiques et conversions ---

float degToRad(float degrees) { return degrees * (PI / 180.0f); }
float RadtoDeg(float radians) { return radians * (180.0f / PI); }

float refraction(float alt_deg) {
	if (alt_deg < -0.8f) { return alt_deg; }
	float r = 1.02f / tan(degToRad(alt_deg + 10.3f / (alt_deg + 5.11f)));
	return alt_deg + r / 60.0f;
}

float lst_deg(float timeUTC) {
	float jd = 2440587.5f + timeUTC / 86400.0f;
	float T = (jd - 2451545.0f) / 36525.0f;
	float gst = 280.46061837f + 360.98564736629f * (jd - 2451545.0f) + T * T * (0.000387933f - T / 38710000.0f);

	gst = fmod(gst, 360.0f);
	return gst;
}

// --- Calculs Astronomiques Optimisés ---

float AltAzConverter(float raDeg, float decDeg, float timeUTC) {
	float ra = degToRad(raDeg);
	float dec = degToRad(decDeg);
	float lst = degToRad(lst_deg(timeUTC));
	float ha = lst - ra;

	float latRad = degToRad(LATITUDE);
	float sinLat = sin(latRad);
	float cosLat = cos(latRad);
	float sinDec = sin(dec);
	float cosDec = cos(dec);

	float sin_Alt = sinDec * sinLat + cosDec * cosLat * cos(ha);
	sin_Alt = fmaxf(-1.0f, fminf(1.0f, sin_Alt));
	float alt = asin(sin_Alt);

	float cos_Az = (sinDec - sin(alt) * sinLat) / (cos(alt) * cosLat);
	cos_Az = fmaxf(-1.0f, fminf(1.0f, cos_Az));
	float az = acos(cos_Az);

	return refraction(RadtoDeg(alt));
}

float RiseTransitSet(float ra_deg, float dec_deg, float dateUTC) {
	float dec = degToRad(dec_deg);
	float latRad = degToRad(LATITUDE);

	float cosH = -(sin(latRad) * sin(dec)) / (cos(latRad) * cos(dec));

	if (cosH < -1.0f) { return INFINITY; }
	if (cosH > 1.0f) { return NAN; }

	float ra = degToRad(ra_deg);
	float lst = degToRad(lst_deg(dateUTC));
	float H = acos(cosH);

	float tr = dateUTC + (ra - lst) / (2.0f * PI) * 86400.0f;
	return static_cast<float>(time(nullptr));
}

// --- API et Réseau ---

std::string urlencode(const std::string& str) {
	std::ostringstream escaped;
	escaped.fill('0');
	escaped << std::hex;
	for (char c : str) {
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			escaped << c;
		}
		else {
			escaped << '%' << std::setw(2) << int((unsigned char)c);
		}
	}
	return escaped.str();
}

std::string build_url(const std::string& command, const std::string& start_time, const std::string& stop_time) {
	std::string base = "https://ssd.jpl.nasa.gov/api/horizons.api?";
	base += "format=text&COMMAND=" + urlencode("'" + command + "'");
	base += "&OBJ_DATA='NO'&MAKE_EPHEM='YES'&EPHEM_TYPE='OBSERVER'";
	base += "&CENTER=" + urlencode("'" + CENTER + "'");
	base += "&START_TIME=" + urlencode("'" + start_time + "'");
	base += "&STOP_TIME=" + urlencode("'" + stop_time + "'");
	base += "&STEP_SIZE=" + urlencode("'" + STEP_SIZE + "'");
	base += "&QUANTITIES='1,13'&ANG_FORMAT='DEG'&EXTRA_PREC='YES'";
	return base;
}

std::string exec(const char* cmd) {
	std::array<char, 128> buffer;
	std::string result;
	std::unique_ptr<FILE, decltype(&_pclose)> pipe(_popen(cmd, "r"), _pclose);
	if (!pipe) throw std::runtime_error("Erreur avec _popen()");
	while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr) {
		result += buffer.data();
	}
	return result;
}

bool parse_horizons(const std::string& text, double& out_ra, double& out_dec, double& out_ang) {
	std::istringstream stream(text);
	std::string line;
	bool in_table = false;

	while (std::getline(stream, line)) {
		if (line.find("$$SOE") != std::string::npos) {
			in_table = true;
			continue;
		}
		if (line.find("$$EOE") != std::string::npos) {
			in_table = false;
		}
		if (in_table && line.find_first_not_of(" \r\n\t") != std::string::npos) {
			std::vector<std::string> tokens;
			std::string token;
			std::istringstream linestream(line);
			tokens.reserve(15);
			while (linestream >> token) {
				tokens.push_back(token);
			}
			if (tokens.size() >= 5) {
				try {
					out_ra = std::stod(tokens[tokens.size() - 3]);
					out_dec = std::stod(tokens[tokens.size() - 2]);
					out_ang = std::stod(tokens[tokens.size() - 1]);
					return true;
				}
				catch (...) {
					return false;
				}
			}
		}
	}
	return false;
}

std::string generate_json(const PlanetData& planet, double alt, double az, const std::string& cardinal, const std::string& rise, const std::string& transit, const std::string& set, bool visible, double ang_size) {
	std::ostringstream json;
	json << std::fixed << std::setprecision(4);
	json << "{\n  \"planet\": \"" << planet.name << "\",\n";
	json << "  \"alt\": " << alt << ",\n  \"az\": " << az << ",\n";
	json << "  \"cardinal\": \"" << cardinal << "\",\n";
	json << "  \"rise\": \"" << rise << "\",\n  \"transit\": \"" << transit << "\",\n";
	json << "  \"set\": \"" << set << "\",\n  \"visible\": " << (visible ? "true" : "false") << ",\n";
	json << "  \"ang_size\": " << ang_size << "\n}";
	return json.str();
}

// --- Point d'Entrée Principal ---

int main() {
	httplib::Server svr;

	// Cette route permet au Web de demander des données
	svr.Get("/api", [&](const httplib::Request& req, httplib::Response& res) {
		std::string pName = req.get_param_value("planet");

		// 1. Trouver l'ID et les données de la planète
		PlanetData currentPlanet = planets[0]; // Soleil par défaut
		for (const auto& p : planets) {
			if (p.name == pName) {
				currentPlanet = p;
				break;
			}
		}

		// 2. Préparer les dates (UTC)
		time_t now = std::time(nullptr);
		struct tm* gmt = std::gmtime(&now);
		char start[30], stop[30];
		std::strftime(start, sizeof(start), "%Y-%m-%d %H:%M:%S", gmt);
		time_t end = now + 60;
		struct tm* gmt_end = std::gmtime(&end);
		std::strftime(stop, sizeof(stop), "%Y-%m-%d %H:%M:%S", gmt_end);

		// 3. Récupérer et parser les données NASA
		std::string url = build_url(currentPlanet.id, start, stop);
		std::string rawData = exec(("curl -s -L \"" + url + "\"").c_str());

		double ra, dec, ang;
		if (parse_horizons(rawData, ra, dec, ang)) {
			float alt = AltAzConverter((float)ra, (float)dec, (float)now);

			// 4. Construire le vrai JSON en passant la bonne planète
			std::string json = generate_json(currentPlanet, alt, 0.0, "S", "00:00", "12:00", "23:59", alt > 0, ang);
			res.set_content(json, "application/json");
		}
		else {
			res.set_content("{\"error\":\"NASA data failed\"}", "application/json");
		}

		res.set_header("Access-Control-Allow-Origin", "*");
		});

	std::cout << "--- EPHEMERIS BACKEND ---" << std::endl;
	std::cout << "Serveur lance et en ecoute sur http://localhost:8080" << std::endl;
	std::cout << "Ne fermez pas cette fenetre noire." << std::endl;

	svr.listen("0.0.0.0", 8080);
	return 0;
}