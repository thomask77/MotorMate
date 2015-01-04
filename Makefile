# MCU name
#
MCU   = atmega8
F_CPU = 8000000

# Object files directory
# Warning: this will be removed by make clean!
#
OBJDIR = obj

# Target file name (without extension)
TARGET = $(OBJDIR)/MotorMate

# Define all c source files here
# (dependencies are generated automatically)
#
SOURCES  = MotorMate.c

OBJECTS  = $(addprefix $(OBJDIR)/,$(addsuffix .o,$(basename $(SOURCES))))

# Optimization level, can be [0, 1, 2, 3, s]. 
#     0 = turn off optimization. s = optimize for size.
# 
OPT = 2

# List any extra directories to look for include files here.
#     Each directory must be seperated by a space.
#     Use forward slashes for directory separators.
#     For a directory that has spaces, enclose it in quotes.
EXTRAINCDIRS = .


# Compiler flag to set the C Standard level.
#     c89   = "ANSI" C
#     gnu89 = c89 plus GCC extensions
#     c99   = ISO C99 standard (not yet fully implemented)
#     gnu99 = c99 plus GCC extensions
CSTANDARD = -std=gnu99

# Place -D, -U or -I options here for C and C++ sources
CPPFLAGS  = -DF_CPU=$(F_CPU)UL
CPPFLAGS += $(addprefix -I,$(EXTRAINCDIRS))


#---------------- Compiler Options C ----------------
#  -g*:          generate debugging information
#  -O*:          optimization level
#  -f...:        tuning, see GCC manual and avr-libc documentation
#  -Wall...:     warning level
#  -Wa,...:      tell GCC to pass this to the assembler.
#    -adhlns...: create assembler listing
CFLAGS  = -O$(OPT)
CFLAGS += $(CSTANDARD)
CFLAGS += -gdwarf-2
CFLAGS += -ffunction-sections
CFLAGS += -fdata-sections
CFLAGS += -funsigned-char
CFLAGS += -funsigned-bitfields
CFLAGS += -fpack-struct
CFLAGS += -fshort-enums
CFLAGS += -fno-exceptions
CFLAGS += -Wall
CFLAGS += -Wundef
CFLAGS += -Wsign-compare
CFLAGS += -Wno-format        # annoying for avr-gcc..
#CFLAGS += -Winline
#CFLAGS += -Wunreachable-code
CFLAGS += -Wstrict-prototypes
CFLAGS += -Wa,-adhlns=$(OBJDIR)/$(*F).lst

#---------------- Compiler Options C++ ----------------
#
CXXFLAGS  = $(CFLAGS)

#---------------- Assembler Options ----------------
#  -Wa,...:   tell GCC to pass this to the assembler.
#  -adhlns:   create listing
#
ASFLAGS = -Wa,-adhlns=$(OBJDIR)/$(*F).lst


#---------------- Library Options ----------------
# Minimalistic printf version
PRINTF_LIB_MIN = -Wl,-u,vfprintf -lprintf_min

# Floating point printf version (requires MATH_LIB = -lm below)
PRINTF_LIB_FLOAT = -Wl,-u,vfprintf -lprintf_flt

# If this is left blank, then it will use the Standard printf version.
PRINTF_LIB = $(PRINTF_LIB_FLOAT)
#PRINTF_LIB = $(PRINTF_LIB_MIN)

# Minimalistic scanf version
SCANF_LIB_MIN = -Wl,-u,vfscanf -lscanf_min

# Floating point + %[ scanf version (requires MATH_LIB = -lm below)
SCANF_LIB_FLOAT = -Wl,-u,vfscanf -lscanf_flt

# If this is left blank, then it will use the Standard scanf version.
SCANF_LIB = 
#SCANF_LIB = $(SCANF_LIB_MIN)
#SCANF_LIB = $(SCANF_LIB_FLOAT)

MATH_LIB = -lm

# List any extra directories to look for libraries here.
#     Each directory must be seperated by a space.
#     Use forward slashes for directory separators.
#     For a directory that has spaces, enclose it in quotes.
EXTRALIBDIRS =


#---------------- Linker Options ----------------
#  -Wl,...:     tell GCC to pass this to linker.
#    -Map:      create map file
#    --cref:    add cross reference to  map file
LDFLAGS  = -Wl,-Map=$(TARGET).map,--cref
LDFLAGS += -Wl,--gc-sections
LDFLAGS += $(PRINTF_LIB) $(SCANF_LIB) $(MATH_LIB)
LDFLAGS +=$(addprefix -I,$(EXTRALIBDIRS))

#---------------- Programming Options (avrdude) ----------------

# Programming hardware
# Type: avrdude -c ?
# to get a full listing.
#
AVRDUDE_PROGRAMMER = avrisp2
#AVRDUDE_PROGRAMMER = jtag2pdi
#AVRDUDE_PROGRAMMER = avr109

AVRDUDE_PORT = usb
# AVRDUDE_PORT = com10 -b 115200

AVRDUDE_FLAGS  = -p $(MCU) -P $(AVRDUDE_PORT) -c $(AVRDUDE_PROGRAMMER)

AVRDUDE_WRITE_FLASH += -U flash:w:$(TARGET).hex
# AVRDUDE_WRITE_EEPROM += -U eeprom:w:$(TARGET).eep

# 06.09.2011: 
#   *SIGH* avrdude _still_ can't handle fuse bytes..
#   Please keep them synchronized with main.c 
#
# AVRDUDE_FUSES += -U fuse0:w:0xFF:m
# AVRDUDE_FUSES += -U fuse1:w:0xFF:m
# AVRDUDE_FUSES += -U fuse2:w:0xBE:m
# AVRDUDE_FUSES += -U fuse4:w:0xFF:m
# AVRDUDE_FUSES += -U fuse5:w:0xE2:m

#============================================================================


# Define programs and commands.
CC      = avr-gcc
OBJCOPY = avr-objcopy
OBJDUMP = avr-objdump
SIZE    = avr-size
NM      = avr-nm
AVRDUDE = avrdude


# Compiler flags to generate dependency files.
GENDEPFLAGS = -MMD -MP -MF $(OBJDIR)/$(*F).d


# Combine all necessary flags and optional flags.
# Add target processor to flags.
CFLAGS   += -mmcu=$(MCU)
CXXFLAGS += -mmcu=$(MCU)
ASFLAGS  += -mmcu=$(MCU)
LDFLAGS  += -mmcu=$(MCU)

# Default target.
all:  gccversion build showsize

build: elf hex eep lss sym

elf: $(TARGET).elf
hex: $(TARGET).hex
eep: $(TARGET).eep
lss: $(TARGET).lss
sym: $(TARGET).sym

# Display compiler version information.
gccversion: 
	@$(CC) --version

# Show the final program size
showsize: elf
	@echo
	@$(SIZE) --mcu=$(MCU) --format=avr $(TARGET).elf 2>/dev/null

# Flash the device.  
flash: hex eep
	$(AVRDUDE) $(AVRDUDE_FLAGS) $(AVRDUDE_FUSES) $(AVRDUDE_WRITE_FLASH) $(AVRDUDE_WRITE_EEPROM)

# Create final output files (.hex, .eep) from ELF output file.
%.hex: %.elf
	@echo
	@echo Creating load file for Flash: $@
	$(OBJCOPY) -O ihex -R .eeprom -R .fuse -R .lock $< $@


%.eep: %.elf
	@echo
	@echo Creating load file for EEPROM: $@
	-$(OBJCOPY) -j .eeprom --set-section-flags=.eeprom="alloc,load" \
	--change-section-lma .eeprom=0 --no-change-warnings -O ihex $< $@

# Create extended listing file from ELF output file.
%.lss: %.elf
	@echo
	@echo Creating Extended Listing: $@
	$(OBJDUMP) -h -S -z $< > $@


# Create a symbol table from ELF output file.
%.sym: %.elf
	@echo
	@echo Creating Symbol Table: $@
	$(NM) -n $< > $@


# Link: create ELF output file from object files.
.SECONDARY: $(TARGET).elf
.PRECIOUS:  $(OBJECTS)
$(TARGET).elf: $(OBJECTS)
	@echo
	@echo Linking: $@
	$(CC) $^ $(LDFLAGS) --output $@ 

# Compile: create object files from C source files.
$(OBJDIR)/%.o : %.c
	@echo
	@echo Compiling C: $<
	$(CC) -c $(CPPFLAGS) $(CFLAGS) $(GENDEPFLAGS) $< -o $@ 


# Compile: create object files from C++ source files.
$(OBJDIR)/%.o : %.cpp
	@echo
	@echo Compiling CPP: $<
	$(CC) -c $(CPPFLAGS) $(CXXFLAGS) $(GENDEPFLAGS) $< -o $@ 


# Assemble: create object files from assembler source files.
$(OBJDIR)/%.o : %.S
	@echo
	@echo Assembling: $<
	$(CC) -c $(CPPFLAGS) $(ASFLAGS) $< -o $@


# Target: clean project.
clean:
	@echo Cleaning project:
	rm -rf $(OBJDIR)
	rm -rf docs/html

# Create object files directory
$(shell mkdir -p $(OBJDIR) 2>/dev/null)

# Include the dependency files.
-include $(wildcard $(OBJDIR)/*.d)


# Listing of phony targets.
.PHONY: all build flash clean \
        elf hex eep lss sym \
        showsize gccversion
