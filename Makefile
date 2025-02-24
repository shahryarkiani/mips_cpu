
CXX = g++
CXXFLAGS= -g3 -Wall -std=c++11 #-DENABLE_DEBUG 
OPTFLAGS= -O3
ASM = mipsel-linux-gnu-gcc


EXE_NAME=processor
SRCS := main.cpp memory.cpp processor.cpp 
OBJS := $(SRCS:.cpp=.o)
ASM_SRCS := $(wildcard mips_src/*.s)
ASM_OBJS := $(ASM_SRCS:mips_src/%.s=mips_bin/%)

.PHONY: all clean asm_obj_dir

all: $(EXE_NAME) asm_obj_dir $(ASM_OBJS)

$(EXE_NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) -o $@ $^

processor.o: regfile.h ALU.h control.h processor.h pipeline.h
memory.o: memory.h
main.o: memory.h processor.h

asm_obj_dir:
	mkdir -p mips_bin

mips_bin/% : mips_src/%.s
	$(ASM) -mips32 $< -nostartfiles -Ttext=0 -o $@

clean:
	$(RM) $(EXE_NAME) $(OBJS) $(ASM_OBJS)


