#include "processing_element.hpp"

// ======================
// = Processing Element =
// ======================
ProcessingElement::ProcessingElement(){};
void ProcessingElement::setup(channel_t co_offset, ImageCache *ICache,
                              WeightsCache *WCache, OutputCache *OCache) {
  LOG("PE[%d]: setup(co_offset: %d, ICache, WCache, OCache)\n", (int)co_offset,
      (int)co_offset);
  this->ICache = ICache;
  this->WCache = WCache;
  this->OCache = OCache;
  this->co_offset = co_offset;
};

void ProcessingElement::setLayerConfig(layer_t &layer) {
  kernel = layer.kernel;
  ch_out = layer.channels_out;
  width_in = layer.width;
  height_in = layer.height;

  if (LOG_DETAILS) {
    LOG("PE[%d]: setLayerConfig\n", (int)co_offset);
    LOG(" - kernel   = %d\n", (int)kernel);
    LOG(" - ch_out   = %d\n", (int)ch_out);
    LOG(" - width_in = %d\n", (int)width_in);
  }
}

void ProcessingElement::preloadPixels(coordinate_t y_center,
                                      coordinate_t x_center, channel_t ci,
                                      data_t buffer[9]) {
  LOG("PE[%d]: preloadPixels (y_center: %2d, x_center: %2d, ci: %2d)\n",
      (int)co_offset, (int)y_center, (int)x_center, (int)ci);

  for (int k = 0; k < 3; k++) {
    for (int l = 0; l < 3; l++) {
      coordinate_t y = y_center + k - 1;
      coordinate_t x = x_center + l - 1;
      data_t px;
      bool pad = (x < 0 | y < 0 | x >= width_in | y >= height_in);
      LOG_LEVEL++;
      px = pad ? 0.0 : ICache->getPixel(y, x, ci);
      buffer[k * 3 + l] = px;
      LOG_LEVEL--;
      if (LOG_DETAILS)
        LOG(" - loaded (y: %2d, x: %2d) = %6.2f %s\n", (int)y, (int)x, px,
            (pad ? "(pad)" : ""));
    }
  }
}

void ProcessingElement::processInputChannel(coordinate_t y, coordinate_t x,
                                            channel_t ci) {
  LOG("PE[%d]: processInputChannel (y: %2d, x: %2d, ci: %2d)\n", (int)co_offset,
      (int)y, (int)x, (int)ci);
  LOG_LEVEL++;
#pragma HLS DATAFLOW
  data_t pixel_buffer[9];
  preloadPixels(y, x, ci, pixel_buffer);
  processAllCHout(pixel_buffer);
  LOG_LEVEL--;
}

void ProcessingElement::processAllCHout(data_t pixels[9]) {
  LOG("PE[%d]: processAllCHout\n", (int)co_offset);
  LOG_LEVEL++;

  data_t weights_local[9];
  data_t result;
  for (channel_t co = co_offset; co < ch_out; co += N_PE) {
    LOG(" - process output channel %d\n", (int)co);
    LOG_LEVEL++;
    {
      // fetch weights
      WCache->getNineWeights(co, weights_local);
      // multiply-accumulate
      result = macc2d(pixels, weights_local);
      // save result to Output Buffer
      OCache->accumulateChannel(co, result);
    }
    LOG_LEVEL--;
  };
  LOG_LEVEL--;
}

data_t ProcessingElement::macc2d(data_t pixels[9], data_t weights[9]) {
  data_t multresult[9];
  for (int i = 0; i < 9; i++) {
#pragma HLS unroll
    multresult[i] = pixels[i] * weights[i];
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
  data_t accumulator = 0.00;
  for (int i = 0; i < 9; i++) {
#pragma HLS unroll
    accumulator = accumulator + multresult[i];
  }
  LOG("PE[%d]: macc2D -> %.2f \n", (int)co_offset, accumulator);

  return accumulator;
}