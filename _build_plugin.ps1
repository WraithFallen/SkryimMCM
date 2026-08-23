# SkyLink SKSE plugin build — clone CommonLibSSE-NG (if missing), configure, build Release.
$ErrorActionPreference = "Stop"
$env:VCPKG_ROOT = "C:\vcpkg"
# cmake ships with VS 2022 BuildTools but isn't on PATH in a bare shell.
$cmakeBin = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin"
if (Test-Path $cmakeBin) { $env:PATH = "$cmakeBin;$env:PATH" }
$plugin = "C:\Modding\Skyforge\tools\skylink-src\SkyrimMCPPlugin"
Set-Location $plugin

if (-not (Test-Path "extern\CommonLibSSE-NG\CMakeLists.txt")) {
    Write-Host "=== Cloning CommonLibSSE-NG (shallow) ==="
    git clone --depth=1 https://github.com/CharmedBaryon/CommonLibSSE-NG.git extern\CommonLibSSE-NG
} else {
    Write-Host "=== CommonLibSSE-NG already present ==="
}

Write-Host "=== CMake configure (preset default) ==="
cmake --preset default
if ($LASTEXITCODE -ne 0) { throw "configure failed ($LASTEXITCODE)" }

Write-Host "=== CMake build Release ==="
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { throw "build failed ($LASTEXITCODE)" }

Write-Host "=== DONE ==="
Get-ChildItem -Recurse -Filter SkyrimMCPPlugin.dll build,output -ErrorAction SilentlyContinue | Select-Object FullName, Length, LastWriteTime
