#include "superscalar_processor.h"
#include <cstdint>

void SuperscalarProcessor::advance() {
    
    bool if_stall = false;
    bool mem_stall = false;
    bool branch_mispredict = false;
    bool dont_fetch = false;
    bool load_use_stall_a = false;
    bool load_use_stall_b = false;

    // We need to ensure that the older instr is always in pipeline_a
    // and the younger one is always in pipeline_b

    // Writeback
    {
        int write_reg_a = wb_in_a.link ? 31 : wb_in_a.reg_dest ? wb_in_a.rd : wb_in_a.rt;
        uint32_t write_data_a = wb_in_a.link ? regfile.pc + 8 : wb_in_a.mem_to_reg ? wb_in_a.read_data_mem : wb_in_a.alu_result;  
        int write_reg_b = wb_in_b.link ? 31 : wb_in_b.reg_dest ? wb_in_b.rd : wb_in_b.rt;
        uint32_t write_data_b = wb_in_b.link ? regfile.pc + 8 : wb_in_b.mem_to_reg ? wb_in_b.read_data_mem : wb_in_b.alu_result;  

        // Write Back
        regfile.access(0, 0, wb_in_a.read_data_2, wb_in_a.read_data_2, write_reg_a, wb_in_a.reg_write, write_data_a);
        regfile.access(0, 0, wb_in_b.read_data_2, wb_in_b.read_data_2, write_reg_b, wb_in_b.reg_write, write_data_b);
        regfile.pc = wb_in_b.pc;
    }
    // Instruction Decode
    {
        id_out_a.load(id_in_a.instruction);
        id_out_b.load(id_in_b.instruction);
        
        id_out_a.pc = id_in_a.pc;
        id_out_b.pc = id_in_b.pc;

        regfile.access(id_out_a.rs, id_out_a.rt, id_out_a.read_data_1, id_out_a.read_data_2, 0, 0, 0);
        regfile.access(id_out_b.rs, id_out_b.rt, id_out_b.read_data_1, id_out_b.read_data_2, 0, 0, 0);

        id_out_a.imm = id_out_a.zero_extend ? id_out_a.imm : (id_out_a.imm >> 15) ? 0xffff0000 | id_out_a.imm : id_out_a.imm;
        id_out_b.imm = id_out_b.zero_extend ? id_out_b.imm : (id_out_b.imm >> 15) ? 0xffff0000 | id_out_b.imm : id_out_b.imm;
    }
    // Execute
    {
        if((ex_in_a.mem_read && ( ex_in_a.rt == id_out_a.rs || (ex_in_a.rt == id_out_a.rt && id_out_a.reg_dest)))
            || (ex_in_b.mem_read && ( ex_in_b.rt == id_out_a.rs || (ex_in_b.rt == id_out_a.rt && id_out_a.reg_dest)))) {
            load_use_stall_a = true;
        } // Are we using a register that one of the previous instruction pairs is loading into?
        if((ex_in_a.mem_read && ( ex_in_a.rt == id_out_b.rs || (ex_in_a.rt == id_out_b.rt && id_out_b.reg_dest)))
            || (ex_in_b.mem_read && ( ex_in_b.rt == id_out_b.rs || (ex_in_b.rt == id_out_b.rt && id_out_b.reg_dest)))) {
            load_use_stall_b = true;
        }

        // Forwarding for pipeline A, first we check if we need to forward from the previous A's wb
        int reg_write = wb_in_a.reg_dest ? wb_in_a.rd : wb_in_a.rt;
        if(wb_in_a.reg_write) {
            auto read_data = wb_in_a.mem_to_reg ? wb_in_a.read_data_mem : wb_in_a.alu_result;
            if(reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = read_data;}
            else if(reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = read_data;}
        }
        // Or if we need to do it from the previous B's wb, which is newer
        reg_write = wb_in_b.reg_dest ? wb_in_b.rd : wb_in_b.rt;
        if(wb_in_b.reg_write) {
            auto read_data = wb_in_b.mem_to_reg ? wb_in_b.read_data_mem : wb_in_b.alu_result;
            if(reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = read_data;}
            else if(reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = read_data;}
        }
        // Then we do the same forwarding but for pipeline B,
        reg_write = wb_in_a.reg_dest ? wb_in_a.rd : wb_in_a.rt;
        if(wb_in_a.reg_write) {
            auto read_data = wb_in_a.mem_to_reg ? wb_in_a.read_data_mem : wb_in_a.alu_result;
            if(reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = read_data;}
            else if(reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = read_data;}
        }
        reg_write = wb_in_b.reg_dest ? wb_in_b.rd : wb_in_b.rt;
        if(wb_in_b.reg_write) {
            auto read_data = wb_in_b.mem_to_reg ? wb_in_b.read_data_mem : wb_in_b.alu_result;
            if(reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = read_data;}
            else if(reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = read_data;}
        }


        // Forwarding for pipeline A, from mem
        reg_write = mem_in_a.reg_dest ? mem_in_a.rd : mem_in_a.rt;
        if(mem_in_a.reg_write) {
            if (reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = mem_in_a.alu_result;}
            else if (reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = mem_in_a.alu_result;}
        }
        reg_write = mem_in_b.reg_dest ? mem_in_b.rd : mem_in_b.rt;
        if(mem_in_b.reg_write) {
            if (reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = mem_in_b.alu_result;}
            else if (reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = mem_in_b.alu_result;}
        }
        // Forwarding for Pipeline B, from mem
        reg_write = mem_in_a.reg_dest ? mem_in_a.rd : mem_in_a.rt;
        if(mem_in_a.reg_write) {
            if (reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = mem_in_a.alu_result;}
            else if (reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = mem_in_a.alu_result;}
        }
        reg_write = mem_in_b.reg_dest ? mem_in_b.rd : mem_in_b.rt;
        if(mem_in_b.reg_write) {
            if (reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = mem_in_b.alu_result;}
            else if (reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = mem_in_b.alu_result;}
        }

        uint32_t alu_zero = 0;

        alu_a.generate_control_inputs(ex_in_a.ALU_op, ex_in_a.funct, ex_in_a.opcode);
        uint32_t operand_1_a = ex_in_a.shift ? ex_in_a.shamt : ex_in_a.read_data_1;
        uint32_t operand_2_a = ex_in_a.ALU_src ? ex_in_a.imm : ex_in_a.read_data_2;
        uint32_t alu_result_a = alu_a.execute(operand_1_a, operand_2_a, alu_zero);

        alu_b.generate_control_inputs(ex_in_b.ALU_op, ex_in_b.funct, ex_in_b.opcode);
        uint32_t operand_1_b = ex_in_b.shift ? ex_in_b.shamt : ex_in_b.read_data_1;
        uint32_t operand_2_b = ex_in_b.ALU_src ? ex_in_b.imm : ex_in_b.read_data_2;
        uint32_t alu_result_b = alu_b.execute(operand_1_b, operand_2_b, alu_zero);
        
        // // This is to work with the original code
        // if(ex_in.branch && ((ex_in.bne && !alu_zero) || (!ex_in.bne && alu_zero))) {
        //     dont_fetch = true;
        // }

        ex_out_a.load_from(ex_in_a);
        ex_out_a.alu_result = alu_result_a;
        
        ex_out_b.load_from(ex_in_b);
        ex_out_b.alu_result = alu_result_b;
    }
    // Memory
    {
        //Check if we have a branch misprediction
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

        if(mem_success) {
            // Stores: sb or sh mask and preserve original leftmost bits
            write_data_mem = mem_in.halfword ? (read_data_mem & 0xffff0000) | (mem_in.read_data_2 & 0xffff) : 
                            mem_in.byte ? (read_data_mem & 0xffffff00) | (mem_in.read_data_2 & 0xff): mem_in.read_data_2;
            // Write to memory only if mem_write is 1, i.e store
            memory->access(mem_in.alu_result, read_data_mem, write_data_mem, 
                mem_in.mem_read, mem_in.mem_write);
            // Loads: lbu or lhu modify read data by masking
            read_data_mem &= mem_in.halfword ? 0xffff : mem_in.byte ? 0xff : 0xffffffff;

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
            DEBUG(cout << "Fetch_Pc is 0x" << std::hex << fetch_pc << std::dec << "\n";);
            bool mem_success_1 = memory->access(fetch_pc, if_out.instruction, 0, 1, 0);
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