# === CONFIGURATION ===
ID1 = 315634022
SODIR = ./sos
OUT_SO = $(SODIR)/GameManager_$(ID1).so

# Compiler & Flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -pedantic -g -fPIC -I./include -I../common

# Platform-specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS = -shared -undefined dynamic_lookup
else
    LDFLAGS = -shared
endif

# === FILE STRUCTURE ===
SRCDIR = ./src
USERCOMMONDIR = ../UserCommon

# Source files
SRC_CPP = $(wildcard $(SRCDIR)/*.cpp)
USERCOMMON_CPP = $(wildcard $(USERCOMMONDIR)/*.cpp)
OBJS = $(SRC_CPP:.cpp=.o) $(USERCOMMON_CPP:.cpp=.o)

# === BUILD RULES ===

all: $(OUT_SO)

$(OUT_SO): $(OBJS) | $(SODIR)
	$(CXX) $(LDFLAGS) -o $@ $^

$(SODIR):
	mkdir -p $(SODIR)


%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(USERCOMMONDIR)/*.o $(OUT_SO)
	rm -rf $(SODIR)

.PHONY: all clean