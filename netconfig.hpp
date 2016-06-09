//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  netconfig.h
//
//  Declaration of <layer_t> and <network_t> structs that hold configuration.
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#ifndef _NETCONFIG_H_
#define _NETCONFIG_H_

#include <string>
#include <cmath>
#include <cstdlib>
#include "ap_int.h"

// ================================
// = Bit-Width Calculation MACROs =
// ================================
// NBITS(constant) = how many bits needed to represent <constant>
#define NBITS2(n) ((n & 2) ? 1 : 0)
#define NBITS4(n) ((n & (0xC)) ? (2 + NBITS2(n >> 2)) : (NBITS2(n)))
#define NBITS8(n) ((n & 0xF0) ? (4 + NBITS4(n >> 4)) : (NBITS4(n)))
#define NBITS16(n) ((n & 0xFF00) ? (8 + NBITS8(n >> 8)) : (NBITS8(n)))
#define NBITS32(n) ((n & 0xFFFF0000) ? (16 + NBITS16(n >> 16)) : (NBITS16(n)))
#define NBITS(n) ((n) == 0 ? 1 : NBITS32((n)) + 1)

// =================================
// = Network-Independent Constants =
// =================================
const int NET_NAME_MAX_LEN = 6;            // max length of layer names
const int MEMORY_ALIGNMENT = 1024 * 1024;  // align data in DRAM to 1MB borders
// floats needed to hold one layer_t (for conversion layer_t <-> float array):
const int NUM_FLOATS_PER_LAYER = 12;

// ============================
// = Network Type-Definitions =
// ============================

// Chose Bit-Vectors ideally wide for their contents
// -> adapts to specific Network with definitions in network.hpp
// ! need to include network.hpp first!
typedef ap_uint<NBITS(MAX_DIMENSION)> dimension_t;
typedef ap_uint<NBITS(MAX_CHANNELS + 9)> channel_t;
typedef ap_uint<2> kernel_t;  // =1 or =3
typedef ap_uint<2> stride_t;  // =1 or =2
typedef ap_uint<NBITS(MAX_WEIGHTS_PER_LAYER)> weightaddr_t;
typedef ap_uint<NBITS(MAX_NUM_LAYERS)> numlayers_t;    // saves number of layers
typedef ap_uint<NBITS(MAX_NUM_LAYERS - 1)> layerid_t;  // counts to num_layers-1
typedef float data_t;
typedef ap_uint<23> memaddr_t;  // must remain <= 23 bits to fit into float

typedef enum {
  LAYER_CONV,
  LAYER_NONE  // not really used...
  // LAYER_RELU,  // implicit after LAYER_CONV
  // LAYER_DATA,  // implicit before first and after last layer
  // LAYER_POOL,  // not supported, use all-convolutional networks
} layertype_t;

typedef enum {
  POOL_NONE,
  POOL_GLOBAL
  // POOL_2x2S2,  // not supported, use all-convolutional networks
  // POOL_3x3S2,  // not supported, use all-convolutional networks
} pooltype_t;

// ==================
// = Struct LAYER_T =
// ==================
// Structure that holds one single CNN layer (actually, one CONV layer)
struct layer_t {
  char name[NET_NAME_MAX_LEN + 1];
  layertype_t type;
  dimension_t width;  // input dimensions
  dimension_t height;
  channel_t channels_in;
  channel_t channels_out;
  kernel_t kernel;  // kernel sizes supported: 3 or 1
  bool pad;         // padding is either 0 or 1 pixel
  stride_t stride;  // only stride 1 or 2 supported
  memaddr_t mem_addr_input;
  memaddr_t mem_addr_output;
  memaddr_t mem_addr_weights;
  bool is_expand_layer;
  // ap_uint<31> dummy;
  pooltype_t pool;
  // full constructor, used to define network in network.cpp
  layer_t(const char *n, layertype_t t, int w, int h, int ci, int co, int k,
          int p, int s, int mem_i = 0, int mem_o = 0, int mem_w = 0,
          bool is_expand = false, pooltype_t pool = POOL_NONE)
      : type(t),
        width(w),
        height(h),
        channels_in(ci),
        channels_out(co),
        kernel(k),
        pad(p),
        stride(s),
        mem_addr_input(mem_i),
        mem_addr_output(mem_o),
        mem_addr_weights(mem_w),
        is_expand_layer(is_expand),
        pool(pool) {
    for (int i = 0; i < NET_NAME_MAX_LEN; i++) {
      name[i] = n[i];
      if (n[i] == 0) break;
    }
    name[NET_NAME_MAX_LEN] = 0;
  };
  // empty constructor, needed for empty array of layer_t in FPGA BRAM
  layer_t()
      : type(LAYER_NONE),
        width(0),
        height(0),
        channels_in(0),
        channels_out(0),
        kernel(0),
        pad(0),
        stride(0),
        mem_addr_input(0),
        mem_addr_output(0),
        mem_addr_weights(0),
        is_expand_layer(0),
        pool(POOL_NONE) {
    name[0] = 0;
  };
};

// =========================
// = CPU <-> FPGA BUS TYPE =
// =========================
typedef union {
  data_t f;
  int i;
} bus_t;
// const int transactions_per_layer = sizeof(layer_t) / sizeof(bus_t);

// ====================
// = Struct NETWORK_T =
// ====================
// Structure that holds an entire CNN Network Defintion
struct network_t {
  layer_t *layers;
  numlayers_t num_layers;
  data_t *weights;
  int num_weights;
  int total_pixel_mem;
  // default constructor: need to give max_layers and max_weights
  // allocates layers[max_layers] and weights[max_weights] on Heap
  // -> can only be used on CPU, not on FPGA
  network_t(int max_layers, int max_weights)
      : num_layers(0), num_weights(0), total_pixel_mem(0) {
    layers = (layer_t *)malloc((sizeof(layer_t)) * max_layers);
    weights = (float *)malloc((sizeof(float)) * max_weights);
  }
};

// =========================================
// = Print Overview Table of given Network =
// =========================================
void print_layers(network_t *net);
void print_layer(layer_t *layer);

// ==============================
// = Add Layer to given Network =
// ==============================
// Assumes that Network has been allocated with enough memory (not checked!)
// If update_memory_address==true, advances layer's memory addresses for input
//    and output memories and weights
// If is_expand_layer==true, reserves double amount of output memory to allow
//    implicit out-channel concatenation (use for expand1x1 layer)
// If is_expand_layer==true and update_memory_address==false, uses same input
//    memory address as in last layer, and only slightly shifted output address
//    (use for expand3x3 layer)
// Use POOL_TYPE = POOL_GLOBAL in last layer to sum over spatial dimension
//    (output becomes 1x1xCH_OUT)
void addLayer(network_t *net, layer_t layer, bool is_expand_layer = false,
              bool update_memory_address = true,
              pooltype_t pool_type = POOL_NONE);

// =================================
// = Load Weights from Binary File =
// =================================
// prepare with convert_caffemodel.py
void loadWeightsFromFile(network_t *net, const char *filename);

// ================================================
// = Convert layer_t struct from / to float array =
// ================================================
void layer_to_floats(layer_t &layer, float floats[NUM_FLOATS_PER_LAYER]);
void floats_to_layer(float floats[NUM_FLOATS_PER_LAYER], layer_t &layer);

#endif
