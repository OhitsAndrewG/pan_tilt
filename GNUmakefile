###############################################################################
# Local targets for this project.
#
# GNU make looks for GNUmakefile BEFORE Makefile, so everything here survives
# CubeMX regeneration (which rewrites Makefile from the .ioc). Put hand-written
# targets in this file, never in Makefile.
###############################################################################

include Makefile

# --- User sources -----------------------------------------------------------
# CubeMX rewrites Makefile's C_SOURCES from the .ioc and may drop hand-added
# files. Re-scan Core/Src (and Core/Lib if present) so your own .c files are
# always compiled. $(sort) also de-duplicates, so files CubeMX already listed
# do not get linked twice.
# Files CubeMX did not list (it rewrites C_SOURCES from the .ioc):
USER_C_SOURCES := $(filter-out $(C_SOURCES),$(wildcard Core/Src/*.c) $(wildcard Core/Lib/*.c))
USER_OBJECTS   := $(addprefix $(BUILD_DIR)/,$(notdir $(USER_C_SOURCES:.c=.o)))

C_SOURCES  := $(sort $(C_SOURCES) $(USER_C_SOURCES))
C_INCLUDES += -ICore/Lib

# The link recipe expands $(OBJECTS) at run time, so redefining it here is
# enough to get the extra files onto the command line...
OBJECTS  = $(addprefix $(BUILD_DIR)/,$(notdir $(C_SOURCES:.c=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASM_SOURCES:.s=.o)))
OBJECTS += $(addprefix $(BUILD_DIR)/,$(notdir $(ASMM_SOURCES:.S=.o)))
vpath %.c $(sort $(dir $(C_SOURCES)))

# ...but make expanded the .elf rule's PREREQUISITES back when Makefile was
# read, so the new objects must be attached to that target or they never get
# built. A rule with no recipe adds prerequisites without overriding it.
$(BUILD_DIR)/$(TARGET).elf: $(USER_OBJECTS)

## sources: list every .c file that will be compiled
sources:
	@for f in $(C_SOURCES); do echo "  $$f"; done

# --- SEGGER J-Link ----------------------------------------------------------
JLINK        ?= JLinkExe
JLINK_DEVICE ?= STM32F411CE
JLINK_IF     ?= SWD
JLINK_SPEED  ?= 4000
JLINK_OPTS    = -NoGui 1 -ExitOnError 1 -Device $(JLINK_DEVICE) \
                -If $(JLINK_IF) -Speed $(JLINK_SPEED) -AutoConnect 1

GDB_SERVER   ?= /Applications/SEGGER/JLink/JLinkGDBServerCLExe
GDB_PORT     ?= 2331

# Run a J-Link Commander script built from the given newline-separated commands
define jlink
@printf '$(1)\nqc\n' > $(BUILD_DIR)/cmd.jlink
@$(JLINK) $(JLINK_OPTS) -CommanderScript $(BUILD_DIR)/cmd.jlink
endef

## flash: build, program the target over SWD, and run it
flash: all
	$(call jlink,loadfile $(BUILD_DIR)/$(TARGET).hex\nr\ng)

## erase: full chip erase (only needed for option bytes / read-protected parts)
erase: | $(BUILD_DIR)
	$(call jlink,erase)

## reset: reset and run whatever is already on the target
reset: | $(BUILD_DIR)
	$(call jlink,r\ng)

## gdbserver: start the J-Link GDB server (for `make debug` or a manual gdb)
gdbserver:
	$(GDB_SERVER) -device $(JLINK_DEVICE) -if $(JLINK_IF) \
	  -speed $(JLINK_SPEED) -port $(GDB_PORT) -nogui

## debug: attach gdb to an already-running `make gdbserver`
debug: all
	$(GDB_PREFIX)$(PREFIX)gdb $(BUILD_DIR)/$(TARGET).elf \
	  -ex "target extended-remote localhost:$(GDB_PORT)" -ex "load" -ex "break main"

## size: per-section size breakdown of the ELF
size: all
	$(SZ) -A -x $(BUILD_DIR)/$(TARGET).elf

## help: list these targets
help:
	@grep -E '^## ' $(firstword $(MAKEFILE_LIST)) | sed 's/^## /  make /'

.PHONY: sources flash erase reset gdbserver debug size help
