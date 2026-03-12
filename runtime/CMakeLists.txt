set(NEURON_RUNTIME_SOURCES
    src/runtime.c
    src/modulecpp_runtime.cpp
    src/tensor.c
    src/tensor/tensor_config.c
    src/tensor/tensor_core.c
    src/tensor/tensor_math.c
    src/tensor/tensor_microkernel.c
    src/nn.c
    src/gpu.c
    src/io.c
    src/platform/platform_manager.c
    src/platform/common/platform_error.c
    src/graphics/graphics_core_state.c
    src/graphics/graphics_core_window_canvas_api.c
    src/graphics/graphics_core_assets_api.c
    src/graphics/graphics_window_canvas.c
    src/graphics/graphics_assets.c
    src/graphics/scene2d/graphics_scene2d_scene.c
    src/graphics/scene2d/graphics_scene2d_renderer.c
    src/graphics/assets/graphics_asset_png_wic.c
    src/graphics/assets/graphics_asset_obj_parser.c
    src/graphics/backend/vulkan/graphics_vk_dispatch_loader.c
    src/graphics/backend/vulkan/graphics_vk_swapchain.c
    src/graphics/backend/vulkan/graphics_vk_pipeline.c
    src/graphics/backend/vulkan/graphics_vk_backend_lifecycle.c
    src/graphics/backend/vulkan/graphics_vk_backend_frame.c
    src/graphics/backend/vulkan/graphics_vk_backend_interop.c
    src/graphics/backend/stub/graphics_vk_backend_stub.c
)

if(EMSCRIPTEN)
    list(APPEND NEURON_RUNTIME_SOURCES
        src/graphics/backend/webgpu/graphics_webgpu_backend.c
        src/platform/web/platform_web_library.c
        src/platform/web/platform_web_env.c
        src/platform/web/platform_web_time.c
        src/platform/web/platform_web_path.c
        src/platform/web/platform_web_diagnostics.c
        src/platform/web/platform_web_process.c
        src/platform/web/platform_web_thread.c
        src/platform/web/platform_web_window.c
        src/platform/gpu_webgpu.c
    )
elseif(WIN32)
    list(APPEND NEURON_RUNTIME_SOURCES
        src/platform/win32/platform_win32_library.c
        src/platform/win32/platform_win32_env.c
        src/platform/win32/platform_win32_time.c
        src/platform/win32/platform_win32_path.c
        src/platform/win32/platform_win32_diagnostics.c
        src/platform/win32/platform_win32_process.c
        src/platform/win32/platform_win32_thread.c
        src/platform/win32/platform_win32_window.c
        src/graphics/platform/graphics_platform_win32.c
    )
elseif(APPLE)
    list(APPEND NEURON_RUNTIME_SOURCES
        src/platform/apple/platform_apple_library.c
        src/platform/apple/platform_apple_env.c
        src/platform/apple/platform_apple_time.c
        src/platform/apple/platform_apple_path.c
        src/platform/apple/platform_apple_diagnostics.c
        src/platform/apple/platform_apple_process.c
        src/platform/apple/platform_apple_thread.c
        src/platform/apple/platform_apple_window.c
    )
elseif(UNIX)
    list(APPEND NEURON_RUNTIME_SOURCES
        src/platform/posix/platform_posix_library.c
        src/platform/posix/platform_posix_env.c
        src/platform/posix/platform_posix_time.c
        src/platform/posix/platform_posix_path.c
        src/platform/posix/platform_posix_diagnostics.c
        src/platform/posix/platform_posix_process.c
        src/platform/posix/platform_posix_thread.c
        src/platform/posix/platform_posix_window_x11.c
    )
endif()
if(NPP_ENABLE_VULKAN_BACKEND)
    list(APPEND NEURON_RUNTIME_SOURCES
        src/gpu_vulkan/gpu_vulkan_core.c
        src/gpu_vulkan/gpu_vulkan_engine.c
        src/gpu_vulkan/gpu_vulkan_public_api.c
        src/gpu_vulkan/gpu_vulkan_init.c
        src/gpu_vulkan/gpu_vulkan_scope.c
        src/gpu_vulkan/gpu_vulkan_dispatch.c
        src/gpu_vulkan/gpu_vulkan_interop.c
        src/vulkan_common_state.c
        src/vulkan_common_loader.c
        src/vulkan_common_device.c
        src/vulkan_common_api.c
    )
endif()
if(NPP_ENABLE_CUDA_BACKEND)
    list(APPEND NEURON_RUNTIME_SOURCES src/gpu_cuda.c)
endif()

function(npp_configure_runtime_target target_name)
    target_include_directories(${target_name} PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${NPP_FFI_INCLUDE_DIR}
    )
    target_compile_definitions(${target_name} PRIVATE
        NPP_ENABLE_CUDA_BACKEND=$<BOOL:${NPP_ENABLE_CUDA_BACKEND}>
        NPP_ENABLE_VULKAN_BACKEND=$<BOOL:${NPP_ENABLE_VULKAN_BACKEND}>
        NPP_ENABLE_WEBGPU_BACKEND=$<BOOL:${EMSCRIPTEN}>
    )
    target_link_libraries(${target_name} PUBLIC ${NPP_FFI_LIBRARY})
    if(UNIX AND NOT APPLE AND NOT EMSCRIPTEN)
        target_link_libraries(${target_name} PUBLIC dl m)
    endif()
    if(WIN32)
        target_link_libraries(${target_name} PUBLIC user32 gdi32 ole32 windowscodecs uuid shell32)
    endif()
endfunction()

add_library(neuron_runtime SHARED ${NEURON_RUNTIME_SOURCES})
npp_configure_runtime_target(neuron_runtime)
target_compile_definitions(neuron_runtime PRIVATE NEURON_RUNTIME_BUILD_SHARED)
set_target_properties(neuron_runtime PROPERTIES
    OUTPUT_NAME neuron_runtime
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    LIBRARY_OUTPUT_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}
    WINDOWS_EXPORT_ALL_SYMBOLS ON
)

add_library(ncon_runtime_static STATIC ${NEURON_RUNTIME_SOURCES})
npp_configure_runtime_target(ncon_runtime_static)
set_target_properties(ncon_runtime_static PROPERTIES OUTPUT_NAME neuron_runtime_static)

add_library(ncon_runtime ALIAS ncon_runtime_static)
