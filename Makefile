CXX := g++
CXXFLAGS := -std=c++20 -O3 -Wall -Wextra -pthread -MMD -MP

TARGET := build/tsp

.PHONY: all test clean

all: $(TARGET)

$(TARGET): src/main.cpp src/task.hpp src/tsptask.hpp src/tspgraph.hpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o $@ $<

test: $(TARGET)
	./$(TARGET) reference/tsptask-v4/dj11.tsp 10

clean:
	rm -rf build

-include build/tsp.d

.PHONY: test-stack

test-stack:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o build/test_stack tests/test_stack.cpp
	./build/test_stack

.PHONY: test-runner

test-runner:
	mkdir -p build
	$(CXX) $(CXXFLAGS) -o build/test_runner tests/test_runner.cpp
	./build/test_runner
