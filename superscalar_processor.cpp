#include "superscalar_processor.h"

void SuperscalarProcessor::advance() {
    regfile.pc += 4;
}