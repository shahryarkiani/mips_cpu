#ifndef _OUTOFORDER_H
#define _OUTOFORDER_H
#include "memory.h"
#include "regfile.h"
#include "ALU.h"
#include "control.h"
#include <cstddef>
#include <cstdint>
#include <deque>
#include "pipeline.h"   

struct instruction {
    uint32_t instruction;
    uint32_t pc;
};

typedef id_ex_buffer_t decoded_instr;

struct reorder_entry {
    uint32_t pc;
    uint8_t dest_reg;
    bool mem_write;
    decoded_instr instruction;
};

struct reg_mapping {
    uint8_t reg_num;
    uint8_t phys_reg;
    bool ready;
};


class OutOfOrderProcessor {
    private:
        int opt_level;
        ALU alu_a;
        ALU alu_b;
        control_t control;
        Memory *memory;
        // Visible architectural registers
        Registers &retirement_regfile;

        // Internal register file
        Registers regfile;

        std::vector<reg_mapping> map_table;
        std::vector<uint8_t> free_list;
        uint32_t fetch_pc;

        instruction fetched_instruction;
        std::deque<reorder_entry> reorder_buffer;

        // We keep a single shared reservation station
        std::vector<uint32_t> reservation_station;


        const size_t reorder_buffer_capacity = 16;
        const size_t reservation_station_capacity = 8;
        const size_t map_table_capacity = 32;
    public:
        OutOfOrderProcessor(Memory *mem, Registers& other_regfile) : retirement_regfile()(other_regfile) {
            regfile.pc = 0;
            fetch_pc = 0;
            memory = mem; 

            for(uint8_t i = 0; i < 32; i++) {
                free_list.add(i);
            }
        }

        // Get PC
        uint32_t getPC() { return retirement_regfile.pc; }

        // Prints the Register File
        void printRegFile() { retirement_regfile.print(); }
        
        void initialize();

        // Advances the processor to an appropriate state every cycle
        void advance(); 
};
#endif 