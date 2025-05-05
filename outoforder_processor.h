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
    uint8_t dst_reg;
    uint8_t src_reg;
    bool reg_write;
    bool issued;
    decoded_instr instruction;
};

struct reservation_entry {
    decoded_instr instruction;
    bool r1_ready;
    bool r2_ready;
};

struct reg_mapping {
    uint8_t arch_reg; // from
    uint8_t phys_reg; // to
    uint8_t read_count;
    bool ready;
};


class OutOfOrderProcessor {
    private:
        int opt_level;
        ALU alu;
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
        std::deque<reservation_entry> reservation_station;

        uint8_t execute_dst_reg = 32;
        uint8_t execute_result = 0;

        uint8_t mem_dst_reg = 32;
        uint8_t mem_result = 0;

        decoded_instr exec_in{};
        decoded_instr mem_in{};

        const size_t reorder_buffer_capacity = 16;
        const size_t reservation_station_capacity = 8;
        const size_t map_table_capacity = 32;

        bool is_reg_ready(uint8_t phys_reg) {
            for(auto& map_entry : map_table) {
                if(map_entry.phys_reg == phys_reg) {
                    return map_entry.ready;
                }
            }

            return false;
        }

    public:
        OutOfOrderProcessor(Memory *mem, Registers& other_regfile) : retirement_regfile(other_regfile) {
            regfile.pc = 0;
            fetch_pc = 0;
            memory = mem; 

            for(uint8_t i = 0; i < 32; i++) {
                // physical registers are from 32 to 63
                // makes it easy to detect which regfile we should be accessing
                free_list.push_back(i + 32);
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