param(
  [string]$ShaderDir = $PSScriptRoot,
  [string]$OutHeader = (Join-Path $PSScriptRoot "..\\src\\vulkan_shaders.h")
)

$ErrorActionPreference = "Stop"

$glslang = Get-Command glslangValidator -ErrorAction SilentlyContinue
if (-not $glslang) {
  throw "glslangValidator not found in PATH. Install Vulkan shader tools first."
}

$tempDir = Join-Path $env:TEMP "npp_vulkan_shader_gen"
New-Item -ItemType Directory -Force -Path $tempDir | Out-Null

$elementwiseSrc = Join-Path $ShaderDir "elementwise_binary.comp"
$binaryChainSrc = Join-Path $ShaderDir "binary_chain.comp"
$binaryThenFmaSrc = Join-Path $ShaderDir "binary_then_fma.comp"
$binaryChainThenFmaSrc = Join-Path $ShaderDir "binary_chain_then_fma.comp"
$fmaSrc = Join-Path $ShaderDir "fma.comp"
$matmulDenseSrc = Join-Path $ShaderDir "matmul_dense.comp"
$matmulPackedSrc = Join-Path $ShaderDir "matmul_packed.comp"
$elementwiseSpv = Join-Path $tempDir "elementwise_binary.comp.spv"
$binaryChainSpv = Join-Path $tempDir "binary_chain.comp.spv"
$binaryThenFmaSpv = Join-Path $tempDir "binary_then_fma.comp.spv"
$binaryChainThenFmaSpv = Join-Path $tempDir "binary_chain_then_fma.comp.spv"
$fmaSpv = Join-Path $tempDir "fma.comp.spv"
$matmulDenseSpv = Join-Path $tempDir "matmul_dense.comp.spv"
$matmulPackedSpv = Join-Path $tempDir "matmul_packed.comp.spv"

function Compile-Shader([string]$src, [string]$out) {
  & $glslang.Source -V $src -o $out | Out-Null
  if ($LASTEXITCODE -ne 0 -or -not (Test-Path $out)) {
    throw "glslangValidator failed for shader: $src"
  }
}

Compile-Shader $elementwiseSrc $elementwiseSpv
Compile-Shader $binaryChainSrc $binaryChainSpv
Compile-Shader $binaryThenFmaSrc $binaryThenFmaSpv
Compile-Shader $binaryChainThenFmaSrc $binaryChainThenFmaSpv
Compile-Shader $fmaSrc $fmaSpv
Compile-Shader $matmulDenseSrc $matmulDenseSpv
Compile-Shader $matmulPackedSrc $matmulPackedSpv

@'
import struct
from pathlib import Path
import sys

elementwise_path = Path(sys.argv[1])
binary_chain_path = Path(sys.argv[2])
binary_then_fma_path = Path(sys.argv[3])
binary_chain_then_fma_path = Path(sys.argv[4])
fma_path = Path(sys.argv[5])
matmul_dense_path = Path(sys.argv[6])
matmul_packed_path = Path(sys.argv[7])
out_header = Path(sys.argv[8])

def load_words(path: Path):
    data = path.read_bytes()
    if len(data) % 4 != 0:
        raise RuntimeError(f"{path} size is not uint32-aligned")
    return struct.unpack("<" + "I" * (len(data) // 4), data)

def emit_array(name: str, words):
    lines = [f"static const uint32_t {name}[] = {{"]
    for i in range(0, len(words), 8):
        chunk = words[i:i+8]
        row = ", ".join(f"0x{w:08x}u" for w in chunk)
        lines.append(f"    {row},")
    lines.append("};")
    lines.append(
        f"static const size_t {name}WordCount = sizeof({name}) / sizeof({name}[0]);"
    )
    lines.append("")
    return lines

header_lines = []
header_lines.append("#ifndef Neuron_RUNTIME_VULKAN_SHADERS_H")
header_lines.append("#define Neuron_RUNTIME_VULKAN_SHADERS_H")
header_lines.append("")
header_lines.append("#include <stdint.h>")
header_lines.append("#include <stddef.h>")
header_lines.append("")
header_lines.extend(emit_array("kElementwiseBinarySpirv", load_words(elementwise_path)))
header_lines.extend(emit_array("kBinaryChainSpirv", load_words(binary_chain_path)))
header_lines.extend(emit_array("kBinaryThenFmaSpirv", load_words(binary_then_fma_path)))
header_lines.extend(emit_array("kBinaryChainThenFmaSpirv", load_words(binary_chain_then_fma_path)))
header_lines.extend(emit_array("kFmaSpirv", load_words(fma_path)))
header_lines.extend(emit_array("kMatMulDenseSpirv", load_words(matmul_dense_path)))
header_lines.extend(emit_array("kMatMulPackedSpirv", load_words(matmul_packed_path)))
header_lines.append("#endif // Neuron_RUNTIME_VULKAN_SHADERS_H")
header_lines.append("")

out_header.write_text("\n".join(header_lines), encoding="utf-8")
'@ | python - $elementwiseSpv $binaryChainSpv $binaryThenFmaSpv $binaryChainThenFmaSpv $fmaSpv $matmulDenseSpv $matmulPackedSpv $OutHeader

Write-Host "Generated SPIR-V header: $OutHeader"
