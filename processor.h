#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include "pipeline.h"
class Processor {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;
        uint32_t fetch_pc;
        // add other structures as needed

        // pipelined processor
        pipeline_buffer_t if_id; //  fetch wrtites to this, decode reads from it
        pipeline_buffer_t id_ex; //  decode writes to this, alu reads from it
        pipeline_buffer_t ex_mem; // alu writes to this, mem reads from it
        pipeline_buffer_t mem_wb; // mem writes to this, writeback reads from it
        // add private functions
        void single_cycle_processor_advance();
        void pipelined_processor_advance();
 
    public:
        Processor(Memory *mem) { 
            regfile.pc = 0;
            fetch_pc = 0;
            memory = mem; 
            if_id.reset();
            id_ex.reset();
            ex_mem.reset();
            mem_wb.reset();
        }

        // Get PC
        uint32_t getPC() { return regfile.pc; }

        // Prints the Register File
        void printRegFile() { regfile.print(); }
        
        // Initializes the processor appropriately based on the optimization level
        void initialize(int opt_level);

        // Advances the processor to an appropriate state every cycle
        void advance(); 
};
