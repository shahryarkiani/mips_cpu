#include "outoforder_processor.h"
#include <cassert>
#include <cstdint>
#include <deque>

#ifdef ENABLE_DEBUG
#define DEBUG(x) x
#else
#define DEBUG(x) 
#endif 



void OutOfOrderProcessor::advance() {
    /*
    * TODO
    * Fetch 1 instructions at a time into instruction queue
    * Decode + Rename Instructions
    * Issue Instructions
    * Execute Instructions / Do Mem Operations
    * Commit and Writeback
    */
    
    // Fetch stage will write to these variables
    uint32_t fetch_pc_out = 0;
    instruction instruction_out{};
    bool if_stall = false;

    // Decode stage will write to these
    reorder_entry rob_entry_out{};
    bool id_stall = false;

    // Issue stage will write to these
    decoded_instr issue_execute_out;
    issue_execute_out.reset();
    decoded_instr issue_memory_out;
    issue_memory_out.reset();
    // Exec/Mem stage will write to these
    uint8_t execute_dst_reg_out = 32;
    uint8_t execute_result_out = 0;

    uint8_t mem_dst_reg_out = 32;
    uint8_t mem_result_out = 0;
    bool mem_stall = false;
    { // Fetch
        
        instruction instr_a;

        instr_a.pc = fetch_pc;

        bool mem_success_a = memory->access(instr_a.pc, instr_a.instruction, 0, 1, 0);

        if(mem_success_a) {
            fetch_pc_out = fetch_pc + 4;
        } else {
            instr_a.instruction = 0;
            instr_a.pc = 0;
            fetch_pc_out = fetch_pc;
            if_stall = true;
        }

        instruction_out = instr_a;

    }

    if(fetched_instruction.instruction != 0) {
        // Decode + Rename
        
        decoded_instr decoded;
        
        decoded.load(fetched_instruction.instruction);
        decoded.pc = fetched_instruction.pc;
        decoded.imm = decoded.zero_extend ? decoded.imm : (decoded.imm >> 15) ? 0xffff0000 | decoded.imm : decoded.imm;
        
        reorder_entry rob_entry;
        rob_entry.pc = fetched_instruction.pc;
        rob_entry.reg_write = decoded.reg_write;
        rob_entry.dst_reg = decoded.reg_dest == 1 ? decoded.rd : decoded.rt;

        reg_mapping mapping;
        
        if(decoded.reg_write && free_list.size() > 0 && reorder_buffer.size() + 1 < reorder_buffer_capacity) {
            // Do we need to allocate another register, if so is there a free one?
            uint8_t new_reg = free_list.back();
            free_list.pop_back();

            mapping.phys_reg = new_reg;
            mapping.read_count = 0;
            mapping.ready = false;
            map_table.push_back(mapping);

            if(decoded.reg_dest) {
                rob_entry.src_reg = mapping.phys_reg;
                mapping.arch_reg = decoded.rd;
            } else {
                rob_entry.src_reg = mapping.phys_reg;
                mapping.arch_reg = decoded.rt;
            }
        } else if ((decoded.reg_write && free_list.empty()) || reorder_buffer.size() + 1 == reorder_buffer_capacity) {
            id_stall = true;
        }

        if(!id_stall) {
            for(auto& reg_mapping : map_table) {
                if(reg_mapping.arch_reg == decoded.rd) {
                    decoded.rd = reg_mapping.phys_reg;
                }
                if(reg_mapping.arch_reg == decoded.rt) {
                    decoded.rt = reg_mapping.phys_reg;
                }
                if(reg_mapping.arch_reg == decoded.rs) {
                    decoded.rs = reg_mapping.phys_reg;
                }
            }

            rob_entry.instruction = decoded;

            rob_entry_out = rob_entry;
        }
    }

    { // Writeback
        // Take the results of the execute units and store them in the physical register file
        // This can happen in parallel with Commit, which is to another register file
        assert(execute_dst_reg >= 32);
        assert(mem_dst_reg >= 32);

        uint32_t _unused;

        regfile.access(0, 0, _unused, _unused, execute_dst_reg, true, execute_result);
        regfile.access(0, 0, _unused, _unused, mem_dst_reg, true, mem_result);
    }

    { // Issue, move instruction from reorder buffer to reservation station
        // In parallel, find ready instruction and put it into execute stage

        // Update instructions in reservation station with execute results
        for(auto& reservation_entry : reservation_station) {
            if(!reservation_entry.r1_ready) {
                if(reservation_entry.instruction.rs == execute_dst_reg) {
                    reservation_entry.instruction.read_data_1 = execute_result;
                    reservation_entry.r1_ready = true;
                } else if (reservation_entry.instruction.rs == mem_dst_reg) {
                    reservation_entry.instruction.read_data_2 = mem_result;
                    reservation_entry.r1_ready = true;
                }
            }

            if(!reservation_entry.r2_ready) {
                if(reservation_entry.instruction.rt == execute_dst_reg) {
                    reservation_entry.instruction.read_data_2 = execute_result;
                    reservation_entry.r2_ready = true;
                } else if (reservation_entry.instruction.rt == mem_dst_reg) {
                    reservation_entry.instruction.read_data_2 = mem_result;
                    reservation_entry.r2_ready = true;
                }
            }
        }

        if(reservation_station.empty()) { // if the reservation station is empty
            // we take the the first unissued instruction and try to move it to execute directly
            // if that's not possible, we put it in the reservation station
            if(!reorder_buffer.empty()) {
                // auto first = reorder_buffer.front();
                
            }

        } else {
            // if there's already instructions in the reservation stations, we see if we can move one of them to the execute stage and one of them to the mem stage
            
            auto it = reservation_station.begin(); 

            while (it != reservation_station.end()) {
                reservation_entry entry = *it;

                if(entry.r1_ready && entry.r2_ready && (!entry.instruction.mem_write && !entry.instruction.mem_read)) {
                    break;
                }
            }

            if(it != reservation_station.end()) {
                reservation_entry ready_entry = *it;
                reservation_station.erase(it);
                issue_execute_out = ready_entry.instruction;
            }


            it = reservation_station.begin();

            while (it != reservation_station.end()) {
                reservation_entry entry = *it;

                if(entry.r1_ready && entry.r2_ready && (entry.instruction.mem_write || entry.instruction.mem_read)) {
                    break;
                }
            }

            if(it != reservation_station.end()) {
                reservation_entry ready_entry = *it;
                reservation_station.erase(it);
                issue_memory_out = ready_entry.instruction;
            }
        
            // in parallel, we would also move an unissued instruction into the reservation stations if possible
        }

    }
    
    { // Execute + Mem
        // Single Execute Unit
        uint32_t alu_zero = 0;


        alu.generate_control_inputs(exec_in.ALU_op, exec_in.funct, exec_in.opcode);
        uint32_t operand_1 = exec_in.shift ? exec_in.shamt : exec_in.read_data_1;
        uint32_t operand_2 = exec_in.ALU_src ? exec_in.imm : exec_in.read_data_2;
        uint32_t alu_result = alu.execute(operand_1, operand_2, alu_zero);

        execute_dst_reg_out = exec_in.reg_dest == 1 ? exec_in.rd : exec_in.rt;
        execute_result_out = alu_result;
        DEBUG(cout << "exec fetch_pc: " << exec_in.pc << ", dst: " << execute_dst_reg_out << ", rs: " << exec_in.rs << "\n");


        // Single Mem Unit
        uint32_t read_data_mem = 0;
        uint32_t write_data_mem = 0;
        // First read no matter whether it is a load or a store

        // This is always the same operation, so it should be able to be ran in the same cycle as the rest of mem
        uint32_t mem_alu_result = mem_in.rs + mem_in.imm; 

        bool mem_success = memory->access(mem_alu_result, read_data_mem, 0, 
            mem_in.mem_read | mem_in.mem_write, 0);

        cout << "mem fetch_pc = " << mem_in.pc << ", addr = " << mem_alu_result << "\n"; 
        if(mem_success) {
            // Stores: sb or sh mask and preserve original leftmost bits
            write_data_mem = mem_in.halfword ? (read_data_mem & 0xffff0000) | (mem_in.read_data_2 & 0xffff) : 
                            mem_in.byte ? (read_data_mem & 0xffffff00) | (mem_in.read_data_2 & 0xff): mem_in.read_data_2;
            // Write to memory only if mem_write is 1, i.e store
            memory->access(mem_alu_result, read_data_mem, write_data_mem, 
                mem_in.mem_read, mem_in.mem_write);
            // Loads: lbu or lhu modify read data by masking
            read_data_mem &= mem_in.halfword ? 0xffff : mem_in.byte ? 0xff : 0xffffffff;

            cout << "mem read " << read_data_mem << "\n";

            if(!mem_in.mem_read) {
                mem_dst_reg_out = 32;
                mem_result_out = 0;
            } else {
                mem_dst_reg_out = mem_in.reg_dest == 1 ? mem_in.rd : mem_in.rt;
                mem_result_out = read_data_mem;   
            }
        } else {
            mem_stall = true;
            DEBUG(cout << "mem_stalling\n";)
        }
    }
    { // Commit
        // Look at head of reorder buffer, see if it's ready to commit
        // Write to retirement register file and free the dst register 
        // Maybe can't free dst register until all readers of that register have been committed
        if(!reorder_buffer.empty()) {
            auto first_it = reorder_buffer.begin();
            reorder_entry first = *first_it;
            uint32_t _unused_read; // also need to check phys regfile for ready registers, to see if we can commit
            if(first.src_reg == execute_dst_reg) {
                retirement_regfile.access(0, 0, _unused_read, _unused_read, first.dst_reg, first.reg_write, execute_result);
                retirement_regfile.pc = first.pc;
                reorder_buffer.erase(first_it);
            } else if(first.src_reg == mem_dst_reg) {
                retirement_regfile.access(0, 0, _unused_read, _unused_read, first.dst_reg, first.reg_write, mem_result);
                retirement_regfile.pc = first.pc;
                reorder_buffer.erase(first_it);
            }

        }
    }

    { // Update, not a processor stage

        // Move values from output of each stage to input of next

        // fetched to decode
        if(!if_stall && !id_stall) {
            fetch_pc = fetch_pc_out;
            fetched_instruction = instruction_out;
        } else if(!id_stall) {
            fetched_instruction.instruction = 0;
            fetched_instruction.pc = 0;
            fetch_pc = fetch_pc_out;
        } else if (id_stall) {
            reorder_buffer.push_back(rob_entry_out);
            assert(reorder_buffer.size() <= reorder_buffer_capacity);            
        }
        // issue to exec/mem



        //exec/mem to out's
        execute_dst_reg = execute_dst_reg_out == 0 ? 32 : execute_dst_reg_out;
        execute_result = execute_result_out;

        if(!mem_stall) {
            mem_dst_reg = mem_dst_reg_out == 0 ? 32 : mem_dst_reg_out;
            mem_result = mem_result_out;
        }
    }

    return;
}