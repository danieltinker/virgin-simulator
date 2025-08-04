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
CXXFLAGS = -std=c++20 -Wall -Wextra -Werror -pedantic -g -fPIC -I../UserCommon -I../common

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

# All .cpp in simulator root and userCommon (ErrorLogger etc.)
SIMULATOR_CPP = $(wildcard $(SRCDIR)/*.cpp)
USERCOMMON_CPP = $(wildcard $(USERCOMMONDIR)/*.cpp)

# Build objects into build/
BUILD_DIR = build
SIM_OBJS = $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(notdir $(SIMULATOR_CPP)))
USERCOMMON_OBJS = $(patsubst $(USERCOMMONDIR)/%.cpp,$(BUILD_DIR)/UserCommon/%.o,$(USERCOMMON_CPP))

OBJS = $(SIM_OBJS) $(USERCOMMON_OBJS)

# === Rules ===

all: $(OUT_EXE)

$(OUT_EXE): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# Simulator .cpp -> object
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

# UserCommon .cpp -> object (kept under build/UserCommon/)
$(BUILD_DIR)/UserCommon/%.o: $(USERCOMMONDIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -I$(USERCOMMONDIR) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
	rm -f $(OUT_EXE)

.PHONY: all clean
