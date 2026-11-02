# =============================================================================
# СЭДМ — сборка дистрибутива (п. 4.6.1 ТЗ)
#
# 1. Сборка 64- и 32-разрядной версий (CMake + MinGW 8.1, Qt 5.15.2).
# 2. Каталоги развёртывания stage\x64 и stage\x86: программы, библиотеки Qt
#    (windeployqt), драйвер QPSQL, клиентская библиотека PostgreSQL libpq.
# 3. Проверка: интеграционные тесты запускаются из каталога развёртывания
#    без Qt и PostgreSQL в PATH — дистрибутив самодостаточен.
# 4. Установщик Inno Setup: output\SEDM-<версия>-setup.exe.
#
# Запуск (PowerShell):  powershell -ExecutionPolicy Bypass -File build_installer.ps1
# Необходимое: Qt 5.15.2 (mingw81_64, mingw81_32), MinGW 8.1 (Qt Tools), CMake,
# Ninja, PostgreSQL 14+ x64 (libpq), Inno Setup 6. Недостающие файлы сторонних
# производителей (32-разрядная libpq из PostgreSQL 10, распространяемые пакеты
# Visual C++) загружаются в каталог cache при первом запуске.
# =============================================================================
param(
    [string]$QtRoot = 'C:\Qt',
    [string]$PgBinX64 = 'C:\Program Files\PostgreSQL\18\bin',
    [string]$Iscc = (Join-Path $env:LOCALAPPDATA 'Programs\Inno Setup 6\ISCC.exe'),
    [switch]$SkipTests
)
$ErrorActionPreference = 'Stop'
$here = $PSScriptRoot
$src = Split-Path -Parent $here                       # каталог sedm
$cache = Join-Path $here 'cache'
$stageRoot = Join-Path $here 'stage'
New-Item -ItemType Directory -Force $cache, $stageRoot | Out-Null

$version = (Select-String -Path (Join-Path $src 'CMakeLists.txt') -Pattern 'VERSION (\d+\.\d+\.\d+)').Matches[0].Groups[1].Value
Write-Host "СЭДМ $version — сборка дистрибутива"

$kits = @{
    x64 = @{ Qt = "$QtRoot\5.15.2\mingw81_64"; MinGW = "$QtRoot\Tools\mingw810_64" }
    x86 = @{ Qt = "$QtRoot\5.15.2\mingw81_32"; MinGW = "$QtRoot\Tools\mingw810_32" }
}

function Get-Download([string]$url, [string]$file) {
    $path = Join-Path $cache $file
    if (-not (Test-Path $path)) {
        Write-Host "Загрузка $file"
        Invoke-WebRequest -Uri $url -OutFile $path -UseBasicParsing
    }
    return $path
}

# --- сторонние файлы: распространяемые пакеты Visual C++ и 32-разрядная libpq
$vc64 = Get-Download 'https://aka.ms/vs/17/release/vc_redist.x64.exe' 'vc_redist.x64.exe'
$vc86 = Get-Download 'https://aka.ms/highdpimfc2013x86enu' 'vcredist_2013_x86.exe'
foreach ($f in @($vc64, $vc86)) {
    $sig = Get-AuthenticodeSignature $f
    if ($sig.Status -ne 'Valid' -or $sig.SignerCertificate.Subject -notlike '*Microsoft Corporation*') {
        throw "Подпись файла $f не подтверждена"
    }
}
$libpq86 = Join-Path $cache 'libpq-x86'
if (-not (Test-Path (Join-Path $libpq86 'libpq.dll'))) {
    # PostgreSQL 10 — последняя версия с 32-разрядными сборками для Windows;
    # libpq 10 поддерживает SCRAM-SHA-256 и работает с серверами 14 и новее.
    $zip = Get-Download 'https://get.enterprisedb.com/postgresql/postgresql-10.23-1-windows-binaries.zip' 'postgresql-10.23-1-windows-binaries.zip'
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    New-Item -ItemType Directory -Force $libpq86 | Out-Null
    $archive = [System.IO.Compression.ZipFile]::OpenRead($zip)
    try {
        foreach ($name in @('libpq.dll', 'libssl-1_1.dll', 'libcrypto-1_1.dll', 'libintl-8.dll', 'libiconv-2.dll')) {
            $entry = $archive.GetEntry("pgsql/bin/$name")
            [System.IO.Compression.ZipFileExtensions]::ExtractToFile($entry, (Join-Path $libpq86 $name), $true)
        }
    } finally {
        $archive.Dispose()
    }
}
$libpqFiles = @{
    x64 = @{ Dir = $PgBinX64; Files = @('libpq.dll', 'libssl-3-x64.dll', 'libcrypto-3-x64.dll', 'libintl-9.dll', 'libiconv-2.dll') }
    x86 = @{ Dir = $libpq86;  Files = @('libpq.dll', 'libssl-1_1.dll', 'libcrypto-1_1.dll', 'libintl-8.dll', 'libiconv-2.dll') }
}

foreach ($arch in @('x64', 'x86')) {
    $kit = $kits[$arch]
    $build = Join-Path $src "build-$arch"
    $env:PATH = "$($kit.MinGW)\bin;$($kit.Qt)\bin;$env:PATH"

    # --- 1. сборка
    Write-Host "[$arch] сборка"
    & cmake -S $src -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release "-DCMAKE_PREFIX_PATH=$($kit.Qt)" `
        "-DCMAKE_C_COMPILER=$($kit.MinGW)\bin\gcc.exe" "-DCMAKE_CXX_COMPILER=$($kit.MinGW)\bin\g++.exe" | Out-Null
    if ($LASTEXITCODE -ne 0) { throw "[$arch] ошибка настройки CMake" }
    & cmake --build $build
    if ($LASTEXITCODE -ne 0) { throw "[$arch] ошибка сборки" }

    # --- 2. каталог развёртывания
    $stage = Join-Path $stageRoot $arch
    if (Test-Path $stage) { Remove-Item -Recurse -Force $stage }
    New-Item -ItemType Directory -Force $stage | Out-Null
    Copy-Item "$build\bin\sedm_admin.exe", "$build\bin\sedm_taster.exe" $stage
    # Состав Qt задаётся явно: windeployqt 5.15.2 ошибочно считает модули MinGW-сборки
    # отладочными и пропускает их. Нужны: модули Qt, модуль платформы Windows, стиль,
    # печать, драйвер PostgreSQL, форматы изображений, среда MinGW, перевод qtbase_ru.
    foreach ($m in @('Qt5Core', 'Qt5Gui', 'Qt5Widgets', 'Qt5Sql', 'Qt5Svg', 'Qt5PrintSupport')) {
        Copy-Item "$($kit.Qt)\bin\$m.dll" $stage
    }
    $plugins = @{
        'platforms' = @('qwindows.dll'); 'styles' = @('qwindowsvistastyle.dll');
        'printsupport' = @('windowsprintersupport.dll'); 'sqldrivers' = @('qsqlpsql.dll');
        'imageformats' = @('qsvg.dll', 'qico.dll', 'qjpeg.dll', 'qgif.dll'); 'iconengines' = @('qsvgicon.dll')
    }
    foreach ($dir in $plugins.Keys) {
        New-Item -ItemType Directory -Force "$stage\$dir" | Out-Null
        foreach ($p in $plugins[$dir]) { Copy-Item "$($kit.Qt)\plugins\$dir\$p" "$stage\$dir" }
    }
    $gcc = if ($arch -eq 'x64') { 'libgcc_s_seh-1.dll' } else { 'libgcc_s_dw2-1.dll' }
    foreach ($r in @($gcc, 'libstdc++-6.dll', 'libwinpthread-1.dll')) { Copy-Item "$($kit.MinGW)\bin\$r" $stage }
    New-Item -ItemType Directory -Force "$stage\translations" | Out-Null
    Copy-Item "$($kit.Qt)\translations\qtbase_ru.qm" "$stage\translations"
    # клиентская библиотека PostgreSQL и её зависимости
    foreach ($f in $libpqFiles[$arch].Files) { Copy-Item (Join-Path $libpqFiles[$arch].Dir $f) $stage }

    # --- 3. проверка самодостаточности: тесты из копии каталога развёртывания
    if (-not $SkipTests -and $env:SEDM_TEST_PGSUPERPASS) {
        $check = Join-Path $env:TEMP "sedm_stage_check_$arch"
        if (Test-Path $check) { Remove-Item -Recurse -Force $check }
        Copy-Item -Recurse $stage $check
        # тесты и то, что нужно только им: Qt Test и платформа offscreen (окна не показываются)
        Copy-Item "$build\bin\tst_services.exe", "$build\bin\tst_taster_ui.exe", "$build\bin\tst_admin_ui.exe" $check
        Copy-Item "$($kit.Qt)\bin\Qt5Test.dll" $check
        Copy-Item "$($kit.Qt)\plugins\platforms\qoffscreen.dll" "$check\platforms"
        $env:QT_QPA_FONTDIR = "$env:SystemRoot\Fonts"
        $saved = $env:PATH
        $env:PATH = "$env:SystemRoot\System32;$env:SystemRoot;$env:SystemRoot\System32\Wbem"   # без Qt и PostgreSQL
        if ($arch -eq 'x86') {
            # среда Visual C++ 2013 (x86) для libpq 10 — из распространяемого пакета (если не установлена)
            if (-not (Test-Path "$env:SystemRoot\SysWOW64\msvcr120.dll")) {
                Write-Host '[x86] среда Visual C++ 2013 не установлена — проверка x86 пропущена'
                $env:PATH = $saved
                continue
            }
        }
        Push-Location $check
        $failed = @()
        foreach ($t in @('tst_services', 'tst_taster_ui', 'tst_admin_ui')) {
            & ".\$t.exe" -o "$t.txt,txt" | Out-Null
            if ($LASTEXITCODE -ne 0) { $failed += $t }
            Write-Host "[$arch] $t из каталога развёртывания: $((Get-Content "$t.txt" | Select-String 'Totals').Line)"
        }
        Pop-Location
        $env:PATH = $saved
        if ($failed.Count -gt 0) { throw "[$arch] не пройдены: $($failed -join ', ')" }
    }
}

# --- 4. установщик
$redist = Join-Path $here 'redist'
New-Item -ItemType Directory -Force $redist | Out-Null
Copy-Item $vc64, $vc86 $redist
& $Iscc "/DAppVersion=$version" (Join-Path $here 'sedm.iss')
if ($LASTEXITCODE -ne 0) { throw 'Ошибка сборки установщика' }
Write-Host "Готово: $(Join-Path $here "output\SEDM-$version-setup.exe")"
