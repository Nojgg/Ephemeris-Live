#pragma once
#include <cmath>
#include <algorithm>

namespace AstroMath {
    constexpr float MY_PI = 3.14159265358979323846f;
    constexpr float DEG_TO_RAD = MY_PI / 180.0f;
    constexpr float RAD_TO_DEG = 180.0f / MY_PI;

    struct HorizonCoords {
        float alt;
        float az;
    };

    inline float DegToRad(float deg) { return deg * DEG_TO_RAD; }
    inline float RadToDeg(float rad) { return rad * RAD_TO_DEG; }

    inline float ApplyRefraction(float altDeg) {
        if (altDeg < -0.85f) return altDeg;
        float r = 1.02f / std::tan(DegToRad(altDeg + 10.3f / (altDeg + 5.11f)));
        return altDeg + (r / 60.0f);
    }

    inline double GetJulianDate(long long timeUTC) {
        return 2440587.5 + (static_cast<double>(timeUTC) / 86400.0);
    }

    inline float GetLocalSiderealTime(long long timeUTC, float longitude) {
        double jd = GetJulianDate(timeUTC);
        double d = jd - 2451545.0;
        double gst = 280.46061837 + 360.98564736629 * d;
        float lst = std::fmod(static_cast<float>(gst) + longitude, 360.0f);
        if (lst < 0.0f) lst += 360.0f;
        return lst;
    }

    inline HorizonCoords EquatorialToHorizontal(float raDeg, float decDeg, float latDeg, float lstDeg) {
        float ra = DegToRad(raDeg);
        float dec = DegToRad(decDeg);
        float lat = DegToRad(latDeg);
        float lst = DegToRad(lstDeg);

        float ha = lst - ra;

        float sinDec = std::sin(dec);
        float cosDec = std::cos(dec);
        float sinLat = std::sin(lat);
        float cosLat = std::cos(lat);
        float cosHa = std::cos(ha);

        float sinAlt = sinDec * sinLat + cosDec * cosLat * cosHa;
        sinAlt = std::clamp(sinAlt, -1.0f, 1.0f);
        float alt = RadToDeg(std::asin(sinAlt));

        float y = -cosDec * std::sin(ha);
        float x = sinDec * cosLat - cosDec * cosHa * sinLat;
        float az = RadToDeg(std::atan2(y, x));

        if (az < 0.0f) az += 360.0f;

        return { ApplyRefraction(alt), az };
    }

    // Structure interne pour stocker les coordonnées cartésiennes 3D héliocentriques
    struct Vector3D { double x, y, z; };

    // Éphémérides Képlériennes complètes JPL (Précision proche de Stellarium sur plusieurs siècles)
    inline void ComputeAnalyticalPlanet(int planetId, double julianDate, float& outRa, float& outDec, float& outAngSize) {
        double T = (julianDate - 2451545.0) / 36525.0; // Siècles Juliens depuis J2000

        // Obliquité de l'écliptique de la Terre
        double epsilon = DegToRad(static_cast<float>(23.439291 - 0.0130041 * T));

        // Éléments de l'orbite de la Terre (Nécessaire pour le calcul géocentrique des autres planètes)
        double a_earth = 1.00000261 + 0.00000003 * T;
        double e_earth = 0.01671123 - 0.00003842 * T;
        double i_earth = 0.0;
        double L_earth = 100.46457166 + 35999.37244981 * T;
        double longPeri_earth = 102.93768193 + 0.32327364 * T;
        double longNode_earth = 0.0;

        // Configuration des éléments orbitaux selon la planète (a, e, i, L, longPeri, longNode)
        double a = 1.0, e = 0.0, inclination = 0.0, L = 0.0, longPeri = 0.0, longNode = 0.0;
        outAngSize = 5.0f; // Valeur par défaut de secours

        if (planetId == 0) { // --- SOLEIL (Vu de la Terre) ---
            double M = DegToRad(static_cast<float>(L_earth - longPeri_earth));
            double E = M + e_earth * std::sin(M) * (1.0 + e_earth * std::cos(M));
            double xv = a_earth * (std::cos(E) - e_earth);
            double yv = a_earth * (std::sqrt(1.0 - e_earth * e_earth) * std::sin(E));
            double r = std::sqrt(xv * xv + yv * yv);
            double lonVac = std::atan2(yv, xv) + DegToRad(static_cast<float>(longPeri_earth));

            double x_geo = -r * std::cos(lonVac);
            double y_geo = -r * std::sin(lonVac);
            double x_equat = x_geo;
            double y_equat = y_geo * std::cos(epsilon);
            double z_equat = y_geo * std::sin(epsilon);

            outRa = RadToDeg(static_cast<float>(std::atan2(y_equat, x_equat))) + 180.0f;
            if (outRa >= 360.0f) outRa -= 360.0f;
            outDec = RadToDeg(static_cast<float>(std::asin(z_equat / r)));
            outAngSize = 1919.3f;
            return;
        }
        else if (planetId == 1) { // --- LUNE (Modèle de Meeus simplifié) ---
            double L_prime = 218.316 + 481267.881 * T;
            double D = 297.850 + 445267.111 * T;
            double M_prime = 134.963 + 477198.868 * T;
            double F = 93.272 + 483202.018 * T;

            double lambda = L_prime + 6.289 * std::sin(DegToRad(static_cast<float>(M_prime)))
                - 1.274 * std::sin(DegToRad(static_cast<float>(M_prime - 2 * D)))
                + 0.658 * std::sin(DegToRad(static_cast<float>(2 * D)));
            double beta = 5.128 * std::sin(DegToRad(static_cast<float>(F)));

            double l_rad = DegToRad(static_cast<float>(lambda));
            double b_rad = DegToRad(static_cast<float>(beta));

            double x_ec = std::cos(b_rad) * std::cos(l_rad);
            double y_ec = std::cos(b_rad) * std::sin(l_rad);
            double z_ec = std::sin(b_rad);

            double x_eq = x_ec;
            double y_eq = y_ec * std::cos(epsilon) - z_ec * std::sin(epsilon);
            double z_eq = y_ec * std::sin(epsilon) + z_ec * std::cos(epsilon);

            outRa = RadToDeg(static_cast<float>(std::atan2(y_eq, x_eq)));
            if (outRa < 0.0f) outRa += 360.0f;
            outDec = RadToDeg(static_cast<float>(std::asin(z_eq)));
            outAngSize = 1800.0f;
            return;
        }

        // --- CHARGEMENT DES COMPOSANTES HÉLIOCENTRIQUES DES PLANÈTES ---
        switch (planetId) {
        case 2: // Mercure
            a = 0.38709893 + 0.00000000 * T; e = 0.20563069 + 0.00002040 * T; inclination = 7.00487 + 0.00643 * T;
            L = 252.250323 + 149472.674112 * T; longPeri = 77.45645 + 0.15901 * T; longNode = 48.33167 - 0.12530 * T;
            outAngSize = 7.0f; break;
        case 3: // Vénus
            a = 0.72333199 + 0.00000006 * T; e = 0.00677323 - 0.00004776 * T; inclination = 3.39471 + 0.00079 * T;
            L = 181.979099 + 58517.815387 * T; longPeri = 131.53298 + 0.00201 * T; longNode = 76.68069 - 0.27769 * T;
            outAngSize = 30.0f; break;
        case 4: // Mars
            a = 1.52366231 - 0.00000197 * T; e = 0.09341233 + 0.00011902 * T; inclination = 1.85061 - 0.00724 * T;
            L = 355.453316 + 19140.302684 * T; longPeri = 336.04084 + 0.44388 * T; longNode = 49.57854 - 0.29277 * T;
            outAngSize = 12.0f; break;
        case 5: // Jupiter
            a = 5.20336301 + 0.00060737 * T; e = 0.04839266 - 0.00012880 * T; inclination = 1.30530 - 0.00415 * T;
            L = 34.404380 + 3034.746124 * T; longPeri = 14.75385 + 0.19152 * T; longNode = 100.55615 + 0.20405 * T;
            outAngSize = 45.0f; break;
        case 6: // Saturne
            a = 9.53707032 - 0.00301530 * T; e = 0.05415060 - 0.00036762 * T; inclination = 2.48446 + 0.00193 * T;
            L = 49.944323 + 1222.113794 * T; longPeri = 92.43194 - 0.41897 * T; longNode = 113.71504 - 0.36293 * T;
            outAngSize = 18.5f; break;
        case 7: // Uranus
            a = 19.19126393 + 0.00152025 * T; e = 0.04716771 - 0.00019150 * T; inclination = 0.76986 - 0.00209 * T;
            L = 313.232184 + 428.482027 * T; longPeri = 170.96424 + 0.40805 * T; longNode = 74.22988 - 0.20917 * T;
            outAngSize = 3.6f; break;
        case 8: // Neptune
            a = 30.06896348 - 0.00125196 * T; e = 0.00858587 + 0.00002514 * T; inclination = 1.76917 - 0.00364 * T;
            L = 304.880034 + 218.459453 * T; longPeri = 44.97135 - 0.15813 * T; longNode = 131.72169 - 0.00485 * T;
            outAngSize = 2.3f; break;
        case 9: // Pluton
            a = 39.48168677 - 0.00769120 * T; e = 0.24880766 + 0.00006465 * T; inclination = 17.14175 + 0.00307 * T;
            L = 238.928810 + 145.207747 * T; longPeri = 224.06676 - 0.03673 * T; longNode = 110.30347 - 0.01061 * T;
            outAngSize = 0.1f; break;
        }

        // --- CALCUL HELIOCENTRIQUE DE LA TERRE ---
        double M_earth = DegToRad(static_cast<float>(L_earth - longPeri_earth));
        double E_earth = M_earth;
        for (int loop = 0; loop < 3; loop++) E_earth = M_earth + e_earth * std::sin(E_earth);
        double xv_earth = a_earth * (std::cos(E_earth) - e_earth);
        double yv_earth = a_earth * (std::sqrt(1.0 - e_earth * e_earth) * std::sin(E_earth));
        double r_earth = std::sqrt(xv_earth * xv_earth + yv_earth * yv_earth);
        double lon_earth = std::atan2(yv_earth, xv_earth) + DegToRad(static_cast<float>(longPeri_earth));

        Vector3D earthPos = { r_earth * std::cos(lon_earth), r_earth * std::sin(lon_earth), 0.0 };

        // --- CALCUL HELIOCENTRIQUE DE LA PLANETE CIBLE ---
        double M = DegToRad(static_cast<float>(L - longPeri));
        double E = M;
        for (int loop = 0; loop < 3; loop++) E = M + e * std::sin(E);

        double xv = a * (std::cos(E) - e);
        double yv = a * (std::sqrt(1.0 - e * e) * std::sin(E));
        double r_helio = std::sqrt(xv * xv + yv * yv);
        double true_anomaly = std::atan2(yv, xv);

        double argument_of_latitude = true_anomaly + DegToRad(static_cast<float>(longPeri - longNode));
        double node_rad = DegToRad(static_cast<float>(longNode));
        double inc_rad = DegToRad(static_cast<float>(inclination));

        Vector3D planetHelio = {
            r_helio * (std::cos(node_rad) * std::cos(argument_of_latitude) - std::sin(node_rad) * std::sin(argument_of_latitude) * std::cos(inc_rad)),
            r_helio * (std::sin(node_rad) * std::cos(argument_of_latitude) + std::cos(node_rad) * std::sin(argument_of_latitude) * std::cos(inc_rad)),
            r_helio * (std::sin(argument_of_latitude) * std::sin(inc_rad))
        };

        // --- SOUSTRACTION VECTORIELLE POUR SÉCURISER LE POINT DE VUE GÉOCENTRIQUE (Depuis la Terre) ---
        double x_geo = planetHelio.x - earthPos.x;
        double y_geo = planetHelio.y - earthPos.y;
        double z_geo = planetHelio.z - earthPos.z;

        // Équatoriales conversion via l'obliquité terrestre
        double x_eq = x_geo;
        double y_eq = y_geo * std::cos(epsilon) - z_geo * std::sin(epsilon);
        double z_eq = y_geo * std::sin(epsilon) + z_geo * std::cos(epsilon);

        double distance_to_earth = std::sqrt(x_eq * x_eq + y_eq * y_eq + z_eq * z_eq);

        outRa = RadToDeg(static_cast<float>(std::atan2(y_eq, x_eq)));
        if (outRa < 0.0f) outRa += 360.0f;
        outDec = RadToDeg(static_cast<float>(std::asin(z_eq / distance_to_earth)));
    }
}