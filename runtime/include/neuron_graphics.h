#ifndef NEURON_GRAPHICS_H
#define NEURON_GRAPHICS_H

#include "neuron_runtime_export.h"

#include <stdint.h>
#include "neuron_tensor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct NeuronGraphicsWindow NeuronGraphicsWindow;
typedef struct NeuronGraphicsCanvas NeuronGraphicsCanvas;
typedef struct NeuronGraphicsTexture NeuronGraphicsTexture;
typedef struct NeuronGraphicsMesh NeuronGraphicsMesh;
typedef struct NeuronGraphicsMaterial NeuronGraphicsMaterial;
typedef struct NeuronGraphicsSampler NeuronGraphicsSampler;
typedef struct NeuronGraphicsColor NeuronGraphicsColor;
typedef struct NeuronGraphicsVector2 NeuronGraphicsVector2;
typedef struct NeuronGraphicsVector3 NeuronGraphicsVector3;
typedef struct NeuronGraphicsVector4 NeuronGraphicsVector4;
typedef struct NeuronGraphicsScene NeuronGraphicsScene;
typedef struct NeuronGraphicsEntity NeuronGraphicsEntity;
typedef struct NeuronGraphicsTransform NeuronGraphicsTransform;
typedef struct NeuronGraphicsRenderer2D NeuronGraphicsRenderer2D;
typedef struct NeuronGraphicsCamera2D NeuronGraphicsCamera2D;
typedef struct NeuronGraphicsSpriteRenderer2D NeuronGraphicsSpriteRenderer2D;
typedef struct NeuronGraphicsShapeRenderer2D NeuronGraphicsShapeRenderer2D;
typedef struct NeuronGraphicsTextRenderer2D NeuronGraphicsTextRenderer2D;
typedef struct NeuronGraphicsFont NeuronGraphicsFont;

typedef enum {
  NEURON_GRAPHICS_SHADER_STAGE_VERTEX = 1,
  NEURON_GRAPHICS_SHADER_STAGE_FRAGMENT = 2,
} NeuronGraphicsShaderStageFlags;

typedef enum {
  NEURON_GRAPHICS_SHADER_BINDING_UNKNOWN = 0,
  NEURON_GRAPHICS_SHADER_BINDING_VEC4 = 1,
  NEURON_GRAPHICS_SHADER_BINDING_TEXTURE2D = 2,
  NEURON_GRAPHICS_SHADER_BINDING_SAMPLER = 3,
  NEURON_GRAPHICS_SHADER_BINDING_MATRIX4 = 4,
} NeuronGraphicsShaderBindingKind;

typedef enum {
  NEURON_GRAPHICS_VERTEX_LAYOUT_POSITION = 1,
  NEURON_GRAPHICS_VERTEX_LAYOUT_UV = 2,
  NEURON_GRAPHICS_VERTEX_LAYOUT_NORMAL = 4,
} NeuronGraphicsVertexLayoutMask;

typedef struct {
  const char *name;
  uint32_t kind;
  uint32_t slot;
  uint32_t descriptor_binding;
  uint32_t uniform_offset;
  uint32_t uniform_size;
} NeuronGraphicsShaderBindingDescriptor;

typedef struct {
  const char *name;
  uint32_t stage_mask;
  uint32_t binding_count;
  const NeuronGraphicsShaderBindingDescriptor *bindings;
  uint32_t vertex_layout_mask;
  uint32_t uniform_buffer_size;
  uint32_t mvp_offset;
  const uint32_t *vertex_spirv_words;
  uint32_t vertex_spirv_word_count;
  const uint32_t *fragment_spirv_words;
  uint32_t fragment_spirv_word_count;
  const char *vertex_wgsl_source;
  uint32_t vertex_wgsl_size;
  const char *fragment_wgsl_source;
  uint32_t fragment_wgsl_size;
} NeuronGraphicsShaderDescriptor;

typedef struct {
  float px;
  float py;
  float pz;
  float u;
  float v;
  float nx;
  float ny;
  float nz;
} NeuronGraphicsVertex;

NEURON_RUNTIME_API NeuronGraphicsWindow *
neuron_graphics_create_window(int32_t width, int32_t height,
                              const char *title);
NEURON_RUNTIME_API void
neuron_graphics_window_free(NeuronGraphicsWindow *window);

NEURON_RUNTIME_API NeuronGraphicsCanvas *
neuron_graphics_create_canvas(NeuronGraphicsWindow *window);
NEURON_RUNTIME_API void
neuron_graphics_canvas_free(NeuronGraphicsCanvas *canvas);
NEURON_RUNTIME_API int32_t
neuron_graphics_canvas_pump(NeuronGraphicsCanvas *canvas);
NEURON_RUNTIME_API int32_t
neuron_graphics_canvas_should_close(NeuronGraphicsCanvas *canvas);
NEURON_RUNTIME_API int32_t
neuron_graphics_canvas_take_resize(NeuronGraphicsCanvas *canvas);
NEURON_RUNTIME_API void
neuron_graphics_canvas_begin_frame(NeuronGraphicsCanvas *canvas);
NEURON_RUNTIME_API void
neuron_graphics_canvas_end_frame(NeuronGraphicsCanvas *canvas);

NEURON_RUNTIME_API NeuronGraphicsTexture *
neuron_graphics_texture_load(const char *path);
NEURON_RUNTIME_API void
neuron_graphics_texture_free(NeuronGraphicsTexture *texture);
NEURON_RUNTIME_API NeuronGraphicsSampler *
neuron_graphics_sampler_create(void);
NEURON_RUNTIME_API void
neuron_graphics_sampler_free(NeuronGraphicsSampler *sampler);

NEURON_RUNTIME_API NeuronGraphicsMesh *
neuron_graphics_mesh_load(const char *path);
NEURON_RUNTIME_API NeuronGraphicsMesh *neuron_graphics_mesh_create_static(
    const NeuronGraphicsVertex *vertices, int32_t vertex_count,
    const uint32_t *indices, int32_t index_count);
NEURON_RUNTIME_API void neuron_graphics_mesh_free(NeuronGraphicsMesh *mesh);

NEURON_RUNTIME_API NeuronGraphicsMaterial *
neuron_graphics_material_create(
    const NeuronGraphicsShaderDescriptor *shader_descriptor);
NEURON_RUNTIME_API void
neuron_graphics_material_free(NeuronGraphicsMaterial *material);
NEURON_RUNTIME_API void neuron_graphics_material_set_vec4(
    NeuronGraphicsMaterial *material, const char *binding_name,
    void *color_token);
NEURON_RUNTIME_API void neuron_graphics_material_set_texture(
    NeuronGraphicsMaterial *material, const char *binding_name,
    NeuronGraphicsTexture *texture);
NEURON_RUNTIME_API void neuron_graphics_material_set_sampler(
    NeuronGraphicsMaterial *material, const char *binding_name,
    NeuronGraphicsSampler *sampler);
NEURON_RUNTIME_API void neuron_graphics_material_set_matrix4(
    NeuronGraphicsMaterial *material, const char *binding_name,
    const float *matrix4_values);

NEURON_RUNTIME_API NeuronGraphicsColor *
neuron_graphics_color_rgba(double red, double green, double blue, double alpha);
NEURON_RUNTIME_API void neuron_graphics_color_free(NeuronGraphicsColor *color);
NEURON_RUNTIME_API NeuronGraphicsVector2 *neuron_graphics_vector2_create(
    double x, double y);
NEURON_RUNTIME_API void neuron_graphics_vector2_free(
    NeuronGraphicsVector2 *value);
NEURON_RUNTIME_API NeuronGraphicsVector3 *neuron_graphics_vector3_create(
    double x, double y, double z);
NEURON_RUNTIME_API void neuron_graphics_vector3_free(
    NeuronGraphicsVector3 *value);
NEURON_RUNTIME_API NeuronGraphicsVector4 *neuron_graphics_vector4_create(
    double x, double y, double z, double w);
NEURON_RUNTIME_API void neuron_graphics_vector4_free(
    NeuronGraphicsVector4 *value);

NEURON_RUNTIME_API void neuron_graphics_draw(void *target, void *shader);
NEURON_RUNTIME_API void neuron_graphics_draw_indexed(void *target,
                                                     void *shader);
NEURON_RUNTIME_API void neuron_graphics_draw_instanced(void *target,
                                                       void *shader,
                                                       int32_t instances);
NEURON_RUNTIME_API int32_t
neuron_graphics_draw_tensor(NeuronTensor *tensor);
NEURON_RUNTIME_API void neuron_graphics_clear(void *color_token);
NEURON_RUNTIME_API void neuron_graphics_present(void);

NEURON_RUNTIME_API int32_t
neuron_graphics_window_get_width(NeuronGraphicsWindow *window);
NEURON_RUNTIME_API int32_t
neuron_graphics_window_get_height(NeuronGraphicsWindow *window);

NEURON_RUNTIME_API NeuronGraphicsScene *neuron_graphics_scene_create(void);
NEURON_RUNTIME_API void neuron_graphics_scene_free(NeuronGraphicsScene *scene);
NEURON_RUNTIME_API NeuronGraphicsEntity *
neuron_graphics_scene_create_entity(NeuronGraphicsScene *scene,
                                    const char *name);
NEURON_RUNTIME_API void
neuron_graphics_scene_destroy_entity(NeuronGraphicsScene *scene,
                                     NeuronGraphicsEntity *entity);
NEURON_RUNTIME_API NeuronGraphicsEntity *
neuron_graphics_scene_find_entity(NeuronGraphicsScene *scene,
                                  const char *name);

NEURON_RUNTIME_API NeuronGraphicsTransform *
neuron_graphics_entity_get_transform(NeuronGraphicsEntity *entity);
NEURON_RUNTIME_API NeuronGraphicsCamera2D *
neuron_graphics_entity_add_camera2d(NeuronGraphicsEntity *entity);
NEURON_RUNTIME_API NeuronGraphicsSpriteRenderer2D *
neuron_graphics_entity_add_sprite_renderer2d(NeuronGraphicsEntity *entity);
NEURON_RUNTIME_API NeuronGraphicsShapeRenderer2D *
neuron_graphics_entity_add_shape_renderer2d(NeuronGraphicsEntity *entity);
NEURON_RUNTIME_API NeuronGraphicsTextRenderer2D *
neuron_graphics_entity_add_text_renderer2d(NeuronGraphicsEntity *entity);

NEURON_RUNTIME_API void neuron_graphics_transform_set_parent(
    NeuronGraphicsTransform *transform, NeuronGraphicsEntity *parent);
NEURON_RUNTIME_API void neuron_graphics_transform_set_position(
    NeuronGraphicsTransform *transform, NeuronGraphicsVector3 *value);
NEURON_RUNTIME_API void neuron_graphics_transform_set_rotation(
    NeuronGraphicsTransform *transform, NeuronGraphicsVector3 *value);
NEURON_RUNTIME_API void neuron_graphics_transform_set_scale(
    NeuronGraphicsTransform *transform, NeuronGraphicsVector3 *value);

NEURON_RUNTIME_API NeuronGraphicsRenderer2D *neuron_graphics_renderer2d_create(
    void);
NEURON_RUNTIME_API void neuron_graphics_renderer2d_free(
    NeuronGraphicsRenderer2D *renderer);
NEURON_RUNTIME_API void neuron_graphics_renderer2d_set_clear_color(
    NeuronGraphicsRenderer2D *renderer, NeuronGraphicsColor *color);
NEURON_RUNTIME_API void neuron_graphics_renderer2d_set_camera(
    NeuronGraphicsRenderer2D *renderer, NeuronGraphicsCamera2D *camera);
NEURON_RUNTIME_API void neuron_graphics_renderer2d_render(
    NeuronGraphicsRenderer2D *renderer, NeuronGraphicsScene *scene);

NEURON_RUNTIME_API void neuron_graphics_camera2d_set_zoom(
    NeuronGraphicsCamera2D *camera, double value);
NEURON_RUNTIME_API void neuron_graphics_camera2d_set_primary(
    NeuronGraphicsCamera2D *camera, int32_t value);

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_texture(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsTexture *texture);
NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_color(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsColor *color);
NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_size(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsVector2 *size);
NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_pivot(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsVector2 *pivot);
NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_flip_x(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value);
NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_flip_y(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value);
NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_sorting_layer(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value);
NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_order_in_layer(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value);

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_rectangle(
    NeuronGraphicsShapeRenderer2D *renderer, NeuronGraphicsVector2 *size);
NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_circle(
    NeuronGraphicsShapeRenderer2D *renderer, double radius, int32_t segments);
NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_line(
    NeuronGraphicsShapeRenderer2D *renderer, NeuronGraphicsVector2 *start,
    NeuronGraphicsVector2 *end, double thickness);
NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_color(
    NeuronGraphicsShapeRenderer2D *renderer, NeuronGraphicsColor *color);
NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_filled(
    NeuronGraphicsShapeRenderer2D *renderer, int32_t value);
NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_sorting_layer(
    NeuronGraphicsShapeRenderer2D *renderer, int32_t value);
NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_order_in_layer(
    NeuronGraphicsShapeRenderer2D *renderer, int32_t value);

NEURON_RUNTIME_API NeuronGraphicsFont *neuron_graphics_font_load(
    const char *path);
NEURON_RUNTIME_API void neuron_graphics_font_free(NeuronGraphicsFont *font);

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_font(
    NeuronGraphicsTextRenderer2D *renderer, NeuronGraphicsFont *font);
NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_text(
    NeuronGraphicsTextRenderer2D *renderer, const char *text);
NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_font_size(
    NeuronGraphicsTextRenderer2D *renderer, double value);
NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_color(
    NeuronGraphicsTextRenderer2D *renderer, NeuronGraphicsColor *color);
NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_alignment(
    NeuronGraphicsTextRenderer2D *renderer, int32_t value);
NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_sorting_layer(
    NeuronGraphicsTextRenderer2D *renderer, int32_t value);
NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_order_in_layer(
    NeuronGraphicsTextRenderer2D *renderer, int32_t value);

NEURON_RUNTIME_API const char *neuron_graphics_last_error(void);

#ifdef __cplusplus
}
#endif

#endif // NEURON_GRAPHICS_H
