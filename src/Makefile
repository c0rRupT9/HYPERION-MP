CXX = g++
CXXFLAGS = -O2 -std=c++17
BIN_NAME = main
SRCS = main.cpp
PROG ?= stressTest
CYC ?= 100
TRACE ?= 0000
PROG_PATH = ../programs/

# Detect OS & Set Extensions
ifeq ($(OS),Windows_NT)
    EXE = .exe
    EXEC = $(BIN_NAME)$(EXE)
    PYTHON = python
    RM_CMD = powershell -Command "Remove-Item -Path $(TARGET), ../hexTraces/*.txt, ../programs/*.hex -ErrorAction SilentlyContinue"
    RM_CMD_HEX = powershell -Command "Remove-Item -Path ../hexTraces/*.txt -ErrorAction SilentlyContinue"
else
    EXE =
    EXEC = ./$(BIN_NAME)
    PYTHON = python3
    RM_CMD = rm -f $(TARGET) ../hexTraces/*.txt ../programs/*.hex 
    RM_CMD_HEX = rm -f ../hexTraces/*.txt
endif

# Target files
TARGET = $(BIN_NAME)$(EXE)
ASM_SRC = $(PROG_PATH)$(PROG).asm
TARGETPROG = $(PROG_PATH)$(PROG).hex

.PHONY: build asm run debug debugInline clean

# Default alias target
build: $(TARGET)

# Recompile binary only if source changed
$(TARGET): $(SRCS)
	$(CXX) $(CXXFLAGS) $(SRCS) -o $(TARGET)


asm: $(TARGETPROG)

# Re-assemble only if .asm source is newer than .hex file
$(TARGETPROG): $(ASM_SRC)
	$(PYTHON) ../tools/assembler.py $<

run: $(TARGET) $(TARGETPROG)
	$(EXEC) $(TARGETPROG) -c $(CYC)

debug: $(TARGET) $(TARGETPROG)
	$(EXEC) $(TARGETPROG) -d -c $(CYC)
	$(PYTHON) ../tools/trace_reconstruct.py ../hexTraces/$(TRACE).txt

debugInline: $(TARGET) $(TARGETPROG)
	$(EXEC) $(TARGETPROG) -t -c $(CYC)

clean:
	$(RM_CMD)

cleanHex:
	$(RM_CMD_HEX)