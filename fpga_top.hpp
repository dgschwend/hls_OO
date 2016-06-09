//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  fpga_top.hpp
//
//  Top-Level Module for SqueezeNetOnFGPA.
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#ifndef _FPGA_TOP_H_
#define _FPGA_TOP_H_

#include <cassert>
#include <cmath>
#include "ap_int.h"

#ifdef __SYNTHESIS__
#include <ap_utils.h>
#include <hls_math.h>
#endif

#include "network.hpp"
#include "netconfig.hpp"

// ==========================
// = Architecture Constants =
// ==========================

const int NUM_IMG_CACHE_LINES = (3);
// Actually, just need 3 Image Cache Lines
// Using 4 might simplify hardware -> check!

// Number of Processing Elements
const int N_PE = 1;

// ====================
// = Type Definitions =
// ====================
typedef ap_uint<2> cacheline_t;  // cache height = 4 lines
typedef ap_uint<NBITS(MAX_INPUT_PER_LAYER)> imgdramoffset_t;
typedef ap_uint<NBITS(MAX_IMAGE_CACHE_SIZE)> imgcacheaddr_t;
typedef ap_uint<NBITS(MAX_IMAGE_CACHE_SIZE / 4)> pixelperrow_t;
typedef ap_uint<4> numfilterelems_t;  // either =1 or =9

typedef ap_int<NBITS(MAX_DIMENSION) + 2> coordinate_t;
// coordinates run worst-case from -1 ... +W or +H
// -> need bits for (W or H) + 1 bit more for signed + 1 bit more for +H / +W
// -> could be implemented more efficiently, but coordinates become more ugly.

// ==============================
// = FPGA Top Function / Module =
// ==============================
void fpga_top(data_t *SHARED_DRAM, unsigned int num_layers,
              unsigned int weights_offset, unsigned int input_offset);

// =====================
// = Memory Controller =
// =====================
class MemoryController {
 public:
  MemoryController(data_t *mempointer, unsigned int weights_offset,
                   unsigned int data_offset);
  void loadConfig(int num_layers, layer_t *configBRAM);
  void setLayerConfig(layer_t &layer);
  data_t loadNextWeight();
  void setPixelLoadRow(coordinate_t y);
  data_t loadNextChannel();
  void writeBackOutputPixel(coordinate_t y_out, coordinate_t x_out,
                            data_t *outputCache);
  void writeBackResult(data_t *globalPoolCache);

 private:
  data_t *SHARED_DRAM;
  data_t *DRAM_DATA;
  data_t *DRAM_WEIGHTS;
  memaddr_t dram_weights_offset;
  memaddr_t dram_input_offset;
  memaddr_t dram_output_offset;
  memaddr_t dram_pixel_offset;
  pixelperrow_t pixels_per_row;
  dimension_t width_out;
  channel_t ch_out;
  bool is_expand_layer;

  void loadConfigViaFloatUnion(int num_layers, layer_t *configBRAM);
  void floatsToLayerT(float floats[NUM_FLOATS_PER_LAYER], layer_t &layer);
};

// ===============
// = Image Cache =
// ===============
class ImageCache {
 public:
  ImageCache();
  void reset();
  void setNextChannel(data_t value);
  void preloadPixelFromDRAM(MemoryController *DRAM);
  void preloadRowFromDRAM(MemoryController *DRAM);
  void setLayerConfig(layer_t &layer);
  data_t getPixel(coordinate_t y, coordinate_t x, channel_t ci);

 private:
  data_t BRAM[MAX_IMAGE_CACHE_SIZE];
  cacheline_t curr_img_cache_line;
  imgcacheaddr_t next_img_cache_addr;
  imgcacheaddr_t line_width;
  imgdramoffset_t loads_left;
  dimension_t width_in;
  channel_t ch_in;
};

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
  data_t *BRAMPointer();
  void print_setup();

 private:
  weightaddr_t write_addr;
  data_t BRAM[MAX_WEIGHTS_PER_LAYER];
  kernel_t kernel;
  channel_t ch_out;
  channel_t ch_in;
  weightaddr_t ci_offset;
  numfilterelems_t weights_per_filter;
};

// ================
// = Output Cache =
// ================
class OutputCache {
 public:
  OutputCache(const char *name = "OutputCache");
  void accumulateChannel(channel_t c, data_t data);
  data_t getChannel(channel_t c);
  void setChannel(channel_t c, data_t data);
  void reset();
  data_t *BRAMPointer() { return BRAM; }

 private:
  const char *_name;  // for debugging / logging
  data_t BRAM[MAX_NUM_CHOUT];
};

// ======================
// = Processing Element =
// ======================
class ProcessingElement {
 public:
  ProcessingElement();
  void setup(channel_t co_offset, ImageCache *ICache, WeightsCache *WCache,
             OutputCache *OCache);
  void setLayerConfig(layer_t &layer);
  void processInputChannel(coordinate_t y_in, coordinate_t x_in, channel_t ci);

 private:
  void preloadPixels(coordinate_t y_in, coordinate_t x_in, channel_t ci,
                     data_t buffer[9]);
  void processAllCHout(data_t pixels[9]);
  data_t macc2d(data_t pixels[9], data_t weights[9]);
  ImageCache *ICache;
  WeightsCache *WCache;
  OutputCache *OCache;
  kernel_t kernel;
  channel_t ch_out;
  channel_t co_offset;
  dimension_t width_in;
  dimension_t height_in;
  data_t mult_result[9];
  data_t acumulator;
};

// ================================
// = Debugging Output (Helper Fn) =
// ================================
// debug mode, -DEBUG
extern int LOG_LEVEL;
extern void print_indent(int lvl);
#if defined(EBUG) && !defined(__SYNTHESIS__)
#define FNAME() \
  fprintf(stdout, "\n%s (%s, line %d)\n", __func__, __FILE__, __LINE__)
#define DBG(...)                 \
  {                              \
    /*print_indent(LOG_LEVEL);*/ \
    printf(__VA_ARGS__);         \
  }
#define LOG(...)             \
  {                          \
    print_indent(LOG_LEVEL); \
    printf(__VA_ARGS__);     \
  }
#else
#define FNAME() \
  do {          \
  } while (0)
#define DBG(...) \
  do {           \
  } while (0)
#define LOG(...) \
  do {           \
  } while (0)
#endif  // EBUG

// ===================================================================
// = Pragma Indirection (allows use of DEFINED variables in #pragma) =
// ===================================================================
//#define PRAGMA_SUB(x) _Pragma(#x)
//#define PRAGMA_HLS(x) PRAGMA_SUB(x)

#endif
