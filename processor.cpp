#include <cstdint>
#include <memory>
#include <iostream>
#include <memory>
#include "processor.h"
#include "control.h"
#include "outoforder_processor.h"
#include "pipeline.h"
#include "regfile.h"
#include "superscalar_bp_processor.h"
#include "superscalar_processor.h"
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
    if(opt_level == 3) {
        superscalar_processor = unique_ptr<SuperscalarProcessor>(new SuperscalarProcessor(memory, regfile));
    } else if(opt_level == 2) {
        superscalar_bp_processor = unique_ptr<SuperscalarBpProcessor>(new SuperscalarBpProcessor(memory, regfile));
    }
}

void Processor::advance() {
    switch (opt_level) {
        case 0: single_cycle_processor_advance();
                break;
        case 1: pipelined_processor_advance();
                break;
        case 3: superscalar_processor_advance();
                break;
        // other optimization levels go here
        case 2: superscalar_bp_processor->advance(); 
                break;
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

    bool if_stall = false;
    bool mem_stall = false;
    bool branch_mispredict = false;
    bool dont_fetch = false;
    bool load_use_stall = false;

    // Writeback
    {
        int write_reg = wb_in.link ? 31 : wb_in.reg_dest ? wb_in.rd : wb_in.rt;
        
        uint32_t write_data = wb_in.link ? regfile.pc + 8 : wb_in.mem_to_reg ? wb_in.read_data_mem : wb_in.alu_result;  
    
        cout << "wb fetch_pc = " << wb_in.pc << ", r" << write_reg << " = " << write_data << "\n";
        // Write Back
        regfile.access(0, 0, wb_in.read_data_2, wb_in.read_data_2, write_reg, wb_in.reg_write, write_data);
        regfile.pc = wb_in.pc;
    }
    // Instruction Decode
    {
        id_out.load(id_in.instruction);
        id_out.pc = id_in.pc;
        regfile.access(id_out.rs, id_out.rt, id_out.read_data_1, id_out.read_data_2, 0, 0, 0);
        cout << "id_out: fetch_pc = " << id_out.pc << ", r" << id_out.rs << " = " << id_out.read_data_1 << ", r" << id_out.rt << " = " << id_out.read_data_2 << "\n";
        id_out.imm = id_out.zero_extend ? id_out.imm : (id_out.imm >> 15) ? 0xffff0000 | id_out.imm : id_out.imm;
    }
    // Execute
    {
        if(ex_in.mem_read && ((ex_in.rt == id_out.rs) || ((ex_in.rt == id_out.rt && id_out.reg_dest))) ) {
            cout << "load use stalling\n";
            load_use_stall = true;
        }

        // Forwarding from writeback
        int reg_write = wb_in.reg_dest ? wb_in.rd : wb_in.rt;
        if(wb_in.reg_write && reg_write == ex_in.rs) {
            ex_in.read_data_1 = wb_in.mem_to_reg ? wb_in.read_data_mem : wb_in.alu_result; 
        }
        if(wb_in.reg_write && reg_write == ex_in.rt) {
            ex_in.read_data_2 = wb_in.mem_to_reg ? wb_in.read_data_mem : wb_in.alu_result; 
        }

        // Forwarding from mem
        reg_write = mem_in.reg_dest ? mem_in.rd : mem_in.rt;
        if(mem_in.reg_write && reg_write == ex_in.rs) {
            ex_in.read_data_1 = mem_in.alu_result;
        }
        if(mem_in.reg_write && reg_write == ex_in.rt) {
            ex_in.read_data_2 = mem_in.alu_result;
        }

        alu.generate_control_inputs(ex_in.ALU_op, ex_in.funct, ex_in.opcode);
        uint32_t operand_1 = ex_in.shift ? ex_in.shamt : ex_in.read_data_1;
        uint32_t operand_2 = ex_in.ALU_src ? ex_in.imm : ex_in.read_data_2;
        uint32_t alu_zero = 0;
        uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);
        
        if(ex_in.branch && ((ex_in.bne && !alu_zero) || (!ex_in.bne && alu_zero))) {
            dont_fetch = true;
        }

        cout << "alu: fetch_pc = " << ex_in.pc << ", opA = " << operand_1 << ", opB = " << operand_2 << ", result = " << alu_result << "\n";


        ex_out.load_from(ex_in);
        ex_out.alu_result = alu_result;
    }
    // Memory
    {
        // Also check if we have a branch misprediction

        if(mem_in.branch && ((mem_in.bne && mem_in.alu_result) || (!mem_in.bne && !mem_in.alu_result))) {
            branch_mispredict = true;
            fetch_pc = mem_in.pc + 4 + (mem_in.imm << 2);
            DEBUG(cout << "mem_in.pc == " << mem_in.pc << "\n";)
        }
        uint32_t read_data_mem = 0;
        uint32_t write_data_mem = 0;
        // First read no matter whether it is a load or a store
        bool mem_success = memory->access(mem_in.alu_result, read_data_mem, 0, 
            mem_in.mem_read | mem_in.mem_write, 0);

        cout << "mem fetch_pc = " << mem_in.pc << ", addr = " << mem_in.alu_result << "\n"; 



        if(mem_success) {
            // Stores: sb or sh mask and preserve original leftmost bits
            write_data_mem = mem_in.halfword ? (read_data_mem & 0xffff0000) | (mem_in.read_data_2 & 0xffff) : 
                            mem_in.byte ? (read_data_mem & 0xffffff00) | (mem_in.read_data_2 & 0xff): mem_in.read_data_2;
            // Write to memory only if mem_write is 1, i.e store
            memory->access(mem_in.alu_result, read_data_mem, write_data_mem, 
                mem_in.mem_read, mem_in.mem_write);
            // Loads: lbu or lhu modify read data by masking
            read_data_mem &= mem_in.halfword ? 0xffff : mem_in.byte ? 0xff : 0xffffffff;

            cout << "mem read " << read_data_mem << "\n";


            mem_out.load_from(mem_in);
            mem_out.read_data_mem = read_data_mem;
        } else {
            mem_stall = true;
            DEBUG(cout << "mem_stalling\n";)
        }
    }
    // Instruction Fetch
    {   
        if(!mem_stall && !dont_fetch && !load_use_stall) {
            cout << "if_out fetch_pc = " << fetch_pc << "\n";
            bool mem_success = memory->access(fetch_pc, if_out.instruction, 0, 1, 0);
            if(!mem_success) {
                if_stall = true;
            } else {
                if_out.pc = fetch_pc;
            }
        } 
    }
    // Update Pipeline Registers and do stalling if needed
    {   
        if(branch_mispredict) {
            DEBUG(cout << "Misprediction\n";)
            ex_out.reset();
            id_out.reset();
            ex_out.pc = mem_in.pc;
            id_out.pc = mem_in.pc;
        }
        if(mem_stall) {
            DEBUG(cout << "mem stalling\n";)
            return;
        }
        if (load_use_stall) {
            DEBUG(cout << "Stalling for use after mem read for ex_in.rt == " << ex_in.rt << " and id_out.rt == " << id_out.rt << "\n";)
            id_out.reset();
            ex_in = id_out;
            mem_in = ex_out;
            wb_in = mem_out;
        } else if (if_stall) {
            DEBUG(cout << "IF Stalling\n";)
            if_out.reset();
            if(branch_mispredict) {
                if_out.pc = mem_in.pc;
            } else {
                if_out.pc = id_in.pc;
            }
            id_in = if_out;
            ex_in = id_out;
            mem_in = ex_out;
            wb_in = mem_out;
        } else {
            fetch_pc += 4;
            id_in = if_out;
            ex_in = id_out;
            mem_in = ex_out;
            wb_in = mem_out;
        }
    }
}

void Processor::superscalar_processor_advance() {
    superscalar_processor->advance();
}