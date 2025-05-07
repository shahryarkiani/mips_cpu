#ifndef _BRANCH_PREDICTOR_H
#define _BRANCH_PREDICTOR_H
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <deque>

#define HISTORY_LENGTH (12)
#define THRESHOLD ((int)(1.93 * HISTORY_LENGTH) + 14)
struct perceptron_table_entry {
  uint32_t pc;
  uint32_t target;
  int8_t weights[HISTORY_LENGTH];  
};

class BranchPredictor{
    private:
    std::deque<bool> history;

    perceptron_table_entry perceptrons_table[64];

    public:
    BranchPredictor() {
        for(int i = 0; i < HISTORY_LENGTH; i++) {
            history.push_back(false);
        }

        for(int i = 0; i < 64; i++) {
            perceptrons_table[i] = {};
            perceptrons_table[i].pc = UINT32_MAX;
            perceptrons_table[i].weights[0] = 1;
        }
    }

    bool makePrediction(uint32_t pc) {
        uint8_t hash = ((pc >> 2) ^ (pc >> 10) ^ (pc >> 18) ^ (pc >> 26)) & 0b111111;


        const auto perceptron = perceptrons_table[hash];

        if(perceptron.pc != pc) {
            return false;
        }

        int sum = perceptron.weights[0];

        for(int i = 1; i < HISTORY_LENGTH; i++) {
            sum += perceptron.weights[i] * (history[i - 1] ? 1 : -1);
        }
        
        return sum >= 0;
    }

    uint32_t getTarget(uint32_t pc) {
        uint8_t hash = ((pc >> 2) ^ (pc >> 10) ^ (pc >> 18) ^ (pc >> 26)) & 0b111111;

        const auto perceptron = perceptrons_table[hash];

        assert(pc == perceptron.pc);

        return perceptron.target;
    }
    
    void recordBranch(uint32_t pc, uint32_t target, bool actual, bool predicted) {
        uint8_t hash = ((pc >> 2) ^ (pc >> 10) ^ (pc >> 18) ^ (pc >> 26)) & 0b111111;
    
        auto& perceptron = perceptrons_table[hash];

        if(perceptron.pc != pc) {
            perceptron.pc = pc;
            perceptron.target = target;

            perceptron.weights[0] = 1;
            for(int i = 1; i < HISTORY_LENGTH; i++) {
                perceptron.weights[i] = 0;
            }
        }

        
        int sum = perceptron.weights[0];

        for(int i = 1; i < HISTORY_LENGTH; i++) {
            sum += perceptron.weights[i] * (history[i - 1] ? 1 : -1);
        }

        if(actual != predicted || abs(sum) <= THRESHOLD) {
            if(actual && perceptron.weights[0] != INT8_MAX) {
                perceptron.weights[0]++;
            } else if(perceptron.weights[0] != INT8_MIN) {
                perceptron.weights[0]--;
            }


            for(int i = 1; i < HISTORY_LENGTH; i++) {
                if(actual == history[i - 1] && perceptron.weights[i] != INT8_MAX) {
                    perceptron.weights[i]++;
                } else if (actual != history[i - 1] && perceptron.weights[i] != INT8_MIN) {
                    perceptron.weights[i]--;
                }
            }
        }

        history.push_front(actual);
        history.pop_back();
    }
};

#endif