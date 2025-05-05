#include "superscalar_bp_processor.h"
#include <cstdint>


#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif 

void SuperscalarBpProcessor::advance() {
    
    bool if_stall = false;
    bool mem_stall = false;
    bool branch_mispredict_a = false;
    bool branch_mispredict_b = false;
    bool dont_fetch = false;
    bool load_use_stall_a = false;
    bool load_use_stall_b = false;
    bool load_use_stall = false;
    bool dependent_stall = false;
    bool mem_fetch_conflict_a = false;
    bool mem_fetch_conflict_b = false;

    bool branch_predicted_a = false;
    bool branch_predicted_b = false;
    uint32_t branch_target_a = 0;
    uint32_t branch_target_b = 0;
    // We need to ensure that the older instr is always in pipeline_a
    // and the younger one is always in pipeline_b, 
    // like the instruction in A's decode should be older than B's decode, and so on for the other stages

    // Writeback
    {
        int write_reg_a = wb_in_a.link ? 31 : wb_in_a.reg_dest ? wb_in_a.rd : wb_in_a.rt;
        uint32_t write_data_a = wb_in_a.link ? regfile.pc + 8 : wb_in_a.mem_to_reg ? wb_in_a.read_data_mem : wb_in_a.alu_result;  
        int write_reg_b = wb_in_b.link ? 31 : wb_in_b.reg_dest ? wb_in_b.rd : wb_in_b.rt;
        uint32_t write_data_b = wb_in_b.link ? regfile.pc + 8 : wb_in_b.mem_to_reg ? wb_in_b.read_data_mem : wb_in_b.alu_result;  

        DEBUG(cout << "wb_a fetch_pc = " << wb_in_a.pc << ", r" << write_reg_a << " = " << write_data_a << "\n";)
        DEBUG(cout << "wb_b fetch_pc = " << wb_in_b.pc << ", r" << write_reg_b << " = " << write_data_b << "\n";)

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

        id_out_a.taken = id_in_a.taken;
        id_out_b.taken = id_in_b.taken;

        regfile.access(id_out_a.rs, id_out_a.rt, id_out_a.read_data_1, id_out_a.read_data_2, 0, 0, 0);
        regfile.access(id_out_b.rs, id_out_b.rt, id_out_b.read_data_1, id_out_b.read_data_2, 0, 0, 0);

        DEBUG(cout << "id_out_a: fetch_pc = " << id_out_a.pc << ", r" << id_out_a.rs << " = " << id_out_a.read_data_1 << ", r" << id_out_a.rt << " = " << id_out_a.read_data_2 << "\n";)
        DEBUG(cout << "id_out_b: fetch_pc = " << id_out_b.pc << ", r" << id_out_b.rs << " = " << id_out_b.read_data_1 << ", r" << id_out_b.rt << " = " << id_out_b.read_data_2 << "\n";)
        id_out_a.imm = id_out_a.zero_extend ? id_out_a.imm : (id_out_a.imm >> 15) ? 0xffff0000 | id_out_a.imm : id_out_a.imm;
        id_out_b.imm = id_out_b.zero_extend ? id_out_b.imm : (id_out_b.imm >> 15) ? 0xffff0000 | id_out_b.imm : id_out_b.imm;

        // Detect if these two instructions cannot be ran in parallel
        int reg_write = id_out_a.reg_dest ? id_out_a.rd : id_out_a.rt;
        if(id_out_a.reg_write && reg_write != 0) {
            if(reg_write == id_out_b.rs || reg_write == id_out_b.rt) { 
                dependent_stall = true; 
            }
        }
    }
    // Execute
    {
        
        if((ex_in_a.mem_read && ( ex_in_a.rt == id_out_a.rs || (ex_in_a.rt == id_out_a.rt)))
            || (ex_in_b.mem_read && ( ex_in_b.rt == id_out_a.rs || (ex_in_b.rt == id_out_a.rt)))) {
                load_use_stall_a = true;
        } // Are we using a register that one of the previous instruction pairs is loading into?
        if((ex_in_a.mem_read && ( ex_in_a.rt == id_out_b.rs || (ex_in_a.rt == id_out_b.rt)))
            || (ex_in_b.mem_read && ( ex_in_b.rt == id_out_b.rs || (ex_in_b.rt == id_out_b.rt)))) {
                load_use_stall_b = true;
        }
        load_use_stall = load_use_stall_a || load_use_stall_b;

        // Maybe need to rethink ordering of this, we should first try to forward from B wb, then B mem, then from A wb then from A mem
        // Forwarding for pipeline A, first we check if we need to forward from the previous A's wb
        int reg_write = wb_in_a.reg_dest ? wb_in_a.rd : wb_in_a.rt;
        if(wb_in_a.reg_write) { // Forward to pipelineA from wb A
            auto read_data = wb_in_a.mem_to_reg ? wb_in_a.read_data_mem : wb_in_a.alu_result;
            if(reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = read_data;}
            if(reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = read_data;}
        }
        // Or if we need to do it from the previous B's wb, which is newer
        reg_write = wb_in_b.reg_dest ? wb_in_b.rd : wb_in_b.rt;
        if(wb_in_b.reg_write) { // Forward to pipeline A from wb B
            auto read_data = wb_in_b.mem_to_reg ? wb_in_b.read_data_mem : wb_in_b.alu_result;
            if(reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = read_data;}
            if(reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = read_data;}
        }
        // Then we do the same forwarding but for pipeline B,
        reg_write = wb_in_a.reg_dest ? wb_in_a.rd : wb_in_a.rt;
        if(wb_in_a.reg_write) { // Forward to pipeline B from wb A
            auto read_data = wb_in_a.mem_to_reg ? wb_in_a.read_data_mem : wb_in_a.alu_result;
            if(reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = read_data;}
            if(reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = read_data;}
        }
        reg_write = wb_in_b.reg_dest ? wb_in_b.rd : wb_in_b.rt;
        if(wb_in_b.reg_write) { // Forward to pipeline B from wb B
            auto read_data = wb_in_b.mem_to_reg ? wb_in_b.read_data_mem : wb_in_b.alu_result;
            if(reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = read_data;}
            if(reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = read_data;}
        }


        // Forwarding for pipeline A, from mem
        reg_write = mem_in_a.reg_dest ? mem_in_a.rd : mem_in_a.rt;
        if(mem_in_a.reg_write) { // Forward to pipeline A from mem A
            if (reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = mem_in_a.alu_result;}
            if (reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = mem_in_a.alu_result;}
        }
        reg_write = mem_in_b.reg_dest ? mem_in_b.rd : mem_in_b.rt;
        if(mem_in_b.reg_write) { // Forward to pipeline A from mem B
            if (reg_write == ex_in_a.rs) {ex_in_a.read_data_1 = mem_in_b.alu_result;}
            if (reg_write == ex_in_a.rt) {ex_in_a.read_data_2 = mem_in_b.alu_result;}
        }
        // Forwarding for Pipeline B, from mem
        reg_write = mem_in_a.reg_dest ? mem_in_a.rd : mem_in_a.rt;
        if(mem_in_a.reg_write) { // Forward to pipeline B from mem A
            if (reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = mem_in_a.alu_result;}
            if (reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = mem_in_a.alu_result;}
        }
        reg_write = mem_in_b.reg_dest ? mem_in_b.rd : mem_in_b.rt;
        if(mem_in_b.reg_write) { // Forward to pipeline B from mem B
            if (reg_write == ex_in_b.rs) {ex_in_b.read_data_1 = mem_in_b.alu_result;}
            if (reg_write == ex_in_b.rt) {ex_in_b.read_data_2 = mem_in_b.alu_result;}
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
        
        DEBUG(cout << "alu_a: fetch_pc = " << ex_in_a.pc << ", opA = " << operand_1_a << ", opB = " << operand_2_a << ", result = " << alu_result_a << "\n";)
        DEBUG(cout << "alu_b: fetch_pc = " << ex_in_b.pc << ", opA = " << operand_1_b << ", opB = " << operand_2_b << ", result = " << alu_result_b << "\n";)

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
        // TODO: handle superscalar mispredict
        // If detect branch misprediction in pipeline A, we've got to flush out B's mem, and also flush out 
        // the F D E parts of both pipeline A and B
        // If it's detected in pipeline B, we just need to flush out F D E for both
        // For both cases, update the PC accordingly
        // Special case: If a branch misprediction happens in both pipeline A + B, we want the PC from A, not B 
        // Special case: Dependent stalls interact weirdly with branch misprediction, if there's been a misprediction
        // We don't want to dependent stall, because those instructions are being thrown out anyways
        if(mem_in_b.branch) {
            bool actual_taken = ((mem_in_b.bne && mem_in_b.alu_result) || (!mem_in_b.bne && !mem_in_b.alu_result));
            if(actual_taken != mem_in_b.taken) {
                branch_mispredict_b = true;
                dependent_stall = false;
                if(actual_taken) {
                    fetch_pc = mem_in_b.pc + 4 + (mem_in_b.imm << 2); 
                } else {
                    fetch_pc = mem_in_b.pc + 4;
                }
                DEBUG(cout << "mem_in_b was wrong should have gone to " << fetch_pc << "\n");
            }
            DEBUG(cout << "updating branch predictor for mem_in_b\n");
            uint32_t target = mem_in_b.pc + 4 + (mem_in_b.imm << 2); 
            predictor.recordBranch(mem_in_b.pc, target, actual_taken, mem_in_b.taken);
        }

        if(mem_in_a.branch) {
            bool actual_taken = ((mem_in_a.bne && mem_in_a.alu_result) || (!mem_in_a.bne && !mem_in_a.alu_result)); 
            if(actual_taken != mem_in_a.taken) {
                branch_mispredict_a = true;
                dependent_stall = false;
                if(actual_taken) {
                    fetch_pc = mem_in_a.pc + 4 + (mem_in_a.imm << 2);
                } else {
                    fetch_pc = mem_in_a.pc + 4;
                }
                DEBUG(cout << "mem_in_a was wrong should have gone to " << fetch_pc << "\n");
            }  
            DEBUG(cout << "updating branch predictor for mem_in_a\n");
            uint32_t target = mem_in_a.pc + 4 + (mem_in_a.imm << 2); 
            predictor.recordBranch(mem_in_a.pc, target, actual_taken, mem_in_a.taken);
        }

        // If we have mem stall, we need to stall both pipelines, since we don't have reorder buffer

        uint32_t read_data_mem_a = 0;
        uint32_t write_data_mem_a = 0;
        uint32_t read_data_mem_b = 0;
        uint32_t write_data_mem_b = 0;
        // First read no matter whether it is a load or a store
        bool mem_success_a = memory->access(mem_in_a.alu_result, read_data_mem_a, 0, 
            mem_in_a.mem_read | mem_in_a.mem_write, 0);

        DEBUG(cout << "mem_a fetch_pc = " << mem_in_a.pc << ", addr = " << mem_in_a.alu_result << "\n";) 
        DEBUG(cout << "mem_b fetch_pc = " << mem_in_b.pc << ", addr = " << mem_in_b.alu_result << "\n";)
        
        if(mem_success_a) {
            // Stores: sb or sh mask and preserve original leftmost bits
            write_data_mem_a = mem_in_a.halfword ? (read_data_mem_a & 0xffff0000) | (mem_in_a.read_data_2 & 0xffff) : 
                            mem_in_a.byte ? (read_data_mem_a & 0xffffff00) | (mem_in_a.read_data_2 & 0xff): mem_in_a.read_data_2;
            // Write to memory only if mem_write is 1, i.e store
            memory->access(mem_in_a.alu_result, read_data_mem_a, write_data_mem_a, 
                mem_in_a.mem_read, mem_in_a.mem_write);
            // Loads: lbu or lhu modify read data by masking
            read_data_mem_a &= mem_in_a.halfword ? 0xffff : mem_in_a.byte ? 0xff : 0xffffffff;
            

            bool mem_success_b = memory->access(mem_in_b.alu_result, read_data_mem_b, 0, 
                mem_in_b.mem_read | mem_in_b.mem_write, 0);
            
            DEBUG(cout << "mem_a read " << read_data_mem_a << "\n";)

            if(mem_success_b) {
                write_data_mem_b = mem_in_b.halfword ? (read_data_mem_b & 0xffff0000) | (mem_in_b.read_data_2 & 0xffff) : 
                                mem_in_b.byte ? (read_data_mem_b & 0xffffff00) | (mem_in_b.read_data_2 & 0xff): mem_in_b.read_data_2; 
                memory->access(mem_in_b.alu_result, read_data_mem_b, write_data_mem_b,
                            mem_in_b.mem_read, mem_in_b.mem_write);

                read_data_mem_b &= mem_in_b.halfword ? 0xffff : mem_in_b.byte ? 0xff : 0xffffffff;

                DEBUG(cout << "mem_b read " << read_data_mem_b << "\n";)
                

                uint32_t a_addr = mem_in_a.alu_result & ~0x3;
                uint32_t b_addr = mem_in_b.alu_result & ~0x3;
                for(auto& fetch_addr : recent_fetchs) {
                    if(mem_in_a.mem_write && a_addr == fetch_addr) {
                        mem_fetch_conflict_a = true;
                    } else if (mem_in_b.mem_write && b_addr == fetch_addr) {
                        if(mem_in_b.pc != b_addr)
                            mem_fetch_conflict_b = true;
                    } 
                }

                mem_out_a.load_from(mem_in_a);
                mem_out_a.read_data_mem = read_data_mem_a;

                mem_out_b.load_from(mem_in_b);
                mem_out_b.read_data_mem = read_data_mem_b;
            }
            else {
                mem_stall = true;
            }
        } else {
            mem_stall = true;
        }
    }
    // Instruction Fetch
    {   
        if(!mem_stall && !dont_fetch && !load_use_stall && !(mem_fetch_conflict_a || mem_fetch_conflict_b)) {

            bool mem_success_a = memory->access(fetch_pc, if_out_a.instruction, 0, 1, 0);
            if(!mem_success_a) {
                if_stall = true;
            } else {
                bool mem_success_b = memory->access(fetch_pc + 4, if_out_b.instruction, 0, 1, 0);
                if(!mem_success_b) {
                    if_stall = true;
                }

                if_out_a.pc = fetch_pc;
                if_out_b.pc = fetch_pc + 4;

                // Check if either the fetched instructions have a branch prediction
                branch_predicted_a = predictor.makePrediction(if_out_a.pc);
                branch_predicted_b = predictor.makePrediction(if_out_b.pc);

                if(branch_predicted_a) {
                    branch_target_a = predictor.getTarget(if_out_a.pc);
                }
                if(branch_predicted_b) {
                    branch_target_b = predictor.getTarget(if_out_b.pc);
                }

                // Keep track of the recent fetched instructions
                // To detect conflicts between stores and fetches
                recent_fetchs.push_back(fetch_pc + 4);
                recent_fetchs.push_back(fetch_pc);
                while(recent_fetchs.size() > 7) {
                    recent_fetchs.pop_front();
                }
                DEBUG(cout << "if_out_a fetch_pc = " << fetch_pc << "\n";)
                DEBUG(cout << "if_out_b fetch_pc = " << fetch_pc + 4 << "\n";)
            }
        } 
    }
    // Update Pipeline Registers and do stalling if needed
    {   
        if (branch_mispredict_a || branch_mispredict_b) {
            id_out_a.reset();
            id_out_b.reset();

            ex_out_a.reset();
            ex_out_b.reset();

            if(branch_mispredict_a) {
                mem_out_b.reset();
            }

            ex_out_a.pc = mem_in_a.pc;
            ex_out_b.pc = mem_in_b.pc;

            id_out_a.pc = mem_in_a.pc;
            id_out_b.pc = mem_in_b.pc;
        } 
        if (mem_stall) {
            DEBUG(cout << "mem stalling\n";)
            return;
        }

        if(mem_fetch_conflict_a || mem_fetch_conflict_b) {
            recent_fetchs.clear();
            if(mem_fetch_conflict_a) {
                fetch_pc = mem_in_a.pc + 4;
                mem_out_b.reset();
            } else {
                fetch_pc = mem_in_b.pc + 4;
            }

            if_out_a.reset();
            if_out_b.reset();

            id_in_a = if_out_a;
            id_in_b = if_out_b;

            id_out_a.reset();
            id_out_b.reset();

            ex_in_a = id_out_a;
            ex_in_b = id_out_b;

            ex_out_a.reset();
            ex_out_b.reset();

            mem_in_a = ex_out_a;
            mem_in_b = ex_out_b;

            wb_in_a = mem_out_a;
            wb_in_b = mem_out_b;
        } else if (load_use_stall) {
            DEBUG(cout << "load use stalling\n";)
            id_out_a.reset();
            id_out_b.reset();
            
            ex_in_a = id_out_a;
            ex_in_b = id_out_b;
            
            mem_in_a = ex_out_a;
            mem_in_b = ex_out_b;

            wb_in_a = mem_out_a;
            wb_in_b = mem_out_b;
        } else if (dependent_stall) {
            DEBUG(cout << "dependent stalling\n";)
            if(if_stall) {
                if_out_a.reset();
                if_out_b.reset();
            } else {
                fetch_pc += 4;
            }

            id_in_a = id_in_b;
            id_in_b = if_out_a;

            ex_in_a = id_out_a;
            id_out_b.reset();
            ex_in_b = id_out_b;

            mem_in_a = ex_out_a;
            mem_in_b = ex_out_b;

            wb_in_a = mem_out_a;
            wb_in_b = mem_out_b;
        } else {
            if(if_stall) {
                DEBUG(cout << "IF Stalling\n";)
                if_out_a.reset();
                if_out_b.reset();
            } else {
                if(branch_predicted_a) {
                    // a has a positive branch prediction, so we want to discard B
                    if_out_b.reset();
                    if_out_a.taken = true;
                    fetch_pc = branch_target_a;
                    DEBUG(cout << "fetch_pc_a predicted taken to " << branch_target_a << "\n");
                } else if (branch_predicted_b && !branch_predicted_a) {
                    // only b has a positive prediction, we don't discard A
                    if_out_b.taken = true;
                    fetch_pc = branch_target_b;
                    DEBUG(cout << "fetch_pc_b predicted taken to" << branch_target_b << "\n");
                } else {
                    fetch_pc += 8;
                }
            }
            id_in_a = if_out_a;
            id_in_b = if_out_b;

            ex_in_a = id_out_a;
            ex_in_b = id_out_b;
            
            mem_in_a = ex_out_a;
            mem_in_b = ex_out_b;

            wb_in_a = mem_out_a;
            wb_in_b = mem_out_b;            
        }
    }
}