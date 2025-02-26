#ifndef PIPELINE_CLASS
#define PIPELINE_CLASS
#include <vector>
#include <cstdint>
#include <iostream>
using namespace std;

struct if_id_buffer_t {
    uint32_t instruction;
    uint32_t pc;
    void reset() {
        instruction = 0;
        pc = 0;
    }
};

struct id_ex_buffer_t {
    bool reg_dest;           // 0 if rt, 1 if rd
    bool jump;               // 1 if jummp
    bool jump_reg;           // 1 if jr
    bool link;               // 1 if jal
    bool shift;              // 1 if sll or srl
    bool branch;             // 1 if branch
    bool bne;                // 1 if bne
    bool mem_read;           // 1 if memory needs to be read
    bool mem_to_reg;         // 1 if memory needs to written to reg
    unsigned ALU_op : 2;     // 10 for R-type, 00 for LW/SW, 01 for BEQ/BNE, 11 for others
    bool mem_write;          // 1 if needs to be written to memory
    bool halfword;           // 1 if loading/storing halfword from memory
    bool byte;               // 1 if loading/storing a byte from memory
    bool ALU_src;            // 0 if second operand is from reg_file, 1 if imm
    bool reg_write;          // 1 if need to write back to reg file
    bool zero_extend;        // 1 if immediate needs to be zero-extended


    // information about instruction stored in this stage
    int opcode;
    int rs;
    int rt;
    int rd;
    int shamt;
    int funct;
    uint32_t imm;
    uint32_t pc;
    // Variables to read data into(regfile -> read_data_)
    uint32_t read_data_1;
    uint32_t read_data_2;

    void reset() {
        pc = 0;
        reg_dest = 0;         
        jump = 0;             
        jump_reg = 0;         
        link = 0;             
        shift = 0;            
        branch = 0;           
        bne = 0;              
        mem_read = 0;         
        mem_to_reg = 0;       
        ALU_op = 0;           
        mem_write = 0;         
        halfword = 0;          
        byte = 0;              
        ALU_src = 0;           
        reg_write = 0;          
        zero_extend = 0;        
        opcode = 0;
        rs = 0;
        rs = 0;
        rt = 0;
        rd = 0;
        shamt = 0;
        funct = 0;
        imm = 0;
        read_data_1 = 0;
        read_data_2 = 0;
    }

    // Extract information from instruction into variables
    void load(uint32_t instruction) {
        decode(instruction);
        opcode = (instruction >> 26) & 0x3f;
        rs = (instruction >> 21) & 0x1f;
        rt = (instruction >> 16) & 0x1f;
        rd = (instruction >> 11) & 0x1f;
        shamt = (instruction >> 6) & 0x1f;
        funct = instruction & 0x3f;
        imm = (instruction & 0xffff);
        read_data_1 = 0;
        read_data_2 = 0;
    }

    // Decode instructions into control signals
    void decode(uint32_t instruction) {
        reset();
        // Get OPCODE
        int opcode = (instruction >> 26) & 0x3f;
        
        if (!opcode) { // R-Type Instructions
            reg_dest = 1;
            reg_write = 1;
            ALU_op = 2;

            // Special Case: jr
            if ((instruction & 0x3f) == 0x08) {
                reg_dest = 0;
                reg_write = 0;
                ALU_op = 0;
                jump = 1;
                jump_reg = 1;
            }

            // Special Case: shift
            if ((instruction & 0x3f) == 0x0 || (instruction & 0x3f) == 0x2) {
                shift = 1;
            }
        } // end R-Type
        
        else if (opcode == 0x2 || opcode == 0x3) { // J-Type Instructions
            jump = 1;
            
            // Special Case: jal
            if (opcode == 0x3) {
                link = 1;
                reg_write = 1;
            }
        } // end J-Type

        else { // I-Type Instructions
            ALU_src = 1; // choose immediate for alu source
            
            if (opcode == 0x4 || opcode == 0x5) { // beq, bne
                branch = 1;
                ALU_op = 1;
                ALU_src = 0;
                
                // Special Case: bne
                if (opcode == 0x5) {
                    bne = 1;
                }
            } // end beq, bne
            
            else if (opcode == 0x2b || opcode == 0x28 || opcode == 0x29) { // Stores
                mem_write = 1;

                // Special Case: sb, sh
                if (opcode == 0x28) { // sb
                    byte = 1;
                }
                else if (opcode == 0x29) { // sh
                    halfword = 1;
                }
            } // end stores

            else if ((opcode >= 0x23 && opcode <= 0x25) || opcode == 0x30) { // Loads
                mem_read = 1;
                mem_to_reg = 1;
                reg_write = 1;
                
                // Special Case: lbu, lhu
                if (opcode == 0x24) { // lbu
                    byte = 1;
                }
                else if (opcode == 0x25) { // lhu
                    halfword = 1;
                }
            } // end loads

            else { // Catch all I-type Instrcutions
                reg_write = 1;
                ALU_op = 3; 
                // Special Case: ori, andi
                if (opcode == 0xc || opcode == 0xd) {
                    zero_extend = 1;
                }
            }

        }// end I-Type
    }
};

struct ex_mem_buffer_t {
    bool reg_dest;           // 0 if rt, 1 if rd
    bool jump;               // 1 if jummp
    bool jump_reg;           // 1 if jr
    bool link;               // 1 if jal
    bool branch;             // 1 if branch
    bool bne;                // 1 if bne
    bool mem_read;           // 1 if memory needs to be read
    bool mem_to_reg;         // 1 if memory needs to written to reg
    bool mem_write;          // 1 if needs to be written to memory
    bool halfword;           // 1 if loading/storing halfword from memory
    bool byte;               // 1 if loading/storing a byte from memory
    bool reg_write;          // 1 if need to write back to reg file

    // information about instruction stored in this stage
    int opcode;
    int rs;
    int rt;
    int rd;
    uint32_t imm;
    uint32_t pc;
    // Variables to read data into(regfile -> read_data_)
    uint32_t read_data_1;
    uint32_t read_data_2;
    uint32_t alu_result;

    void reset() {
        pc = 0;
        reg_dest = 0;         
        jump = 0;             
        jump_reg = 0;         
        link = 0;             
        branch = 0;           
        bne = 0;              
        mem_read = 0;         
        mem_to_reg = 0;       
        mem_write = 0;         
        halfword = 0;          
        byte = 0;              
        reg_write = 0;          
        opcode = 0;
        rs = 0;
        rs = 0;
        rt = 0;
        rd = 0;
        imm = 0;
        read_data_1 = 0;
        read_data_2 = 0;
        alu_result = 0;
    }    

    void load_from(const id_ex_buffer_t& id_ex_buffer) {
        reg_dest = id_ex_buffer.reg_dest;
        jump = id_ex_buffer.jump;
        jump_reg = id_ex_buffer.jump_reg;
        link = id_ex_buffer.link;
        branch = id_ex_buffer.branch;
        bne = id_ex_buffer.bne;
        mem_read = id_ex_buffer.mem_read;
        mem_to_reg = id_ex_buffer.mem_to_reg;
        mem_write = id_ex_buffer.mem_write;
        halfword = id_ex_buffer.halfword;
        byte = id_ex_buffer.byte;
        reg_write = id_ex_buffer.reg_write;

        opcode = id_ex_buffer.opcode;
        rs = id_ex_buffer.rs;
        rt = id_ex_buffer.rt;
        rd = id_ex_buffer.rd;
        imm = id_ex_buffer.imm;
        pc = id_ex_buffer.pc;
        read_data_1 = id_ex_buffer.read_data_1;
        read_data_2 = id_ex_buffer.read_data_2;
    }
};

struct mem_wb_buffer_t {
    bool reg_dest;           // 0 if rt, 1 if rd
    bool link;               // 1 if jal
    bool mem_to_reg;         // 1 if memory needs to written to reg
    bool reg_write;          // 1 if need to write back to reg file

    // information about instruction stored in this stage
    int opcode;
    int rs;
    int rt;
    int rd;
    uint32_t imm;
    uint32_t pc;
    // Variables to read data into(regfile -> read_data_)
    uint32_t read_data_1;
    uint32_t read_data_2;
    uint32_t alu_result;
    uint32_t read_data_mem;

    void reset() {
        pc = 0;
        reg_dest = 0;         
        link = 0;             
        mem_to_reg = 0;       
        reg_write = 0;          
        opcode = 0;
        rs = 0;
        rs = 0;
        rt = 0;
        rd = 0;
        imm = 0;
        read_data_1 = 0;
        read_data_2 = 0;
        alu_result = 0;
        read_data_mem = 0;
    }    

    void load_from(const ex_mem_buffer_t& mem_ex_buffer) {
        reg_dest = mem_ex_buffer.reg_dest;
        link = mem_ex_buffer.link;
        mem_to_reg = mem_ex_buffer.mem_to_reg;
        reg_write = mem_ex_buffer.reg_write;

        opcode = mem_ex_buffer.opcode;
        rs = mem_ex_buffer.rs;
        rt = mem_ex_buffer.rt;
        rd = mem_ex_buffer.rd;
        imm = mem_ex_buffer.imm;
        pc = mem_ex_buffer.pc;
        read_data_1 = mem_ex_buffer.read_data_1;
        read_data_2 = mem_ex_buffer.read_data_2;
        alu_result = mem_ex_buffer.alu_result;
    }
};

#endif
