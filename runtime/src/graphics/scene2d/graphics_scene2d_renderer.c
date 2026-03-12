#include "graphics/scene2d/graphics_scene2d_internal.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
  int sorting_layer;
  int order_in_layer;
  float z;
  uint32_t creation_order;
  int kind;
  void *payload;
} NeuronGraphicsSceneRenderItem;

enum {
  NEURON_SCENE_RENDER_ITEM_SPRITE = 1,
  NEURON_SCENE_RENDER_ITEM_SHAPE = 2,
  NEURON_SCENE_RENDER_ITEM_TEXT = 3,
};

static const NeuronGraphicsShaderBindingDescriptor kSceneColorBindings[] = {
    {"tint", NEURON_GRAPHICS_SHADER_BINDING_VEC4, 0, 0, 0, 16},
};

static const NeuronGraphicsShaderDescriptor kSceneColorShader = {
    "Scene2DColor",
    NEURON_GRAPHICS_SHADER_STAGE_VERTEX |
        NEURON_GRAPHICS_SHADER_STAGE_FRAGMENT,
    1,
    kSceneColorBindings,
    NEURON_GRAPHICS_VERTEX_LAYOUT_POSITION,
    16,
    UINT32_MAX,
    NULL,
    0,
    NULL,
    0,
    NULL,
    0,
    NULL,
    0,
};

static const NeuronGraphicsShaderBindingDescriptor kSceneSpriteBindings[] = {
    {"tint", NEURON_GRAPHICS_SHADER_BINDING_VEC4, 0, 0, 0, 16},
    {"albedo", NEURON_GRAPHICS_SHADER_BINDING_TEXTURE2D, 1, 1, UINT32_MAX, 0},
    {"linearSampler", NEURON_GRAPHICS_SHADER_BINDING_SAMPLER, 2, 2, UINT32_MAX,
     0},
};

static const NeuronGraphicsShaderDescriptor kSceneSpriteShader = {
    "Scene2DSprite",
    NEURON_GRAPHICS_SHADER_STAGE_VERTEX |
        NEURON_GRAPHICS_SHADER_STAGE_FRAGMENT,
    3,
    kSceneSpriteBindings,
    NEURON_GRAPHICS_VERTEX_LAYOUT_POSITION | NEURON_GRAPHICS_VERTEX_LAYOUT_UV,
    16,
    UINT32_MAX,
    NULL,
    0,
    NULL,
    0,
    NULL,
    0,
    NULL,
    0,
};

static NeuronGraphicsVector3 neuron_graphics_scene_world_position(
    const NeuronGraphicsEntity *entity) {
  NeuronGraphicsVector3 result = {0.0f, 0.0f, 0.0f};
  const NeuronGraphicsEntity *cursor = entity;
  while (cursor != NULL) {
    result.x += cursor->transform.position.x;
    result.y += cursor->transform.position.y;
    result.z += cursor->transform.position.z;
    cursor = cursor->transform.parent;
  }
  return result;
}

static NeuronGraphicsVector3 neuron_graphics_scene_world_scale(
    const NeuronGraphicsEntity *entity) {
  NeuronGraphicsVector3 result = {1.0f, 1.0f, 1.0f};
  const NeuronGraphicsEntity *cursor = entity;
  while (cursor != NULL) {
    result.x *= cursor->transform.scale.x;
    result.y *= cursor->transform.scale.y;
    result.z *= cursor->transform.scale.z;
    cursor = cursor->transform.parent;
  }
  return result;
}

static NeuronGraphicsVector3 neuron_graphics_scene_world_rotation(
    const NeuronGraphicsEntity *entity) {
  NeuronGraphicsVector3 result = {0.0f, 0.0f, 0.0f};
  const NeuronGraphicsEntity *cursor = entity;
  while (cursor != NULL) {
    result.x += cursor->transform.rotation.x;
    result.y += cursor->transform.rotation.y;
    result.z += cursor->transform.rotation.z;
    cursor = cursor->transform.parent;
  }
  return result;
}

static void neuron_graphics_scene_apply_clip_transform(
    float x, float y, float z, const NeuronGraphicsCamera2D *camera,
    int32_t window_width, int32_t window_height, float *out_x, float *out_y,
    float *out_z) {
  float cam_x = 0.0f;
  float cam_y = 0.0f;
  float zoom = 1.0f;
  if (camera != NULL && camera->entity != NULL) {
    NeuronGraphicsVector3 camera_position =
        neuron_graphics_scene_world_position(camera->entity);
    cam_x = camera_position.x;
    cam_y = camera_position.y;
    zoom = camera->zoom > 0.0f ? camera->zoom : 1.0f;
  }
  *out_x = ((x - cam_x) * zoom) / (float)(window_width > 0 ? window_width : 1) *
           2.0f;
  *out_y = ((y - cam_y) * zoom) / (float)(window_height > 0 ? window_height : 1) *
           2.0f;
  *out_z = z;
}

static int neuron_graphics_scene_render_item_compare(const void *lhs,
                                                     const void *rhs) {
  const NeuronGraphicsSceneRenderItem *a =
      (const NeuronGraphicsSceneRenderItem *)lhs;
  const NeuronGraphicsSceneRenderItem *b =
      (const NeuronGraphicsSceneRenderItem *)rhs;
  if (a->sorting_layer != b->sorting_layer) {
    return a->sorting_layer < b->sorting_layer ? -1 : 1;
  }
  if (a->order_in_layer != b->order_in_layer) {
    return a->order_in_layer < b->order_in_layer ? -1 : 1;
  }
  if (a->z != b->z) {
    return a->z < b->z ? -1 : 1;
  }
  if (a->creation_order != b->creation_order) {
    return a->creation_order < b->creation_order ? -1 : 1;
  }
  return a->kind - b->kind;
}

static NeuronGraphicsCamera2D *neuron_graphics_scene_select_camera(
    const NeuronGraphicsRenderer2D *renderer, NeuronGraphicsScene *scene) {
  size_t i = 0;
  NeuronGraphicsCamera2D *first_camera = NULL;
  if (renderer != NULL && renderer->explicit_camera != NULL) {
    return renderer->explicit_camera;
  }
  if (scene == NULL) {
    return NULL;
  }
  for (i = 0; i < scene->entity_count; ++i) {
    NeuronGraphicsEntity *entity = scene->entities[i];
    if (entity == NULL || entity->camera2d == NULL) {
      continue;
    }
    if (entity->camera2d->primary) {
      return entity->camera2d;
    }
    if (first_camera == NULL) {
      first_camera = entity->camera2d;
    }
  }
  return first_camera;
}

static int neuron_graphics_scene_ensure_sprite_resources(
    NeuronGraphicsSpriteRenderer2D *renderer) {
  const uint32_t quad_indices[] = {0u, 1u, 2u, 0u, 2u, 3u};
  NeuronGraphicsVertex empty_vertices[4];
  memset(empty_vertices, 0, sizeof(empty_vertices));
  if (renderer == NULL) {
    return 0;
  }
  if (renderer->sampler == NULL) {
    renderer->sampler = neuron_graphics_sampler_create();
  }
  if (renderer->material == NULL) {
    renderer->material = neuron_graphics_material_create(&kSceneSpriteShader);
  }
  if (renderer->mesh == NULL) {
    renderer->mesh =
        neuron_graphics_mesh_create_static(empty_vertices, 4, quad_indices, 6);
  }
  if (renderer->material == NULL || renderer->mesh == NULL || renderer->sampler == NULL) {
    return 0;
  }
  neuron_graphics_material_set_sampler(renderer->material, "linearSampler",
                                       renderer->sampler);
  if (renderer->texture != NULL) {
    neuron_graphics_material_set_texture(renderer->material, "albedo",
                                         renderer->texture);
  }
  neuron_graphics_material_set_vec4(renderer->material, "tint", &renderer->color);
  return 1;
}

static int neuron_graphics_scene_ensure_shape_material(
    NeuronGraphicsShapeRenderer2D *renderer) {
  if (renderer == NULL) {
    return 0;
  }
  if (renderer->material == NULL) {
    renderer->material = neuron_graphics_material_create(&kSceneColorShader);
  }
  if (renderer->material == NULL) {
    return 0;
  }
  neuron_graphics_material_set_vec4(renderer->material, "tint", &renderer->color);
  return 1;
}

static int neuron_graphics_scene_ensure_text_material(
    NeuronGraphicsTextRenderer2D *renderer) {
  if (renderer == NULL) {
    return 0;
  }
  if (renderer->material == NULL) {
    renderer->material = neuron_graphics_material_create(&kSceneColorShader);
  }
  if (renderer->material == NULL) {
    return 0;
  }
  neuron_graphics_material_set_vec4(renderer->material, "tint", &renderer->color);
  return 1;
}

static void neuron_graphics_scene_fill_quad(NeuronGraphicsVertex *vertices,
                                            float center_x, float center_y,
                                            float z, float width, float height,
                                            float pivot_x, float pivot_y,
                                            float rotation_deg,
                                            int flip_x, int flip_y,
                                            const NeuronGraphicsCamera2D *camera,
                                            int32_t window_width,
                                            int32_t window_height) {
  float min_x = -pivot_x * width;
  float min_y = -pivot_y * height;
  float max_x = min_x + width;
  float max_y = min_y + height;
  float corners[4][2] = {
      {min_x, min_y}, {max_x, min_y}, {max_x, max_y}, {min_x, max_y}};
  float uvs[4][2] = {{0.0f, 1.0f}, {1.0f, 1.0f}, {1.0f, 0.0f}, {0.0f, 0.0f}};
  const float radians = rotation_deg * 0.01745329251994329577f;
  const float cosine = cosf(radians);
  const float sine = sinf(radians);
  size_t i = 0;

  for (i = 0; i < 4u; ++i) {
    const float local_x = corners[i][0];
    const float local_y = corners[i][1];
    const float rotated_x = local_x * cosine - local_y * sine;
    const float rotated_y = local_x * sine + local_y * cosine;
    neuron_graphics_scene_apply_clip_transform(
        center_x + rotated_x, center_y + rotated_y, z, camera, window_width,
        window_height, &vertices[i].px, &vertices[i].py, &vertices[i].pz);
    vertices[i].u = flip_x ? 1.0f - uvs[i][0] : uvs[i][0];
    vertices[i].v = flip_y ? 1.0f - uvs[i][1] : uvs[i][1];
    vertices[i].nx = 0.0f;
    vertices[i].ny = 0.0f;
    vertices[i].nz = 1.0f;
  }
}

static void neuron_graphics_scene_render_sprite(
    NeuronGraphicsSpriteRenderer2D *renderer,
    const NeuronGraphicsCamera2D *camera, int32_t window_width,
    int32_t window_height) {
  NeuronGraphicsVector3 position;
  NeuronGraphicsVector3 scale;
  NeuronGraphicsVector3 rotation;
  if (renderer == NULL || renderer->entity == NULL || renderer->texture == NULL ||
      !neuron_graphics_scene_ensure_sprite_resources(renderer)) {
    return;
  }
  position = neuron_graphics_scene_world_position(renderer->entity);
  scale = neuron_graphics_scene_world_scale(renderer->entity);
  rotation = neuron_graphics_scene_world_rotation(renderer->entity);
  neuron_graphics_scene_fill_quad(
      renderer->mesh->vertices, position.x, position.y, position.z,
      renderer->size.x * scale.x, renderer->size.y * scale.y, renderer->pivot.x,
      renderer->pivot.y, rotation.z, renderer->flip_x, renderer->flip_y, camera,
      window_width, window_height);
  neuron_graphics_draw_indexed(renderer->mesh, renderer->material);
}

static void neuron_graphics_scene_render_rectangle(
    NeuronGraphicsShapeRenderer2D *renderer,
    const NeuronGraphicsCamera2D *camera, int32_t window_width,
    int32_t window_height) {
  const uint32_t quad_indices[] = {0u, 1u, 2u, 0u, 2u, 3u};
  NeuronGraphicsVertex vertices[4];
  NeuronGraphicsVector3 position;
  NeuronGraphicsVector3 scale;
  NeuronGraphicsVector3 rotation;
  memset(vertices, 0, sizeof(vertices));
  if (renderer == NULL || renderer->entity == NULL ||
      !neuron_graphics_scene_ensure_shape_material(renderer)) {
    return;
  }
  position = neuron_graphics_scene_world_position(renderer->entity);
  scale = neuron_graphics_scene_world_scale(renderer->entity);
  rotation = neuron_graphics_scene_world_rotation(renderer->entity);
  neuron_graphics_scene_fill_quad(
      vertices, position.x, position.y, position.z,
      renderer->rectangle_size.x * scale.x, renderer->rectangle_size.y * scale.y,
      0.5f, 0.5f, rotation.z, 0, 0, camera, window_width, window_height);
  neuron_graphics_mesh_free(renderer->mesh);
  renderer->mesh = neuron_graphics_mesh_create_static(vertices, 4, quad_indices, 6);
  if (renderer->mesh != NULL) {
    neuron_graphics_draw_indexed(renderer->mesh, renderer->material);
  }
}

static void neuron_graphics_scene_render_line(
    NeuronGraphicsShapeRenderer2D *renderer,
    const NeuronGraphicsCamera2D *camera, int32_t window_width,
    int32_t window_height) {
  const uint32_t quad_indices[] = {0u, 1u, 2u, 0u, 2u, 3u};
  NeuronGraphicsVertex vertices[4];
  NeuronGraphicsVector3 position;
  const float dx = renderer->line_end.x - renderer->line_start.x;
  const float dy = renderer->line_end.y - renderer->line_start.y;
  const float length = sqrtf(dx * dx + dy * dy);
  memset(vertices, 0, sizeof(vertices));
  if (renderer == NULL || renderer->entity == NULL ||
      !neuron_graphics_scene_ensure_shape_material(renderer)) {
    return;
  }
  position = neuron_graphics_scene_world_position(renderer->entity);
  neuron_graphics_scene_fill_quad(
      vertices, position.x + (renderer->line_start.x + renderer->line_end.x) * 0.5f,
      position.y + (renderer->line_start.y + renderer->line_end.y) * 0.5f,
      position.z, length, renderer->line_thickness > 0.0f ? renderer->line_thickness : 1.0f,
      0.5f, 0.5f, atan2f(dy, dx) * 57.29577951308232f, 0, 0, camera,
      window_width, window_height);
  neuron_graphics_mesh_free(renderer->mesh);
  renderer->mesh = neuron_graphics_mesh_create_static(vertices, 4, quad_indices, 6);
  if (renderer->mesh != NULL) {
    neuron_graphics_draw_indexed(renderer->mesh, renderer->material);
  }
}

static void neuron_graphics_scene_render_circle(
    NeuronGraphicsShapeRenderer2D *renderer,
    const NeuronGraphicsCamera2D *camera, int32_t window_width,
    int32_t window_height) {
  const int32_t segments = renderer->circle_segments > 2 ? renderer->circle_segments : 16;
  const size_t vertex_count = (size_t)segments + 1u;
  const size_t index_count = (size_t)segments * 3u;
  NeuronGraphicsVertex *vertices =
      (NeuronGraphicsVertex *)calloc(vertex_count, sizeof(NeuronGraphicsVertex));
  uint32_t *indices =
      (uint32_t *)calloc(index_count, sizeof(uint32_t));
  NeuronGraphicsVector3 position;
  int32_t i = 0;
  if (vertices == NULL || indices == NULL || renderer == NULL ||
      renderer->entity == NULL || !neuron_graphics_scene_ensure_shape_material(renderer)) {
    free(vertices);
    free(indices);
    return;
  }
  position = neuron_graphics_scene_world_position(renderer->entity);
  neuron_graphics_scene_apply_clip_transform(position.x, position.y, position.z,
                                             camera, window_width, window_height,
                                             &vertices[0].px, &vertices[0].py,
                                             &vertices[0].pz);
  for (i = 0; i < segments; ++i) {
    const float angle =
        ((float)i / (float)segments) * 6.28318530717958647692f;
    const float x = position.x + cosf(angle) * renderer->circle_radius;
    const float y = position.y + sinf(angle) * renderer->circle_radius;
    neuron_graphics_scene_apply_clip_transform(
        x, y, position.z, camera, window_width, window_height,
        &vertices[(size_t)i + 1u].px, &vertices[(size_t)i + 1u].py,
        &vertices[(size_t)i + 1u].pz);
    indices[(size_t)i * 3u + 0u] = 0u;
    indices[(size_t)i * 3u + 1u] = (uint32_t)i + 1u;
    indices[(size_t)i * 3u + 2u] =
        (uint32_t)((i + 1) % segments) + 1u;
  }
  neuron_graphics_mesh_free(renderer->mesh);
  renderer->mesh = neuron_graphics_mesh_create_static(
      vertices, (int32_t)vertex_count, indices, (int32_t)index_count);
  free(vertices);
  free(indices);
  if (renderer->mesh != NULL) {
    neuron_graphics_draw_indexed(renderer->mesh, renderer->material);
  }
}

static void neuron_graphics_scene_render_shape(
    NeuronGraphicsShapeRenderer2D *renderer,
    const NeuronGraphicsCamera2D *camera, int32_t window_width,
    int32_t window_height) {
  if (renderer == NULL) {
    return;
  }
  if (renderer->mode == NEURON_GRAPHICS_SHAPE_MODE_LINE) {
    neuron_graphics_scene_render_line(renderer, camera, window_width, window_height);
    return;
  }
  if (renderer->mode == NEURON_GRAPHICS_SHAPE_MODE_CIRCLE) {
    neuron_graphics_scene_render_circle(renderer, camera, window_width, window_height);
    return;
  }
  neuron_graphics_scene_render_rectangle(renderer, camera, window_width, window_height);
}

static void neuron_graphics_scene_render_text(
    NeuronGraphicsTextRenderer2D *renderer,
    const NeuronGraphicsCamera2D *camera, int32_t window_width,
    int32_t window_height) {
  const uint32_t *indices = NULL;
  NeuronGraphicsVertex *vertices = NULL;
  uint32_t *owned_indices = NULL;
  size_t char_count = 0;
  size_t i = 0;
  NeuronGraphicsVector3 position;
  float cursor_x = 0.0f;
  float glyph_width = 0.0f;
  float total_width = 0.0f;

  if (renderer == NULL || renderer->entity == NULL || renderer->text == NULL ||
      !neuron_graphics_scene_ensure_text_material(renderer)) {
    return;
  }
  char_count = strlen(renderer->text);
  if (char_count == 0u) {
    return;
  }
  glyph_width = renderer->font_size * 0.6f;
  total_width = (float)char_count * glyph_width;
  if (renderer->alignment == 1) {
    cursor_x = -total_width * 0.5f;
  } else if (renderer->alignment == 2) {
    cursor_x = -total_width;
  }
  vertices = (NeuronGraphicsVertex *)calloc(char_count * 4u, sizeof(*vertices));
  owned_indices = (uint32_t *)calloc(char_count * 6u, sizeof(*owned_indices));
  if (vertices == NULL || owned_indices == NULL) {
    free(vertices);
    free(owned_indices);
    return;
  }
  position = neuron_graphics_scene_world_position(renderer->entity);
  for (i = 0; i < char_count; ++i) {
    neuron_graphics_scene_fill_quad(
        &vertices[i * 4u], position.x + cursor_x + glyph_width * 0.5f,
        position.y, position.z, glyph_width, renderer->font_size, 0.5f, 0.5f,
        0.0f, 0, 0, camera, window_width, window_height);
    owned_indices[i * 6u + 0u] = (uint32_t)(i * 4u + 0u);
    owned_indices[i * 6u + 1u] = (uint32_t)(i * 4u + 1u);
    owned_indices[i * 6u + 2u] = (uint32_t)(i * 4u + 2u);
    owned_indices[i * 6u + 3u] = (uint32_t)(i * 4u + 0u);
    owned_indices[i * 6u + 4u] = (uint32_t)(i * 4u + 2u);
    owned_indices[i * 6u + 5u] = (uint32_t)(i * 4u + 3u);
    cursor_x += glyph_width;
  }
  indices = owned_indices;
  neuron_graphics_mesh_free(renderer->mesh);
  renderer->mesh = neuron_graphics_mesh_create_static(
      vertices, (int32_t)(char_count * 4u), indices, (int32_t)(char_count * 6u));
  free(vertices);
  free(owned_indices);
  if (renderer->mesh != NULL) {
    neuron_graphics_draw_indexed(renderer->mesh, renderer->material);
  }
}

NEURON_RUNTIME_API NeuronGraphicsRenderer2D *neuron_graphics_renderer2d_create(
    void) {
  NeuronGraphicsRenderer2D *renderer =
      (NeuronGraphicsRenderer2D *)calloc(1, sizeof(NeuronGraphicsRenderer2D));
  if (renderer == NULL) {
    neuron_graphics_set_error("Out of memory allocating Renderer2D");
    return NULL;
  }
  renderer->clear_color.alpha = 1.0f;
  return renderer;
}

NEURON_RUNTIME_API void neuron_graphics_renderer2d_free(
    NeuronGraphicsRenderer2D *renderer) {
  free(renderer);
}

NEURON_RUNTIME_API void neuron_graphics_renderer2d_set_clear_color(
    NeuronGraphicsRenderer2D *renderer, NeuronGraphicsColor *color) {
  if (renderer == NULL || color == NULL) {
    neuron_graphics_set_error("Renderer2D.SetClearColor requires both renderer and color");
    return;
  }
  renderer->has_clear_color = 1;
  renderer->clear_color = *color;
}

NEURON_RUNTIME_API void neuron_graphics_renderer2d_set_camera(
    NeuronGraphicsRenderer2D *renderer, NeuronGraphicsCamera2D *camera) {
  if (renderer == NULL) {
    neuron_graphics_set_error("Renderer2D.SetCamera requires a renderer");
    return;
  }
  renderer->explicit_camera = camera;
}

NEURON_RUNTIME_API void neuron_graphics_renderer2d_render(
    NeuronGraphicsRenderer2D *renderer, NeuronGraphicsScene *scene) {
  size_t i = 0;
  size_t item_count = 0;
  NeuronGraphicsSceneRenderItem *items = NULL;
  NeuronGraphicsCamera2D *camera = NULL;
  int32_t window_width = 1280;
  int32_t window_height = 720;

  if (renderer == NULL || scene == NULL) {
    neuron_graphics_set_error("Renderer2D.Render requires both renderer and scene");
    return;
  }
  if (g_active_canvas == NULL || !g_active_canvas->frame_active) {
    neuron_graphics_set_error("Renderer2D.Render requires an active canvas frame");
    return;
  }
  if (g_active_canvas->window != NULL) {
    window_width = g_active_canvas->window->width;
    window_height = g_active_canvas->window->height;
  }

  if (renderer->has_clear_color) {
    neuron_graphics_clear(&renderer->clear_color);
  } else {
    neuron_graphics_clear(NULL);
  }

  camera = neuron_graphics_scene_select_camera(renderer, scene);
  items = (NeuronGraphicsSceneRenderItem *)calloc(scene->entity_count * 3u,
                                                  sizeof(*items));
  if (items == NULL) {
    neuron_graphics_set_error("Out of memory allocating Renderer2D draw list");
    return;
  }

  for (i = 0; i < scene->entity_count; ++i) {
    NeuronGraphicsEntity *entity = scene->entities[i];
    NeuronGraphicsVector3 position;
    if (entity == NULL) {
      continue;
    }
    position = neuron_graphics_scene_world_position(entity);
    if (entity->sprite_renderer2d != NULL) {
      items[item_count].sorting_layer = entity->sprite_renderer2d->sorting_layer;
      items[item_count].order_in_layer = entity->sprite_renderer2d->order_in_layer;
      items[item_count].z = position.z;
      items[item_count].creation_order = entity->creation_order;
      items[item_count].kind = NEURON_SCENE_RENDER_ITEM_SPRITE;
      items[item_count].payload = entity->sprite_renderer2d;
      ++item_count;
    }
    if (entity->shape_renderer2d != NULL) {
      items[item_count].sorting_layer = entity->shape_renderer2d->sorting_layer;
      items[item_count].order_in_layer = entity->shape_renderer2d->order_in_layer;
      items[item_count].z = position.z;
      items[item_count].creation_order = entity->creation_order;
      items[item_count].kind = NEURON_SCENE_RENDER_ITEM_SHAPE;
      items[item_count].payload = entity->shape_renderer2d;
      ++item_count;
    }
    if (entity->text_renderer2d != NULL) {
      items[item_count].sorting_layer = entity->text_renderer2d->sorting_layer;
      items[item_count].order_in_layer = entity->text_renderer2d->order_in_layer;
      items[item_count].z = position.z;
      items[item_count].creation_order = entity->creation_order;
      items[item_count].kind = NEURON_SCENE_RENDER_ITEM_TEXT;
      items[item_count].payload = entity->text_renderer2d;
      ++item_count;
    }
  }

  qsort(items, item_count, sizeof(*items), neuron_graphics_scene_render_item_compare);
  for (i = 0; i < item_count; ++i) {
    if (items[i].kind == NEURON_SCENE_RENDER_ITEM_SPRITE) {
      neuron_graphics_scene_render_sprite(
          (NeuronGraphicsSpriteRenderer2D *)items[i].payload, camera,
          window_width, window_height);
    } else if (items[i].kind == NEURON_SCENE_RENDER_ITEM_SHAPE) {
      neuron_graphics_scene_render_shape(
          (NeuronGraphicsShapeRenderer2D *)items[i].payload, camera,
          window_width, window_height);
    } else if (items[i].kind == NEURON_SCENE_RENDER_ITEM_TEXT) {
      neuron_graphics_scene_render_text(
          (NeuronGraphicsTextRenderer2D *)items[i].payload, camera, window_width,
          window_height);
    }
  }

  free(items);
}
