// Graphics runtime tests - included from tests/test_main.cpp
#include "neuron_graphics.h"

#include <string>

namespace {

const NeuronGraphicsShaderBindingDescriptor kBasicLitBindings[] = {
    {"tint", NEURON_GRAPHICS_SHADER_BINDING_VEC4, 0, 0, 0, 16},
};

const NeuronGraphicsShaderDescriptor kBasicLitShader = {
    "BasicLit",
    NEURON_GRAPHICS_SHADER_STAGE_VERTEX |
        NEURON_GRAPHICS_SHADER_STAGE_FRAGMENT,
    1,
    kBasicLitBindings,
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

const NeuronGraphicsShaderBindingDescriptor kTexturedBindings[] = {
    {"tint", NEURON_GRAPHICS_SHADER_BINDING_VEC4, 0, 0, 0, 16},
    {"albedo", NEURON_GRAPHICS_SHADER_BINDING_TEXTURE2D, 1, 1, UINT32_MAX, 0},
    {"linearSampler", NEURON_GRAPHICS_SHADER_BINDING_SAMPLER, 2, 2, UINT32_MAX,
     0},
};

const NeuronGraphicsShaderDescriptor kTexturedShader = {
    "TexturedQuad",
    NEURON_GRAPHICS_SHADER_STAGE_VERTEX |
        NEURON_GRAPHICS_SHADER_STAGE_FRAGMENT,
    3,
    kTexturedBindings,
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

} // namespace

TEST(GraphicsLifecycleApiIsCallable) {
  NeuronGraphicsWindow *window =
      neuron_graphics_create_window(320, 240, "graphics-test");
  ASSERT_TRUE(window != nullptr);

  NeuronGraphicsCanvas *canvas = neuron_graphics_create_canvas(window);
  ASSERT_TRUE(canvas != nullptr);

  (void)neuron_graphics_canvas_pump(canvas);
  const int32_t closeState = neuron_graphics_canvas_should_close(canvas);
  ASSERT_TRUE(closeState == 0 || closeState == 1);
  const int32_t resizeState = neuron_graphics_canvas_take_resize(canvas);
  ASSERT_TRUE(resizeState == 0 || resizeState == 1);

  NeuronGraphicsMesh *mesh =
      neuron_graphics_mesh_load("examples/assets/triangle.obj");
  ASSERT_TRUE(mesh != nullptr);

  NeuronGraphicsMaterial *material =
      neuron_graphics_material_create(&kBasicLitShader);
  ASSERT_TRUE(material != nullptr);

  NeuronGraphicsColor *drawColor =
      neuron_graphics_color_rgba(1.0, 0.15, 0.10, 1.0);
  ASSERT_TRUE(drawColor != nullptr);
  neuron_graphics_material_set_vec4(material, "tint", drawColor);

  neuron_graphics_canvas_begin_frame(canvas);
  neuron_graphics_clear(drawColor);
  neuron_graphics_draw_indexed(mesh, material);
  neuron_graphics_present();
  neuron_graphics_canvas_end_frame(canvas);

  neuron_graphics_color_free(drawColor);
  neuron_graphics_material_free(material);
  neuron_graphics_mesh_free(mesh);
  neuron_graphics_canvas_free(canvas);
  neuron_graphics_window_free(window);
  return true;
}

TEST(GraphicsNullAssetPathsAreRejected) {
  ASSERT_TRUE(neuron_graphics_texture_load(nullptr) == nullptr);
  ASSERT_TRUE(neuron_graphics_mesh_load(nullptr) == nullptr);
  return true;
}

TEST(GraphicsLastErrorReportsFailures) {
  NeuronGraphicsCanvas *canvas = neuron_graphics_create_canvas(nullptr);
  ASSERT_TRUE(canvas == nullptr);
  const char *error = neuron_graphics_last_error();
  ASSERT_TRUE(error != nullptr);
  ASSERT_FALSE(std::string(error).empty());
  return true;
}

TEST(GraphicsMaterialAndColorApisAreCallable) {
  NeuronGraphicsMaterial *material =
      neuron_graphics_material_create(&kBasicLitShader);
  ASSERT_TRUE(material != nullptr);

  NeuronGraphicsColor *color =
      neuron_graphics_color_rgba(0.08, 0.08, 0.10, 1.0);
  ASSERT_TRUE(color != nullptr);

  neuron_graphics_color_free(color);
  neuron_graphics_material_free(material);
  return true;
}

TEST(GraphicsDrawRequiresActiveFrameAndExplicitResources) {
  NeuronGraphicsMesh *mesh =
      neuron_graphics_mesh_load("examples/assets/triangle.obj");
  ASSERT_TRUE(mesh != nullptr);

  NeuronGraphicsMaterial *material =
      neuron_graphics_material_create(&kBasicLitShader);
  ASSERT_TRUE(material != nullptr);

  neuron_graphics_draw(mesh, material);
  const char *error = neuron_graphics_last_error();
  ASSERT_TRUE(error != nullptr);
  ASSERT_TRUE(std::string(error).find("active canvas frame") != std::string::npos);

  neuron_graphics_material_free(material);
  neuron_graphics_mesh_free(mesh);
  return true;
}

TEST(GraphicsStaticMeshTextureAndSamplerApisAreCallable) {
  const NeuronGraphicsVertex quad_vertices[] = {
      {-0.8f, -0.8f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f},
      {0.8f, -0.8f, 0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f},
      {0.8f, 0.8f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f},
      {-0.8f, 0.8f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f},
  };
  const uint32_t quad_indices[] = {0u, 1u, 2u, 0u, 2u, 3u};

  NeuronGraphicsMesh *mesh = neuron_graphics_mesh_create_static(
      quad_vertices, 4, quad_indices, 6);
  ASSERT_TRUE(mesh != nullptr);

  NeuronGraphicsMaterial *material =
      neuron_graphics_material_create(&kTexturedShader);
  ASSERT_TRUE(material != nullptr);
  NeuronGraphicsTexture *texture =
      neuron_graphics_texture_load("examples/assets/checker.png");
  ASSERT_TRUE(texture != nullptr);
  NeuronGraphicsSampler *sampler = neuron_graphics_sampler_create();
  ASSERT_TRUE(sampler != nullptr);
  NeuronGraphicsColor *tint =
      neuron_graphics_color_rgba(1.0, 1.0, 1.0, 1.0);
  ASSERT_TRUE(tint != nullptr);

  neuron_graphics_material_set_vec4(material, "tint", tint);
  neuron_graphics_material_set_texture(material, "albedo", texture);
  neuron_graphics_material_set_sampler(material, "linearSampler", sampler);

  neuron_graphics_color_free(tint);
  neuron_graphics_sampler_free(sampler);
  neuron_graphics_texture_free(texture);
  neuron_graphics_material_free(material);
  neuron_graphics_mesh_free(mesh);
  return true;
}

TEST(GraphicsScene2DApisManageEntitiesAndComponents) {
  NeuronGraphicsScene *scene = neuron_graphics_scene_create();
  ASSERT_TRUE(scene != nullptr);

  NeuronGraphicsEntity *entity =
      neuron_graphics_scene_create_entity(scene, "Player");
  ASSERT_TRUE(entity != nullptr);
  ASSERT_TRUE(neuron_graphics_scene_find_entity(scene, "Player") == entity);

  NeuronGraphicsTransform *transform =
      neuron_graphics_entity_get_transform(entity);
  ASSERT_TRUE(transform != nullptr);

  NeuronGraphicsSpriteRenderer2D *sprite =
      neuron_graphics_entity_add_sprite_renderer2d(entity);
  ASSERT_TRUE(sprite != nullptr);
  ASSERT_TRUE(neuron_graphics_entity_add_sprite_renderer2d(entity) == nullptr);
  const char *duplicateError = neuron_graphics_last_error();
  ASSERT_TRUE(duplicateError != nullptr);
  ASSERT_TRUE(std::string(duplicateError).find("already has") !=
              std::string::npos);

  neuron_graphics_scene_destroy_entity(scene, entity);
  ASSERT_TRUE(neuron_graphics_scene_find_entity(scene, "Player") == nullptr);
  neuron_graphics_scene_free(scene);
  return true;
}

TEST(GraphicsScene2DRejectsTransformCyclesAtRuntime) {
  NeuronGraphicsScene *scene = neuron_graphics_scene_create();
  ASSERT_TRUE(scene != nullptr);
  NeuronGraphicsEntity *parent =
      neuron_graphics_scene_create_entity(scene, "Parent");
  NeuronGraphicsEntity *child =
      neuron_graphics_scene_create_entity(scene, "Child");
  ASSERT_TRUE(parent != nullptr);
  ASSERT_TRUE(child != nullptr);

  NeuronGraphicsTransform *parentTransform =
      neuron_graphics_entity_get_transform(parent);
  NeuronGraphicsTransform *childTransform =
      neuron_graphics_entity_get_transform(child);
  ASSERT_TRUE(parentTransform != nullptr);
  ASSERT_TRUE(childTransform != nullptr);

  neuron_graphics_transform_set_parent(childTransform, parent);
  neuron_graphics_transform_set_parent(parentTransform, child);
  const char *cycleError = neuron_graphics_last_error();
  ASSERT_TRUE(cycleError != nullptr);
  ASSERT_TRUE(std::string(cycleError).find("cycle") != std::string::npos);

  neuron_graphics_scene_free(scene);
  return true;
}

TEST(GraphicsRenderer2DRenderWorksInsideActiveFrame) {
  NeuronGraphicsWindow *window =
      neuron_graphics_create_window(320, 240, "scene2d-frame");
  ASSERT_TRUE(window != nullptr);
  NeuronGraphicsCanvas *canvas = neuron_graphics_create_canvas(window);
  ASSERT_TRUE(canvas != nullptr);
  NeuronGraphicsScene *scene = neuron_graphics_scene_create();
  ASSERT_TRUE(scene != nullptr);
  NeuronGraphicsRenderer2D *renderer = neuron_graphics_renderer2d_create();
  ASSERT_TRUE(renderer != nullptr);
  NeuronGraphicsColor *clear =
      neuron_graphics_color_rgba(0.02, 0.03, 0.04, 1.0);
  ASSERT_TRUE(clear != nullptr);

  neuron_graphics_renderer2d_set_clear_color(renderer, clear);
  neuron_graphics_canvas_begin_frame(canvas);
  neuron_graphics_renderer2d_render(renderer, scene);
  neuron_graphics_canvas_end_frame(canvas);

  const char *error = neuron_graphics_last_error();
  ASSERT_TRUE(error == nullptr || std::string(error).empty());

  neuron_graphics_color_free(clear);
  neuron_graphics_renderer2d_free(renderer);
  neuron_graphics_scene_free(scene);
  neuron_graphics_canvas_free(canvas);
  neuron_graphics_window_free(window);
  return true;
}
