#ifndef PROCESSING_ELEMENT_HPP_C609EBE9
#define PROCESSING_ELEMENT_HPP_C609EBE9

// Data Types for FPGA Implementation
#include "fpga_top.hpp" 

#include "image_cache.hpp"
#include "weights_cache.hpp"
#include "output_cache.hpp"

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

#endif /* end of include guard: PROCESSING_ELEMENT_HPP_C609EBE9 */