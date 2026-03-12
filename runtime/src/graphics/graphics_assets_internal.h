#ifndef NPP_RUNTIME_GRAPHICS_ASSETS_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_ASSETS_INTERNAL_H

#include "neuron_graphics.h"

NeuronGraphicsTexture *npp_graphics_core_texture_load(const char *path);
void npp_graphics_core_texture_free(NeuronGraphicsTexture *texture);
NeuronGraphicsSampler *npp_graphics_core_sampler_create(void);
void npp_graphics_core_sampler_free(NeuronGraphicsSampler *sampler);
NeuronGraphicsMesh *npp_graphics_core_mesh_load(const char *path);
NeuronGraphicsMesh *npp_graphics_core_mesh_create_static(
    const NeuronGraphicsVertex *vertices, int32_t vertex_count,
    const uint32_t *indices, int32_t index_count);
void npp_graphics_core_mesh_free(NeuronGraphicsMesh *mesh);
NeuronGraphicsMaterial *npp_graphics_core_material_create(
    const NeuronGraphicsShaderDescriptor *shader_descriptor);
void npp_graphics_core_material_free(NeuronGraphicsMaterial *material);
void npp_graphics_core_material_set_vec4(NeuronGraphicsMaterial *material,
                                         const char *binding_name,
                                         void *color_token);
void npp_graphics_core_material_set_texture(NeuronGraphicsMaterial *material,
                                            const char *binding_name,
                                            NeuronGraphicsTexture *texture);
void npp_graphics_core_material_set_sampler(NeuronGraphicsMaterial *material,
                                            const char *binding_name,
                                            NeuronGraphicsSampler *sampler);
void npp_graphics_core_material_set_matrix4(NeuronGraphicsMaterial *material,
                                            const char *binding_name,
                                            const float *matrix4_values);
NeuronGraphicsColor *npp_graphics_core_color_rgba(double red, double green,
                                                  double blue, double alpha);
void npp_graphics_core_color_free(NeuronGraphicsColor *color);
NeuronGraphicsVector2 *npp_graphics_core_vector2_create(double x, double y);
void npp_graphics_core_vector2_free(NeuronGraphicsVector2 *value);
NeuronGraphicsVector3 *npp_graphics_core_vector3_create(double x, double y,
                                                        double z);
void npp_graphics_core_vector3_free(NeuronGraphicsVector3 *value);
NeuronGraphicsVector4 *npp_graphics_core_vector4_create(double x, double y,
                                                        double z, double w);
void npp_graphics_core_vector4_free(NeuronGraphicsVector4 *value);
void npp_graphics_core_draw(void *target, void *shader);
void npp_graphics_core_draw_indexed(void *target, void *shader);
void npp_graphics_core_draw_instanced(void *target, void *shader,
                                      int32_t instances);
int32_t npp_graphics_core_draw_tensor(NeuronTensor *tensor);
void npp_graphics_core_clear(void *color_token);

#endif
