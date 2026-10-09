$src = $PSScriptRoot
$finalOut = Join-Path $src "執行檔(C++)\AIClock.exe"
$tmpOut = Join-Path $env:TEMP "aiclock_build.exe"

$zig = Get-Command "zig" -ErrorAction SilentlyContinue
if (-not $zig) {
    $p = [Environment]::GetEnvironmentVariable("Path", "Machine")
    $u = [Environment]::GetEnvironmentVariable("Path", "User")
    $env:Path = $p + ";" + $u
    $zig = Get-Command "zig" -ErrorAction SilentlyContinue
    if (-not $zig) { Write-Output "Cannot find zig!"; exit 1 }
}

Write-Output "===== Compiling with Zig ====="
$drive = "Z:"
cmd /c "subst $drive /d 2>nul" | Out-Null
cmd /c "subst $drive `"$src`" 2>nul"
if ($LASTEXITCODE -ne 0) { Write-Output "subst failed!"; exit 1 }

& $zig.Source c++ -std=c++20 -O2 `
    "-Xlinker" "/subsystem:windows" `
    "-Xlinker" "--dynamicbase" "-Xlinker" "--nxcompat" "-Xlinker" "--high-entropy-va" `
    "${drive}\main.cpp" "${drive}\config.cpp" "${drive}\clock.rc" `
    -o "$tmpOut" `
    -lgdi32 -lgdiplus -lshell32 -lcomctl32 -lcomdlg32 -ladvapi32 -lole32 -luuid

$ok = $LASTEXITCODE -eq 0
cmd /c "subst $drive /d 2>nul" | Out-Null
if (-not $ok) { Write-Output "Compilation failed!"; exit 1 }

Copy-Item -LiteralPath "$tmpOut" -Destination "$finalOut" -Force
Remove-Item -LiteralPath "$tmpOut" -Force

Remove-Item -LiteralPath "$finalOut" -Stream "Zone.Identifier" -ErrorAction SilentlyContinue
# Not signing: self-signed certs trigger Defender more than unsigned binaries

Write-Output "===== Done: $finalOut ====="
