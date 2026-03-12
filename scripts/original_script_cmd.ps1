$ErrorActionPreference = "Stop"

$env:Path = "C:\msys64\mingw64\bin;C:\msys64\usr\bin;" + $env:Path
$runtimeDefines = @("-DNPP_ENABLE_CUDA_BACKEND=1", "-DNPP_ENABLE_VULKAN_BACKEND=1")
$includes = @("-Iruntime/include", "-Iruntime/src")
gcc -c runtime/src/runtime.c $includes $runtimeDefines -o runtime/runtime.o
gcc -c runtime/src/gpu.c $includes $runtimeDefines -o runtime/gpu.o
gcc -c runtime/src/gpu_cuda.c $includes $runtimeDefines -o runtime/gpu_cuda.o
gcc -c runtime/src/tensor_new.c $includes $runtimeDefines -o runtime/tensor.o
gcc -c runtime/src/tensor/tensor_config.c $includes $runtimeDefines -o runtime/tensor_config.o
gcc -c runtime/src/nn.c $includes $runtimeDefines -o runtime/nn.o
gcc -c runtime/src/io.c $includes $runtimeDefines -o runtime/io.o
echo "Runtime built successfully."
