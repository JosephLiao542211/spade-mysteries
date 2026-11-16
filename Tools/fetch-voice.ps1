# Downloads the pinned voice programs and models for the jam player, and checks their SHA-256.
#   hearing : whisper.cpp server + its English speech model (the detective's microphone)
#   speaking: sherpa-onnx text-to-speech + the Kokoro voice model (the suspects' voices)
# Usage (from the repo root):  powershell -ExecutionPolicy Bypass -File Tools\fetch-voice.ps1
# Files land in ThirdParty\whisper, ThirdParty\sherpa-onnx and Models\voice, all git-ignored (about 600 MB on disk).
# sherpa-onnx must stay on 1.x: 2.0 removed the pronunciation data path this Kokoro model needs.

$ErrorActionPreference = 'Stop'
$Root = Split-Path -Parent $PSScriptRoot

$Items = @(
    @{ Key = 'whisper.cpp b5454'; Kind = 'zip'
       Url = 'https://github.com/ggml-org/whisper.cpp/releases/download/b5454/whisper-bin-x64.zip'
       Sha = '6ba69e3482d7826214f90a6a9c84ca07782aec1e1d0c6a7c30c994fd5d816ccb'
       Dest = 'ThirdParty\whisper\b5454'; Have = 'ThirdParty\whisper\b5454\Release\whisper-server.exe' },
    @{ Key = 'whisper base.en model'; Kind = 'file'
       Url = 'https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.en.bin'
       Sha = 'a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002'
       Dest = 'Models\voice\ggml-base.en.bin'; Have = 'Models\voice\ggml-base.en.bin' },
    @{ Key = 'sherpa-onnx 1.13.8'; Kind = 'tar'
       Url = 'https://github.com/k2-fsa/sherpa-onnx/releases/download/v1.13.8/sherpa-onnx-v1.13.8-win-x64-shared-MT-Release.tar.bz2'
       Sha = '6dffdc715a4465b989446a6105265d2cb345e7101591a17d35534b6758f6e8df'
       Dest = 'ThirdParty\sherpa-onnx'
       Have = 'ThirdParty\sherpa-onnx\sherpa-onnx-v1.13.8-win-x64-shared-MT-Release\bin\sherpa-onnx-offline-tts.exe' },
    @{ Key = 'Kokoro voices v0.19'; Kind = 'tar'
       Url = 'https://github.com/k2-fsa/sherpa-onnx/releases/download/tts-models/kokoro-en-v0_19.tar.bz2'
       Sha = '912804855a04745fa77a30be545b3f9a5d15c4d66db00b88cbcd4921df605ac7'
       Dest = 'Models\voice'; Have = 'Models\voice\kokoro-en-v0_19\model.onnx' }
)

foreach ($it in $Items) {
    if (Test-Path (Join-Path $Root $it.Have)) { Write-Host "[ok]   $($it.Key) already present"; continue }
    $dest = Join-Path $Root $it.Dest
    if ($it.Kind -eq 'file') {
        New-Item -ItemType Directory -Force (Split-Path $dest) | Out-Null
        $tmp = "$dest.part"
    } else {
        New-Item -ItemType Directory -Force $dest | Out-Null
        $tmp = Join-Path $Root ("Models\voice\_download-" + [IO.Path]::GetFileName($it.Url) + ".part")
        New-Item -ItemType Directory -Force (Split-Path $tmp) | Out-Null
    }

    Write-Host "[get]  $($it.Key) <- $($it.Url)"
    & curl.exe -L --fail --retry 3 -C - -o $tmp $it.Url
    if ($LASTEXITCODE -ne 0) { throw "Download failed: $($it.Key)" }
    if ((Get-FileHash -Algorithm SHA256 $tmp).Hash.ToLower() -ne $it.Sha) { Remove-Item $tmp; throw "SHA-256 mismatch: $($it.Key)" }

    if ($it.Kind -eq 'file') {
        Move-Item -Force $tmp $dest
    } elseif ($it.Kind -eq 'zip') {
        $zip = $tmp -replace '\.part$', ''
        Move-Item -Force $tmp $zip
        Expand-Archive -Force $zip $dest
        Remove-Item $zip
    } else {
        # tar.exe ships with Windows 10 and later and reads .tar.bz2.
        & tar.exe -xjf $tmp -C $dest
        if ($LASTEXITCODE -ne 0) { throw "Could not unpack: $($it.Key)" }
        Remove-Item $tmp
    }
    if (-not (Test-Path (Join-Path $Root $it.Have))) { throw "Unpacked, but $($it.Have) is missing: $($it.Key)" }
    Write-Host "[done] $($it.Key)"
}
Write-Host 'Voice is installed. Start the game with: python Tools\jam-player\player.py'
