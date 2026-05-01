# ============================================================================
# Makefile for QNX Concurrent File Encryption Service
# ============================================================================
# 
# Usage:
#   make          - Build the project
#   make clean    - Remove build artifacts
#   make rebuild  - Clean and rebuild
# ============================================================================

# Artifact name (change to 'qnx_encrypt_server' or 'qnx_encrypt_client')
ARTIFACT = qnx_encrypt_client

# Build architecture
PLATFORM ?= x86_64

# Build profile: debug, release, profile, coverage
BUILD_PROFILE ?= debug

# Configuration name
CONFIG_NAME ?= $(PLATFORM)-$(BUILD_PROFILE)
OUTPUT_DIR = build/$(CONFIG_NAME)
TARGET = $(OUTPUT_DIR)/$(ARTIFACT)

# Compiler tools
CC = qcc -Vgcc_nto$(PLATFORM)
CXX = q++ -Vgcc_nto$(PLATFORM)_cxx
LD = $(CC)

# Include paths (add custom paths here if needed)
INCLUDES += -I./src

# Compiler flags by build profile
CCFLAGS_release   += -O2 -DNDEBUG
CCFLAGS_debug     += -g -O0 -fno-builtin -DDEBUG
CCFLAGS_coverage  += -g -O0 -ftest-coverage -fprofile-arcs
CCFLAGS_profile   += -g -O0 -finstrument-functions

LDFLAGS_coverage  += -ftest-coverage -fprofile-arcs
LIBS_profile      += -lprofilingS

# Generic compiler flags
CCFLAGS_all += -Wall -Wextra -Wpedantic
CCFLAGS_all += -fmessage-length=0
CCFLAGS_all += $(CCFLAGS_$(BUILD_PROFILE))

# Linker flags
LDFLAGS_all += $(LDFLAGS_$(BUILD_PROFILE))
LIBS_all    += $(LIBS_$(BUILD_PROFILE))

# Dependency generation
DEPS = -Wp,-MMD,$(@:%.o=%.d),-MT,$@

# Recursive wildcard function to find source files
rwildcard = $(wildcard $(addprefix $1/*.,$2)) \
            $(foreach d,$(wildcard $1/*),$(call rwildcard,$d,$2))

# Source files list
SRCS = $(call rwildcard, src, c)

# Object files list
OBJS = $(addprefix $(OUTPUT_DIR)/,$(addsuffix .o,$(basename $(SRCS))))

# Compilation rule
$(OUTPUT_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	$(CC) -c $(DEPS) -o $@ $(INCLUDES) $(CCFLAGS_all) $(CCFLAGS) $<

# Linking rule
$(TARGET): $(OBJS)
	$(LD) -o $(TARGET) $(LDFLAGS_all) $(LDFLAGS) $(OBJS) $(LIBS_all) $(LIBS)
	@echo "Build complete: $(TARGET)"

# Default target
all: $(TARGET)

# Clean build artifacts
clean:
	rm -rf $(OUTPUT_DIR)
	@echo "Clean complete"

# Rebuild target
rebuild: clean all

# Include dependency files
-include $(OBJS:%.o=%.d)

# Phony targets
.PHONY: all clean rebuild