# Aufgabe 1: GPS/CDMA-Decoder
#
#   make        -> baut satellite_decoder
#   make run    -> Decoder auf gps_sequence_7.txt laufen lassen
#   make clean  -> Build-Artefakte entfernen

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra -Wpedantic
BIN      := satellite_decoder

.PHONY: all run clean

all: $(BIN)

$(BIN): main.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

run: $(BIN)
	./$(BIN) gps_sequence_7.txt

clean:
	rm -f $(BIN)
