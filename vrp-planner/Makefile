CXX = g++
CXXFLAGS = -std=c++17 -O2 -Wall -Wextra -Wpedantic
CPPFLAGS = -Iinclude
ifeq ($(OS),Windows_NT)
EXE = .exe
else
EXE =
endif
SOURCES = $(filter-out src/main.cpp,$(wildcard src/*.cpp))
HEADERS = $(wildcard include/*.hpp include/solvers/*.hpp)
.PHONY: all test bench clean web web-test
all: vrp$(EXE)
vrp$(EXE): $(SOURCES) src/main.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) src/main.cpp -o $@
tests/vrp_tests$(EXE): $(SOURCES) tests/test.cpp $(HEADERS)
	$(CXX) $(CPPFLAGS) $(CXXFLAGS) $(SOURCES) tests/test.cpp -o $@
test: tests/vrp_tests$(EXE) vrp$(EXE)
	./tests/vrp_tests$(EXE)
bench: vrp$(EXE)
	./vrp$(EXE) bench --runs 10 --seed 42 --out results/results.csv
web: vrp$(EXE)
	node web/server.mjs
web-test: vrp$(EXE)
	node --test web/tests/*.test.mjs
clean:
	$(RM) vrp vrp.exe tests/vrp_tests tests/vrp_tests.exe
