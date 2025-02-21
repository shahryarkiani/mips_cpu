#include <cstdint>
#include <iostream>
#include "processor.h"
#include "control.h"
#include "pipeline.h"
using namespace std;

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif 

void Processor::initialize(int level) {
    // Initialize Control
    control = {.reg_dest = 0, 
               .jump = 0,
               .jump_reg = 0,
               .link = 0,
               .shift = 0,
               .branch = 0,
               .bne = 0,
               .mem_read = 0,
               .mem_to_reg = 0,
               .ALU_op = 0,
               .mem_write = 0,
               .halfword = 0,
               .byte = 0,
               .ALU_src = 0,
               .reg_write = 0,
               .zero_extend = 0};
   
    opt_level = level;
    // Optimization level-specific initialization
}

void Processor::advance() {
    switch (opt_level) {
        case 0: single_cycle_processor_advance();
                break;
        case 1: pipelined_processor_advance();
                break;
        // other optimization levels go here
        default: break;
    }
}

void Processor::single_cycle_processor_advance() {
    // fetch
    uint32_t instruction;
    memory->access(regfile.pc, instruction, 0, 1, 0);
    DEBUG(cout << "\nPC: 0x" << std::hex << regfile.pc << std::dec << "\n");
    // increment pc
    regfile.pc += 4;
    
    // decode into contol signals
    control.decode(instruction);
    DEBUG(control.print());

    // extract rs, rt, rd, imm, funct 
    int opcode = (instruction >> 26) & 0x3f;
    int rs = (instruction >> 21) & 0x1f;
    int rt = (instruction >> 16) & 0x1f;
    int rd = (instruction >> 11) & 0x1f;
    int shamt = (instruction >> 6) & 0x1f;
    int funct = instruction & 0x3f;
    uint32_t imm = (instruction & 0xffff);
    int addr = instruction & 0x3ffffff;
    // Variables to read data into(regfile -> read_data_)
    uint32_t read_data_1 = 0;
    uint32_t read_data_2 = 0;
    
    // Read from reg file into variables above
    regfile.access(rs, rt, read_data_1, read_data_2, 0, 0, 0);
    
    // Sets the value of alu.ALU_control_inputs based on what operation the alu needs to do
    alu.generate_control_inputs(control.ALU_op, funct, opcode);
   
    // Sign Extend Or Zero Extend the immediate
    // Using Arithmetic right shift in order to replicate 1 
    imm = control.zero_extend ? imm : (imm >> 15) ? 0xffff0000 | imm : imm;
    
    // Find operands for the ALU Execution
    // Operand 1 is always R[rs] -> read_data_1, except sll and srl(shifts, shamt is shift amount)
    // Operand 2 is immediate if ALU_src = 1, for I-type
    uint32_t operand_1 = control.shift ? shamt : read_data_1;
    uint32_t operand_2 = control.ALU_src ? imm : read_data_2;
    uint32_t alu_zero = 0;

    // ALU Execution, performs operation determined by alu.generate_control_inputs
    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    
    //We read into this variable 
    uint32_t read_data_mem = 0;
    
    uint32_t write_data_mem = 0;

    // Memory
    // First read no matter whether it is a load or a store
    memory->access(alu_result, read_data_mem, 0, control.mem_read | control.mem_write, 0);
    // Stores: sb or sh mask and preserve original leftmost bits
    write_data_mem = control.halfword ? (read_data_mem & 0xffff0000) | (read_data_2 & 0xffff) : 
                    control.byte ? (read_data_mem & 0xffffff00) | (read_data_2 & 0xff): read_data_2;
    // Write to memory only if mem_write is 1, i.e store
    memory->access(alu_result, read_data_mem, write_data_mem, control.mem_read, control.mem_write);
    // Loads: lbu or lhu modify read data by masking
    read_data_mem &= control.halfword ? 0xffff : control.byte ? 0xff : 0xffffffff;

    int write_reg = control.link ? 31 : control.reg_dest ? rd : rt;

    uint32_t write_data = control.link ? regfile.pc + 8 : control.mem_to_reg ? read_data_mem : alu_result;  

    // Write Back
    regfile.access(0, 0, read_data_2, read_data_2, write_reg, control.reg_write, write_data);
    
    // Update PC
    regfile.pc += (control.branch && !control.bne && alu_zero) || (control.bne && !alu_zero) ? imm << 2 : 0; 
    regfile.pc = control.jump_reg ? read_data_1 : control.jump ? (regfile.pc & 0xf0000000) & (addr << 2): regfile.pc;
}

void Processor::pipelined_processor_advance() {
    // pipelined processor logic goes here
    // does nothing currently -- if you call it from the cmd line, you'll run into an infinite loop
    // might be helpful to implement stages in a separate module

    // need structs (representing buffer) for each stage's operations

    // We process through the pipeline in reverse
    
    // Writeback stage
    int write_reg = mem_wb.link ? 31 : mem_wb.reg_dest ? mem_wb.rd : mem_wb.rt;
    uint32_t write_data = mem_wb.link ? regfile.pc + 8 : mem_wb.mem_to_reg ? mem_wb.read_data_mem : mem_wb.alu_result;  
    regfile.access(0, 0, mem_wb.read_data_2, mem_wb.read_data_2, write_reg, mem_wb.reg_write, write_data);
    
    // Memory stage    
    uint32_t read_data_mem;
    uint32_t write_data_mem = 0;
    memory->access(ex_mem.alu_result, read_data_mem, 0, ex_mem.mem_read | ex_mem.mem_write, 0);
    write_data_mem = control.halfword ? (read_data_mem & 0xffff0000) | (ex_mem.read_data_2 & 0xffff) : 
                    control.byte ? (read_data_mem & 0xffffff00) | (ex_mem.read_data_2 & 0xff): ex_mem.read_data_2;
    memory->access(ex_mem.alu_result, read_data_mem, write_data_mem, ex_mem.mem_read, ex_mem.mem_write);
    read_data_mem &= ex_mem.halfword ? 0xffff : ex_mem.byte ? 0xff : 0xffffffff;
    // Now we need to transfer information from ex_mem to mem_wb
    mem_wb = ex_mem; // Almost everything is the same
    mem_wb.read_data_mem = read_data_mem; // but we need to pass the result of the memory read through

    // Execute stage

    // TODO, handle data hazard here

    int dest_reg = ex_mem.reg_dest ? ex_mem.rd : ex_mem.rt;

    if(dest_reg == id_ex.rs) {
        id_ex.read_data_1 = ex_mem.alu_result;
    } 
    if (dest_reg == id_ex.rt) {
        id_ex.read_data_2 = ex_mem.alu_result;
    }

    alu.generate_control_inputs(id_ex.ALU_op, id_ex.funct, id_ex.opcode);
    
    uint32_t operand_1 = id_ex.shift ? id_ex.shamt : id_ex.read_data_1;
    uint32_t operand_2 = id_ex.ALU_src ? id_ex.imm : id_ex.read_data_2;
    uint32_t alu_zero = 0;

    uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
    // Now write to ex_mem 
    ex_mem = id_ex;
    ex_mem.alu_result = alu_result;
    DEBUG(cout << "ex_mem.alu_result == " << ex_mem.alu_result << "\n");

    // Decode stage
    id_ex = if_id;
    regfile.access(if_id.rs, if_id.rt, id_ex.read_data_1, id_ex.read_data_2, 0, 0, 0);
    id_ex.imm = if_id.zero_extend ? if_id.imm : (if_id.imm >> 15) ? 0xffff0000 | if_id.imm : if_id.imm;
    
    // Fetch Stage
    uint32_t instruction;
    bool mem_success = memory->access(regfile.pc, instruction, 0, 1, 0);
    DEBUG(cout << "\nPC: 0x" << std::hex << regfile.pc << std::dec << "\n");
    if(!mem_success) {
        DEBUG(cout << "stalling IF\n");
        return;
    }
    // increment pc
    regfile.pc += 4;
    if_id.load(instruction);
}
