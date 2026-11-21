# Checks a built Mozlumi MSI and gives it the release name.
#
# - Reads the MSI Property table read-only, the same way the Windows updater
#   (src/win32/updater/mozlumi_update.ps1) does, and checks ProductName,
#   ProductVersion (<Version>.<Build>), Manufacturer and UpgradeCode.
# - Copies the MSI to <OutDir>\Mozlumi_v<Version>_build<Build>_windows_x64.msi
#   and writes "<that name>.sha256" as "<sha256>  <name>" + LF (UTF-8 without
#   BOM), the format the updater reads.
# - With -SummaryPath, appends a Markdown summary (GITHUB_STEP_SUMMARY).
#
# Used by .github/workflows/mozlumi-windows-build.yml. Written for Windows
# PowerShell 5.1, like the updater. Keep this file ASCII.
#
# Try it on a local build before changing the workflow, for example:
#   powershell -NoProfile -ExecutionPolicy Bypass -File mozlumi_check_msi.ps1 `
#       -MsiPath src\bazel-bin\win32\installer\Mozlumi64.msi `
#       -Version 1.3.0 -Build 45 -OutDir $env:TEMP\mozlumi-msi-check
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)][string]$MsiPath,
  [Parameter(Mandatory = $true)][string]$Version,
  [Parameter(Mandatory = $true)][string]$Build,
  [Parameter(Mandatory = $true)][string]$OutDir,
  [string]$Source = '',
  [string]$SummaryPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

if ($Version -notmatch '^\d+\.\d+\.\d+$') {
  throw "-Version must look like 1.3.0, got '$Version'."
}
if ($Build -notmatch '^\d+$') {
  throw "-Build must be a number, got '$Build'."
}

$expected = [ordered]@{
  ProductName    = 'Mozlumi'
  ProductVersion = "$Version.$Build"
  Manufacturer   = 'Curren Lumi Project'
  UpgradeCode    = '{584AEC93-7347-4103-AF0D-8C17411BE02F}'
}

function Invoke-Com($Object, [string]$Name, [string]$Kind, [object[]]$Arguments) {
  $flags = if ($Kind -eq 'Get') { 'GetProperty' } else { 'InvokeMethod' }
  return $Object.GetType().InvokeMember($Name, $flags, $null, $Object, $Arguments)
}

function Release-Com($Object) {
  if ($null -ne $Object) {
    [void][Runtime.InteropServices.Marshal]::FinalReleaseComObject($Object)
  }
}

# Same as Get-MsiProperties in mozlumi_update.ps1. $Path must be a plain
# [string]: COM rejects a wrapped object (such as what Join-Path returns) in
# the argument array with "Type mismatch".
function Get-MsiProperties([string]$Path) {
  $result = @{}
  $installer = $null
  $database = $null
  try {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    # 0: msiOpenDatabaseModeReadOnly.
    $database = Invoke-Com $installer 'OpenDatabase' 'Call' @($Path, 0)
    foreach ($name in $expected.Keys) {
      $view = $null
      $record = $null
      try {
        $view = Invoke-Com $database 'OpenView' 'Call' @(
            ('SELECT `Value` FROM `Property` WHERE `Property` = ''{0}''' -f $name))
        [void](Invoke-Com $view 'Execute' 'Call' @())
        $record = Invoke-Com $view 'Fetch' 'Call' @()
        if ($null -ne $record) {
          $result[$name] = [string](Invoke-Com $record 'StringData' 'Get' @(1))
        }
        [void](Invoke-Com $view 'Close' 'Call' @())
      } finally {
        Release-Com $record
        Release-Com $view
      }
    }
  } finally {
    Release-Com $database
    Release-Com $installer
    [GC]::Collect()
    [GC]::WaitForPendingFinalizers()
  }
  return $result
}

if (-not (Test-Path -LiteralPath $MsiPath -PathType Leaf)) {
  throw "MSI not found: $MsiPath"
}
$msi = [string](Resolve-Path -LiteralPath $MsiPath).ProviderPath

$props = Get-MsiProperties $msi
$problems = @()
foreach ($key in $expected.Keys) {
  $actual = if ($props.ContainsKey($key)) { [string]$props[$key] } else { '' }
  Write-Host ('{0,-15} {1}' -f $key, $actual)
  if ($actual -ne $expected[$key]) {
    $problems += "$key is '$actual', expected '$($expected[$key])'."
  }
}
if ($problems.Count -ne 0) {
  throw ($problems -join ' ')
}

New-Item -ItemType Directory -Force -Path $OutDir | Out-Null
$outDirFull = [string](Resolve-Path -LiteralPath $OutDir).ProviderPath
$name = "Mozlumi_v${Version}_build${Build}_windows_x64.msi"
$out = [string](Join-Path $outDirFull $name)
Copy-Item -LiteralPath $msi -Destination $out -Force
$hash = (Get-FileHash -LiteralPath $out -Algorithm SHA256).Hash.ToLowerInvariant()
$utf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText("$out.sha256", "$hash  $name`n", $utf8)
$size = (Get-Item -LiteralPath $out).Length

if (-not [string]::IsNullOrEmpty($SummaryPath)) {
  $lines = @("## $name", '', '| | |', '| --- | --- |')
  if (-not [string]::IsNullOrEmpty($Source)) {
    $lines += "| Source | $Source |"
  }
  $lines += "| Version | $Version Build $Build (MSI $($props['ProductVersion'])) |"
  $lines += "| Size | $size bytes |"
  $lines += "| SHA-256 | ``$hash`` |"
  $lines += ''
  $lines += "Files: ``$name``, ``$name.sha256``"
  $lines += ''
  [IO.File]::AppendAllText($SummaryPath, ($lines -join "`n"), $utf8)
}

Write-Host "$hash  $name ($size bytes)"
Write-Host "Written to $outDirFull"
