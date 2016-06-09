//------------------------------------------------------------------------------
//  SqueezeNetOnFPGA
//------------------------------------------------------------------------------
//
//	File:  netconfig.cpp
//
//  addLayer(), loadWeightsFromFile(), print_layer(), print_layers()
//
//	(c) David Gschwend, 2016
//
//------------------------------------------------------------------------------

#include "network.hpp"
#include "netconfig.hpp"

// ==============================
// = Add Layer to given Network =
// ==============================

void addLayer(network_t *net, layer_t layer, bool is_expand_layer,
              bool update_memory_address, pooltype_t pool_type) {
  // Assumes that Network has been initialized with enough memory (not checked!)
  // Uses static variables -> can only use for 1 network definitions!
  // If update_memory_address==true, advances layer's memory addresses for input
  //    and output memories and weights
  // If is_expand_layer==true, reserves double amount of output memory to allow
  //    implicit out-channel concatenation (use for expand1x1 layer)
  // If is_expand_layer==true and update_memory_address==false, uses same input
  //    memory address as in last layer, and only slightly shifted output
  //    address (use for expand3x3 layer)
  // Use POOL_TYPE = POOL_GLOBAL in last layer to sum over spatial dimension
  //    (output becomes 1x1xCH_OUT)

  // Keep Track of Memory Locations for Activations + Weights (-> static)
  static int next_activation_mem = 0;
  static int last_activation_mem = 0;
  static int next_weights_mem = 0;

  // Align to memory borders (float needed because of ceil() operation below)
  float mem_border = MEMORY_ALIGNMENT / sizeof(data_t);

  // Data Size Calculations
  int input_data_pixels = layer.width * layer.height * layer.channels_in;
  int width_out =
      std::floor((float)(layer.width + 2 * layer.pad - layer.kernel) /
                 layer.stride) +
      1;
  int height_out =
      std::floor((float)(layer.height + 2 * layer.pad - layer.kernel) /
                 layer.stride) +
      1;
  int output_data_pixels = width_out * height_out * layer.channels_out;
  int num_weights =  // conv + bias weights
      layer.channels_out * layer.channels_in * layer.kernel * layer.kernel +
      layer.channels_out;

  // Normal Layers:
  // - increase "next_activation_mem" to next available (1MB) memory border
  // Data + First Expand1x1 Layers:
  // - do not increase memory addresses
  if (update_memory_address) {  // "normal layer"

    last_activation_mem = next_activation_mem;
    // next free memory address = behind input of current layer
    // input data size:
    next_activation_mem += input_data_pixels;
    // align to 1MB memory borders
    next_activation_mem = ceil(next_activation_mem / mem_border) * mem_border;
  }

  /*printf("layer %s, update_memory_address = %d, last_activation_mem = %d, "
   "next_activation_mem = %d", layer.name, update_memory_address,
   last_activation_mem, next_activation_mem);*/

  // Write Memory Addresses into Layer Config
  layer.mem_addr_input = last_activation_mem;
  layer.mem_addr_output = next_activation_mem;
  layer.mem_addr_weights = next_weights_mem;

  // Update next available Weights Memory Address for Layers which have Weights:
  if (layer.type == LAYER_CONV) {
    next_weights_mem += num_weights;
    // do not align (weights can be read completely sequentially)
    // next_weights_mem = ceil(next_weights_mem / mem_border) * mem_border;
  }

  // For 2nd "expand" (expand3x3) layer:
  // - will read from same input location
  // - will write to slightly offset memory location (interleave out channels)
  if (is_expand_layer && !update_memory_address) {
    layer.mem_addr_output = next_activation_mem + layer.channels_out;
  }

  // Write Options into Layer Config
  layer.is_expand_layer = is_expand_layer;
  layer.pool = pool_type;

  // Add Layer to network
  net->layers[net->num_layers] = layer;
  net->num_layers++;
  net->total_pixel_mem = next_activation_mem + output_data_pixels;
};

// =================================
// = Load Weights from Binary File =
// =================================
// prepare with convert_caffemodel.py

void loadWeightsFromFile(network_t *net, const char *filename) {
  FILE *filehandle = fopen(filename, "rb");
  if (!filehandle) {
    printf("ERROR: File %s could not be opened!\n", filename);
    exit(-1);
  }

  for (int i = 0; i < net->num_layers; i++) {
    layer_t *layer = &net->layers[i];
    if (layer->type == LAYER_CONV) {
      int chout = layer->channels_out;
      int chin = layer->channels_in;
      int kernel = layer->kernel;

      // calculate address within weight memory section
      int num_weights = chout * chin * kernel * kernel + chout;
      float *weights_addr = (net->weights + layer->mem_addr_weights);

      // read portion of input file
      fread(weights_addr, sizeof(data_t), num_weights, filehandle);
    }
  }

  fclose(filehandle);
}

// =========================================
// = Convert layer_t struct to float array =
// =========================================
void layer_to_floats(layer_t &layer, float floats[NUM_FLOATS_PER_LAYER]) {
  // struct layer_t fields:
  // ----------------------
  // char name[NET_NAME_MAX_LEN + 1];
  // layertype_t type;
  // dimension_t width;  // input dimensions
  // dimension_t height;
  // channel_t channels_in;
  // channel_t channels_out;
  // kernel_t kernel;  // kernel sizes supported: 3 or 1
  // bool pad;         // padding is either 0 or 1 pixel
  // stride_t stride;  // only stride 1 or 2 supported
  // memaddr_t mem_addr_input;
  // memaddr_t mem_addr_output;
  // memaddr_t mem_addr_weights;
  // bool is_expand_layer;
  // pooltype_t pool;

  // clang-format off
  union { float f; unsigned int i; } u;
  u.i = layer.width;        floats[0] = u.f;
  u.i = layer.height;       floats[1] = u.f;
  u.i = layer.channels_in;  floats[2] = u.f;
  u.i = layer.channels_out; floats[3] = u.f;
  u.i = layer.kernel;       floats[4] = u.f;
  u.i = layer.stride;       floats[5] = u.f;
  u.i = layer.pad;          floats[6] = u.f;
  u.i = layer.mem_addr_input;   floats[7] = u.f;
  u.i = layer.mem_addr_output;  floats[8] = u.f;
  u.i = layer.mem_addr_weights; floats[9] = u.f;
  u.i = layer.is_expand_layer;  floats[10] = u.f;
  u.i = (layer.pool == POOL_GLOBAL ? 1 : 0);  floats[11] = u.f;
  // clang-format on
}

// =========================================
// = Print Overview Table of given Network =
// =========================================
// Print List of all Layers + Attributes + Memory Locations
#define use_KB 0
#if use_KB
#define unit "k"
#define divi 1024
#else
#define unit ""
#define divi 1
#endif

void print_layer(layer_t *layer) {
  int memory_needed = layer->height * layer->width * layer->channels_in;
  int weights_size =
      layer->kernel * layer->kernel * layer->channels_in * layer->channels_out +
      layer->channels_out;

  printf("%6s: IN %3d x %3d x %3d @mem(%6lu-%6lu" unit "B), OUT @mem(%6lu" unit
         "B)",
         layer->name, (int)layer->height, (int)layer->width,
         (int)layer->channels_in,
         long(layer->mem_addr_input * sizeof(float) / divi),
         long((layer->mem_addr_input + memory_needed) * sizeof(float) / divi),
         long(layer->mem_addr_output * sizeof(float) / divi));

  printf(", CONV (%dx%d)/%d%s", (int)layer->kernel, (int)layer->kernel,
         (int)layer->stride, layer->pad ? "p" : " ");
  printf(", PARAM @mem(%4lu-%4lu" unit "B)",
         long(layer->mem_addr_weights * sizeof(float) / divi),
         long((layer->mem_addr_weights + weights_size) * sizeof(float) / divi));
  if (layer->is_expand_layer)
    printf(" (exact out %8lu B)", long(layer->mem_addr_output * sizeof(float)));
  if (layer->pool == POOL_GLOBAL) {
    printf(" POOL GLOBAL");
  } /*else if (layer->pool == POOL_3x3S2) {
   printf(" POOL 3x3/S2");
   } else if (layer->pool == POOL_2x2S2) {
   printf(" POOL 2x2/S2");
   }*/
  printf("\n");
};

void print_layers(network_t *net) {
  for (int i = 0; i < net->num_layers; i++) {
    layer_t *layer = &net->layers[i];
    print_layer(layer);
  }
}
