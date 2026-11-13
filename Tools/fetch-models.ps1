# Downloads the pinned llama.cpp builds and model files, and checks their SHA-256.
# Usage (from the repo root):  powershell -ExecutionPolicy Bypass -File Tools\fetch-models.ps1 [-Only vulkan,cpu,e4b,e2b]
# Files land in ThirdParty\llama\<build>\ and Models\, both git-ignored.

param(
    [string[]]$Only = @()
)

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot
# With -File, "-Only a,b" arrives as one string; split it.
$Only = @($Only | ForEach-Object { $_ -split ',' } | ForEach-Object { $_.Trim() } | Where-Object { $_ })

# Pinned versions. Change these together and update docs/llm-implementation-plan.md.
$LlamaTag = 'b12368'
$Items = @(
    @{ Key = 'vulkan'; Kind = 'zip'
       Url = "https://github.com/ggml-org/llama.cpp/releases/download/$LlamaTag/llama-$LlamaTag-bin-win-vulkan-x64.zip"
       Sha = '49a9ad97c9d7fe568696e7d760334b4330900721763f12f4353de573dbbca39e'
       Dest = "ThirdParty\llama\$LlamaTag-vulkan" },
    @{ Key = 'cpu'; Kind = 'zip'
       Url = "https://github.com/ggml-org/llama.cpp/releases/download/$LlamaTag/llama-$LlamaTag-bin-win-cpu-x64.zip"
       Sha = 'd81c941ecee91006bbc5b64f5121e5a89e56a36e72bbe0e72120527751ae3dee'
       Dest = "ThirdParty\llama\$LlamaTag-cpu" },
    @{ Key = 'e4b'; Kind = 'file'
       Url = 'https://huggingface.co/unsloth/gemma-4-E4B-it-GGUF/resolve/bfc15c382204943c3a8fff0c750b94ae2364d7a3/gemma-4-E4B-it-Q4_K_M.gguf'
       Sha = '85a896a047553e842f25297ee5b031d64ff30147d9c4af17b1e4b394cd1fab87'
       Dest = 'Models\gemma-4-E4B-it-Q4_K_M.gguf' },
    @{ Key = 'e2b'; Kind = 'file'
       Url = 'https://huggingface.co/unsloth/gemma-4-E2B-it-GGUF/resolve/0314792d7f1f7e229411f620751375812bb9faf2/gemma-4-E2B-it-Q4_K_M.gguf'
       Sha = '740185b21d22ceb83a11c3aa62ad5842ef32c70f6096d756bbee85a1e4ec34b8'
       Dest = 'Models\gemma-4-E2B-it-Q4_K_M.gguf' }
)

function Test-Sha($Path, $Expected) {
    (Get-FileHash -Algorithm SHA256 $Path).Hash.ToLower() -eq $Expected
}

foreach ($it in $Items) {
    if ($Only.Count -gt 0 -and $Only -notcontains $it.Key) { continue }
    $dest = Join-Path $Root $it.Dest

    if ($it.Kind -eq 'file') {
        New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
        if ((Test-Path $dest) -and (Test-Sha $dest $it.Sha)) { Write-Host "[ok]   $($it.Key) already present"; continue }
        $tmp = "$dest.part"
    } else {
        if (Test-Path (Join-Path $dest 'llama-server.exe')) { Write-Host "[ok]   $($it.Key) already present"; continue }
        $tmp = Join-Path $Root "ThirdParty\llama\_download-$($it.Key).zip.part"
        New-Item -ItemType Directory -Force (Split-Path $tmp) | Out-Null
    }

    Write-Host "[get]  $($it.Key) <- $($it.Url)"
    & curl.exe -L --fail --retry 3 -C - -o $tmp $it.Url
    if ($LASTEXITCODE -ne 0) { throw "Download failed: $($it.Key)" }
    if (-not (Test-Sha $tmp $it.Sha)) { Remove-Item $tmp; throw "SHA-256 mismatch: $($it.Key)" }

    if ($it.Kind -eq 'file') {
        Move-Item -Force $tmp $dest
    } else {
        if (Test-Path $dest) { throw "$dest exists but has no llama-server.exe; remove it and retry" }
        New-Item -ItemType Directory $dest | Out-Null
        $zip = $tmp -replace '\.part$', ''
        Move-Item -Force $tmp $zip
        Expand-Archive -Path $zip -DestinationPath $dest
        Remove-Item $zip
        # Some release zips nest everything in one folder; flatten it.
        $inner = Get-ChildItem $dest
        if ($inner.Count -eq 1 -and $inner[0].PSIsContainer) {
            Get-ChildItem $inner[0].FullName | Move-Item -Destination $dest
            Remove-Item $inner[0].FullName
        }
    }
    Write-Host "[done] $($it.Key) -> $($it.Dest)"
}
