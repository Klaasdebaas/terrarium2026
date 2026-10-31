CXX ?= g++
CXXFLAGS = -std=c++17 -Wall -Wextra -Werror -I../terrarium -Ivendor
PURE_SRCS = control.cpp sensor_logic.cpp alarms.cpp config_json.cpp logformat.cpp sms_modem.cpp display_format.cpp web_json.cpp
ARDUINOJSON_URL = https://github.com/bblanchon/ArduinoJson/releases/download/v7.2.1/ArduinoJson-v7.2.1.h
BUILD = build
TEST_SRCS = $(filter-out test_main.cpp,$(wildcard test_*.cpp))
PURE = $(wildcard $(addprefix ../terrarium/,$(PURE_SRCS)))

.PHONY: test clean
test: vendor/ArduinoJson.h $(BUILD)/all_tests
	./$(BUILD)/all_tests

vendor/ArduinoJson.h:
	mkdir -p vendor
	curl -fsSL -o $@.tmp $(ARDUINOJSON_URL) && mv $@.tmp $@

$(BUILD)/all_tests: test_main.cpp $(TEST_SRCS) $(wildcard ../terrarium/*.h) $(PURE) minitest.h Makefile
	mkdir -p $(BUILD)
	$(CXX) $(CXXFLAGS) test_main.cpp $(TEST_SRCS) $(PURE) -o $@

clean:
	rm -rf $(BUILD)
