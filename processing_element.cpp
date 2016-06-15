//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  processing_element.cpp
//
//  Processing (MACC) Module for FPGA
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#include "processing_element.hpp"

// ======================
// = Processing Element =
// ======================
ProcessingElement::ProcessingElement(){};
void ProcessingElement::setup(ImageCache *ICache, WeightsCache *WCache,
                              OutputCache *OCache) {
  LOG("PE: setup(ICache, WCache, OCache)\n");
  this->ICache = ICache;
  this->WCache = WCache;
  this->OCache = OCache;
};

void ProcessingElement::setLayerConfig(layer_t &layer) {
  kernel = layer.kernel;
  ch_out = layer.channels_out;
  width_in = layer.width;
  height_in = layer.height;

  if (LOG_DETAILS) {
    LOG("PE: setLayerConfig\n");
    LOG(" - kernel   = %d\n", (int)kernel);
    LOG(" - ch_out   = %d\n", (int)ch_out);
    LOG(" - width_in = %d\n", (int)width_in);
  }
}

void ProcessingElement::preloadPixels(coordinate_t y_center,
                                      coordinate_t x_center, channel_t ci,
                                      data_t buffer[9]) {
  LOG("PE: preloadPixels (y_center: %2d, x_center: %2d, ci: %2d)\n",
      (int)y_center, (int)x_center, (int)ci);

L_PE_loadPixel_Y:
  for (int k = 0; k < 3; k++) {
  L_PE_loadPixel_X:
    for (int l = 0; l < 3; l++) {

#pragma HLS PIPELINE II=2
#pragma HLS unroll factor=2

      coordinate_t y = y_center + k - 1;
      coordinate_t x = x_center + l - 1;
      data_t px;
      bool pad = (x < 0 | y < 0 | x >= width_in | y >= height_in);
      LOG_LEVEL_INCR;
      px = pad ? 0.0f : ICache->getPixel(y, x, ci);
      buffer[k * 3 + l] = px;
      LOG_LEVEL_DECR;
      if (LOG_DETAILS)
        LOG(" - loaded (y: %2d, x: %2d) = %6.2f %s\n", (int)y, (int)x, px,
            (pad ? "(pad)" : ""));
    }
  }
}

void ProcessingElement::processInputChannel(const coordinate_t y,
                                            const coordinate_t x,
                                            const channel_t ci) {
  data_t pixel_buffer[9];
#pragma HLS ARRAY_PARTITION variable = pixel_buffer complete dim = 0

  LOG("PE: processInputChannel (y: %2d, x: %2d, ci: %2d)\n", (int)y, (int)x,
      (int)ci);
  LOG_LEVEL_INCR;
  // Setup Weights Cache (will deliver weights for CH_IN ci)
  WCache->setInputChannel(ci);
  // Preload Image Pixel Buffer (fetch pixels around (y,x,ci))
  preloadPixels(y, x, ci, pixel_buffer);
  // Process All Output Channels (MACC output pixels (y, x, ...))
  processAllCHout(pixel_buffer);
  LOG_LEVEL_DECR;
}

void ProcessingElement::processAllCHout(const data_t pixels[9]) {
  LOG("PE: processAllCHout\n");
  LOG_LEVEL_INCR;

L_CH_OUT:
  for (channel_t co = 0; co < ch_out; co++) {
	data_t result, weights_local[9];
#pragma HLS ARRAY_PARTITION variable = weights_local complete dim = 0

#pragma HLS LOOP_TRIPCOUNT min=16 max=1024 avg=258
//#pragma HLS unroll factor=8
#pragma HLS PIPELINE II=1 rewind

    LOG(" - process output channel %d\n", (int)co);
    LOG_LEVEL_INCR;
    // fetch weights
    WCache->getNineWeights(co, weights_local);
    // multiply-accumulate
    macc2d(pixels, weights_local, result);
    // save result to Output Buffer
    OCache->accumulateChannel(co, result);
    LOG_LEVEL_DECR;
  };
  LOG_LEVEL_DECR;
}

void ProcessingElement::macc2d(const data_t pixels[9], const data_t weights[9],
                               data_t &result) {
//#pragma HLS inline off

  data_t accumulator = 0.0f;
  data_t multresult[9];
#pragma HLS ARRAY_PARTITION variable = multresult complete dim = 0

L_MACC_multiply:
  for (int i = 0; i < 9; i++) {
#pragma HLS unroll
    multresult[i] = pixels[i] * weights[i];
  }

L_MACC_accumulate:
  for (int i = 0; i < 9; i++) {
#pragma HLS unroll
    accumulator = accumulator + multresult[i];
  }

  if (LOG_DETAILS) {
    LOG(" - W: [ %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ]\n",
        weights[0], weights[1], weights[2], weights[3], weights[4], weights[5],
        weights[6], weights[7], weights[8]);
    LOG(" - P: [ %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ]\n",
        pixels[0], pixels[1], pixels[2], pixels[3], pixels[4], pixels[5],
        pixels[6], pixels[7], pixels[8]);
    LOG(" - M: [ %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f %6.2f ]\n",
        multresult[0], multresult[1], multresult[2], multresult[3],
        multresult[4], multresult[5], multresult[6], multresult[7],
        multresult[8]);
  }
  LOG("PE: macc2D -> %.2f \n", accumulator);

  result = accumulator;
}
