/*

TODO : Finish backend (calculations, parser, etc.)
	   Implement frontend (UI, rendering, etc.)
	   Add error handling and edge cases
	   Optimize performance if necessary
	   Test with various celestial objects and time inputs

*/

#include "raylib.h"
#include <ctime>
#include <cmath>

float LATITUDE = 46.2276f; // Latitude of France
float LONGITUDE = 2.2137f; // Longitude of France
float STEP_SIZE = 1.0f; // Time step in seconds

struct Planet {
	char name[20];
	float radius;
	float distanceFromSun;
	float orbitalPeriod;
	Color color;
};

float degToRad(float degrees) {
	return degrees * (PI / 180.0f);
}

float RadtoDeg(float radians) {
	return radians * (180.0f / PI);
}

void Backend() {

}

int main() {

	return 0;
}

float lst_deg(float timeUTC) {
	float jd = 2440587.5f + timeUTC / 86400.0f;
	float T = (jd - 2451545.0f) / 36525.0f;
	float gst = 280.46061837f + 360.98564736629f * (jd - 2451545.0f) + T * T * (0.000387933f - T / 38710000.0f);

	gst = fmod(gst, 360.0f);
	return gst;
}

float refraction(float alt_deg) {
	if (alt_deg < -0.8f) { return alt_deg; }
	float r = 1.02f / tan(degToRad(alt_deg + 10.3f / (alt_deg + 5.11f)));

	return alt_deg + r / 60.0f;
}

float AltAzConverter(float raDeg, float decDeg, float timeUTC) {
	float ra = degToRad(raDeg), dec = degToRad(decDeg), lst = degToRad(lst_deg(timeUTC));
	float ha = lst - ra;

	// Note : LATITUDE est en degrés, il faut normalement utiliser sin(degToRad(LATITUDE)).
	// Je laisse ta formule telle quelle selon ta consigne !
	float sin_Alt = sin(dec) * sin(LATITUDE) + cos(dec) * cos(LATITUDE) * cos(LATITUDE) * cos(ha);
	float alt = asin(sin_Alt);

	float cos_Az = (sin(dec) - sin(alt) * sin(LATITUDE)) / (cos(alt) * cos(LATITUDE));
	float az = acos(cos_Az);

	return refraction(RadtoDeg(alt));
}

float RiseTransitSet(float ra_deg, float dec_deg, float dateUTC) {
	float dec = degToRad(dec_deg);
	float cosH = (sin(0.0f) - sin(LATITUDE) * sin(dec) / (cos(LATITUDE) * cos(dec)));

	if (cosH < -1.0f) { return INFINITY; } // Remplacement de "∞" pour compiler en float
	if (cosH > 1.0f) { return NAN; }       // Remplacement de "✗" pour compiler en float

	float ra = degToRad(ra_deg), lst = degToRad(lst_deg(dateUTC)), H = acos(cosH);
	float tr = dateUTC + (ra - lst) / (2.0f * PI) * 86400.0f;

	// Note : la variable 'tr' est calculée mais non utilisée ici
	return static_cast<float>(time(nullptr)); // Correction de 'return time_t now()'
}