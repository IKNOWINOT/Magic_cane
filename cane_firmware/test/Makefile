# Magic Cane – host-side unit-test Makefile
# Usage:  make          (build + run)
#         make build    (compile only)
#         make test     (build + run)
#         make clean    (remove artefacts)

CXX       := g++
CXXFLAGS  := -std=c++17 -Wall -Wextra -Wpedantic -DUNIT_TEST
INCLUDES  := -I ../include

SRCDIR    := ../src
TESTDIR   := test_hazard

SOURCES   := $(TESTDIR)/test_main.cpp \
             $(SRCDIR)/hazard_engine.cpp \
             $(SRCDIR)/haptic_mapper.cpp

TARGET    := test_runner

.PHONY: all build test clean

all: test

build: $(TARGET)

$(TARGET): $(SOURCES)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -o $@ $^

test: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)
