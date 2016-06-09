#ifndef _UNITTESTS_H_
#define _UNITTESTS_H_

// ========================
// = Standard C Libraries =
// ========================
#include <cstdio>     // printf
#include <ctime>      // time() for random seed
#include <cmath>      // fabs, fmax, ...

// ===========================
// = CNN Network Definitions =
// ===========================
#include "network.hpp"    // load before netconfig.hpp for bit-width calculation
#include "netconfig.hpp"  // network config (layer_t, network_t)

// ==================
// = FPGA Algorithm =
// ==================
#include "fpga_top.hpp"  // top-level FPGA module

// ==============
// = Unit Tests =
// ==============

bool do_unittests();

#endif
