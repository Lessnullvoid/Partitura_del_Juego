CXX ?= clang++
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I../src
SRC = ../src/data/PdjvRuntime.cpp
TEST = test_pdjv.cpp
OUT = test_pdjv

all: $(OUT)
	./$(OUT) ../..

$(OUT): $(TEST) $(SRC) ../src/data/PdjvCommon.h ../src/data/PdjvRuntime.h
	$(CXX) $(CXXFLAGS) $(TEST) $(SRC) -o $(OUT)

clean:
	rm -f $(OUT)
