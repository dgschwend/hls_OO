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
WeightsCache::WeightsCache() {
//#pragma HLS ARRAY_RESHAPE variable=BRAM block factor=9 dim=1
#pragma HLS ARRAY_RESHAPE variable=BRAM cyclic factor=2 dim=1
};

void WeightsCache::loadFromDRAM(MemoryController *DRAM) {
  LOG("WeightsCache: loadFromDRAM (total %d weights, %d biases)\n",
      (int)(ch_in * ch_out * weights_per_filter), (int)ch_out);
  LOG_LEVEL_INCR;
  LOG_LEVEL_INCR;

// Load Filter Coefficients
  weightaddr_t num_weights = ch_in * ch_out * weights_per_filter;
  assert(num_weights <= MAX_WEIGHTS_PER_LAYER && "Loading too many Weights!");
L_WCACHE_LOAD_WEIGHTS:
  for (weightaddr_t addr = 0; addr < num_weights; addr++) {
#pragma HLS pipeline
    data_t weight = DRAM->loadNextWeight();
    addWeight(weight);
  }

// Load Biases
  assert(ch_out <= MAX_NUM_CHOUT && "Tried to load too many Biases!");
L_WCACHE_LOAD_BIAS:
  for (weightaddr_t addr = 0; addr < ch_out; addr++) {
#pragma HLS pipeline
    data_t bias = DRAM->loadNextWeight();
    addWeight(bias);
  }

  LOG_LEVEL_DECR;
  LOG_LEVEL_DECR;
}

void WeightsCache::addWeight(data_t weight) {
  assert(write_addr < MAX_WEIGHTS_PER_LAYER && "Too many weights loaded!");
  /*if (LOG_DETAILS)
    LOG("WeightsCache: addWeight WCACHE[%3d] = %6.2f\n", (int)write_addr,
        weight);*/
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

data_t WeightsCache::getWeight(const weightaddr_t addr) {
#pragma HLS inline
  return BRAM[addr];
}

void WeightsCache::getNineWeights(channel_t co, data_t wbuffer[9]) {
  weightaddr_t addr = ci_offset + co * weights_per_filter;
  LOG("WeightsCache: getNineWeights( co=%-2d ) from WCache[%3d]+\n", (int)co,
      (int)addr);
  data_t weight_0 = getWeight(addr);
  L_getNineWeights:
  for (int i = 0; i < 9; i++) {
#pragma HLS unroll
	  data_t weight_i = getWeight(addr + i);
	  data_t fetched = (kernel == 3) ? (weight_i) : (i==4 ? weight_0 : 0.0f);
	  wbuffer[i] = fetched;
  }
}

data_t WeightsCache::getOneWeight(channel_t co) {
#pragma HLS inline
  /*LOG("WeightsCache: getOneWeight( co=%-2d ) from WCache[%3d] -> %.2f\n",
      (int)co, (int)(ci_offset + co), BRAM[ci_offset + co]);*/
	return getWeight(ci_offset + co);
}
