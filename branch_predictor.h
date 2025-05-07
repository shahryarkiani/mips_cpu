#ifndef _BRANCH_PREDICTOR_H
#define _BRANCH_PREDICTOR_H
#include <bitset>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <deque>

#define HISTORY_LENGTH (12)
#define THRESHOLD ((int)(1.93 * HISTORY_LENGTH) + 14)
struct perceptron_table_entry {
  uint32_t pc;
  uint32_t target;
  int8_t bias_weight;
  int8_t weights[HISTORY_LENGTH];  
};

class BranchPredictor{
    private:

    uint32_t hash(uint32_t value) {
        return ((value >> 2) ^ (value >> 10) ^ (value >> 18) ^ (value >> 26)) & 0b111111;
    }


    std::bitset<HISTORY_LENGTH> history;

    perceptron_table_entry perceptrons_table[64];

    public:
    BranchPredictor() {
        history.reset();

        for(int i = 0; i < 64; i++) {
            perceptrons_table[i] = {};
            perceptrons_table[i].pc = UINT32_MAX;
            perceptrons_table[i].target = UINT32_MAX;
            perceptrons_table[i].bias_weight = 1;
        }
    }

    int makePrediction(uint32_t pc) {
        uint8_t hash = BranchPredictor::hash(pc);

        const auto perceptron = perceptrons_table[hash];

        if(perceptron.pc != pc) {
            return -1;
        }

        int sum = perceptron.bias_weight;

        for(int i = 0; i < HISTORY_LENGTH; i++) {
            sum += perceptron.weights[i] * (history[i] ? 1 : -1);
        }
        
        return sum;
    }

    uint32_t getTarget(uint32_t pc) {
        uint8_t hash = BranchPredictor::hash(pc);

        const auto perceptron = perceptrons_table[hash];

        assert(pc == perceptron.pc);

        return perceptron.target;
    }
    
    void recordBranch(uint32_t pc, uint32_t target, bool actual, bool predicted, int perceptron_sum) {
        uint8_t hash = BranchPredictor::hash(pc);
    
        auto& perceptron = perceptrons_table[hash];

        if(perceptron.pc != pc) {
            perceptron.pc = pc;
            perceptron.target = target;

            // reset weights for new pc
            perceptron.bias_weight = 1;
            for(int i = 0; i < HISTORY_LENGTH; i++) {
                perceptron.weights[i] = 0;
            }
        }

        
        if(actual != predicted || abs(perceptron_sum) <= THRESHOLD) {
             
            if(actual && perceptron.bias_weight != INT8_MAX) {
                perceptron.bias_weight++;
            } else if(perceptron.bias_weight != INT8_MIN) {
                perceptron.bias_weight--;
            }


            for(int i = 0; i < HISTORY_LENGTH; i++) {
                if(actual == history[i] && perceptron.weights[i] != INT8_MAX) {
                    perceptron.weights[i]++;
                } else if (perceptron.weights[i] != INT8_MIN) {
                    perceptron.weights[i]--;
                }
            }
        }

        history = history << 1;
        history.set(0, actual);
    }
};

#endif