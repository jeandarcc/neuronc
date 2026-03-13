$root = "C:\Users\yusuf\Desktop\Neuron"
Write-Host "Scanning $root..."
$files = Get-ChildItem -Path $root -Recurse -File

Write-Host "Found $($files.Count) files."

foreach ($file in $files) {
    # Skip .git etc
    if ($file.FullName -match "\\.git\\") { continue }
    if ($file.FullName -match "\\build\\") { continue }
    if ($file.FullName -match "\\bin\\") { continue }
    if ($file.FullName -match "\\obj\\") { continue }
    if ($file.Extension -eq ".exe" -or $file.Extension -eq ".dll" -or $file.Extension -eq ".obj" -or $file.Extension -eq ".lib" -or $file.Extension -eq ".pdb") { continue }

    try {
        $content = Get-Content -LiteralPath $file.FullName -Raw -ErrorAction Stop
        if ([string]::IsNullOrEmpty($content)) { continue }

        $newContent = $content
        $changed = $false

        # Replace .nr -> .nr
        if ($newContent -match "\.nr") {
            $newContent = $newContent -replace "\.nr", ".nr"
            $changed = $true
        }

        # Replace Neuron -> Neuron
        # Use regex carefully to avoid matching Neuron -> Neuron
        if ($newContent -match "Neuron\+\+") {
            $newContent = $newContent -replace "Neuron\+\+", "Neuron"
            $changed = $true
        }

        if ($newContent -match "Neuron") {
            $newContent = $newContent -replace "Neuron", "Neuron"
            $changed = $true
        }

        if ($newContent -match "Neuron") {
            $newContent = $newContent -replace "Neuron", "Neuron"
            $changed = $true
        }

        if ($changed) {
            Set-Content -LiteralPath $file.FullName -Value $newContent -NoNewline -Encoding UTF8
            Write-Host "Updated: $($file.Name)"
        }
    }
    catch {
        Write-Host "Error processing $($file.Name): $_"
    }
}

