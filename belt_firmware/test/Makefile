CXX      ?= g++
CXXFLAGS := -std=c++17 -Wall -Wextra -DUNIT_TEST -I ../include

SRC := test_belt/test_belt_main.cpp \
       ../src/ble_peripheral.cpp \
       ../src/battery.cpp

TARGET := test_runner

.PHONY: test clean

test: $(TARGET)
	./$(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $@ $^

clean:
	rm -f $(TARGET)
