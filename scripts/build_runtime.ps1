$ErrorActionPreference = "Stop"
echo "Building NPP Runtime..."

$env:Path = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:Path
$runtimeDefines = @("-DNPP_ENABLE_CUDA_BACKEND=1", "-DNPP_ENABLE_VULKAN_BACKEND=1")
$includes = @("-Iruntime/include", "-Iruntime/src")

gcc -c runtime/src/runtime.c $includes $runtimeDefines -o runtime/runtime.o
gcc -c runtime/src/gpu.c $includes $runtimeDefines -o runtime/gpu.o
gcc -c runtime/src/gpu_cuda.c $includes $runtimeDefines -o runtime/gpu_cuda.o
gcc -Iruntime/src $includes -c -O3 -mfma -mavx2 -fopenmp -fPIC runtime/src/tensor/tensor_core.c -o runtime/tensor_core.o
gcc -Iruntime/src $includes -c -O3 -mfma -mavx2 -fopenmp -fPIC runtime/src/tensor/tensor_math.c -o runtime/tensor_math.o
gcc -Iruntime/src $includes -c -O3 -mfma -mavx2 -fopenmp -fPIC runtime/src/tensor/tensor_microkernel.c -o runtime/tensor_microkernel.o
gcc -Iruntime/src $includes -c -O3 -mfma -mavx2 -fopenmp -fPIC runtime/src/tensor.c -o runtime/tensor.o
gcc -c runtime/src/tensor/tensor_config.c $includes $runtimeDefines -o runtime/tensor_config.o
gcc -c runtime/src/nn.c $includes $runtimeDefines -o runtime/nn.o
gcc -c runtime/src/io.c $includes $runtimeDefines -o runtime/io.o
gcc -c runtime/src/platform/platform_manager.c $includes $runtimeDefines -o runtime/platform_manager.o
gcc -c runtime/src/platform/common/platform_error.c $includes $runtimeDefines -o runtime/platform_error.o
gcc -c runtime/src/platform/win32/platform_win32_library.c $includes $runtimeDefines -o runtime/platform_win32_library.o
gcc -c runtime/src/platform/win32/platform_win32_env.c $includes $runtimeDefines -o runtime/platform_win32_env.o
gcc -c runtime/src/platform/win32/platform_win32_time.c $includes $runtimeDefines -o runtime/platform_win32_time.o
gcc -c runtime/src/platform/win32/platform_win32_path.c $includes $runtimeDefines -o runtime/platform_win32_path.o
gcc -c runtime/src/platform/win32/platform_win32_diagnostics.c $includes $runtimeDefines -o runtime/platform_win32_diagnostics.o
gcc -c runtime/src/platform/win32/platform_win32_process.c $includes $runtimeDefines -o runtime/platform_win32_process.o
gcc -c runtime/src/platform/win32/platform_win32_thread.c $includes $runtimeDefines -o runtime/platform_win32_thread.o
gcc -c runtime/src/platform/win32/platform_win32_window.c $includes $runtimeDefines -o runtime/platform_win32_window.o

echo "Runtime built successfully."
