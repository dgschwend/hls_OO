#ifndef MEMORY_CONTROLLER_HPP_EBD6F5A3
#define MEMORY_CONTROLLER_HPP_EBD6F5A3

// Data Types for FPGA Implementation
#include "fpga_top.hpp" 

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

#endif /* end of include guard: MEMORY_CONTROLLER_HPP_EBD6F5A3 */
