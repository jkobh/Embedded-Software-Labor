// Aufgabe 1: GPS/CDMA-Decoder
//
// Liest ein Summensignal (1023 Chips) aus einer Textdatei, korreliert es
// zyklisch mit den Gold-Codes aller 24 GPS-Satelliten und gibt aus, welche
// Satelliten senden, welches Bit sie senden und wo die Chipsequenz beginnt.
//
//   ./satellite_decoder <summensignal.txt>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <numeric>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr int CHIP_COUNT    = 1023;      // Chips pro Gold-Code (2^10 - 1)
constexpr int SATELLITE_MAX = 24;        // Anzahl GPS-Satelliten
constexpr double PEAK_Z_SCORE = 5.0;     // Schwellwert fuer Peak-Erkennung

// Tap-Paare (Register a, b) pro Satellit – aus der Tabelle in Abb. 9 des Skripts.
constexpr std::array<std::pair<int, int>, SATELLITE_MAX> SAT_TAPS{{
    {2, 6},  {3, 7},  {4, 8},  {5, 9},  {1, 9},  {2, 10}, {1, 8},  {2, 9},
    {3, 10}, {2, 3},  {3, 4},  {5, 6},  {6, 7},  {7, 8},  {8, 9},  {9, 10},
    {1, 4},  {2, 5},  {3, 6},  {4, 7},  {5, 8},  {6, 9},  {1, 3},  {4, 6},
}};

// 10-stufiges Schieberegister (LFSR). Registerposition k (1-basiert) = Bit k-1,
// das Feedbackbit landet in Bit 0. Startbelegung "1111111111".
struct Lfsr {
    unsigned reg = 0x3FF;

    unsigned bit(int pos) const { return (reg >> (pos - 1)) & 1u; }
    void shift(unsigned feedback) { reg = ((reg << 1) & 0x3FFu) | (feedback & 1u); }
};

// Gold-Code eines Satelliten als +1/-1-Chipfolge.
// G1 hat das Feedback-Polynom 1 + X^3 + X^10, G2 das Polynom
// 1 + X^2 + X^3 + X^6 + X^8 + X^9 + X^10.
std::array<int, CHIP_COUNT> goldCode(int sat) {
    const auto [tapA, tapB] = SAT_TAPS[sat - 1];
    Lfsr g1;
    Lfsr g2;

    std::array<int, CHIP_COUNT> chips{};
    for (int i = 0; i < CHIP_COUNT; ++i) {
        chips[i] = (g1.bit(10) ^ g2.bit(tapA) ^ g2.bit(tapB)) ? +1 : -1;
        g1.shift(g1.bit(3) ^ g1.bit(10));
        g2.shift(g2.bit(2) ^ g2.bit(3) ^ g2.bit(6) ^ g2.bit(8) ^ g2.bit(9) ^ g2.bit(10));
    }
    return chips;
}

// Summensignal aus einer Textdatei lesen (durch Leerzeichen getrennte Zahlen).
std::vector<int> loadSignal(const std::string& filename) {
    std::ifstream in(filename);
    if (!in) throw std::runtime_error("Datei kann nicht geoeffnet werden: " + filename);

    std::vector<int> values;
    int v;
    while (in >> v) values.push_back(v);
    if (values.empty()) throw std::runtime_error("Datei enthaelt keine Werte: " + filename);
    return values;
}

// Ein erkanntes Satellitensignal.
struct Detection {
    int satelliteId;  // 1..24
    int bit;          // gesendetes Bit (0 oder 1)
    int delta;        // Startposition der Chipsequenz im Summensignal
};

// Alle Satelliten erkennen: zyklische Kreuzkorrelation mit jedem Gold-Code,
// dann pruefen, ob sich der staerkste Korrelationswert deutlich vom Rauschen
// abhebt. Vorzeichen des Peaks = gesendetes Bit (Bit 1: Sequenz unveraendert,
// Bit 0: invertiert), Position des Maximums = delta.
std::vector<Detection> decode(const std::vector<int>& signal) {
    std::vector<Detection> detections;

    for (int sat = 1; sat <= SATELLITE_MAX; ++sat) {
        const std::array<int, CHIP_COUNT> code = goldCode(sat);

        std::array<int, CHIP_COUNT> corr{};
        for (int delta = 0; delta < CHIP_COUNT; ++delta) {
            int sum = 0;
            for (int i = 0; i < CHIP_COUNT; ++i)
                sum += code[i] * signal[(delta + i) % signal.size()];
            corr[delta] = sum;
        }

        // Mittelwert und Standardabweichung der Korrelationsreihe (Rauschen).
        const double mean =
            std::accumulate(corr.begin(), corr.end(), 0.0) / corr.size();
        double sqSum = 0.0;
        for (int v : corr) sqSum += (v - mean) * (v - mean);
        const double stddev = std::sqrt(sqSum / corr.size());

        // Staerkste Verschiebung = Ort, an dem die Chipsequenz beginnt.
        const int best = static_cast<int>(
            std::max_element(corr.begin(), corr.end(), [](int a, int b) {
                return std::abs(a) < std::abs(b);
            }) - corr.begin());

        // Echter Treffer: Peak liegt >= 5 Sigma ueber dem Rauschen.
        if (stddev > 0.0 && (std::abs(corr[best]) - mean) / stddev >= PEAK_Z_SCORE)
            detections.push_back({sat, corr[best] > 0 ? 1 : 0, best});
    }
    return detections;
}

}  // namespace

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Aufruf: " << argv[0] << " <summensignal.txt>\n";
        return EXIT_FAILURE;
    }

    try {
        const std::vector<int> signal = loadSignal(argv[1]);
        if (signal.size() < CHIP_COUNT) {
            std::cerr << "Fehler: Summensignal muss mindestens " << CHIP_COUNT
                      << " Chips enthalten (Datei hat " << signal.size() << ").\n";
            return EXIT_FAILURE;
        }

        const std::vector<Detection> detections = decode(signal);
        for (const Detection& d : detections)
            std::cout << "Satellite " << d.satelliteId << " has sent bit "
                      << d.bit << " (delta = " << d.delta << ")\n";

        if (detections.empty()) std::cerr << "Kein Satellitensignal erkannt.\n";
    } catch (const std::exception& e) {
        std::cerr << "Fehler: " << e.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}