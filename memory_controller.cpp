#include "memory_controller.hpp"

// =====================
// = Memory Controller =
// =====================
MemoryController::MemoryController(data_t *mempointer,
                                   unsigned int weights_offset,
                                   unsigned int data_offset) {
  SHARED_DRAM = mempointer;
  DRAM_WEIGHTS = (SHARED_DRAM + weights_offset);
  DRAM_DATA = (SHARED_DRAM + data_offset);

  LOG("MemoryCtrl: Constructor.\n");
  LOG(" - SHARED_DRAM     = %lu\n", (long)SHARED_DRAM);
  LOG(" - DRAM_WEIGHTS    = @%luB\n", (long)DRAM_WEIGHTS - (long)SHARED_DRAM);
  LOG(" - DRAM_DATA       = @%luB\n", (long)DRAM_DATA - (long)SHARED_DRAM);
}

void MemoryController::loadConfig(int num_layers, layer_t *configBRAM) {
  loadConfigViaFloatUnion(num_layers, configBRAM);
  LOG("MemoryCtrl: loadConfig (%d layers).\n", (int)num_layers);
}

void MemoryController::loadConfigViaFloatUnion(int num_layers,
                                               layer_t *configBRAM) {
  // Fetch Layer Configuration by Transferring floats + Converting to layer_t
  float floats[NUM_FLOATS_PER_LAYER];
  layer_t layer;
  for (numlayers_t l = 0; l < num_layers; l++) {
    memcpy(floats, &SHARED_DRAM[l * NUM_FLOATS_PER_LAYER],
           NUM_FLOATS_PER_LAYER * sizeof(float));
    floatsToLayerT(floats, layer);
    configBRAM[l] = layer;
  }
}

void MemoryController::floatsToLayerT(float floats[NUM_FLOATS_PER_LAYER],
                                      layer_t &layer) {
  // Extract layer_t hidden in fake floats (which are actually uint32s)

  // clang-format not used on this section to keep it compact
  // clang-format off
  union { float f; unsigned int i;} u;
  //layer.type = LAYER_NONE;  // type not used
  layer.name[0] = '\0';     // name not used in FPGA
  u.f = floats[0]; layer.width = u.i;
  u.f = floats[1]; layer.height = u.i;
  u.f = floats[2]; layer.channels_in = u.i;
  u.f = floats[3]; layer.channels_out = u.i;
  u.f = floats[4]; layer.kernel = u.i;
  u.f = floats[5]; layer.stride = u.i;
  u.f = floats[6]; layer.pad = u.i;
  u.f = floats[7]; layer.mem_addr_input = u.i;
  u.f = floats[8]; layer.mem_addr_output = u.i;
  u.f = floats[9]; layer.mem_addr_weights = u.i;
  u.f = floats[10]; layer.is_expand_layer = u.i;
  u.f = floats[11]; layer.pool = (u.i == 1 ? POOL_GLOBAL : POOL_NONE);
  // clang-format on
}

void MemoryController::setLayerConfig(layer_t &layer) {
  dram_weights_offset = layer.mem_addr_weights;
  dram_input_offset = layer.mem_addr_input;
  dram_output_offset = layer.mem_addr_output;
  pixels_per_row = layer.width * layer.channels_in;
  ch_out = layer.channels_out;
  width_out = (layer.stride == 2) ? (layer.width / 2) : (layer.width / 1);
  is_expand_layer = layer.is_expand_layer;

  LOG("MemoryCtrl: setLayerConfig.\n");
  LOG(" - weights offset  = %6d Elements, DRAM @%8luB\n",
      (int)dram_weights_offset, (int)dram_weights_offset * sizeof(data_t));
  LOG(" - input offset    = %6d Elements, DRAM @%8luB\n",
      (int)dram_input_offset, (int)dram_input_offset * sizeof(data_t));
  LOG(" - output offset   = %6d Elements, DRAM @%8luB\n",
      (int)dram_output_offset, (int)dram_output_offset * sizeof(data_t));
  LOG(" - pixels per row  = %d\n", (int)pixels_per_row);
  LOG(" - ch_out          = %d\n", (int)ch_out);
  LOG(" - width_out       = %d\n", (int)width_out);
  LOG(" - is_expand_layer = %d\n", (int)is_expand_layer);
}

data_t MemoryController::loadNextWeight() {
  if (LOG_DETAILS)
    LOG("MemoryCtrl: loadNextWeight  (#%4d from DRAM @%4luB): %6.2f\n",
        (int)dram_weights_offset,
        (long)&DRAM_WEIGHTS[dram_weights_offset] - (long)DRAM_DATA,
        DRAM_WEIGHTS[dram_weights_offset]);
  return DRAM_WEIGHTS[dram_weights_offset++];
}

void MemoryController::setPixelLoadRow(coordinate_t y) {
  LOG("MemoryCtrl: setPixelLoadRow (row %2d).\n", (int)y);
  dram_pixel_offset = dram_input_offset + pixels_per_row * y;
}

data_t MemoryController::loadNextChannel() {
  data_t pixel_from_ram = DRAM_DATA[dram_pixel_offset];
  if (LOG_DETAILS)
    LOG("MemoryCtrl: loadNextChannel (from DRAM @%4luB) -> %.2f\n",
        (int)dram_pixel_offset * sizeof(data_t), pixel_from_ram);
  dram_pixel_offset++;  // increment address for next fetch
  return pixel_from_ram;
};

void MemoryController::writeBackOutputPixel(coordinate_t y_out,
                                            coordinate_t x_out,
                                            data_t *outputCache) {
  // Calculate Output Memory Address
  memaddr_t px_offset = ch_out * (width_out * y_out + x_out);
  data_t *DRAM_LAYER_OUT, *DRAM_PX_OUT;
  DRAM_LAYER_OUT = (DRAM_DATA + dram_output_offset);
  // Leave double space for "expand" layers (more ch_out will be added)
  DRAM_PX_OUT =
      DRAM_LAYER_OUT + (is_expand_layer ? 2 * (int)px_offset : (int)px_offset);

  LOG("MemoryController: writeBackOutputPixel (%2d, %2d)\n", (int)y_out,
      (int)x_out);
  LOG(" - writing %2d channels to DRAM @%luB+\n", (int)ch_out,
      (long)DRAM_PX_OUT - (long)DRAM_DATA);

  LOG_LEVEL++;
L_writeBackOutputPixel:
  for (channel_t co = 0; co < ch_out; co++) {
    DRAM_PX_OUT[co] = outputCache[co];
    LOG(" WB ch%d (@%luB): %6.2f\n", (int)co,
        ((long)(DRAM_PX_OUT + co) - (long)DRAM_DATA), outputCache[co]);
  }
  LOG_LEVEL--;
}

void MemoryController::writeBackResult(data_t *globalPoolCache) {
L_writeBackResult:
  for (int i = 0; i < ch_out; i++) {  // ch_out set from last layer
    DRAM_DATA[i] = globalPoolCache[i];
  }
  LOG("MemoryCtrl: writeBackResult (%d Bytes) to DRAM @0\n",
      (int)(ch_out * sizeof(data_t)));
}