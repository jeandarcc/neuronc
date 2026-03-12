#include "graphics/scene2d/graphics_scene2d_internal.h"

#include <stdlib.h>
#include <string.h>

static int neuron_graphics_scene_append_entity(NeuronGraphicsScene *scene,
                                               NeuronGraphicsEntity *entity) {
  if (scene == NULL || entity == NULL) {
    return 0;
  }
  if (scene->entity_count == scene->entity_capacity) {
    const size_t next_capacity =
        scene->entity_capacity == 0u ? 8u : scene->entity_capacity * 2u;
    NeuronGraphicsEntity **next =
        (NeuronGraphicsEntity **)realloc(scene->entities,
                                         next_capacity * sizeof(*next));
    if (next == NULL) {
      neuron_graphics_set_error("Out of memory growing Scene entity list");
      return 0;
    }
    scene->entities = next;
    scene->entity_capacity = next_capacity;
  }
  scene->entities[scene->entity_count++] = entity;
  return 1;
}

static void neuron_graphics_free_sprite_renderer(
    NeuronGraphicsSpriteRenderer2D *renderer) {
  if (renderer == NULL) {
    return;
  }
  neuron_graphics_mesh_free(renderer->mesh);
  neuron_graphics_material_free(renderer->material);
  neuron_graphics_sampler_free(renderer->sampler);
  free(renderer);
}

static void neuron_graphics_free_shape_renderer(
    NeuronGraphicsShapeRenderer2D *renderer) {
  if (renderer == NULL) {
    return;
  }
  neuron_graphics_mesh_free(renderer->mesh);
  neuron_graphics_material_free(renderer->material);
  free(renderer);
}

static void neuron_graphics_free_text_renderer(
    NeuronGraphicsTextRenderer2D *renderer) {
  if (renderer == NULL) {
    return;
  }
  free(renderer->text);
  neuron_graphics_mesh_free(renderer->mesh);
  neuron_graphics_material_free(renderer->material);
  free(renderer);
}

void neuron_graphics_scene2d_free_entity(NeuronGraphicsEntity *entity) {
  if (entity == NULL) {
    return;
  }
  neuron_graphics_free_sprite_renderer(entity->sprite_renderer2d);
  neuron_graphics_free_shape_renderer(entity->shape_renderer2d);
  neuron_graphics_free_text_renderer(entity->text_renderer2d);
  free(entity->camera2d);
  free(entity->name);
  free(entity);
}

NEURON_RUNTIME_API NeuronGraphicsScene *neuron_graphics_scene_create(void) {
  NeuronGraphicsScene *scene =
      (NeuronGraphicsScene *)calloc(1, sizeof(NeuronGraphicsScene));
  if (scene == NULL) {
    neuron_graphics_set_error("Out of memory allocating Scene");
    return NULL;
  }
  scene->next_creation_order = 1u;
  return scene;
}

NEURON_RUNTIME_API void neuron_graphics_scene_free(NeuronGraphicsScene *scene) {
  size_t i = 0;
  if (scene == NULL) {
    return;
  }
  for (i = 0; i < scene->entity_count; ++i) {
    neuron_graphics_scene2d_free_entity(scene->entities[i]);
  }
  free(scene->entities);
  free(scene);
}

NEURON_RUNTIME_API NeuronGraphicsEntity *
neuron_graphics_scene_create_entity(NeuronGraphicsScene *scene,
                                    const char *name) {
  NeuronGraphicsEntity *entity = NULL;
  if (scene == NULL) {
    neuron_graphics_set_error("Scene.CreateEntity requires a scene");
    return NULL;
  }
  entity = (NeuronGraphicsEntity *)calloc(1, sizeof(NeuronGraphicsEntity));
  if (entity == NULL) {
    neuron_graphics_set_error("Out of memory allocating Entity");
    return NULL;
  }
  entity->scene = scene;
  entity->creation_order = scene->next_creation_order++;
  entity->name = neuron_graphics_copy_string(name != NULL ? name : "");
  entity->transform.entity = entity;
  entity->transform.scale.x = 1.0f;
  entity->transform.scale.y = 1.0f;
  entity->transform.scale.z = 1.0f;
  if (entity->name == NULL || !neuron_graphics_scene_append_entity(scene, entity)) {
    free(entity->name);
    free(entity);
    if (entity->name == NULL) {
      neuron_graphics_set_error("Out of memory copying Entity name");
    }
    return NULL;
  }
  return entity;
}

NEURON_RUNTIME_API void
neuron_graphics_scene_destroy_entity(NeuronGraphicsScene *scene,
                                     NeuronGraphicsEntity *entity) {
  size_t i = 0;
  if (scene == NULL || entity == NULL) {
    neuron_graphics_set_error("Scene.DestroyEntity requires both scene and entity");
    return;
  }
  for (i = 0; i < scene->entity_count; ++i) {
    if (scene->entities[i] == entity) {
      memmove(&scene->entities[i], &scene->entities[i + 1],
              (scene->entity_count - i - 1u) * sizeof(scene->entities[0]));
      --scene->entity_count;
      neuron_graphics_scene2d_free_entity(entity);
      return;
    }
  }
  neuron_graphics_set_error("Scene.DestroyEntity could not find entity");
}

NEURON_RUNTIME_API NeuronGraphicsEntity *
neuron_graphics_scene_find_entity(NeuronGraphicsScene *scene, const char *name) {
  size_t i = 0;
  if (scene == NULL || name == NULL) {
    return NULL;
  }
  for (i = 0; i < scene->entity_count; ++i) {
    if (scene->entities[i] != NULL && scene->entities[i]->name != NULL &&
        strcmp(scene->entities[i]->name, name) == 0) {
      return scene->entities[i];
    }
  }
  return NULL;
}

NEURON_RUNTIME_API NeuronGraphicsTransform *
neuron_graphics_entity_get_transform(NeuronGraphicsEntity *entity) {
  if (entity == NULL) {
    neuron_graphics_set_error("Entity.GetTransform requires an entity");
    return NULL;
  }
  return &entity->transform;
}

NEURON_RUNTIME_API NeuronGraphicsCamera2D *
neuron_graphics_entity_add_camera2d(NeuronGraphicsEntity *entity) {
  if (entity == NULL) {
    neuron_graphics_set_error("Entity.AddCamera2D requires an entity");
    return NULL;
  }
  if (entity->camera2d != NULL) {
    neuron_graphics_set_error("Entity already has a Camera2D component");
    return NULL;
  }
  entity->camera2d =
      (NeuronGraphicsCamera2D *)calloc(1, sizeof(NeuronGraphicsCamera2D));
  if (entity->camera2d == NULL) {
    neuron_graphics_set_error("Out of memory allocating Camera2D");
    return NULL;
  }
  entity->camera2d->entity = entity;
  entity->camera2d->zoom = 1.0f;
  return entity->camera2d;
}

NEURON_RUNTIME_API NeuronGraphicsSpriteRenderer2D *
neuron_graphics_entity_add_sprite_renderer2d(NeuronGraphicsEntity *entity) {
  if (entity == NULL) {
    neuron_graphics_set_error("Entity.AddSpriteRenderer2D requires an entity");
    return NULL;
  }
  if (entity->sprite_renderer2d != NULL) {
    neuron_graphics_set_error("Entity already has a SpriteRenderer2D component");
    return NULL;
  }
  entity->sprite_renderer2d = (NeuronGraphicsSpriteRenderer2D *)calloc(
      1, sizeof(NeuronGraphicsSpriteRenderer2D));
  if (entity->sprite_renderer2d == NULL) {
    neuron_graphics_set_error("Out of memory allocating SpriteRenderer2D");
    return NULL;
  }
  entity->sprite_renderer2d->entity = entity;
  entity->sprite_renderer2d->color.red = 1.0f;
  entity->sprite_renderer2d->color.green = 1.0f;
  entity->sprite_renderer2d->color.blue = 1.0f;
  entity->sprite_renderer2d->color.alpha = 1.0f;
  entity->sprite_renderer2d->size.x = 100.0f;
  entity->sprite_renderer2d->size.y = 100.0f;
  entity->sprite_renderer2d->pivot.x = 0.5f;
  entity->sprite_renderer2d->pivot.y = 0.5f;
  return entity->sprite_renderer2d;
}

NEURON_RUNTIME_API NeuronGraphicsShapeRenderer2D *
neuron_graphics_entity_add_shape_renderer2d(NeuronGraphicsEntity *entity) {
  if (entity == NULL) {
    neuron_graphics_set_error("Entity.AddShapeRenderer2D requires an entity");
    return NULL;
  }
  if (entity->shape_renderer2d != NULL) {
    neuron_graphics_set_error("Entity already has a ShapeRenderer2D component");
    return NULL;
  }
  entity->shape_renderer2d = (NeuronGraphicsShapeRenderer2D *)calloc(
      1, sizeof(NeuronGraphicsShapeRenderer2D));
  if (entity->shape_renderer2d == NULL) {
    neuron_graphics_set_error("Out of memory allocating ShapeRenderer2D");
    return NULL;
  }
  entity->shape_renderer2d->entity = entity;
  entity->shape_renderer2d->color.red = 1.0f;
  entity->shape_renderer2d->color.green = 1.0f;
  entity->shape_renderer2d->color.blue = 1.0f;
  entity->shape_renderer2d->color.alpha = 1.0f;
  entity->shape_renderer2d->rectangle_size.x = 100.0f;
  entity->shape_renderer2d->rectangle_size.y = 100.0f;
  entity->shape_renderer2d->circle_radius = 50.0f;
  entity->shape_renderer2d->circle_segments = 16;
  entity->shape_renderer2d->line_thickness = 2.0f;
  entity->shape_renderer2d->filled = 1;
  return entity->shape_renderer2d;
}

NEURON_RUNTIME_API NeuronGraphicsTextRenderer2D *
neuron_graphics_entity_add_text_renderer2d(NeuronGraphicsEntity *entity) {
  if (entity == NULL) {
    neuron_graphics_set_error("Entity.AddTextRenderer2D requires an entity");
    return NULL;
  }
  if (entity->text_renderer2d != NULL) {
    neuron_graphics_set_error("Entity already has a TextRenderer2D component");
    return NULL;
  }
  entity->text_renderer2d = (NeuronGraphicsTextRenderer2D *)calloc(
      1, sizeof(NeuronGraphicsTextRenderer2D));
  if (entity->text_renderer2d == NULL) {
    neuron_graphics_set_error("Out of memory allocating TextRenderer2D");
    return NULL;
  }
  entity->text_renderer2d->entity = entity;
  entity->text_renderer2d->font_size = 24.0f;
  entity->text_renderer2d->color.red = 1.0f;
  entity->text_renderer2d->color.green = 1.0f;
  entity->text_renderer2d->color.blue = 1.0f;
  entity->text_renderer2d->color.alpha = 1.0f;
  entity->text_renderer2d->text = neuron_graphics_copy_string("");
  return entity->text_renderer2d;
}

static int neuron_graphics_transform_would_cycle(NeuronGraphicsEntity *child,
                                                 NeuronGraphicsEntity *parent) {
  NeuronGraphicsEntity *cursor = parent;
  while (cursor != NULL) {
    if (cursor == child) {
      return 1;
    }
    cursor = cursor->transform.parent;
  }
  return 0;
}

NEURON_RUNTIME_API void neuron_graphics_transform_set_parent(
    NeuronGraphicsTransform *transform, NeuronGraphicsEntity *parent) {
  if (transform == NULL) {
    neuron_graphics_set_error("Transform.SetParent requires a transform");
    return;
  }
  if (parent != NULL && transform->entity != NULL &&
      neuron_graphics_transform_would_cycle(transform->entity, parent)) {
    neuron_graphics_set_error("Transform.SetParent would create a cycle");
    return;
  }
  transform->parent = parent;
}

NEURON_RUNTIME_API void neuron_graphics_transform_set_position(
    NeuronGraphicsTransform *transform, NeuronGraphicsVector3 *value) {
  if (transform == NULL || value == NULL) {
    neuron_graphics_set_error("Transform.SetPosition requires both transform and Vector3");
    return;
  }
  transform->position = *value;
}

NEURON_RUNTIME_API void neuron_graphics_transform_set_rotation(
    NeuronGraphicsTransform *transform, NeuronGraphicsVector3 *value) {
  if (transform == NULL || value == NULL) {
    neuron_graphics_set_error("Transform.SetRotation requires both transform and Vector3");
    return;
  }
  transform->rotation = *value;
}

NEURON_RUNTIME_API void neuron_graphics_transform_set_scale(
    NeuronGraphicsTransform *transform, NeuronGraphicsVector3 *value) {
  if (transform == NULL || value == NULL) {
    neuron_graphics_set_error("Transform.SetScale requires both transform and Vector3");
    return;
  }
  transform->scale = *value;
}

NEURON_RUNTIME_API void neuron_graphics_camera2d_set_zoom(
    NeuronGraphicsCamera2D *camera, double value) {
  if (camera == NULL) {
    neuron_graphics_set_error("Camera2D.SetZoom requires a camera");
    return;
  }
  camera->zoom = value > 0.0 ? (float)value : 1.0f;
}

NEURON_RUNTIME_API void neuron_graphics_camera2d_set_primary(
    NeuronGraphicsCamera2D *camera, int32_t value) {
  if (camera == NULL) {
    neuron_graphics_set_error("Camera2D.SetPrimary requires a camera");
    return;
  }
  camera->primary = value != 0;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_texture(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsTexture *texture) {
  if (renderer == NULL || texture == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetTexture requires both renderer and texture");
    return;
  }
  renderer->texture = texture;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_color(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsColor *color) {
  if (renderer == NULL || color == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetColor requires both renderer and color");
    return;
  }
  renderer->color = *color;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_size(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsVector2 *size) {
  if (renderer == NULL || size == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetSize requires both renderer and Vector2");
    return;
  }
  renderer->size = *size;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_pivot(
    NeuronGraphicsSpriteRenderer2D *renderer, NeuronGraphicsVector2 *pivot) {
  if (renderer == NULL || pivot == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetPivot requires both renderer and Vector2");
    return;
  }
  renderer->pivot = *pivot;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_flip_x(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetFlipX requires a renderer");
    return;
  }
  renderer->flip_x = value != 0;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_flip_y(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetFlipY requires a renderer");
    return;
  }
  renderer->flip_y = value != 0;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_sorting_layer(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetSortingLayer requires a renderer");
    return;
  }
  renderer->sorting_layer = value;
}

NEURON_RUNTIME_API void neuron_graphics_sprite_renderer2d_set_order_in_layer(
    NeuronGraphicsSpriteRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("SpriteRenderer2D.SetOrderInLayer requires a renderer");
    return;
  }
  renderer->order_in_layer = value;
}

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_rectangle(
    NeuronGraphicsShapeRenderer2D *renderer, NeuronGraphicsVector2 *size) {
  if (renderer == NULL || size == NULL) {
    neuron_graphics_set_error("ShapeRenderer2D.SetRectangle requires both renderer and Vector2");
    return;
  }
  renderer->mode = NEURON_GRAPHICS_SHAPE_MODE_RECTANGLE;
  renderer->rectangle_size = *size;
}

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_circle(
    NeuronGraphicsShapeRenderer2D *renderer, double radius, int32_t segments) {
  if (renderer == NULL) {
    neuron_graphics_set_error("ShapeRenderer2D.SetCircle requires a renderer");
    return;
  }
  renderer->mode = NEURON_GRAPHICS_SHAPE_MODE_CIRCLE;
  renderer->circle_radius = (float)radius;
  renderer->circle_segments = segments > 2 ? segments : 16;
}

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_line(
    NeuronGraphicsShapeRenderer2D *renderer, NeuronGraphicsVector2 *start,
    NeuronGraphicsVector2 *end, double thickness) {
  if (renderer == NULL || start == NULL || end == NULL) {
    neuron_graphics_set_error("ShapeRenderer2D.SetLine requires renderer and both Vector2 endpoints");
    return;
  }
  renderer->mode = NEURON_GRAPHICS_SHAPE_MODE_LINE;
  renderer->line_start = *start;
  renderer->line_end = *end;
  renderer->line_thickness = (float)thickness;
}

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_color(
    NeuronGraphicsShapeRenderer2D *renderer, NeuronGraphicsColor *color) {
  if (renderer == NULL || color == NULL) {
    neuron_graphics_set_error("ShapeRenderer2D.SetColor requires both renderer and color");
    return;
  }
  renderer->color = *color;
}

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_filled(
    NeuronGraphicsShapeRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("ShapeRenderer2D.SetFilled requires a renderer");
    return;
  }
  renderer->filled = value != 0;
}

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_sorting_layer(
    NeuronGraphicsShapeRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("ShapeRenderer2D.SetSortingLayer requires a renderer");
    return;
  }
  renderer->sorting_layer = value;
}

NEURON_RUNTIME_API void neuron_graphics_shape_renderer2d_set_order_in_layer(
    NeuronGraphicsShapeRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("ShapeRenderer2D.SetOrderInLayer requires a renderer");
    return;
  }
  renderer->order_in_layer = value;
}

NEURON_RUNTIME_API NeuronGraphicsFont *neuron_graphics_font_load(
    const char *path) {
  NeuronGraphicsFont *font = NULL;
  if (path == NULL || *path == '\0') {
    neuron_graphics_set_error("Font.Load requires a valid file path");
    return NULL;
  }
  font = (NeuronGraphicsFont *)calloc(1, sizeof(NeuronGraphicsFont));
  if (font == NULL) {
    neuron_graphics_set_error("Out of memory allocating Font");
    return NULL;
  }
  font->path = neuron_graphics_copy_string(path);
  if (font->path == NULL) {
    free(font);
    neuron_graphics_set_error("Out of memory copying Font path");
    return NULL;
  }
  return font;
}

NEURON_RUNTIME_API void neuron_graphics_font_free(NeuronGraphicsFont *font) {
  if (font == NULL) {
    return;
  }
  free(font->path);
  free(font);
}

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_font(
    NeuronGraphicsTextRenderer2D *renderer, NeuronGraphicsFont *font) {
  if (renderer == NULL || font == NULL) {
    neuron_graphics_set_error("TextRenderer2D.SetFont requires both renderer and font");
    return;
  }
  renderer->font = font;
}

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_text(
    NeuronGraphicsTextRenderer2D *renderer, const char *text) {
  char *copy = NULL;
  if (renderer == NULL || text == NULL) {
    neuron_graphics_set_error("TextRenderer2D.SetText requires both renderer and text");
    return;
  }
  copy = neuron_graphics_copy_string(text);
  if (copy == NULL) {
    neuron_graphics_set_error("Out of memory copying text");
    return;
  }
  free(renderer->text);
  renderer->text = copy;
}

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_font_size(
    NeuronGraphicsTextRenderer2D *renderer, double value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("TextRenderer2D.SetFontSize requires a renderer");
    return;
  }
  renderer->font_size = value > 0.0 ? (float)value : renderer->font_size;
}

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_color(
    NeuronGraphicsTextRenderer2D *renderer, NeuronGraphicsColor *color) {
  if (renderer == NULL || color == NULL) {
    neuron_graphics_set_error("TextRenderer2D.SetColor requires both renderer and color");
    return;
  }
  renderer->color = *color;
}

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_alignment(
    NeuronGraphicsTextRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("TextRenderer2D.SetAlignment requires a renderer");
    return;
  }
  renderer->alignment = value;
}

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_sorting_layer(
    NeuronGraphicsTextRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("TextRenderer2D.SetSortingLayer requires a renderer");
    return;
  }
  renderer->sorting_layer = value;
}

NEURON_RUNTIME_API void neuron_graphics_text_renderer2d_set_order_in_layer(
    NeuronGraphicsTextRenderer2D *renderer, int32_t value) {
  if (renderer == NULL) {
    neuron_graphics_set_error("TextRenderer2D.SetOrderInLayer requires a renderer");
    return;
  }
  renderer->order_in_layer = value;
}
