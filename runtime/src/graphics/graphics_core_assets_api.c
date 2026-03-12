#include "graphics/assets/graphics_asset_internal.h"
#include "graphics/backend/graphics_backend_internal.h"
#include "graphics/graphics_core_internal.h"
#include "graphics_assets_internal.h"

#include <stdlib.h>
#include <string.h>

static int neuron_graphics_canvas_append_draw(
    NeuronGraphicsCanvas *canvas, NeuronGraphicsDrawKind kind,
    NeuronGraphicsMesh *mesh, NeuronGraphicsMaterial *material,
    int32_t instance_count) {
  if (canvas == NULL) {
    return 0;
  }
  if (canvas->draw_command_count == canvas->draw_command_capacity) {
    size_t next_capacity =
        canvas->draw_command_capacity == 0 ? 8u : canvas->draw_command_capacity * 2u;
    NeuronGraphicsDrawCommand *next =
        (NeuronGraphicsDrawCommand *)realloc(
            canvas->draw_commands,
            next_capacity * sizeof(NeuronGraphicsDrawCommand));
    if (next == NULL) {
      neuron_graphics_set_error("Out of memory growing draw command buffer");
      return 0;
    }
    canvas->draw_commands = next;
    canvas->draw_command_capacity = next_capacity;
  }

  NeuronGraphicsDrawCommand *command =
      &canvas->draw_commands[canvas->draw_command_count++];
  memset(command, 0, sizeof(*command));
  command->kind = kind;
  command->mesh = mesh;
  command->material = material;
  command->instance_count = instance_count;
  return 1;
}

static const NeuronGraphicsShaderBindingDescriptor *
neuron_graphics_find_shader_binding(
    const NeuronGraphicsShaderDescriptor *shader_descriptor,
    const char *binding_name) {
  uint32_t i = 0;
  if (shader_descriptor == NULL || binding_name == NULL || *binding_name == '\0') {
    return NULL;
  }

  for (i = 0; i < shader_descriptor->binding_count; ++i) {
    const NeuronGraphicsShaderBindingDescriptor *binding =
        &shader_descriptor->bindings[i];
    if (binding->name != NULL && strcmp(binding->name, binding_name) == 0) {
      return binding;
    }
  }
  return NULL;
}

static NeuronGraphicsMaterialBinding *
neuron_graphics_find_material_binding(NeuronGraphicsMaterial *material,
                                      const char *binding_name,
                                      uint32_t expected_kind,
                                      const char *api_name) {
  const NeuronGraphicsShaderBindingDescriptor *binding_descriptor = NULL;
  if (material == NULL) {
    neuron_graphics_set_error("%s requires a material instance", api_name);
    return NULL;
  }
  if (binding_name == NULL || *binding_name == '\0') {
    neuron_graphics_set_error("%s requires a binding name", api_name);
    return NULL;
  }
  if (material->shader_descriptor == NULL) {
    neuron_graphics_set_error("%s requires a material with a shader descriptor",
                              api_name);
    return NULL;
  }

  binding_descriptor = neuron_graphics_find_shader_binding(
      material->shader_descriptor, binding_name);
  if (binding_descriptor == NULL) {
    neuron_graphics_set_error("Binding '%s' is not declared on shader '%s'",
                              binding_name, material->shader_descriptor->name);
    return NULL;
  }
  if (expected_kind != NEURON_GRAPHICS_SHADER_BINDING_UNKNOWN &&
      binding_descriptor->kind != expected_kind) {
    neuron_graphics_set_error(
        "Binding '%s' on shader '%s' has incompatible kind %u for %s",
        binding_name, material->shader_descriptor->name,
        (unsigned)binding_descriptor->kind, api_name);
    return NULL;
  }
  if (binding_descriptor->slot >= material->binding_count) {
    neuron_graphics_set_error("Binding '%s' resolved to invalid slot %u",
                              binding_name, (unsigned)binding_descriptor->slot);
    return NULL;
  }

  return &material->bindings[binding_descriptor->slot];
}

static int neuron_graphics_copy_vertices(NeuronGraphicsMesh *mesh,
                                         const NeuronGraphicsVertex *vertices,
                                         int32_t vertex_count) {
  size_t byte_count = 0;
  if (mesh == NULL || vertices == NULL || vertex_count <= 0) {
    return 0;
  }

  byte_count = (size_t)vertex_count * sizeof(NeuronGraphicsVertex);
  mesh->vertices = (NeuronGraphicsVertex *)malloc(byte_count);
  if (mesh->vertices == NULL) {
    neuron_graphics_set_error("Out of memory allocating mesh vertices");
    return 0;
  }
  memcpy(mesh->vertices, vertices, byte_count);
  mesh->vertex_count = (size_t)vertex_count;
  return 1;
}

static int neuron_graphics_copy_indices(NeuronGraphicsMesh *mesh,
                                        const uint32_t *indices,
                                        int32_t index_count) {
  size_t byte_count = 0;
  if (mesh == NULL || indices == NULL || index_count <= 0) {
    return 1;
  }

  byte_count = (size_t)index_count * sizeof(uint32_t);
  mesh->indices = (uint32_t *)malloc(byte_count);
  if (mesh->indices == NULL) {
    neuron_graphics_set_error("Out of memory allocating mesh indices");
    return 0;
  }
  memcpy(mesh->indices, indices, byte_count);
  mesh->index_count = (size_t)index_count;
  return 1;
}

static void neuron_graphics_write_matrix4(float *target, const float *source) {
  if (target == NULL || source == NULL) {
    return;
  }
  memcpy(target, source, 16u * sizeof(float));
}

static void neuron_graphics_write_identity_matrix4(float *target) {
  if (target == NULL) {
    return;
  }
  memset(target, 0, 16u * sizeof(float));
  target[0] = 1.0f;
  target[5] = 1.0f;
  target[10] = 1.0f;
  target[15] = 1.0f;
}

NeuronGraphicsTexture *npp_graphics_core_texture_load(const char *path) {
  NeuronGraphicsTexture *texture = NULL;
  if (path == NULL || *path == '\0') {
    neuron_graphics_set_error("Texture2D.Load requires a valid file path");
    return NULL;
  }

  texture = (NeuronGraphicsTexture *)calloc(1, sizeof(NeuronGraphicsTexture));
  if (texture == NULL) {
    neuron_graphics_set_error("Out of memory allocating texture");
    return NULL;
  }

  texture->path = neuron_graphics_copy_string(path);
  if (texture->path == NULL) {
    free(texture);
    neuron_graphics_set_error("Out of memory copying texture path");
    return NULL;
  }

#if defined(_WIN32)
  if (!neuron_graphics_load_png_wic(path, &texture->pixels, &texture->width,
                                    &texture->height)) {
    free(texture->path);
    free(texture);
    return NULL;
  }
#endif

  return texture;
}

void npp_graphics_core_texture_free(NeuronGraphicsTexture *texture) {
  if (texture == NULL) {
    return;
  }
  free(texture->pixels);
  free(texture->path);
  free(texture);
}

NeuronGraphicsSampler *npp_graphics_core_sampler_create(void) {
  NeuronGraphicsSampler *sampler =
      (NeuronGraphicsSampler *)calloc(1, sizeof(NeuronGraphicsSampler));
  if (sampler == NULL) {
    neuron_graphics_set_error("Out of memory allocating sampler");
    return NULL;
  }
  return sampler;
}

void npp_graphics_core_sampler_free(NeuronGraphicsSampler *sampler) {
  if (sampler == NULL) {
    return;
  }
  free(sampler);
}

NeuronGraphicsMesh *npp_graphics_core_mesh_load(const char *path) {
  NeuronGraphicsMesh *mesh = NULL;
  if (path == NULL || *path == '\0') {
    neuron_graphics_set_error("Mesh.Load requires a valid file path");
    return NULL;
  }

  mesh = (NeuronGraphicsMesh *)calloc(1, sizeof(NeuronGraphicsMesh));
  if (mesh == NULL) {
    neuron_graphics_set_error("Out of memory allocating mesh");
    return NULL;
  }

  mesh->path = neuron_graphics_copy_string(path);
  if (mesh->path == NULL) {
    free(mesh);
    neuron_graphics_set_error("Out of memory copying mesh path");
    return NULL;
  }

  if (!neuron_graphics_load_obj_mesh(path, mesh)) {
    free(mesh->path);
    free(mesh);
    return NULL;
  }
  return mesh;
}

NeuronGraphicsMesh *npp_graphics_core_mesh_create_static(
    const NeuronGraphicsVertex *vertices, int32_t vertex_count,
    const uint32_t *indices, int32_t index_count) {
  NeuronGraphicsMesh *mesh = NULL;
  if (vertices == NULL || vertex_count <= 0) {
    neuron_graphics_set_error(
        "Mesh.CreateStatic requires at least one vertex");
    return NULL;
  }

  mesh = (NeuronGraphicsMesh *)calloc(1, sizeof(NeuronGraphicsMesh));
  if (mesh == NULL) {
    neuron_graphics_set_error("Out of memory allocating mesh");
    return NULL;
  }

  if (!neuron_graphics_copy_vertices(mesh, vertices, vertex_count) ||
      !neuron_graphics_copy_indices(mesh, indices, index_count)) {
    npp_graphics_core_mesh_free(mesh);
    return NULL;
  }
  return mesh;
}

void npp_graphics_core_mesh_free(NeuronGraphicsMesh *mesh) {
  if (mesh == NULL) {
    return;
  }
  free(mesh->vertices);
  free(mesh->indices);
  free(mesh->path);
  free(mesh);
}

NeuronGraphicsMaterial *npp_graphics_core_material_create(
    const NeuronGraphicsShaderDescriptor *shader_descriptor) {
  NeuronGraphicsMaterial *material = NULL;
  uint32_t i = 0;
  if (shader_descriptor == NULL || shader_descriptor->name == NULL ||
      (shader_descriptor->binding_count > 0 &&
       shader_descriptor->bindings == NULL)) {
    neuron_graphics_set_error(
        "Material.Create requires a valid shader descriptor");
    return NULL;
  }

  material = (NeuronGraphicsMaterial *)calloc(1, sizeof(NeuronGraphicsMaterial));
  if (material == NULL) {
    neuron_graphics_set_error("Out of memory allocating material");
    return NULL;
  }

  material->shader_descriptor = shader_descriptor;
  material->binding_count = shader_descriptor->binding_count;
  material->uniform_data_size = shader_descriptor->uniform_buffer_size;
  if (material->binding_count > 0) {
    material->bindings = (NeuronGraphicsMaterialBinding *)calloc(
        material->binding_count, sizeof(NeuronGraphicsMaterialBinding));
    if (material->bindings == NULL) {
      free(material);
      neuron_graphics_set_error("Out of memory allocating material bindings");
      return NULL;
    }

    for (i = 0; i < material->binding_count; ++i) {
      material->bindings[i].descriptor = &shader_descriptor->bindings[i];
      material->bindings[i].vec4_value.red = 1.0f;
      material->bindings[i].vec4_value.green = 1.0f;
      material->bindings[i].vec4_value.blue = 1.0f;
      material->bindings[i].vec4_value.alpha = 1.0f;
      neuron_graphics_write_identity_matrix4(material->bindings[i].matrix4_value);
    }
  }

  if (material->uniform_data_size > 0) {
    material->uniform_data = (uint8_t *)calloc(1, material->uniform_data_size);
    if (material->uniform_data == NULL) {
      npp_graphics_core_material_free(material);
      neuron_graphics_set_error("Out of memory allocating material uniform data");
      return NULL;
    }
    if (shader_descriptor->mvp_offset != UINT32_MAX &&
        shader_descriptor->mvp_offset + 16u * sizeof(float) <=
            material->uniform_data_size) {
      neuron_graphics_write_identity_matrix4(
          (float *)(material->uniform_data + shader_descriptor->mvp_offset));
    }
  }

  return material;
}

void npp_graphics_core_material_free(NeuronGraphicsMaterial *material) {
  if (material == NULL) {
    return;
  }
  free(material->uniform_data);
  free(material->bindings);
  free(material);
}

void npp_graphics_core_material_set_vec4(NeuronGraphicsMaterial *material,
                                         const char *binding_name,
                                         void *color_token) {
  NeuronGraphicsMaterialBinding *binding = NULL;
  if (color_token == NULL) {
    neuron_graphics_set_error("Material.SetVec4 requires a color value");
    return;
  }

  binding = neuron_graphics_find_material_binding(
      material, binding_name, NEURON_GRAPHICS_SHADER_BINDING_VEC4,
      "Material.SetVec4");
  if (binding == NULL) {
    return;
  }

  binding->vec4_value = *(NeuronGraphicsColor *)color_token;
  binding->has_vec4 = 1;
  if (material->uniform_data != NULL && binding->descriptor != NULL &&
      binding->descriptor->uniform_offset != UINT32_MAX &&
      binding->descriptor->uniform_offset + 16u <= material->uniform_data_size) {
    float *dst = (float *)(material->uniform_data + binding->descriptor->uniform_offset);
    dst[0] = binding->vec4_value.red;
    dst[1] = binding->vec4_value.green;
    dst[2] = binding->vec4_value.blue;
    dst[3] = binding->vec4_value.alpha;
  }
}

void npp_graphics_core_material_set_texture(NeuronGraphicsMaterial *material,
                                            const char *binding_name,
                                            NeuronGraphicsTexture *texture) {
  NeuronGraphicsMaterialBinding *binding = NULL;
  if (texture == NULL) {
    neuron_graphics_set_error("Material.SetTexture requires a texture");
    return;
  }

  binding = neuron_graphics_find_material_binding(
      material, binding_name, NEURON_GRAPHICS_SHADER_BINDING_TEXTURE2D,
      "Material.SetTexture");
  if (binding == NULL) {
    return;
  }

  binding->texture_value = texture;
}

void npp_graphics_core_material_set_sampler(NeuronGraphicsMaterial *material,
                                            const char *binding_name,
                                            NeuronGraphicsSampler *sampler) {
  NeuronGraphicsMaterialBinding *binding = NULL;
  if (sampler == NULL) {
    neuron_graphics_set_error("Material.SetSampler requires a sampler");
    return;
  }

  binding = neuron_graphics_find_material_binding(
      material, binding_name, NEURON_GRAPHICS_SHADER_BINDING_SAMPLER,
      "Material.SetSampler");
  if (binding == NULL) {
    return;
  }

  binding->sampler_value = sampler;
}

void npp_graphics_core_material_set_matrix4(NeuronGraphicsMaterial *material,
                                            const char *binding_name,
                                            const float *matrix4_values) {
  NeuronGraphicsMaterialBinding *binding = NULL;
  if (matrix4_values == NULL) {
    neuron_graphics_set_error("Material.SetMatrix4 requires matrix data");
    return;
  }

  binding = neuron_graphics_find_material_binding(
      material, binding_name, NEURON_GRAPHICS_SHADER_BINDING_MATRIX4,
      "Material.SetMatrix4");
  if (binding == NULL) {
    return;
  }

  neuron_graphics_write_matrix4(binding->matrix4_value, matrix4_values);
  binding->has_matrix4 = 1;
  if (material->uniform_data != NULL && binding->descriptor != NULL &&
      binding->descriptor->uniform_offset != UINT32_MAX &&
      binding->descriptor->uniform_offset + 16u * sizeof(float) <=
          material->uniform_data_size) {
    neuron_graphics_write_matrix4(
        (float *)(material->uniform_data + binding->descriptor->uniform_offset),
        matrix4_values);
  }
}

NeuronGraphicsColor *npp_graphics_core_color_rgba(double red, double green,
                                                  double blue, double alpha) {
  NeuronGraphicsColor *color =
      (NeuronGraphicsColor *)calloc(1, sizeof(NeuronGraphicsColor));
  if (color == NULL) {
    neuron_graphics_set_error("Out of memory allocating color");
    return NULL;
  }
  color->red = (float)red;
  color->green = (float)green;
  color->blue = (float)blue;
  color->alpha = (float)alpha;
  return color;
}

void npp_graphics_core_color_free(NeuronGraphicsColor *color) { free(color); }

NeuronGraphicsVector2 *npp_graphics_core_vector2_create(double x, double y) {
  NeuronGraphicsVector2 *value =
      (NeuronGraphicsVector2 *)calloc(1, sizeof(NeuronGraphicsVector2));
  if (value == NULL) {
    neuron_graphics_set_error("Out of memory allocating Vector2");
    return NULL;
  }
  value->x = (float)x;
  value->y = (float)y;
  return value;
}

void npp_graphics_core_vector2_free(NeuronGraphicsVector2 *value) { free(value); }

NeuronGraphicsVector3 *npp_graphics_core_vector3_create(double x, double y,
                                                        double z) {
  NeuronGraphicsVector3 *value =
      (NeuronGraphicsVector3 *)calloc(1, sizeof(NeuronGraphicsVector3));
  if (value == NULL) {
    neuron_graphics_set_error("Out of memory allocating Vector3");
    return NULL;
  }
  value->x = (float)x;
  value->y = (float)y;
  value->z = (float)z;
  return value;
}

void npp_graphics_core_vector3_free(NeuronGraphicsVector3 *value) { free(value); }

NeuronGraphicsVector4 *npp_graphics_core_vector4_create(double x, double y,
                                                        double z, double w) {
  NeuronGraphicsVector4 *value =
      (NeuronGraphicsVector4 *)calloc(1, sizeof(NeuronGraphicsVector4));
  if (value == NULL) {
    neuron_graphics_set_error("Out of memory allocating Vector4");
    return NULL;
  }
  value->x = (float)x;
  value->y = (float)y;
  value->z = (float)z;
  value->w = (float)w;
  return value;
}

void npp_graphics_core_vector4_free(NeuronGraphicsVector4 *value) { free(value); }

void npp_graphics_core_draw(void *target, void *shader) {
  if (g_active_canvas == NULL || !g_active_canvas->frame_active) {
    neuron_graphics_set_error("cmd.Draw requires an active canvas frame");
    return;
  }
  if (target == NULL || shader == NULL) {
    neuron_graphics_set_error("cmd.Draw requires both mesh and material");
    return;
  }
  if (!neuron_graphics_canvas_append_draw(
          g_active_canvas, NEURON_GRAPHICS_DRAW_KIND_NON_INDEXED,
          (NeuronGraphicsMesh *)target, (NeuronGraphicsMaterial *)shader, 1)) {
    return;
  }
}

void npp_graphics_core_draw_indexed(void *target, void *shader) {
  if (g_active_canvas == NULL || !g_active_canvas->frame_active) {
    neuron_graphics_set_error("cmd.DrawIndexed requires an active canvas frame");
    return;
  }
  if (target == NULL || shader == NULL) {
    neuron_graphics_set_error(
        "cmd.DrawIndexed requires both mesh and material");
    return;
  }
  (void)neuron_graphics_canvas_append_draw(
      g_active_canvas, NEURON_GRAPHICS_DRAW_KIND_INDEXED,
      (NeuronGraphicsMesh *)target, (NeuronGraphicsMaterial *)shader, 1);
}

void npp_graphics_core_draw_instanced(void *target, void *shader,
                                      int32_t instances) {
  if (g_active_canvas == NULL || !g_active_canvas->frame_active) {
    neuron_graphics_set_error(
        "cmd.DrawInstanced requires an active canvas frame");
    return;
  }
  if (target == NULL || shader == NULL) {
    neuron_graphics_set_error(
        "cmd.DrawInstanced requires both mesh and material");
    return;
  }
  if (instances <= 0) {
    neuron_graphics_set_error(
        "cmd.DrawInstanced requires a positive instance count");
    return;
  }
  (void)neuron_graphics_canvas_append_draw(
      g_active_canvas, NEURON_GRAPHICS_DRAW_KIND_INSTANCED,
      (NeuronGraphicsMesh *)target, (NeuronGraphicsMaterial *)shader,
      instances);
}

int32_t npp_graphics_core_draw_tensor(NeuronTensor *tensor) {
  if (g_active_canvas == NULL || !g_active_canvas->frame_active ||
      g_active_canvas->backend == NULL || tensor == NULL ||
      tensor->data == NULL || tensor->size <= 0 || tensor->element_size <= 0) {
    return 0;
  }

  return neuron_graphics_backend_set_tensor_interop(g_active_canvas->backend,
                                                    tensor);
}

void npp_graphics_core_clear(void *color_token) {
  if (g_active_canvas == NULL || !g_active_canvas->frame_active) {
    neuron_graphics_set_error("cmd.Clear requires an active canvas frame");
    return;
  }

  g_active_canvas->has_clear_color = 1;
  if (color_token != NULL) {
    g_active_canvas->clear_color = *(NeuronGraphicsColor *)color_token;
    return;
  }

  g_active_canvas->clear_color.red = 0.0f;
  g_active_canvas->clear_color.green = 0.0f;
  g_active_canvas->clear_color.blue = 0.0f;
  g_active_canvas->clear_color.alpha = 1.0f;
}
