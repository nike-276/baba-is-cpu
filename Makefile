# Phase 1 build. CMake replaces this in Phase 2 once raylib lands.
#
# Targets:
#   make              — build everything into build/
#   make babaiwt      — build the main binary
#   make unit         — build the unit-test binary
#   make test         — run all .test scenarios via babaiwt
#   make check        — run unit tests + scenarios
#   make clean        — remove build artifacts

CXX      ?= clang++
CXXSTD   ?= -std=c++20
WARN     := -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor -Werror=return-type
OPT      ?= -O2
DEBUG    ?= -g
CXXFLAGS := $(CXXSTD) $(WARN) $(OPT) $(DEBUG)

BUILD    := build
SRC      := src
TESTS    := tests
SCEN     := $(TESTS)/scenarios

CORE_SRC := $(wildcard $(SRC)/core/*.cpp)
CLI_SRC  := $(wildcard $(SRC)/cli/*.cpp)
APP_SRC  := $(wildcard $(SRC)/app/*.cpp)
UNIT_SRC := $(wildcard $(TESTS)/unit/*.cpp)

CORE_OBJ := $(patsubst $(SRC)/%.cpp,$(BUILD)/%.o,$(CORE_SRC))
CLI_OBJ  := $(patsubst $(SRC)/%.cpp,$(BUILD)/%.o,$(CLI_SRC))
APP_OBJ  := $(patsubst $(SRC)/%.cpp,$(BUILD)/%.o,$(APP_SRC))
UNIT_OBJ := $(patsubst $(TESTS)/%.cpp,$(BUILD)/tests/%.o,$(UNIT_SRC))

BIN_BABA := $(BUILD)/babaiwt
BIN_UNIT := $(BUILD)/unit_tests

SCENARIOS := $(sort $(wildcard $(SCEN)/*.test))

.PHONY: all babaiwt unit test check clean
all: babaiwt unit

babaiwt: $(BIN_BABA)
unit:    $(BIN_UNIT)

$(BIN_BABA): $(CORE_OBJ) $(CLI_OBJ) $(APP_OBJ)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BIN_UNIT): $(CORE_OBJ) $(UNIT_OBJ)
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -o $@ $^

$(BUILD)/%.o: $(SRC)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -I$(SRC) -c -o $@ $<

$(BUILD)/tests/%.o: $(TESTS)/%.cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -I$(SRC) -c -o $@ $<

test: $(BIN_BABA)
	@if [ -z "$(SCENARIOS)" ]; then echo "no .test files in $(SCEN)"; exit 1; fi
	@$(BIN_BABA) $(SCENARIOS)

check: unit test
	@$(BIN_UNIT)

clean:
	rm -rf $(BUILD)
