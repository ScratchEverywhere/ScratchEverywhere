# Project Configuration
TARGET := scratch-n64
BUILD_DIR := build
include $(N64_INST)/include/n64.mk

# Directories
SRCDIRS :=  source source/runtime source/runtime/blocks \
			source/windowing/n64 source/renderers/n64 source/audio/n64 \
			source/menus source/platforms source/platforms/n64 source/third-party/ryu source/third-party/miniz
INCLUDES := include source source/runtime source/runtime/blocks \
			source/windowing/n64 source/renderers/n64 source/audio/n64 \
			source/menus source/platforms source/platforms/n64 source/third-party source/third-party/miniz

# Find Sources
SOURCES_C   := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.c))
SOURCES_CPP := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.cpp))
SOURCES_S   := $(foreach dir,$(SRCDIRS),$(wildcard $(dir)/*.s))

# Do the obj shit
OBJS := $(addprefix $(BUILD_DIR)/, $(SOURCES_C:.c=.o)) \
        $(addprefix $(BUILD_DIR)/, $(SOURCES_CPP:.cpp=.o)) \
        $(addprefix $(BUILD_DIR)/, $(SOURCES_S:.s=.o))

# Assets
N64ROMFS  := romfs
DFS_FILE   := $(BUILD_DIR)/$(TARGET).dfs

# flags
N64_CXXFLAGS += -fno-rtti -O2 -ffast-math -Wno-error -DRENDERER_N64 -DMINIZ_NO_TIME -DMIPS -D__N64__ -DENABLE_MENU
N64_CFLAGS   += $(N64_CXXFLAGS)

# Include paths
include_flags := $(foreach dir,$(INCLUDES),-I$(dir))
N64_CFLAGS   += $(include_flags)
N64_CXXFLAGS += $(include_flags)

# Main tyargets
all: $(TARGET).z64

# Link the ELF
$(BUILD_DIR)/$(TARGET).elf: $(OBJS)

# Make the fs
$(DFS_FILE): $(shell find $(N64ROMFS) -type f 2>/dev/null)
	@echo "    [DFS] $@"
	@$(N64_MKDFS) $@ $(N64ROMFS)

# Rules
$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@echo "    [CXX] $<"
	@$(CXX) $(N64_CXXFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.o: %.c
	@mkdir -p $(dir $@)
	@echo "    [CC] $<"
	@$(CC) $(N64_CFLAGS) -MMD -MP -c -o $@ $<

$(BUILD_DIR)/%.o: %.s
	@mkdir -p $(dir $@)
	@echo "    [AS] $<"
	@$(AS) $(ASFLAGS) -c -o $@ $<

# Make the shit
$(TARGET).z64: N64_ROM_TITLE := "Scratch Everywhere!"
$(TARGET).z64: $(BUILD_DIR)/$(TARGET).elf $(DFS_FILE)

# Clean up
clean:
	rm -rf $(BUILD_DIR) $(TARGET).z64

.PHONY: all clean

# Dependency tracking
-include $(OBJS:.o=.d)
