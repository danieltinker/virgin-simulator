# # === Compiler and Flags ===
# CXX = g++
# CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -pedantic -I../common -I../UserCommon
# LDFLAGS = -rdynamic

# # === Build Folder and Target Executable ===
# BUILD_DIR = build
# TARGET = simulator_315634022

# # === List of Source Files ===
# SRCS = \
#     main.cpp \
# 	ArgParser.cpp \
# 	AlgorithmRegistrar.cpp \
# 	GameManagerRegistrar.cpp \
# 	ThreadPool.cpp \
# 	GameManagerRegistration.cpp \
# 	TankAlgorithmRegistration.cpp \
# 	PlayerRegistration.cpp \


	
# # === Convert SRCS paths to OBJS paths in build ===
# OBJS := $(SRCS)
# OBJS := $(patsubst ../%, %, $(OBJS)) # remove ../ before creating build paths
# OBJS := $(patsubst %, $(BUILD_DIR)/%, $(OBJS:.cpp=.o))

# # === Rule to Create the Executable ===
# $(TARGET): $(OBJS)
# 	$(CXX) $(CXXFLAGS) $(LDFLAGS) $(OBJS) -o $(TARGET)

# # === Compilation Rules ===
# $(BUILD_DIR)/%.o: ../%.cpp
# 	@mkdir -p $(dir $@)
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# $(BUILD_DIR)/%.o: %.cpp
# 	@mkdir -p $(dir $@)
# 	$(CXX) $(CXXFLAGS) -c $< -o $@

# # === Clean Rules ===
# clean:
# 	rm -r -f $(BUILD_DIR)
# 	rm -f $(TARGET)

# # Optional: automatic clean on make
# .PHONY: clean

# === CONFIGURATION ===
ID1 = 315634022
OUT_EXE = simulator_$(ID1)

# Compiler and flags
CXX = g++
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -pedantic -g -fPIC -I../common

# Platform-specific flags
UNAME_S := $(shell uname -s)
ifeq ($(UNAME_S),Darwin)
    LDFLAGS = 
    LDLIBS = 
else
    LDFLAGS = -rdynamic
    LDLIBS = -ldl
endif

# === SOURCE STRUCTURE ===
SRCDIR = .
USERCOMMONDIR = ../UserCommon

SIMULATOR_CPP = $(wildcard $(SRCDIR)/*.cpp)
USERCOMMON_CPP = $(wildcard $(USERCOMMONDIR)/*.cpp)

OBJS = $(SIMULATOR_CPP:.cpp=.o) $(USERCOMMON_CPP:.cpp=.o)

# === BUILD RULES ===

all: $(OUT_EXE)

$(OUT_EXE): $(OBJS)
	$(CXX) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

clean:
	rm -f $(SRCDIR)/*.o $(USERCOMMONDIR)/*.o $(OUT_EXE)

.PHONY: all clean