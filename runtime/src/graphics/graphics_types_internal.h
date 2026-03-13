#ifndef Neuron_RUNTIME_GRAPHICS_TYPES_INTERNAL_H
#define Neuron_RUNTIME_GRAPHICS_TYPES_INTERNAL_H

#include "neuron_graphics.h"

#include <stddef.h>
#include <stdint.h>

typedef struct NeuronGraphicsBackend NeuronGraphicsBackend;
typedef void *NeuronGraphicsNativeWindowHandle;

typedef enum {
  NEURON_GRAPHICS_DRAW_KIND_NON_INDEXED = 0,
  NEURON_GRAPHICS_DRAW_KIND_INDEXED = 1,
  NEURON_GRAPHICS_DRAW_KIND_INSTANCED = 2,
} NeuronGraphicsDrawKind;

typedef struct {
  uintptr_t image;
  uintptr_t image_memory;
  uintptr_t image_view;
  uintptr_t web_texture;
  uintptr_t web_texture_view;
  int gpu_ready;
} NeuronGraphicsTextureGpuState;

typedef struct {
  uintptr_t native_sampler;
  uintptr_t web_sampler;
  int gpu_ready;
} NeuronGraphicsSamplerGpuState;

typedef struct {
  uintptr_t vertex_buffer;
  uintptr_t index_buffer;
  int gpu_ready;
} NeuronGraphicsMeshGpuState;

struct NeuronGraphicsColor {
  float red;
  float green;
  float blue;
  float alpha;
};

struct NeuronGraphicsVector2 {
  float x;
  float y;
};

struct NeuronGraphicsVector3 {
  float x;
  float y;
  float z;
};

struct NeuronGraphicsVector4 {
  float x;
  float y;
  float z;
  float w;
};

typedef struct {
  const NeuronGraphicsShaderBindingDescriptor *descriptor;
  int has_vec4;
  int has_matrix4;
  NeuronGraphicsColor vec4_value;
  struct NeuronGraphicsTexture *texture_value;
  struct NeuronGraphicsSampler *sampler_value;
  float matrix4_value[16];
} NeuronGraphicsMaterialBinding;

typedef struct {
  NeuronGraphicsDrawKind kind;
  struct NeuronGraphicsMesh *mesh;
  struct NeuronGraphicsMaterial *material;
  int32_t instance_count;
} NeuronGraphicsDrawCommand;

struct NeuronGraphicsWindow {
  int32_t width;
  int32_t height;
  char *title;
  int should_close;
  int resized;
  NeuronGraphicsNativeWindowHandle hwnd;
};

struct NeuronGraphicsCanvas {
  NeuronGraphicsWindow *window;
  NeuronGraphicsBackend *backend;
  int frame_active;
  int frame_presented;
  int has_clear_color;
  struct NeuronGraphicsColor clear_color;
  NeuronGraphicsDrawCommand *draw_commands;
  size_t draw_command_count;
  size_t draw_command_capacity;
};

struct NeuronGraphicsTexture {
  char *path;
  uint32_t width;
  uint32_t height;
  uint8_t *pixels;
  NeuronGraphicsTextureGpuState gpu;
};

struct NeuronGraphicsSampler {
  NeuronGraphicsSamplerGpuState gpu;
};

struct NeuronGraphicsMaterial {
  const NeuronGraphicsShaderDescriptor *shader_descriptor;
  NeuronGraphicsMaterialBinding *bindings;
  uint32_t binding_count;
  uint8_t *uniform_data;
  uint32_t uniform_data_size;
};

struct NeuronGraphicsMesh {
  char *path;
  NeuronGraphicsVertex *vertices;
  size_t vertex_count;
  uint32_t *indices;
  size_t index_count;
  NeuronGraphicsMeshGpuState gpu;
};

#endif
