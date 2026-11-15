# Compila s1-mod (Release x64) sin abrir Visual Studio y copia s1_s_mp.exe a la carpeta del juego.
#
# Uso (desde la carpeta de s1-mod):
#   powershell -ExecutionPolicy Bypass -File .\build.ps1
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -Generate   # fuerza regenerar la solución (premake)
#   powershell -ExecutionPolicy Bypass -File .\build.ps1 -NoCopy     # solo compila, sin copiar al juego
#
# Si hay archivos .cpp/.hpp nuevos en src\client que no están en el proyecto, regenera la solución él solo.

param(
    [switch]$Generate,
    [switch]$NoCopy,
    [string]$GameDir = "D:\Program Files (x86)\SteamLibrary\steamapps\common\Call of Duty Advanced Warfare"
)

$ErrorActionPreference = "Stop"
$root = $PSScriptRoot
Set-Location $root

# Archivos nuevos que no están en client.vcxproj
$project = Join-Path $root "build\client.vcxproj"
if (-not $Generate -and (Test-Path $project)) {
    $projectText = Get-Content $project -Raw
    $missing = Get-ChildItem (Join-Path $root "src\client") -Recurse -Include *.cpp, *.hpp |
        Where-Object { -not $projectText.Contains($_.Name) }
    if ($missing) {
        Write-Host "Archivos nuevos: $($missing.Name -join ', ')" -ForegroundColor Yellow
        $Generate = $true
    }
}

# premake5.lua cambiado desde la ultima generacion
$stamp = Join-Path $root "build\premake.stamp"
$premakeLua = Join-Path $root "premake5.lua"
if (-not $Generate -and (-not (Test-Path $stamp) -or (Get-Item $premakeLua).LastWriteTime -gt (Get-Item $stamp).LastWriteTime)) {
    Write-Host "premake5.lua ha cambiado" -ForegroundColor Yellow
    $Generate = $true
}

if ($Generate -or -not (Test-Path $project)) {
    Write-Host "Regenerando la solucion (premake5 vs2022)..." -ForegroundColor Cyan
    & (Join-Path $root "tools\premake5.exe") vs2022
    if ($LASTEXITCODE -ne 0) { throw "premake5 ha fallado" }
    Set-Content -Path $stamp -Value (Get-Date).ToString("o")
}

# MSBuild de Visual Studio 2022
$vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
$msbuild = & $vswhere -latest -requires Microsoft.Component.MSBuild -find "MSBuild\**\Bin\MSBuild.exe" | Select-Object -First 1
if (-not $msbuild) { throw "No se encuentra MSBuild" }

Write-Host "Compilando..." -ForegroundColor Cyan
& $msbuild (Join-Path $root "build\s1-mod.sln") /t:client /p:Configuration=Release /p:Platform=x64 /m /v:minimal /nologo
if ($LASTEXITCODE -ne 0) {
    Write-Host "La compilacion ha fallado" -ForegroundColor Red
    exit 1
}

$exe = Join-Path $root "build\bin\x64\Release\s1_s_mp.exe"
Write-Host "Compilado: $exe" -ForegroundColor Green

if (-not $NoCopy) {
    try {
        Copy-Item $exe (Join-Path $GameDir "s1_s_mp.exe") -Force
        Write-Host "Copiado a $GameDir\s1_s_mp.exe" -ForegroundColor Green
    }
    catch {
        Write-Host "No se ha podido copiar al juego (esta abierto?): $($_.Exception.Message)" -ForegroundColor Red
        exit 1
    }
}
