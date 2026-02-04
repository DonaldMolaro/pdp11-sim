#pragma once

#include <cstdint>
#include <string>

#include "pdp11.h"

namespace pdp11 {

std::string disassemble(const CPU& cpu, uint16_t pc);

} // namespace pdp11
