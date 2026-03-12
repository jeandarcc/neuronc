#ifndef NPP_RUNTIME_GRAPHICS_BACKEND_INTERNAL_H
#define NPP_RUNTIME_GRAPHICS_BACKEND_INTERNAL_H

#include "graphics/graphics_types_internal.h"

NeuronGraphicsBackend *neuron_graphics_backend_create(NeuronGraphicsWindow *window);
void neuron_graphics_backend_destroy(NeuronGraphicsBackend *backend);
void neuron_graphics_backend_mark_resize(NeuronGraphicsBackend *backend);
int neuron_graphics_backend_begin_frame(NeuronGraphicsBackend *backend);
int neuron_graphics_backend_present(NeuronGraphicsBackend *backend);
int neuron_graphics_backend_set_tensor_interop(NeuronGraphicsBackend *backend,
                                               NeuronTensor *tensor);

#endif
