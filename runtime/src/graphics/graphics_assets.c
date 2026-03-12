#include "graphics_assets_internal.h"

NeuronGraphicsTexture *neuron_graphics_texture_load(const char *path) {
  return npp_graphics_core_texture_load(path);
}

void neuron_graphics_texture_free(NeuronGraphicsTexture *texture) {
  npp_graphics_core_texture_free(texture);
}

NeuronGraphicsSampler *neuron_graphics_sampler_create(void) {
  return npp_graphics_core_sampler_create();
}

void neuron_graphics_sampler_free(NeuronGraphicsSampler *sampler) {
  npp_graphics_core_sampler_free(sampler);
}

NeuronGraphicsMesh *neuron_graphics_mesh_load(const char *path) {
  return npp_graphics_core_mesh_load(path);
}

NeuronGraphicsMesh *neuron_graphics_mesh_create_static(
    const NeuronGraphicsVertex *vertices, int32_t vertex_count,
    const uint32_t *indices, int32_t index_count) {
  return npp_graphics_core_mesh_create_static(vertices, vertex_count, indices,
                                              index_count);
}

void neuron_graphics_mesh_free(NeuronGraphicsMesh *mesh) {
  npp_graphics_core_mesh_free(mesh);
}

NeuronGraphicsMaterial *neuron_graphics_material_create(
    const NeuronGraphicsShaderDescriptor *shader_descriptor) {
  return npp_graphics_core_material_create(shader_descriptor);
}

void neuron_graphics_material_free(NeuronGraphicsMaterial *material) {
  npp_graphics_core_material_free(material);
}

void neuron_graphics_material_set_vec4(NeuronGraphicsMaterial *material,
                                       const char *binding_name,
                                       void *color_token) {
  npp_graphics_core_material_set_vec4(material, binding_name, color_token);
}

void neuron_graphics_material_set_texture(NeuronGraphicsMaterial *material,
                                          const char *binding_name,
                                          NeuronGraphicsTexture *texture) {
  npp_graphics_core_material_set_texture(material, binding_name, texture);
}

void neuron_graphics_material_set_sampler(NeuronGraphicsMaterial *material,
                                          const char *binding_name,
                                          NeuronGraphicsSampler *sampler) {
  npp_graphics_core_material_set_sampler(material, binding_name, sampler);
}

void neuron_graphics_material_set_matrix4(NeuronGraphicsMaterial *material,
                                          const char *binding_name,
                                          const float *matrix4_values) {
  npp_graphics_core_material_set_matrix4(material, binding_name, matrix4_values);
}

NeuronGraphicsColor *neuron_graphics_color_rgba(double red, double green,
                                                double blue, double alpha) {
  return npp_graphics_core_color_rgba(red, green, blue, alpha);
}

void neuron_graphics_color_free(NeuronGraphicsColor *color) {
  npp_graphics_core_color_free(color);
}

NeuronGraphicsVector2 *neuron_graphics_vector2_create(double x, double y) {
  return npp_graphics_core_vector2_create(x, y);
}

void neuron_graphics_vector2_free(NeuronGraphicsVector2 *value) {
  npp_graphics_core_vector2_free(value);
}

NeuronGraphicsVector3 *neuron_graphics_vector3_create(double x, double y,
                                                      double z) {
  return npp_graphics_core_vector3_create(x, y, z);
}

void neuron_graphics_vector3_free(NeuronGraphicsVector3 *value) {
  npp_graphics_core_vector3_free(value);
}

NeuronGraphicsVector4 *neuron_graphics_vector4_create(double x, double y,
                                                      double z, double w) {
  return npp_graphics_core_vector4_create(x, y, z, w);
}

void neuron_graphics_vector4_free(NeuronGraphicsVector4 *value) {
  npp_graphics_core_vector4_free(value);
}

void neuron_graphics_draw(void *target, void *shader) {
  npp_graphics_core_draw(target, shader);
}

void neuron_graphics_draw_indexed(void *target, void *shader) {
  npp_graphics_core_draw_indexed(target, shader);
}

void neuron_graphics_draw_instanced(void *target, void *shader,
                                    int32_t instances) {
  npp_graphics_core_draw_instanced(target, shader, instances);
}

int32_t neuron_graphics_draw_tensor(NeuronTensor *tensor) {
  return npp_graphics_core_draw_tensor(tensor);
}

void neuron_graphics_clear(void *color_token) {
  npp_graphics_core_clear(color_token);
}
