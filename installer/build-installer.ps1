param(
    [string] $Version = '0.1.0',
    [string] $BuildDir = ''
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrWhiteSpace($BuildDir)) {
    $BuildDir = Join-Path $root 'build'
}
$sourceDir = (Resolve-Path -LiteralPath $BuildDir).Path
$mainExe = Join-Path $sourceDir 'CppVisualIDE.exe'
if (-not (Test-Path -LiteralPath $mainExe)) {
    throw "Executável não encontrado: $mainExe. Compile a IDE antes de gerar o instalador."
}

$makensis = (Get-Command makensis.exe -ErrorAction SilentlyContinue).Source
if (-not $makensis) {
    foreach ($candidate in @(
        'C:\Program Files (x86)\NSIS\makensis.exe',
        'C:\Program Files\NSIS\makensis.exe'
    )) {
        if (Test-Path -LiteralPath $candidate) {
            $makensis = $candidate
            break
        }
    }
}
if (-not $makensis) {
    throw "NSIS (makensis.exe) não encontrado. Instale com: winget install -e --id NSIS.NSIS"
}

$templatePath = Join-Path $PSScriptRoot 'setup.nsi.in'
$outputFile = Join-Path $PSScriptRoot 'CppVisualIDE-Setup.exe'
$generatedScript = Join-Path $PSScriptRoot 'generated-setup.nsi'

$template = Get-Content -LiteralPath $templatePath -Raw

$dllLines = @()
foreach ($dll in (Get-ChildItem -LiteralPath $sourceDir -Filter '*.dll' -File)) {
    $dllLines += '    File "' + $dll.FullName + '"'
}
$designerExe = Join-Path $sourceDir 'designer.exe'
if (Test-Path -LiteralPath $designerExe) {
    $dllLines += '    File "' + $designerExe + '"'
}

$pluginLines = @()
foreach ($pluginDir in @('platforms', 'styles', 'imageformats', 'generic', 'networkinformation', 'tls')) {
    $pluginPath = Join-Path $sourceDir $pluginDir
    if (Test-Path -LiteralPath $pluginPath) {
        $pluginLines += '    SetOutPath "$INSTDIR\' + $pluginDir + '"'
        $pluginLines += '    File "' + $pluginPath + '\*.*"'
    }
}

$script = $template.Replace('__DLL_FILES__', ($dllLines -join "`r`n")).Replace('__PLUGIN_LINES__', ($pluginLines -join "`r`n")).Replace('__OUT_FILE__', $outputFile).Replace('__SOURCE_DIR__', $sourceDir).Replace('__VERSION__', $Version)

Set-Content -LiteralPath $generatedScript -Value $script -Encoding UTF8

Write-Host "Gerando instalador a partir de: $sourceDir"
& $makensis $generatedScript
if ($LASTEXITCODE -ne 0) {
    throw "makensis falhou com código $LASTEXITCODE"
}
Write-Host "Instalador pronto: $outputFile"
