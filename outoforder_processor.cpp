#include "outoforder_processor.h"
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
    * Fetch 2 instructions at a time into instruction queue
    * Decode + Rename Instructions
    * Issue Instructions
    * Execute Instructions / Do Mem Operations
    * Commit
    */
    
    // Fetch stage will write to these variables
    uint32_t fetch_pc_out;
    instruction instruction_out;
    bool if_stall;

    // Decode stage will write to these
    decoded_instr decoded_instr_out;

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
        reorder_entry rob_entry;
        rob_entry.pc = fetched_instruction.pc;
        rob_entry.mem_write = decoded.mem_write;

        if()        

    }

    { // Issue, move instruction from reorder buffer to reservation station
        // In parallel, find ready instruction and put it into execute stage
    }
    
    { // Execute + Mem
        // Single Execute Unit

        // Single Mem Unit
    }

    { // Writeback
        // Take the results of the execute units and store them in the physical register file
        // This can happen in parallel with Commit, which is to another register file
    }

    { // Commit
        // Look at head of reorder buffer, see if it's ready to commit
        // Write to retirement register file and free the dst register 
        // Maybe can't free dst register until all readers of that register have been committed

    }

    { // Update, not a processor stage

        // Move values from output of each stage to input of next
    }

    return;
}