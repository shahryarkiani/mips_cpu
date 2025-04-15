#ifndef _SUPERSCALAR_H
#define _SUPERSCALAR_H
#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include "pipeline.h"

class SuperscalarProcessor {
    private:
        int opt_level;
        ALU alu;
        control_t control;
        Memory *memory;
        Registers &regfile;
        uint32_t fetch_pc;

        // A flag to keep track of whether the instructions in pipeline B
        // are after the corresponding ones in B(aka in-order)
        bool in_order = true;

        // need to duplicate each pipeline register
        // if_in is just fetch_pc
        if_id_buffer_t if_out_a;
        if_id_buffer_t if_out_b;
        if_id_buffer_t id_in_a;
        if_id_buffer_t id_in_b;

        id_ex_buffer_t id_out_a;
        id_ex_buffer_t id_out_b;
        id_ex_buffer_t ex_in_a;
        id_ex_buffer_t ex_in_b;

        ex_mem_buffer_t ex_out_a;
        ex_mem_buffer_t ex_out_b;
        ex_mem_buffer_t mem_in_a;
        ex_mem_buffer_t mem_in_b;
        
        mem_wb_buffer_t mem_out_a;
        mem_wb_buffer_t mem_out_b;
        mem_wb_buffer_t wb_in_a;
        mem_wb_buffer_t wb_in_b;
        //wb_out is just regfile

    public:
        SuperscalarProcessor(Memory *mem, Registers& other_regfile) : regfile(other_regfile) {
            regfile.pc = 0;
            fetch_pc = 0;
            memory = mem; 

            if_out_a.reset();
            if_out_b.reset();

            id_in_a.reset();
            id_in_b.reset();

            id_out_a.reset();
            id_out_b.reset();

            ex_in_a.reset();
            ex_in_b.reset();
            
            ex_out_a.reset();
            ex_out_b.reset();
            
            mem_in_a.reset();
            mem_in_b.reset();
            
            mem_out_a.reset();
            mem_out_b.reset();
            
            wb_in_a.reset();
            wb_in_b.reset();
        }

        // Get PC
        uint32_t getPC() { return regfile.pc; }

        // Prints the Register File
        void printRegFile() { regfile.print(); }
        
        void initialize();

        // Advances the processor to an appropriate state every cycle
        void advance(); 
};
#endif 