#ifndef NPP_RUNTIME_GRAPHICS_ASSET_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_ASSET_INTERNAL_H

#include "graphics/graphics_types_internal.h"

#include <stdint.h>

int neuron_graphics_load_png_wic(const char *path, uint8_t **out_pixels,
                                 uint32_t *out_width,
                                 uint32_t *out_height);
int neuron_graphics_load_obj_mesh(const char *path, NeuronGraphicsMesh *mesh);

#endif
