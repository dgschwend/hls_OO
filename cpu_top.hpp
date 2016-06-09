#ifndef _FPGA_SIMULATOR_H_
#define _FPGA_SIMULATOR_H_

// ========================
// = Standard C Libraries =
// ========================
#include <cstdio>     // printf
#include <ctime>      // time() for random seed
#include <cmath>      // fabs, fmax, ...
#include <vector>     // std::vector for softmax calculation
#include <algorithm>  // sort, reverse (on std::vector)

// ===========================
// = CNN Network Definitions =
// ===========================
#include "network.hpp"    // load before netconfig.hpp for bit-width calculation
#include "netconfig.hpp"  // network config (layer_t, network_t)

// ==============
// = Unit Tests =
// ==============
#include "unittests.hpp"  // Unit Tests for Modules

// ==================
// = FPGA Algorithm =
// ==================
#include "fpga_top.hpp"  // top-level FPGA module

#endif
