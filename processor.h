#ifndef _PROCESSOR_H
#define _PROCESSOR_H
#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include "pipeline.h"
#include "superscalar_bp_processor.h"
#include "superscalar_processor.h"
#include "outoforder_processor.h"
#include <memory>
class Processor {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers regfile;
        unique_ptr<SuperscalarProcessor> superscalar_processor;
        unique_ptr<SuperscalarBpProcessor> superscalar_bp_processor;
        unique_ptr<OutOfOrderProcessor> outoforder_processor;
        uint32_t fetch_pc;

        // if_in is just pc
        if_id_buffer_t if_out;
        if_id_buffer_t id_in;

        id_ex_buffer_t id_out;
        id_ex_buffer_t ex_in;

        ex_mem_buffer_t ex_out;
        ex_mem_buffer_t mem_in;
        
        mem_wb_buffer_t mem_out;
        mem_wb_buffer_t wb_in;
        //wb_out is just regfile

        // add private functions
        void single_cycle_processor_advance();
        void pipelined_processor_advance();
        void superscalar_processor_advance();
    public:
        Processor(Memory *mem) { 
            regfile.pc = 0;
            fetch_pc = 0;
            memory = mem; 

            if_out.reset();
            id_in.reset();
            id_out.reset();
            ex_in.reset();
            ex_out.reset();
            mem_in.reset();
            mem_out.reset();
            wb_in.reset();
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
#endif