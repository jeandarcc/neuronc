$files = Get-ChildItem -Recurse -File -Exclude ".git", ".vs", "build", "bin", "obj", "*.exe", "*.dll", "*.pdb", "*.o", "*.obj", "*.lib", "node_modules"

foreach ($file in $files) {
    if ($file.FullName -match "\\.git\\") { continue }
    if ($file.FullName -match "\\build\\") { continue }
    if ($file.FullName -match "\\bin\\") { continue }
    if ($file.FullName -match "\\obj\\") { continue }

    try {
        $content = Get-Content -Path $file.FullName -Raw -ErrorAction SilentlyContinue
        if ($null -eq $content) { continue }

        $newContent = $content
        $changed = $false

        # Replace .nr -> .nr
        if ($newContent -match "\.nr") {
            $newContent = $newContent -replace "\.nr", ".nr"
            $changed = $true
        }

        # Replace Neuron -> Neuron
        if ($newContent -match "Neuron\+\+") {
            $newContent = $newContent -replace "Neuron\+\+", "Neuron"
            $changed = $true
        }

        # Replace Neuron -> Neuron
        if ($newContent -match "Neuron") {
            $newContent = $newContent -replace "Neuron", "Neuron"
            $changed = $true
        }

        # Replace Neuron -> Neuron
        if ($newContent -match "Neuron") {
            $newContent = $newContent -replace "Neuron", "Neuron"
            $changed = $true
        }

        if ($changed) {
            Set-Content -Path $file.FullName -Value $newContent -NoNewline -Encoding OpenAI # OpenAI encoding is not standard, let's omit or use UTF8
            Set-Content -Path $file.FullName -Value $newContent -NoNewline -Encoding UTF8
            Write-Host "Updated: $($file.FullName)"
        }
    }
    catch {
        Write-Host "Error processing $($file.FullName): $_"
    }
}

