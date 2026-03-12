param(
  [string]$KernelDir = $PSScriptRoot,
  [string]$OutHeader = (Join-Path $PSScriptRoot "..\\src\\cuda_kernels.h"),
  [switch]$ForceRebuildPtx
)

$ErrorActionPreference = "Stop"

$ptxPath = Join-Path $KernelDir "tensor_ops.ptx"
$elementwiseCu = Join-Path $KernelDir "elementwise.cu"
$fmaCu = Join-Path $KernelDir "fma.cu"
$matmulDenseCu = Join-Path $KernelDir "matmul_dense.cu"
$matmulPackedCu = Join-Path $KernelDir "matmul_packed.cu"

$nvcc = Get-Command nvcc -ErrorAction SilentlyContinue
if ($ForceRebuildPtx -or $nvcc) {
  if (-not $nvcc) {
    throw "nvcc not found in PATH; cannot rebuild PTX with -ForceRebuildPtx."
  }

  $tmpCu = Join-Path $env:TEMP "npp_tensor_ops_combined.cu"
  @(
    Get-Content $elementwiseCu
    ""
    Get-Content $fmaCu
    ""
    Get-Content $matmulDenseCu
    ""
    Get-Content $matmulPackedCu
  ) | Set-Content -Encoding UTF8 -Path $tmpCu

  & $nvcc.Source -ptx $tmpCu -o $ptxPath
  if ($LASTEXITCODE -ne 0) {
    throw "nvcc failed to generate PTX."
  }
}

if (-not (Test-Path $ptxPath)) {
  throw "PTX file not found: $ptxPath"
}

@'
from pathlib import Path
import sys

ptx_path = Path(sys.argv[1])
out_header = Path(sys.argv[2])
text = ptx_path.read_text(encoding="utf-8")

lines = []
lines.append("#ifndef NPP_RUNTIME_CUDA_KERNELS_H")
lines.append("#define NPP_RUNTIME_CUDA_KERNELS_H")
lines.append("")
lines.append("static const char kCudaTensorOpsPtx[] =")
for raw in text.splitlines():
    escaped = raw.replace("\\\\", "\\\\\\\\").replace('"', '\\"')
    lines.append(f'    "{escaped}\\n"')
lines.append("    ;")
lines.append("")
lines.append("#endif // NPP_RUNTIME_CUDA_KERNELS_H")
lines.append("")

out_header.write_text("\n".join(lines), encoding="utf-8")
'@ | python - $ptxPath $OutHeader

Write-Host "Generated CUDA kernel header: $OutHeader"
