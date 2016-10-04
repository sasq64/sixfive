#pragma once

#include "emulator.h"
#include <string>

namespace sixfive {

int assemble(int pc, Word *output, const std::string &code);

};
