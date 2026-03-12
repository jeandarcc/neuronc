#include "graphics/backend/graphics_backend_internal.h"
#include "graphics/graphics_core_internal.h"

#if defined(__EMSCRIPTEN__)

#include <emscripten.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

struct NeuronGraphicsBackend {
  NeuronGraphicsWindow *window;
  int32_t webgpu_backend_id;
  int resize_pending;
};

EM_JS(int32_t, npp_webgpu_backend_create_js, (int32_t width, int32_t height), {
  var state = globalThis.__nppGraphicsState;
  if (!state) {
    state = globalThis.__nppGraphicsState = {
      nextId: 1,
      backends: {},
      pipelines: {},
      meshes: {},
      textures: {},
      samplers: {}
    };
  }

  var canvas = Module['canvas'] || document.querySelector('canvas');
  if (!canvas || !canvas.getContext || typeof navigator === 'undefined' ||
      !navigator.gpu) {
    return 0;
  }

  canvas.width = width > 0 ? width : canvas.width;
  canvas.height = height > 0 ? height : canvas.height;
  var context = canvas.getContext('webgpu');
  if (!context) {
    return 0;
  }

  var id = state.nextId++;
  var backend = {
    id: id,
    canvas: canvas,
    context: context,
    device: null,
    queue: null,
    format: null,
    ready: false,
    initStarted: false,
    initFailed: false,
    currentEncoder: null,
    currentPass: null,
    transients: []
  };
  state.backends[id] = backend;

  var beginInit = function() {
    if (backend.initStarted) {
      return;
    }
    backend.initStarted = true;
    navigator.gpu.requestAdapter().then(function(adapter) {
      if (!adapter) {
        backend.initFailed = true;
        return null;
      }
      return adapter.requestDevice();
    }).then(function(device) {
      if (!device) {
        backend.initFailed = true;
        return;
      }
      backend.device = device;
      backend.queue = device.queue;
      backend.format = navigator.gpu.getPreferredCanvasFormat();
      backend.context.configure({
        device: device,
        format: backend.format,
        alphaMode: 'opaque'
      });
      backend.ready = true;
    }).catch(function() {
      backend.initFailed = true;
    });
  };

  beginInit();
  return id;
});

EM_JS(void, npp_webgpu_backend_destroy_js, (int32_t backendId), {
  var state = globalThis.__nppGraphicsState;
  if (!state || !state.backends[backendId]) {
    return;
  }
  delete state.backends[backendId];
});

EM_JS(int32_t, npp_webgpu_backend_ensure_ready_js,
      (int32_t backendId, int32_t width, int32_t height), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend) {
    return -1;
  }
  if (backend.canvas) {
    if (width > 0) {
      backend.canvas.width = width;
    }
    if (height > 0) {
      backend.canvas.height = height;
    }
  }
  if (backend.ready && backend.device && backend.context) {
    backend.context.configure({
      device: backend.device,
      format: backend.format,
      alphaMode: 'opaque'
    });
    return 1;
  }
  return backend.initFailed ? -1 : 0;
});

EM_JS(int32_t, npp_webgpu_backend_begin_pass_js,
      (int32_t backendId, float red, float green, float blue, float alpha), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend || !backend.ready || !backend.device || !backend.context) {
    return 0;
  }

  if (backend.transients) {
    for (var i = 0; i < backend.transients.length; ++i) {
      if (backend.transients[i] && backend.transients[i].destroy) {
        backend.transients[i].destroy();
      }
    }
    backend.transients.length = 0;
  } else {
    backend.transients = [];
  }

  var currentTexture = backend.context.getCurrentTexture();
  backend.currentEncoder = backend.device.createCommandEncoder();
  backend.currentPass = backend.currentEncoder.beginRenderPass({
    colorAttachments: [{
      view: currentTexture.createView(),
      clearValue: { r: red, g: green, b: blue, a: alpha },
      loadOp: 'clear',
      storeOp: 'store'
    }]
  });
  return 1;
});

EM_JS(int32_t, npp_webgpu_backend_ensure_pipeline_js,
      (int32_t backendId, int32_t pipelineKey, const char *vertexWgsl,
       const char *fragmentWgsl, uint32_t vertexLayoutMask), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend || !backend.ready || !backend.device) {
    return 0;
  }
  if (state.pipelines[pipelineKey]) {
    return 1;
  }

  var vertexSource = UTF8ToString(vertexWgsl);
  var fragmentSource = UTF8ToString(fragmentWgsl);
  if (!vertexSource || !fragmentSource) {
    return 0;
  }

  var attributes = [];
  if ((vertexLayoutMask & 1) !== 0) {
    attributes.push({ shaderLocation: 0, offset: 0, format: 'float32x3' });
  }
  if ((vertexLayoutMask & 2) !== 0) {
    attributes.push({ shaderLocation: 1, offset: 12, format: 'float32x2' });
  }
  if ((vertexLayoutMask & 4) !== 0) {
    attributes.push({ shaderLocation: 2, offset: 20, format: 'float32x3' });
  }

  var pipeline = backend.device.createRenderPipeline({
    layout: 'auto',
    vertex: {
      module: backend.device.createShaderModule({ code: vertexSource }),
      entryPoint: 'main',
      buffers: [{
        arrayStride: 32,
        stepMode: 'vertex',
        attributes: attributes
      }]
    },
    fragment: {
      module: backend.device.createShaderModule({ code: fragmentSource }),
      entryPoint: 'main',
      targets: [{ format: backend.format }]
    },
    primitive: {
      topology: 'triangle-list',
      cullMode: 'none'
    }
  });

  state.pipelines[pipelineKey] = pipeline;
  return 1;
});

EM_JS(int32_t, npp_webgpu_backend_ensure_mesh_js,
      (int32_t backendId, int32_t meshKey, const void *vertexData,
       int32_t vertexCount, const void *indexData, int32_t indexCount), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend || !backend.ready || !backend.device) {
    return 0;
  }
  if (state.meshes[meshKey]) {
    return 1;
  }

  var vertexSize = vertexCount * 32;
  var vertexBytes = HEAPU8.slice(vertexData, vertexData + vertexSize);
  var vertexBuffer = backend.device.createBuffer({
    size: Math.max(32, (vertexSize + 3) & ~3),
    usage: GPUBufferUsage.VERTEX | GPUBufferUsage.COPY_DST
  });
  backend.queue.writeBuffer(vertexBuffer, 0, vertexBytes);

  var mesh = {
    vertexBuffer: vertexBuffer,
    vertexCount: vertexCount,
    indexBuffer: null,
    indexCount: indexCount
  };
  if (indexData && indexCount > 0) {
    var indexSize = indexCount * 4;
    var indexBytes = HEAPU8.slice(indexData, indexData + indexSize);
    mesh.indexBuffer = backend.device.createBuffer({
      size: Math.max(4, (indexSize + 3) & ~3),
      usage: GPUBufferUsage.INDEX | GPUBufferUsage.COPY_DST
    });
    backend.queue.writeBuffer(mesh.indexBuffer, 0, indexBytes);
  }

  state.meshes[meshKey] = mesh;
  return 1;
});

EM_JS(int32_t, npp_webgpu_backend_ensure_sampler_js,
      (int32_t backendId, int32_t samplerKey), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend || !backend.ready || !backend.device) {
    return 0;
  }
  if (state.samplers[samplerKey]) {
    return 1;
  }
  state.samplers[samplerKey] = {
    sampler: backend.device.createSampler({
      magFilter: 'linear',
      minFilter: 'linear'
    })
  };
  return 1;
});

EM_JS(int32_t, npp_webgpu_backend_ensure_texture_js,
      (int32_t backendId, int32_t textureKey, const char *pathPtr,
       const uint8_t *pixelsPtr, int32_t width, int32_t height), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend || !backend.ready || !backend.device) {
    return 0;
  }

  var existing = state.textures[textureKey];
  if (existing && existing.ready) {
    return 1;
  }
  if (!existing) {
    existing = state.textures[textureKey] = {
      ready: false,
      loading: false,
      texture: null,
      view: null
    };
  }

  var createTextureFromPixels = function(pixelBytes, pixelWidth, pixelHeight) {
    var texture = backend.device.createTexture({
      size: { width: pixelWidth, height: pixelHeight, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.TEXTURE_BINDING |
             GPUTextureUsage.COPY_DST |
             GPUTextureUsage.RENDER_ATTACHMENT
    });
    backend.queue.writeTexture(
      { texture: texture },
      pixelBytes,
      { bytesPerRow: pixelWidth * 4, rowsPerImage: pixelHeight },
      { width: pixelWidth, height: pixelHeight, depthOrArrayLayers: 1 });
    existing.texture = texture;
    existing.view = texture.createView();
    existing.ready = true;
  };

  if (pixelsPtr && width > 0 && height > 0) {
    createTextureFromPixels(HEAPU8.slice(pixelsPtr, pixelsPtr + width * height * 4),
                            width, height);
    return 1;
  }

  var path = pathPtr ? UTF8ToString(pathPtr) : '';
  if (!path) {
    return 0;
  }
  if (existing.loading) {
    return 0;
  }

  existing.loading = true;
  fetch(path).then(function(response) {
    return response.blob();
  }).then(function(blob) {
    return createImageBitmap(blob);
  }).then(function(bitmap) {
    var texture = backend.device.createTexture({
      size: { width: bitmap.width, height: bitmap.height, depthOrArrayLayers: 1 },
      format: 'rgba8unorm',
      usage: GPUTextureUsage.TEXTURE_BINDING |
             GPUTextureUsage.COPY_DST |
             GPUTextureUsage.RENDER_ATTACHMENT
    });
    backend.queue.copyExternalImageToTexture(
      { source: bitmap },
      { texture: texture },
      { width: bitmap.width, height: bitmap.height, depthOrArrayLayers: 1 });
    existing.texture = texture;
    existing.view = texture.createView();
    existing.ready = true;
    existing.loading = false;
  }).catch(function() {
    existing.loading = false;
  });
  return 0;
});

EM_JS(int32_t, npp_webgpu_backend_draw_js,
      (int32_t backendId, int32_t pipelineKey, int32_t meshKey, int32_t indexed,
       int32_t instanceCount, const uint8_t *uniformData,
       int32_t uniformDataSize, int32_t textureKey, int32_t textureBinding,
       int32_t samplerKey, int32_t samplerBinding), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend || !backend.currentPass || !backend.device) {
    return 0;
  }

  var pipeline = state.pipelines[pipelineKey];
  var mesh = state.meshes[meshKey];
  if (!pipeline || !mesh) {
    return 0;
  }

  backend.currentPass.setPipeline(pipeline);
  backend.currentPass.setVertexBuffer(0, mesh.vertexBuffer);

  var entries = [];
  if (uniformData && uniformDataSize > 0) {
    var uniformBuffer = backend.device.createBuffer({
      size: Math.max(16, (uniformDataSize + 15) & ~15),
      usage: GPUBufferUsage.UNIFORM | GPUBufferUsage.COPY_DST
    });
    backend.queue.writeBuffer(
      uniformBuffer, 0, HEAPU8.slice(uniformData, uniformData + uniformDataSize));
    entries.push({ binding: 0, resource: { buffer: uniformBuffer } });
    backend.transients.push(uniformBuffer);
  }

  if (textureKey > 0) {
    var textureState = state.textures[textureKey];
    if (!textureState || !textureState.ready || !textureState.view) {
      return 0;
    }
    entries.push({ binding: textureBinding, resource: textureState.view });
  }

  if (samplerKey > 0) {
    var samplerState = state.samplers[samplerKey];
    if (!samplerState || !samplerState.sampler) {
      return 0;
    }
    entries.push({ binding: samplerBinding, resource: samplerState.sampler });
  }

  if (entries.length > 0) {
    backend.currentPass.setBindGroup(0, backend.device.createBindGroup({
      layout: pipeline.getBindGroupLayout(0),
      entries: entries
    }));
  }

  if (indexed && mesh.indexBuffer) {
    backend.currentPass.setIndexBuffer(mesh.indexBuffer, 'uint32');
    backend.currentPass.drawIndexed(mesh.indexCount, Math.max(1, instanceCount), 0, 0, 0);
  } else {
    backend.currentPass.draw(mesh.vertexCount, Math.max(1, instanceCount), 0, 0);
  }
  return 1;
});

EM_JS(int32_t, npp_webgpu_backend_submit_js, (int32_t backendId), {
  var state = globalThis.__nppGraphicsState;
  var backend = state && state.backends ? state.backends[backendId] : null;
  if (!backend || !backend.currentPass || !backend.currentEncoder || !backend.queue) {
    return 0;
  }
  backend.currentPass.end();
  backend.queue.submit([backend.currentEncoder.finish()]);
  backend.currentPass = null;
  backend.currentEncoder = null;
  return 1;
});

static int neuron_graphics_webgpu_ensure_pipeline(
    NeuronGraphicsBackend *backend,
    const NeuronGraphicsShaderDescriptor *shader_descriptor) {
  if (backend == NULL || shader_descriptor == NULL ||
      shader_descriptor->vertex_wgsl_source == NULL ||
      shader_descriptor->vertex_wgsl_size == 0 ||
      shader_descriptor->fragment_wgsl_source == NULL ||
      shader_descriptor->fragment_wgsl_size == 0) {
    neuron_graphics_set_error(
        "WebGPU graphics backend requires embedded WGSL shader artifacts");
    return 0;
  }
  return npp_webgpu_backend_ensure_pipeline_js(
             backend->webgpu_backend_id, (int32_t)(intptr_t)shader_descriptor,
             shader_descriptor->vertex_wgsl_source,
             shader_descriptor->fragment_wgsl_source,
             shader_descriptor->vertex_layout_mask) != 0
             ? 1
             : 0;
}

static int neuron_graphics_webgpu_ensure_mesh(NeuronGraphicsBackend *backend,
                                              NeuronGraphicsMesh *mesh) {
  if (backend == NULL || mesh == NULL || mesh->vertices == NULL ||
      mesh->vertex_count == 0) {
    neuron_graphics_set_error("WebGPU draw requires mesh vertex data");
    return 0;
  }
  if (mesh->gpu.gpu_ready) {
    return 1;
  }
  if (!npp_webgpu_backend_ensure_mesh_js(
          backend->webgpu_backend_id, (int32_t)(intptr_t)mesh, mesh->vertices,
          (int32_t)mesh->vertex_count, mesh->indices,
          (int32_t)mesh->index_count)) {
    neuron_graphics_set_error("Failed to upload mesh to WebGPU");
    return 0;
  }
  mesh->gpu.gpu_ready = 1;
  return 1;
}

static int neuron_graphics_webgpu_find_binding(
    const NeuronGraphicsMaterial *material, uint32_t kind,
    const NeuronGraphicsMaterialBinding **out_binding) {
  uint32_t i = 0;
  if (out_binding == NULL || material == NULL) {
    return 0;
  }
  *out_binding = NULL;
  for (i = 0; i < material->binding_count; ++i) {
    if (material->bindings[i].descriptor != NULL &&
        material->bindings[i].descriptor->kind == kind) {
      *out_binding = &material->bindings[i];
      return 1;
    }
  }
  return 0;
}

static int neuron_graphics_webgpu_ensure_sampler(
    NeuronGraphicsBackend *backend, const NeuronGraphicsMaterialBinding *binding) {
  if (binding == NULL || binding->sampler_value == NULL) {
    return 1;
  }
  if (binding->sampler_value->gpu.gpu_ready) {
    return 1;
  }
  if (!npp_webgpu_backend_ensure_sampler_js(
          backend->webgpu_backend_id,
          (int32_t)(intptr_t)binding->sampler_value)) {
    neuron_graphics_set_error("Failed to create WebGPU sampler");
    return 0;
  }
  binding->sampler_value->gpu.gpu_ready = 1;
  return 1;
}

static int neuron_graphics_webgpu_ensure_texture(
    NeuronGraphicsBackend *backend, const NeuronGraphicsMaterialBinding *binding) {
  if (binding == NULL || binding->texture_value == NULL) {
    return 1;
  }
  if (binding->texture_value->gpu.gpu_ready) {
    return 1;
  }
  if (!npp_webgpu_backend_ensure_texture_js(
          backend->webgpu_backend_id,
          (int32_t)(intptr_t)binding->texture_value,
          binding->texture_value->path,
          binding->texture_value->pixels,
          (int32_t)binding->texture_value->width,
          (int32_t)binding->texture_value->height)) {
    neuron_graphics_set_error("WebGPU texture is not ready yet");
    return 0;
  }
  binding->texture_value->gpu.gpu_ready = 1;
  return 1;
}

NeuronGraphicsBackend *neuron_graphics_backend_create(
    NeuronGraphicsWindow *window) {
  NeuronGraphicsBackend *backend = NULL;
  if (window == NULL) {
    neuron_graphics_set_error("Cannot create WebGPU backend for null window");
    return NULL;
  }

  backend = (NeuronGraphicsBackend *)calloc(1, sizeof(NeuronGraphicsBackend));
  if (backend == NULL) {
    neuron_graphics_set_error("Out of memory allocating WebGPU backend");
    return NULL;
  }
  backend->window = window;
  backend->webgpu_backend_id =
      npp_webgpu_backend_create_js(window->width, window->height);
  if (backend->webgpu_backend_id == 0) {
    free(backend);
    neuron_graphics_set_error(
        "Failed to initialize browser WebGPU graphics backend");
    return NULL;
  }
  return backend;
}

void neuron_graphics_backend_destroy(NeuronGraphicsBackend *backend) {
  if (backend == NULL) {
    return;
  }
  if (backend->webgpu_backend_id != 0) {
    npp_webgpu_backend_destroy_js(backend->webgpu_backend_id);
  }
  free(backend);
}

void neuron_graphics_backend_mark_resize(NeuronGraphicsBackend *backend) {
  if (backend != NULL) {
    backend->resize_pending = 1;
  }
}

int neuron_graphics_backend_begin_frame(NeuronGraphicsBackend *backend) {
  int32_t ready = 0;
  if (backend == NULL || backend->window == NULL) {
    return 0;
  }
  ready = npp_webgpu_backend_ensure_ready_js(backend->webgpu_backend_id,
                                             backend->window->width,
                                             backend->window->height);
  if (ready < 0) {
    neuron_graphics_set_error("WebGPU backend failed to initialize");
  }
  backend->resize_pending = 0;
  return 1;
}

int neuron_graphics_backend_present(NeuronGraphicsBackend *backend) {
  size_t i = 0;
  int32_t ready = 0;
  NeuronGraphicsColor clear_color = {0.0f, 0.0f, 0.0f, 1.0f};
  if (backend == NULL || backend->window == NULL || g_active_canvas == NULL) {
    return 0;
  }

  ready = npp_webgpu_backend_ensure_ready_js(backend->webgpu_backend_id,
                                             backend->window->width,
                                             backend->window->height);
  if (ready <= 0) {
    return ready == 0 ? 1 : 0;
  }

  if (g_active_canvas->has_clear_color) {
    clear_color = g_active_canvas->clear_color;
  }
  if (!npp_webgpu_backend_begin_pass_js(backend->webgpu_backend_id,
                                        clear_color.red, clear_color.green,
                                        clear_color.blue,
                                        clear_color.alpha)) {
    neuron_graphics_set_error("Failed to begin WebGPU graphics frame");
    return 0;
  }

  for (i = 0; i < g_active_canvas->draw_command_count; ++i) {
    const NeuronGraphicsDrawCommand *command =
        &g_active_canvas->draw_commands[i];
    const NeuronGraphicsMaterialBinding *texture_binding = NULL;
    const NeuronGraphicsMaterialBinding *sampler_binding = NULL;
    int32_t texture_key = 0;
    int32_t sampler_key = 0;
    int32_t texture_binding_index = 0;
    int32_t sampler_binding_index = 0;

    if (command->mesh == NULL || command->material == NULL ||
        command->material->shader_descriptor == NULL) {
      neuron_graphics_set_error("WebGPU draw command is missing mesh or material");
      continue;
    }
    if (!neuron_graphics_webgpu_ensure_pipeline(
            backend, command->material->shader_descriptor) ||
        !neuron_graphics_webgpu_ensure_mesh(backend, command->mesh)) {
      continue;
    }
    (void)neuron_graphics_webgpu_find_binding(
        command->material, NEURON_GRAPHICS_SHADER_BINDING_TEXTURE2D,
        &texture_binding);
    (void)neuron_graphics_webgpu_find_binding(
        command->material, NEURON_GRAPHICS_SHADER_BINDING_SAMPLER,
        &sampler_binding);
    if (texture_binding != NULL) {
      if (!neuron_graphics_webgpu_ensure_texture(backend, texture_binding)) {
        continue;
      }
      texture_key = (int32_t)(intptr_t)texture_binding->texture_value;
      texture_binding_index =
          (int32_t)texture_binding->descriptor->descriptor_binding;
    }
    if (sampler_binding != NULL) {
      if (!neuron_graphics_webgpu_ensure_sampler(backend, sampler_binding)) {
        continue;
      }
      sampler_key = (int32_t)(intptr_t)sampler_binding->sampler_value;
      sampler_binding_index =
          (int32_t)sampler_binding->descriptor->descriptor_binding;
    }

    if (!npp_webgpu_backend_draw_js(
            backend->webgpu_backend_id,
            (int32_t)(intptr_t)command->material->shader_descriptor,
            (int32_t)(intptr_t)command->mesh,
            command->kind == NEURON_GRAPHICS_DRAW_KIND_INDEXED ? 1 : 0,
            command->kind == NEURON_GRAPHICS_DRAW_KIND_INSTANCED &&
                    command->instance_count > 0
                ? command->instance_count
                : 1,
            command->material->uniform_data,
            (int32_t)command->material->uniform_data_size, texture_key,
            texture_binding_index, sampler_key, sampler_binding_index)) {
      neuron_graphics_set_error("Failed to encode WebGPU draw command");
    }
  }

  if (!npp_webgpu_backend_submit_js(backend->webgpu_backend_id)) {
    neuron_graphics_set_error("Failed to submit WebGPU graphics frame");
    return 0;
  }
  return 1;
}

int neuron_graphics_backend_set_tensor_interop(NeuronGraphicsBackend *backend,
                                               NeuronTensor *tensor) {
  (void)backend;
  (void)tensor;
  neuron_graphics_set_error(
      "Tensor interop draw is not implemented for WebGPU graphics backend");
  return 0;
}

#endif
