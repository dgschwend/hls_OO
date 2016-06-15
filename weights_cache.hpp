//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  weights_cache.hpp
//
//  Weights Cache Module for FPGA
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#ifndef WEIGHTS_CACHE_HPP_E36181B8
#define WEIGHTS_CACHE_HPP_E36181B8

// Data Types for FPGA Implementation
#include "fpga_top.hpp"
#include "memory_controller.hpp"

// =================
// = Weights Cache =
// =================
class WeightsCache {
 public:
  WeightsCache();
  void setLayerConfig(layer_t &layer);
  void addWeight(data_t weight);
  void loadFromDRAM(MemoryController *DRAM);
  void setInputChannel(channel_t ci);
  void getNineWeights(channel_t co, data_t wbuffer[9]);
  data_t getOneWeight(channel_t co);

 private:
  data_t getWeight(const weightaddr_t addr);
  weightaddr_t write_addr;
  data_t BRAM[MAX_WEIGHTS_PER_LAYER];
  kernel_t kernel;
  channel_t ch_out;
  channel_t ch_in;
  weightaddr_t ci_offset;
  numfilterelems_t weights_per_filter;
};

#endif /* end of include guard: WEIGHTS_CACHE_HPP_E36181B8 */
