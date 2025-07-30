# GameManager/Makefile

# === CONFIGURATION ===
ID1 = 322996059
ID2 = 211779582
OUT_SO = Algorithm_$(ID1)_$(ID2).so

# Compiler & Flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -pedantic -g -fPIC
LDFLAGS = -shared

# === FILE STRUCTURE ===
SRCDIR = .
USERCOMMONDIR = ../UserCommon

# Source files
SRC_CPP = $(wildcard $(SRCDIR)/*.cpp)
USERCOMMON_CPP = $(wildcard $(USERCOMMONDIR)/*.cpp)
OBJS = $(SRC_CPP:.cpp=.o) $(USERCOMMON_CPP:.cpp=.o)

# === BUILD RULES ===

all: $(OUT_SO)

$(OUT_SO): $(OBJS)
	$(CXX) $(LDFLAGS) -o $@ $^

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(USERCOMMONDIR)/*.o $(OUT_SO)

.PHONY: all clean