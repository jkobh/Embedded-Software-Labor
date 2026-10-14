// Aufgabe 1: GPS/CDMA-Decoder
//
// Liest ein Summensignal (1023 Chips) aus einer Textdatei, korreliert es
// zyklisch mit den Gold-Codes aller 24 GPS-Satelliten und gibt aus, welche
// Satelliten senden, welches Bit sie senden und wo die Chipsequenz beginnt.
//
// Aufruf:  ./satellite_decoder <summensignal.txt>
//
// Ablauf:
//   1. Summensignal aus Textdatei einlesen        (loadSignal)
//   2. Gold-Code fuer jeden Satelliten erzeugen   (generateGoldCode)
//   3. Signal mit jedem Gold-Code kreuzkorrelieren und Peaks erkennen (decode)
//   4. Ergebnis zeilenweise ausgeben              (main)

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

namespace
{
    constexpr int CHIP_COUNT = 1023;     // Chips pro Gold-Code (2^10 - 1)
    constexpr int SATELLITE_MAX = 24;    // Anzahl GPS-Satelliten
    constexpr double PEAK_Z_SCORE = 5.0; // Treffer: Z-Wert des Peaks >= 5.0

    // Tap-Paare (Register a, b) pro Satellit – aus der Tabelle in Abb. 9 des Skripts.
    constexpr std::array<std::pair<int, int>, SATELLITE_MAX> SAT_TAPS{{
        {2, 6},
        {3, 7},
        {4, 8},
        {5, 9},
        {1, 9},
        {2, 10},
        {1, 8},
        {2, 9},
        {3, 10},
        {2, 3},
        {3, 4},
        {5, 6},
        {6, 7},
        {7, 8},
        {8, 9},
        {9, 10},
        {1, 4},
        {2, 5},
        {3, 6},
        {4, 7},
        {5, 8},
        {6, 9},
        {1, 3},
        {4, 6},
    }};

    // 10-stufiges Schieberegister (LFSR). Registerposition k (1-basiert)
    // steht an Bit k-1, das Feedbackbit landet in Bit 0.
    // Startbelegung: "1111111111".
    struct Lfsr
    {
        unsigned registerState = 0x3FF;

        // Wert an Registerposition pos (1..10) auslesen.
        unsigned readBit(int pos) const
        {
            return (registerState >> (pos - 1)) & 1u;
        }

        // Register eine Stelle weiterschieben und Feedbackbit vorne eintragen.
        void advance(unsigned feedback)
        {
            registerState = ((registerState << 1) & 0x3FFu) | (feedback & 1u);
        }
    };

    // Gold-Code eines Satelliten als +1/-1-Chipfolge.
    // G1 hat das Feedback-Polynom 1 + X^3 + X^10,
    // G2 hat das Polynom 1 + X^2 + X^3 + X^6 + X^8 + X^9 + X^10.
    std::array<int, CHIP_COUNT> generateGoldCode(int satelliteId)
    {
        // Tap-Paare (Abgriffe) fuer diesen Satelliten aus der Tabelle.
        const auto [g2FirstTap, g2SecondTap] = SAT_TAPS[satelliteId - 1];
        Lfsr generatorG1;
        Lfsr generatorG2;

        std::array<int, CHIP_COUNT> chipSequence{};
        for (int chipIndex = 0; chipIndex < CHIP_COUNT; ++chipIndex)
        {
            // Chip = XOR aus G1-Ausgang (Position 10) und den beiden G2-Abgriffen.
            const int xorResult = generatorG1.readBit(10) ^
                                  generatorG2.readBit(g2FirstTap) ^
                                  generatorG2.readBit(g2SecondTap);
            chipSequence[chipIndex] = xorResult ? +1 : -1;

            // Naechsten Chip berechnen: Feedback der Register einschieben.
            generatorG1.advance(generatorG1.readBit(3) ^ generatorG1.readBit(10));
            generatorG2.advance(generatorG2.readBit(2) ^ generatorG2.readBit(3) ^
                                generatorG2.readBit(6) ^ generatorG2.readBit(8) ^
                                generatorG2.readBit(9) ^ generatorG2.readBit(10));
        }
        return chipSequence;
    }

    // Summensignal aus einer Textdatei lesen (durch Leerzeichen getrennte Zahlen).
    std::vector<int> loadSignal(const std::string &filename)
    {
        std::ifstream inputFile(filename);
        if (!inputFile)
            throw std::runtime_error("Datei kann nicht geoeffnet werden: " + filename);

        std::vector<int> chipValues;
        int value;
        while (inputFile >> value)
            chipValues.push_back(value);

        if (chipValues.empty())
            throw std::runtime_error("Datei enthaelt keine Werte: " + filename);
        return chipValues;
    }

    // Ein erkanntes Satellitensignal.
    struct Detection
    {
        int satelliteId; // 1..24
        int bit;         // gesendetes Bit (0 oder 1)
        int delta;       // Startposition der Chipsequenz im Summensignal
    };

    // Alle Satelliten erkennen: zyklische Kreuzkorrelation mit jedem Gold-Code,
    // dann pruefen, ob sich der staerkste Korrelationswert deutlich vom Rauschen
    // abhebt. Vorzeichen des Peaks = gesendetes Bit (Bit 1: Sequenz unveraendert,
    // Bit 0: invertiert), Position des Maximums = delta.
    std::vector<Detection> decode(const std::vector<int> &sumSignal)
    {
        std::vector<Detection> detections;

        for (int satelliteId = 1; satelliteId <= SATELLITE_MAX; ++satelliteId)
        {
            const std::array<int, CHIP_COUNT> satelliteCode = generateGoldCode(satelliteId);

            // Kreuzkorrelation: Code gegen jede moegliche Startposition (delta)
            // im Summensignal verschieben. Das Modulo macht die Suche zyklisch,
            // damit auch mitten im Signal beginnende Sequenzen gefunden werden.
            std::array<int, CHIP_COUNT> correlationProfile{};
            for (int delta = 0; delta < CHIP_COUNT; ++delta)
            {
                int correlationTotal = 0;
                for (int chipIndex = 0; chipIndex < CHIP_COUNT; ++chipIndex)
                    correlationTotal += satelliteCode[chipIndex] *
                                        sumSignal[(delta + chipIndex) % sumSignal.size()];
                correlationProfile[delta] = correlationTotal;
            }

            // Mittelwert und Standardabweichung der Korrelationsreihe (= Rauschen).
            const double correlationMean =
                std::accumulate(correlationProfile.begin(), correlationProfile.end(), 0.0) /
                correlationProfile.size();

            double squaredErrorSum = 0.0;
            for (int correlationValue : correlationProfile)
                squaredErrorSum += (correlationValue - correlationMean) *
                                   (correlationValue - correlationMean);
            const double correlationStdDev =
                std::sqrt(squaredErrorSum / correlationProfile.size());

            // Staerkste Verschiebung = Ort, an dem die Chipsequenz beginnt (delta).
            const int bestDelta = static_cast<int>(
                std::max_element(correlationProfile.begin(), correlationProfile.end(),
                                 [](int valueA, int valueB)
                                 { return std::abs(valueA) < std::abs(valueB); }) -
                correlationProfile.begin());

            // Echter Treffer: Peak liegt mindestens 5 Sigma ueber dem Rauschen.
            // Vorzeichen des Peaks: positiv = Bit 1 (Sequenz unveraendert),
            //                       negativ = Bit 0 (Sequenz invertiert).
            const double peakZScore =
                correlationStdDev > 0.0
                    ? (std::abs(correlationProfile[bestDelta]) - correlationMean) /
                          correlationStdDev
                    : 0.0;

#ifdef DEBUG
            // Zwischenergebnis fuer jeden Satelliten ausgeben (nur im Debug-Build).
            std::cerr << "[Debug] Satellit " << satelliteId
                      << ": Peak = " << correlationProfile[bestDelta]
                      << " (delta = " << bestDelta << ")"
                      << ", Mittelwert = " << correlationMean
                      << ", StdDev = " << correlationStdDev
                      << ", Z-Wert = " << peakZScore << '\n';
#endif

            if (peakZScore >= PEAK_Z_SCORE)
            {
                const int transmittedBit = correlationProfile[bestDelta] > 0 ? 1 : 0;
                detections.push_back({satelliteId, transmittedBit, bestDelta});
            }
        }
        return detections;
    }

} // namespace

int main(int argc, char *argv[])
{
    if (argc != 2)
    {
        std::cerr << "Aufruf: " << argv[0] << " <summensignal.txt>\n";
        return EXIT_FAILURE;
    }

    try
    {
        const std::vector<int> sumSignal = loadSignal(argv[1]);
#ifdef DEBUG
        std::cerr << "[Debug] " << sumSignal.size() << " Chips aus "
                  << argv[1] << " geladen.\n";
#endif
        if (sumSignal.size() < CHIP_COUNT)
        {
            std::cerr << "Fehler: Summensignal muss mindestens " << CHIP_COUNT
                      << " Chips enthalten (Datei hat " << sumSignal.size() << ").\n";
            return EXIT_FAILURE;
        }

        const std::vector<Detection> detections = decode(sumSignal);
        for (const Detection &detection : detections)
        {
            std::cout << "Satellite " << detection.satelliteId
                      << " has sent bit " << detection.bit
                      << " (delta = " << detection.delta << ")\n";
        }

        if (detections.empty())
            std::cerr << "Kein Satellitensignal erkannt.\n";
    }
    catch (const std::exception &error)
    {
        std::cerr << "Fehler: " << error.what() << '\n';
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}