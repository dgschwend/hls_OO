//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  output_cache.cpp
//
//  Output Cache Module for FPGA
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#include "output_cache.hpp"

// ================
// = Output Cache =
// ================
#ifdef EBUG
OutputCache::OutputCache(const char *name)
    : _name(name)
#else
OutputCache::OutputCache(const char *name)
{

//#pragma HLS ARRAY_RESHAPE variable = BRAM cyclic factor = N_PE dim = 1
};
#endif

void OutputCache::accumulateChannel(channel_t c, data_t value_to_add) {
#pragma HLS DEPENDENCE variable=BRAM inter false
#pragma HLS DEPENDENCE variable=BRAM intra RAW false
#pragma HLS pipeline II = 1

  data_t old_ch = getChannel(c); /* BRAM[c] */
  data_t new_ch = old_ch + value_to_add;
  setChannel(c, new_ch); /* BRAM[c] = new_ch; */
  LOG("%s: accumulateChannel( ch%-2d ) add %+.2f -> %.2f\n", _name, (int)c,
      value_to_add, new_ch);
};

data_t OutputCache::getChannel(channel_t c) {
  if (LOG_DETAILS)
    LOG("%s: getChannel( ch%-2d ) -> %6.2f\n", _name, (int)c, BRAM[c]);
  return BRAM[c];
}

void OutputCache::setChannel(channel_t c, data_t data) {
  if (LOG_DETAILS)
    LOG("%s: setChannel( ch%-2d ) <- %6.2f\n", _name, (int)c, data);
  BRAM[c] = data;
}

void OutputCache::reset() {
  LOG("%s: reset( all ) <- 0.0f\n", _name);
  LOG_LEVEL_DECR;
  L_OCache_reset:
  for (int i = 0; i < MAX_NUM_CHOUT; i++) {
#pragma HLS unroll
	  setChannel(i, 0.0f);
	  /* BRAM[i] = 0.0f; */
  }
  LOG_LEVEL_DECR;
}
