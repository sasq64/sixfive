#pragma once

#include "emulator.h"
#include <string>

namespace sixfive {

int assemble(int pc, uint8_t *output, const std::string &code);

};
