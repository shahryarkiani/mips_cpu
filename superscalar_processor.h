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

        // need to duplicate each pipeline register
        // if_in is just fetch_pc
        if_id_buffer_t if_out_1;
        if_id_buffer_t if_out_2;
        if_id_buffer_t id_in_1;
        if_id_buffer_t id_in_2;

        id_ex_buffer_t id_out_1;
        id_ex_buffer_t id_out_2;
        id_ex_buffer_t ex_in_1;
        id_ex_buffer_t ex_in_2;

        ex_mem_buffer_t ex_out_1;
        ex_mem_buffer_t ex_out_2;
        ex_mem_buffer_t mem_in_1;
        ex_mem_buffer_t mem_in_2;
        
        mem_wb_buffer_t mem_out_1;
        mem_wb_buffer_t mem_out_2;
        mem_wb_buffer_t wb_in_1;
        mem_wb_buffer_t wb_in_2;
        //wb_out is just regfile

    public:
        SuperscalarProcessor(Memory *mem, Registers& other_regfile) : regfile(other_regfile) {
            regfile.pc = 0;
            fetch_pc = 0;
            memory = mem; 

            if_out_1.reset();
            if_out_2.reset();

            id_in_1.reset();
            id_in_2.reset();

            id_out_1.reset();
            id_out_2.reset();

            ex_in_1.reset();
            ex_in_2.reset();
            
            ex_out_1.reset();
            ex_out_2.reset();
            
            mem_in_1.reset();
            mem_in_2.reset();
            
            mem_out_1.reset();
            mem_out_2.reset();
            
            wb_in_1.reset();
            wb_in_2.reset();
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