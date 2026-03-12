#ifndef NPP_RUNTIME_GRAPHICS_SCENE2D_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_SCENE2D_INTERNAL_H

#include "graphics/graphics_core_internal.h"
#include "graphics/graphics_assets_internal.h"

typedef enum {
  NEURON_GRAPHICS_SHAPE_MODE_NONE = 0,
  NEURON_GRAPHICS_SHAPE_MODE_RECTANGLE = 1,
  NEURON_GRAPHICS_SHAPE_MODE_CIRCLE = 2,
  NEURON_GRAPHICS_SHAPE_MODE_LINE = 3,
} NeuronGraphicsShapeMode;

typedef struct NeuronGraphicsEntity NeuronGraphicsEntity;

struct NeuronGraphicsTransform {
  NeuronGraphicsEntity *entity;
  NeuronGraphicsEntity *parent;
  NeuronGraphicsVector3 position;
  NeuronGraphicsVector3 rotation;
  NeuronGraphicsVector3 scale;
};

struct NeuronGraphicsCamera2D {
  NeuronGraphicsEntity *entity;
  float zoom;
  int primary;
};

struct NeuronGraphicsSpriteRenderer2D {
  NeuronGraphicsEntity *entity;
  NeuronGraphicsTexture *texture;
  NeuronGraphicsColor color;
  NeuronGraphicsVector2 size;
  NeuronGraphicsVector2 pivot;
  int flip_x;
  int flip_y;
  int sorting_layer;
  int order_in_layer;
  NeuronGraphicsSampler *sampler;
  NeuronGraphicsMaterial *material;
  NeuronGraphicsMesh *mesh;
};

struct NeuronGraphicsShapeRenderer2D {
  NeuronGraphicsEntity *entity;
  NeuronGraphicsShapeMode mode;
  NeuronGraphicsColor color;
  NeuronGraphicsVector2 rectangle_size;
  float circle_radius;
  int32_t circle_segments;
  NeuronGraphicsVector2 line_start;
  NeuronGraphicsVector2 line_end;
  float line_thickness;
  int filled;
  int sorting_layer;
  int order_in_layer;
  NeuronGraphicsMaterial *material;
  NeuronGraphicsMesh *mesh;
};

struct NeuronGraphicsFont {
  char *path;
};

struct NeuronGraphicsTextRenderer2D {
  NeuronGraphicsEntity *entity;
  NeuronGraphicsFont *font;
  char *text;
  float font_size;
  NeuronGraphicsColor color;
  int32_t alignment;
  int sorting_layer;
  int order_in_layer;
  NeuronGraphicsMaterial *material;
  NeuronGraphicsMesh *mesh;
};

struct NeuronGraphicsEntity {
  struct NeuronGraphicsScene *scene;
  char *name;
  uint32_t creation_order;
  struct NeuronGraphicsTransform transform;
  struct NeuronGraphicsCamera2D *camera2d;
  struct NeuronGraphicsSpriteRenderer2D *sprite_renderer2d;
  struct NeuronGraphicsShapeRenderer2D *shape_renderer2d;
  struct NeuronGraphicsTextRenderer2D *text_renderer2d;
};

struct NeuronGraphicsScene {
  NeuronGraphicsEntity **entities;
  size_t entity_count;
  size_t entity_capacity;
  uint32_t next_creation_order;
};

struct NeuronGraphicsRenderer2D {
  int has_clear_color;
  NeuronGraphicsColor clear_color;
  NeuronGraphicsCamera2D *explicit_camera;
};

void neuron_graphics_scene2d_free_entity(NeuronGraphicsEntity *entity);

#endif
