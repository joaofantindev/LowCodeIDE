param(
    [Parameter(Mandatory = $true)]
    [string] $Executable,
    [switch] $IncludeDesigner
)

$ErrorActionPreference = 'Stop'

$exePath = (Resolve-Path -LiteralPath $Executable).Path
$destination = Split-Path -Parent $exePath
$bash = 'C:\msys64\usr\bin\bash.exe'
$mingwBin = 'C:\msys64\mingw64\bin'

if (-not (Test-Path -LiteralPath $bash)) {
    throw "MSYS2 não encontrado em C:\msys64. Instale o MSYS2 e o pacote mingw-w64-x86_64-qt6-base."
}
if (-not (Test-Path -LiteralPath $mingwBin)) {
    throw "Pasta $mingwBin não encontrada."
}

$targets = @($exePath)
if ($IncludeDesigner) {
    $designer = Join-Path $mingwBin 'designer.exe'
    if (Test-Path -LiteralPath $designer) {
        $targets += $designer
        Copy-Item -LiteralPath $designer -Destination (Join-Path $destination 'designer.exe') -Force
    }
}

$unixTargets = @()
foreach ($target in $targets) {
    $unixTargets += '/c' + $target.Substring(2).Replace('\', '/')
}
$command = 'export PATH=/mingw64/bin:$PATH; ldd ' + ($unixTargets -join ' ') 

$previousPreference = $ErrorActionPreference
$ErrorActionPreference = 'Continue'
$lddOutput = & $bash -lc $command 2>$null
$ErrorActionPreference = $previousPreference

$dllNames = @()
foreach ($line in $lddOutput) {
    if ($line -match '/mingw64/bin/([^ ]+\.dll)') {
        $dllNames += $Matches[1]
    }
}
$dllNames = $dllNames | Sort-Object -Unique

foreach ($name in $dllNames) {
    $source = Join-Path $mingwBin $name
    if (Test-Path -LiteralPath $source) {
        Copy-Item -LiteralPath $source -Destination (Join-Path $destination $name) -Force
    }
}

Write-Host "Dependências MinGW copiadas: $($dllNames.Count) DLL(s) para $destination"
