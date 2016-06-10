//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  fpga_top.cpp
//
//  Top-Level Module for SqueezeNetOnFGPA.
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#include "fpga_top.hpp"

// FPGA Modules
#include "memory_controller.hpp"
#include "image_cache.hpp"
#include "weights_cache.hpp"
#include "output_cache.hpp"
#include "processing_element.hpp"

// ==============
// =  FPGA TOP  =
// ==============
void fpga_top(data_t *SHARED_DRAM, unsigned int num_layers,
              unsigned int weights_offset, unsigned int input_offset) {
#pragma HLS INTERFACE m_axi depth = DRAM_DEPTH port = SHARED_DRAM offset = \
    direct bundle = memorybus
#pragma HLS INTERFACE s_axilite register port = num_layers bundle = axilite
#pragma HLS INTERFACE s_axilite register port = weights_offset bundle = axilite
#pragma HLS INTERFACE s_axilite register port = input_offset bundle = axilite

  printf("FPGA TOP started.\n");
  LOG_LEVEL++;

  // =============================
  // = Module + Memory Instances =
  // =============================
  MemoryController DRAM(SHARED_DRAM, weights_offset, input_offset);
  layer_t layerConfig[MAX_NUM_LAYERS];
  ImageCache ICache;
  WeightsCache WCache;
  OutputCache OCache("OCache");
  OutputCache GPoolCache("GPoolCache");
  ProcessingElement PE;  //[N_PE];

  coordinate_t y, x;
  channel_t ci, co;
  layerid_t layer_id;

  // Setup Processing Elements
  LOG("Initial Module Setup:\n");
  LOG_LEVEL++;
  {
    // Setup Processing Element
    PE.setup(&ICache, &WCache, &OCache);

    // Setup Global Pooling Cache
    GPoolCache.reset();

    // Load Layer Configuration
    DRAM.loadConfig(num_layers, layerConfig);
  }
  LOG_LEVEL--;

// Layer Loop
L_LAYERS:
  for (layer_id = 0; layer_id < num_layers; layer_id++) {
    LOG("Layer %d:\n", (int)layer_id);
    LOG_LEVEL++;

    // Set Layer Configuration
    layer_t layer = layerConfig[layer_id];
    LOG("Configure Modules for Layer:\n");
    LOG_LEVEL++;
    {
      ICache.setLayerConfig(layer);
      WCache.setLayerConfig(layer);
      DRAM.setLayerConfig(layer);
      PE.setLayerConfig(layer);
    }
    LOG_LEVEL--;

    // Load Weights from DRAM
    WCache.loadFromDRAM(&DRAM);

    // Preload Row 0
    DRAM.setPixelLoadRow(0);
    ICache.preloadRowFromDRAM(&DRAM);
    DRAM.setPixelLoadRow(1);
    ICache.preloadPixelFromDRAM(&DRAM);

  // Y Loop
  L_Y:
    for (y = 0; y < layer.height; y++) {
      LOG("Y = %d:\n", (int)y);
      LOG_LEVEL++;

    // X Loop
    L_X:
      for (x = 0; x < layer.width; x++) {
        LOG("X = %d:\n", (int)x);
        LOG_LEVEL++;

        // Reset Output Cache
        OCache.reset();

        // Load Next Pixel (automatically checks #pixels left)
        ICache.preloadPixelFromDRAM(&DRAM);

        // Stride-2 Skipping
        if (layer.stride == 2 & (x % 2 | y % 2)) {
          LOG("stride-2, skipping pixel\n");
          LOG_LEVEL--;
          continue;
        }

      // Input Channel Loop
      L_CH_IN:
        for (ci = 0; ci < layer.channels_in; ci++) {
#pragma HLS pipeline

          LOG("CI = %d:\n", (int)ci);
          LOG_LEVEL++;
          {
            // Setup Weights Cache
            WCache.setInputChannel(ci);

            // Kick off PEs
            LOG("Start PEs for Pixel(%d,%d), input ch %d, all ouput ch\n",
                (int)y, (int)x, (int)ci);
            LOG_LEVEL++;
            { PE.processInputChannel(y, x, ci); }
            LOG_LEVEL--;
          }
          LOG_LEVEL--;
        }  // end L_CH_IN. Pixel (Y,X) is finished.
        LOG("All CI, CO done for pixel(%d,%d)\n", (int)y, (int)x);

        // ===============
        // = Postprocess =
        // ===============
        data_t raw, biased, rectified;

        LOG("Postprocess Pixel(%d,%d)\n", (int)y, (int)x);
        LOG_LEVEL++;

        // Select bias coefficients
        WCache.setInputChannel(layer.channels_in);

      L_POSTPROCESS:
        for (co = 0; co < layer.channels_out; co++) {
          LOG("Postprocess CO=%d:\n", (int)co);
          LOG_LEVEL++;

#pragma HLS pipeline

          // Read output channel from Cache
          raw = OCache.getChannel(co);
          // Add Bias
          biased = raw + WCache.getOneWeight(co);
          // Add ReLU
          rectified = (biased < 0) ? 0.0 : biased;

          LOG("raw: %8.2f > biased: %8.2f > rectified: %8.2f\n", raw, biased,
              rectified);
          // Write Back to Output Cache
          OCache.setChannel(co, rectified);
          // Accumulate for Global Pooling (if enabled)
          if (layer.pool == POOL_GLOBAL) {
            GPoolCache.accumulateChannel(co, rectified);
          }
          LOG_LEVEL--;
        }
        LOG_LEVEL--;

        // Write Output Pixel to DRAM
        dimension_t y_out = (layer.stride == 2) ? (int)y / 2 : (int)y;
        dimension_t x_out = (layer.stride == 2) ? (int)x / 2 : (int)x;
        DRAM.writeBackOutputPixel(y_out, x_out, OCache.BRAMPointer());

        LOG_LEVEL--;
      }
      LOG_LEVEL--;
    }
    LOG_LEVEL--;
  }
  LOG_LEVEL--;

  // Write Back final Result
  DRAM.writeBackResult(GPoolCache.BRAMPointer());

  LOG_LEVEL--;
  printf("FPGA Top finished.\n");
}

// ===========
// = LOGGING =
// ===========
bool LOG_DETAILS = false;
int LOG_LEVEL = 0;
void print_indent(int lvl) {
  while (lvl--) {
    putchar(' ');
    putchar(' ');
  }
}
