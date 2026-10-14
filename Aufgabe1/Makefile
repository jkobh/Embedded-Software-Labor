# Aufgabe 1: GPS/CDMA-Decoder
#
#   make        -> baut satellite_decoder (Release)
#   make run    -> Decoder auf gps_sequence_7.txt laufen lassen
#   make debug  -> Debug-Build bauen und mit Zwischenergebnissen ausfuehren
#   make clean  -> Build-Artefakte entfernen

CXX        := g++
CXXFLAGS   := -std=c++17 -O2 -Wall -Wextra -Wpedantic
DEBUGFLAGS := -std=c++17 -O0 -g -DDEBUG -Wall -Wextra -Wpedantic
BIN        := satellite_decoder
BIN_DEBUG  := satellite_decoder_debug

.PHONY: all run debug clean

all: $(BIN)

$(BIN): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(BIN)
	./$(BIN) gps_sequence_7.txt

debug: $(BIN_DEBUG)
	./$(BIN_DEBUG) gps_sequence_7.txt

$(BIN_DEBUG): main.cpp
	$(CXX) $(DEBUGFLAGS) -o $@ $<

clean:
	rm -f $(BIN) $(BIN_DEBUG)
