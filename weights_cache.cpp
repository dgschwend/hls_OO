//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  weights_cache.cpp
//
//  Weights Cache Module for FPGA
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#include "weights_cache.hpp"

// =================
// = Weights Cache =
// =================
WeightsCache::WeightsCache(){};

void WeightsCache::print_setup() {
  LOG("MAX_WEIGHTS_PER_LAYER   = %d\n", (int)MAX_WEIGHTS_PER_LAYER);
  LOG("weightaddr_t write_addr = %d\n", (int)write_addr);
  LOG("MAX_WEIGHTS_PER_LAYER   = %d\n", (int)MAX_WEIGHTS_PER_LAYER);
  LOG("kernel_t kernel         = %d\n", (int)kernel);
  LOG("channel_t ch_out        = %d\n", (int)ch_out);
  LOG("channel_t ch_in         = %d\n", (int)ch_in);
  LOG("weightaddr_t ci_offset  = %d\n", (int)ci_offset);
  LOG("weights_per_filter      = %d\n", (int)weights_per_filter);
}

void WeightsCache::loadFromDRAM(MemoryController *DRAM) {
  LOG("WeightsCache: loadFromDRAM (total %d weights, %d biases)\n",
      (int)(ch_in * ch_out * weights_per_filter), (int)ch_out);
  LOG_LEVEL++;
  LOG_LEVEL++;

// Load Filter Coefficients
  weightaddr_t num_weights = ch_in * ch_out * weights_per_filter;
  assert(num_weights <= MAX_WEIGHTS_PER_LAYER && "Loading too many Weights!");
L_WCACHE_LOAD_WEIGHTS:
  for (weightaddr_t addr = 0; addr < num_weights; addr++) {
    data_t weight;
    weight = DRAM->loadNextWeight();
    addWeight(weight);
  }
// Load Biases
  assert(ch_out <= MAX_NUM_CHOUT && "Tried to load too many Biases!");
L_WCACHE_LOAD_BIAS:
  for (weightaddr_t addr = 0; addr < ch_out; addr++) {
    data_t bias = DRAM->loadNextWeight();
    addWeight(bias);
  }

  LOG_LEVEL--;
  LOG_LEVEL--;
}
void WeightsCache::addWeight(data_t weight) {
  assert(write_addr < MAX_WEIGHTS_PER_LAYER && "Too many weights loaded!");
  if (LOG_DETAILS)
    LOG("WeightsCache: addWeight WCACHE[%3d] = %6.2f\n", (int)write_addr,
        weight);
  BRAM[write_addr] = weight;
  write_addr++;
}
void WeightsCache::setLayerConfig(layer_t &layer) {
  kernel = layer.kernel;
  ch_in = layer.channels_in;
  ch_out = layer.channels_out;
  weights_per_filter = (kernel == 3) ? 9 : 1;
  write_addr = 0;

  LOG("WeightsCache: setLayerConfig\n");
  LOG(" - kernel          = %d\n", (int)kernel);
  LOG(" - ch_in           = %d\n", (int)ch_in);
  LOG(" - ch_out          = %d\n", (int)ch_out);
  LOG(" - w_per_filter    = %d\n", (int)weights_per_filter);
}
void WeightsCache::setInputChannel(channel_t ci) {
  ci_offset = ci * ch_out * weights_per_filter;
  LOG("WeightsCache: setInputChannel(%d), ci_offset = %d Elements, @%luB\n",
      (int)ci, (int)ci_offset, (int)ci_offset * sizeof(data_t));
}
void WeightsCache::getNineWeights(channel_t co, data_t wbuffer[9]) {
  weightaddr_t addr = ci_offset + co * weights_per_filter;
  LOG("WeightsCache: getNineWeights( co=%-2d ) from WCache[%3d]+\n", (int)co,
      (int)addr);
  wbuffer[0] = (kernel == 3) ? BRAM[addr + 0] : 0.0;
  wbuffer[1] = (kernel == 3) ? BRAM[addr + 1] : 0.0;
  wbuffer[2] = (kernel == 3) ? BRAM[addr + 2] : 0.0;
  wbuffer[3] = (kernel == 3) ? BRAM[addr + 3] : 0.0;
  wbuffer[4] = (kernel == 3) ? BRAM[addr + 4] : BRAM[addr + 0];
  wbuffer[5] = (kernel == 3) ? BRAM[addr + 5] : 0.0;
  wbuffer[6] = (kernel == 3) ? BRAM[addr + 6] : 0.0;
  wbuffer[7] = (kernel == 3) ? BRAM[addr + 7] : 0.0;
  wbuffer[8] = (kernel == 3) ? BRAM[addr + 8] : 0.0;
}
data_t WeightsCache::getOneWeight(channel_t co) {
  LOG("WeightsCache: getOneWeight( co=%-2d ) from WCache[%3d] -> %.2f\n",
      (int)co, (int)(ci_offset + co), BRAM[ci_offset + co]);
  return BRAM[ci_offset + co];
}
