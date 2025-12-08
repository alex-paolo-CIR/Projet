# Script de compilation ATC_Simulation
Write-Host "Nettoyage du repertoire de build..." -ForegroundColor Yellow
Remove-Item -Path ".\out" -Recurse -Force -ErrorAction SilentlyContinue

Write-Host "Initialisation de l'environnement Visual Studio x64..." -ForegroundColor Yellow
Push-Location "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build"
cmd /c "vcvars64.bat&set" |
ForEach-Object {
  if ($_ -match "=") {
    $v = $_.split("="); Set-Item -Force -Path "ENV:\$($v[0])"  -Value "$($v[1])"
  }
}
Pop-Location

$CMAKE_EXE = "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe"

Write-Host "Configuration du projet avec CMake..." -ForegroundColor Cyan
& $CMAKE_EXE -B out/build -G Ninja -DCMAKE_BUILD_TYPE=Release

if ($LASTEXITCODE -ne 0) {
    Write-Host "Erreur lors de la configuration!" -ForegroundColor Red
    exit 1
}

Write-Host "Compilation du projet..." -ForegroundColor Cyan
& $CMAKE_EXE --build out/build --target ATC_Simulation

if ($LASTEXITCODE -ne 0) {
    Write-Host "Erreur lors de la compilation!" -ForegroundColor Red
    exit 1
}

Write-Host "`nBuild termine avec succes!" -ForegroundColor Green
Write-Host "Executable: out\build\ATC_Simulation.exe" -ForegroundColor Green

if (Test-Path ".\out\build\ATC_Simulation.exe") {
    Write-Host "`nLancement de l'executable..." -ForegroundColor Yellow
    & ".\out\build\ATC_Simulation.exe"
} else {
    Write-Host "Erreur: Executable non trouve!" -ForegroundColor Red
}
