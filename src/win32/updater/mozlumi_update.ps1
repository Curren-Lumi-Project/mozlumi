# Copyright 2026 Curren Lumi Project.
# All rights reserved.
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions are
# met:
#
#     * Redistributions of source code must retain the above copyright
# notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above
# copyright notice, this list of conditions and the following disclaimer
# in the documentation and/or other materials provided with the
# distribution.
#     * Neither the name of the copyright holder nor the names of its
# contributors may be used to endorse or promote products derived from
# this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
# A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
# OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
# SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
# LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
# DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
# THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
# (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
# OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

# Mozlumi updater for Windows (the counterpart of mac/updater/mozlumi_update.zsh).
#
# Checks the latest release of Curren-Lumi-Project/mozlumi on GitHub. When a
# newer version is found, asks the user, downloads the MSI, verifies its
# SHA-256 against the ".sha256" file of the same release and that it is
# Mozlumi's MSI, and installs it with the user's administrator approval.
# This is the only part of Mozlumi that talks to the network.
#
# Usage (mozlumi_tool.exe starts it without a console window):
#   mozlumi_update.ps1 -Mode Auto     From the scheduled task. At most once a
#                                     day, silent unless a new version is found.
#   mozlumi_update.ps1 -Mode Manual   From the input menu. Always reports.
#   mozlumi_update.ps1 -Mode Manual -Tag v1.3.0-build38
#                                     Checks that release of this repository
#                                     instead of the latest one (for testing
#                                     pre-releases).
#   mozlumi_update.ps1 -Mode Install ...
#                                     Internal: the step run as administrator.
#
# Release convention: tag "v<major>.<minor>.<patch>-build<N>" with assets
# "Mozlumi_v<version>_build<N>_windows_x64.msi" and "<that name>.sha256".
#
# Keep this file in UTF-8 with BOM: Windows PowerShell 5.1 reads a file
# without BOM in the ANSI code page.

param(
  [ValidateSet('Auto', 'Manual', 'Install')]
  [string]$Mode = 'Manual',
  [string]$Tag = '',
  [string]$Source = '',
  [string]$Sha256 = '',
  [string]$ExpectedVersion = ''
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$Repo = 'Curren-Lumi-Project/mozlumi'
$ApiBase = "https://api.github.com/repos/$Repo/releases"
# Must match build_installer.py and installer_oss_64bit.wxs.
$UpgradeCode = '{584AEC93-7347-4103-AF0D-8C17411BE02F}'
$ProductName = 'Mozlumi'
$Manufacturer = 'Curren Lumi Project'
$InstallDir = Join-Path ${env:ProgramFiles(x86)} 'Mozlumi'
$ToolPath = Join-Path $InstallDir 'mozlumi_tool.exe'
$TagPattern = '^v([0-9]+)\.([0-9]+)\.([0-9]+)-build([0-9]+)$'
# The detailed log of the last installation by the updater. It stays on this
# computer (the user's temporary folder) and is overwritten each time.
$InstallLogPath = Join-Path ([IO.Path]::GetTempPath()) 'Mozlumi_update_install.log'

# Exit codes of the administrator step (also used by the Mac updater).
$ExitBadRequest = 70
$ExitCopyFailed = 71
$ExitHashChanged = 72
$ExitNotMozlumi = 73

# --------------------------------------------------------------------------
# Common helpers

function Get-Sha256([string]$Path) {
  $stream = [IO.File]::OpenRead($Path)
  try {
    $sha = [Security.Cryptography.SHA256]::Create()
    try {
      return (($sha.ComputeHash($stream) | ForEach-Object { $_.ToString('x2') }) -join '')
    } finally {
      $sha.Dispose()
    }
  } finally {
    $stream.Dispose()
  }
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

# Reads ProductName, Manufacturer, UpgradeCode and ProductVersion of an MSI
# without installing it. The file is closed before returning.
function Get-MsiProperties([string]$Path) {
  $result = @{}
  $installer = $null
  $database = $null
  try {
    $installer = New-Object -ComObject WindowsInstaller.Installer
    # 0: msiOpenDatabaseModeReadOnly.
    $database = Invoke-Com $installer 'OpenDatabase' 'Call' @($Path, 0)
    foreach ($name in @('ProductName', 'Manufacturer', 'UpgradeCode', 'ProductVersion')) {
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

# True if the MSI is Mozlumi's and of the expected version (e.g. 1.3.0.38).
function Test-MozlumiMsi([string]$Path, [string]$Version) {
  try {
    $p = Get-MsiProperties $Path
  } catch {
    return $false
  }
  return ($p.ContainsKey('UpgradeCode') -and $p['UpgradeCode'] -eq $UpgradeCode -and
          $p.ContainsKey('ProductName') -and $p['ProductName'] -ceq $ProductName -and
          $p.ContainsKey('Manufacturer') -and $p['Manufacturer'] -ceq $Manufacturer -and
          $p.ContainsKey('ProductVersion') -and $p['ProductVersion'] -ceq $Version)
}

# --------------------------------------------------------------------------
# The administrator step

function Test-SamePath([string]$A, [string]$B) {
  return [string]::Equals([IO.Path]::GetFullPath($A).TrimEnd('\'),
                          [IO.Path]::GetFullPath($B).TrimEnd('\'),
                          [StringComparison]::OrdinalIgnoreCase)
}

function Invoke-Install {
  $principal = New-Object Security.Principal.WindowsPrincipal(
      [Security.Principal.WindowsIdentity]::GetCurrent())
  if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    return $ExitBadRequest
  }
  if ($Sha256 -notmatch '^[0-9a-f]{64}$' -or
      $ExpectedVersion -notmatch '^[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+$' -or
      [string]::IsNullOrEmpty($Source)) {
    return $ExitBadRequest
  }
  # Only run as the installed copy, whose folder only administrators can write.
  if (-not (Test-SamePath $PSScriptRoot $InstallDir)) {
    return $ExitBadRequest
  }
  # Any process of the user can change the downloaded MSI until it is
  # installed. So copy it to a new folder in the install folder, which only
  # administrators can write, check the SHA-256 and that it is Mozlumi's MSI
  # again there, and install that copy.
  $dir = Join-Path $InstallDir ('update-' + [guid]::NewGuid().ToString('N'))
  try {
    [void](New-Item -ItemType Directory -Path $dir)
  } catch {
    return $ExitCopyFailed
  }
  try {
    $copy = Join-Path $dir 'Mozlumi.msi'
    try {
      Copy-Item -LiteralPath $Source -Destination $copy
    } catch {
      return $ExitCopyFailed
    }
    if ((Get-Sha256 $copy) -ne $Sha256) {
      return $ExitHashChanged
    }
    if (-not (Test-MozlumiMsi $copy $ExpectedVersion)) {
      return $ExitNotMozlumi
    }
    # Do not keep the install folder as the current directory while the
    # installer replaces files in it.
    Set-Location -LiteralPath $env:SystemRoot
    $msiexec = Join-Path $env:SystemRoot 'System32\msiexec.exe'
    $process = Start-Process -FilePath $msiexec -Wait -PassThru -ArgumentList (
        '/i "' + $copy + '" /norestart /l*v "' + $InstallLogPath + '"')
    return $process.ExitCode
  } finally {
    Remove-Item -LiteralPath $dir -Recurse -Force -ErrorAction SilentlyContinue
  }
}

# --------------------------------------------------------------------------
# Windows
#
# The windows are shown by "mozlumi_tool.exe --mode=update_dialog"
# (gui/update_dialog) in the look of the other Mozlumi windows. If the tool
# cannot show them, plain Windows Forms windows are used instead, so that an
# update never stops because of a window.

# Exit codes of the question window. Must match gui/update_dialog.
$ToolExitUpdate = 10
$ToolExitLater = 11
$ToolExitSkip = 12

function Initialize-Ui {
  Add-Type -AssemblyName System.Windows.Forms
  Add-Type -AssemblyName System.Drawing
  [System.Windows.Forms.Application]::EnableVisualStyles()
}

# --update_dialog_text: a line break is "\n", and the whole argument is quoted.
function ConvertTo-ToolText([string]$Text) {
  return $Text.Replace("`r", '').Replace("`n", '\n').Replace('"', "'").TrimEnd('\')
}

# Shows a window by mozlumi_tool.exe and returns its exit code, or $null if
# the tool cannot be started.
function Invoke-ToolWindow([string]$Type, [string]$Text) {
  if (-not (Test-Path -LiteralPath $ToolPath -PathType Leaf)) {
    return $null
  }
  try {
    $process = Start-Process -FilePath $ToolPath -Wait -PassThru -ArgumentList @(
        '--mode=update_dialog', "--update_dialog_type=$Type",
        ('"--update_dialog_text=' + (ConvertTo-ToolText $Text) + '"'))
    return $process.ExitCode
  } catch {
    return $null
  }
}

$script:ProgressFile = $null
$script:ProgressProcess = $null
$script:ProgressForm = $null
$script:ProgressLabel = $null
$script:ProgressBar = $null

# The progress window of the tool follows this file: "<fraction><TAB><text>"
# or "quit". The file is replaced as a whole.
function Write-ProgressFile([string]$Content) {
  if ($null -eq $script:ProgressFile) {
    return
  }
  $temp = $script:ProgressFile + '.tmp'
  try {
    [IO.File]::WriteAllText($temp, $Content, (New-Object Text.UTF8Encoding($false)))
    Move-Item -LiteralPath $temp -Destination $script:ProgressFile -Force
  } catch {
    # The window may be reading the file; the next update writes it again.
  }
}

function Start-FormsProgress {
  $form = New-Object System.Windows.Forms.Form
  $form.Text = 'Mozlumi'
  $form.Font = [System.Drawing.SystemFonts]::MessageBoxFont
  $form.FormBorderStyle = 'FixedDialog'
  $form.ControlBox = $false
  $form.StartPosition = 'CenterScreen'
  $form.TopMost = $true
  $form.ClientSize = New-Object System.Drawing.Size(460, 92)
  $label = New-Object System.Windows.Forms.Label
  $label.Location = New-Object System.Drawing.Point(16, 16)
  $label.Size = New-Object System.Drawing.Size(428, 24)
  $label.Text = '準備中…'
  $bar = New-Object System.Windows.Forms.ProgressBar
  $bar.Location = New-Object System.Drawing.Point(16, 52)
  $bar.Size = New-Object System.Drawing.Size(428, 22)
  $bar.Maximum = 1000
  $bar.Style = 'Marquee'
  $form.Controls.Add($label)
  $form.Controls.Add($bar)
  $form.Show()
  $script:ProgressForm = $form
  $script:ProgressLabel = $label
  $script:ProgressBar = $bar
  [System.Windows.Forms.Application]::DoEvents()
}

# Shows the progress window. $Dir is the work folder for the progress file.
function Start-Progress([string]$Dir) {
  $script:ProgressFile = Join-Path $Dir 'progress.txt'
  Write-ProgressFile "-1`t準備中…"
  if (Test-Path -LiteralPath $ToolPath -PathType Leaf) {
    try {
      $script:ProgressProcess = Start-Process -FilePath $ToolPath -PassThru -ArgumentList @(
          '--mode=update_dialog', '--update_dialog_type=progress',
          ('"--update_progress_file=' + $script:ProgressFile + '"'),
          "--update_parent_pid=$PID")
      return
    } catch {
      $script:ProgressProcess = $null
    }
  }
  Start-FormsProgress
}

# Fraction 0..1, or a negative number for an indeterminate bar.
function Set-Progress([double]$Fraction, [string]$Text) {
  if ($null -ne $script:ProgressProcess) {
    Write-ProgressFile ($Fraction.ToString([Globalization.CultureInfo]::InvariantCulture) +
                        "`t" + $Text)
    return
  }
  if ($null -eq $script:ProgressForm) {
    return
  }
  $script:ProgressLabel.Text = $Text
  if ($Fraction -lt 0) {
    $script:ProgressBar.Style = 'Marquee'
  } else {
    $script:ProgressBar.Style = 'Continuous'
    $script:ProgressBar.Value = [int]([Math]::Min(1.0, $Fraction) * 1000)
  }
  [System.Windows.Forms.Application]::DoEvents()
}

function Stop-Progress {
  if ($null -ne $script:ProgressProcess) {
    Write-ProgressFile 'quit'
    try {
      if (-not $script:ProgressProcess.WaitForExit(3000)) {
        $script:ProgressProcess.Kill()
      }
    } catch {
    }
    $script:ProgressProcess = $null
  }
  if ($null -ne $script:ProgressForm) {
    $script:ProgressForm.Close()
    $script:ProgressForm.Dispose()
    $script:ProgressForm = $null
  }
}

function Show-Message([string]$Text) {
  Stop-Progress
  if ((Invoke-ToolWindow 'message' $Text) -eq 0) {
    return
  }
  # An invisible top-most owner keeps the message in front of other windows.
  $owner = New-Object System.Windows.Forms.Form
  $owner.TopMost = $true
  $owner.ShowInTaskbar = $false
  $owner.Opacity = 0
  $owner.StartPosition = 'CenterScreen'
  $owner.Show()
  try {
    [void][System.Windows.Forms.MessageBox]::Show($owner, $Text, 'Mozlumi')
  } finally {
    $owner.Dispose()
  }
}

# Returns 'update', 'later' or 'skip'.
function Show-UpdateQuestion([string]$Text) {
  Stop-Progress
  $code = Invoke-ToolWindow 'question' $Text
  if ($code -eq $ToolExitUpdate) {
    return 'update'
  }
  if ($code -eq $ToolExitLater) {
    return 'later'
  }
  if ($code -eq $ToolExitSkip) {
    return 'skip'
  }
  $form = New-Object System.Windows.Forms.Form
  $form.Text = 'Mozlumi'
  $form.Font = [System.Drawing.SystemFonts]::MessageBoxFont
  $form.FormBorderStyle = 'FixedDialog'
  $form.MaximizeBox = $false
  $form.MinimizeBox = $false
  $form.StartPosition = 'CenterScreen'
  $form.TopMost = $true
  $form.AutoSize = $true
  $form.AutoSizeMode = 'GrowAndShrink'
  $form.Padding = New-Object System.Windows.Forms.Padding(16)
  $panel = New-Object System.Windows.Forms.FlowLayoutPanel
  $panel.FlowDirection = 'TopDown'
  $panel.AutoSize = $true
  $panel.WrapContents = $false
  $label = New-Object System.Windows.Forms.Label
  $label.Text = $Text
  $label.AutoSize = $true
  $label.MaximumSize = New-Object System.Drawing.Size(440, 0)
  $label.Margin = New-Object System.Windows.Forms.Padding(0, 0, 0, 16)
  $buttons = New-Object System.Windows.Forms.FlowLayoutPanel
  $buttons.FlowDirection = 'LeftToRight'
  $buttons.AutoSize = $true
  $buttons.WrapContents = $false
  foreach ($item in @(@('このバージョンをスキップ', 'Ignore'),
                      @('あとで', 'No'),
                      @('更新する', 'Yes'))) {
    $button = New-Object System.Windows.Forms.Button
    $button.Text = $item[0]
    $button.DialogResult = $item[1]
    $button.AutoSize = $true
    $button.Padding = New-Object System.Windows.Forms.Padding(8, 2, 8, 2)
    $buttons.Controls.Add($button)
    if ($item[1] -eq 'Yes') {
      $form.AcceptButton = $button
    } elseif ($item[1] -eq 'No') {
      $form.CancelButton = $button
    }
  }
  $panel.Controls.Add($label)
  $panel.Controls.Add($buttons)
  $form.Controls.Add($panel)
  try {
    switch ($form.ShowDialog()) {
      'Yes' { return 'update' }
      'Ignore' { return 'skip' }
      default { return 'later' }
    }
  } finally {
    $form.Dispose()
  }
}

# Reports to the user only in manual mode.
function Write-Report([string]$Text) {
  if ($Mode -eq 'Manual') {
    Show-Message $Text
  }
}

# --------------------------------------------------------------------------
# Network

function Enable-Tls {
  $protocols = [Net.ServicePointManager]::SecurityProtocol -bor
      [Net.SecurityProtocolType]::Tls12
  try {
    $protocols = $protocols -bor [Net.SecurityProtocolType]'Tls13'
  } catch {
  }
  [Net.ServicePointManager]::SecurityProtocol = $protocols
}

function New-WebRequest([string]$Url, [int]$TimeoutMsec) {
  $request = [Net.HttpWebRequest][Net.WebRequest]::Create($Url)
  $request.UserAgent = 'Mozlumi-Updater'
  $request.Timeout = $TimeoutMsec
  $request.ReadWriteTimeout = $TimeoutMsec
  $request.AllowAutoRedirect = $true
  return $request
}

function Get-WebText([string]$Url) {
  $request = New-WebRequest $Url 30000
  $request.Accept = 'application/vnd.github+json'
  $response = $request.GetResponse()
  try {
    $reader = New-Object IO.StreamReader($response.GetResponseStream(),
                                         [Text.Encoding]::UTF8)
    try {
      return $reader.ReadToEnd()
    } finally {
      $reader.Dispose()
    }
  } finally {
    $response.Dispose()
  }
}

# Downloads at most $MaxBytes bytes and returns the size. Shows the progress
# when $Total is known.
function Save-WebFile([string]$Url, [string]$Path, [long]$Total, [long]$MaxBytes) {
  $request = New-WebRequest $Url 60000
  $response = $request.GetResponse()
  try {
    $in = $response.GetResponseStream()
    $out = [IO.File]::Create($Path)
    try {
      $buffer = New-Object byte[] 65536
      $done = [long]0
      $shown = [DateTime]::MinValue
      while (($read = $in.Read($buffer, 0, $buffer.Length)) -gt 0) {
        $done += $read
        if ($done -gt $MaxBytes) {
          throw 'The download is larger than expected.'
        }
        $out.Write($buffer, 0, $read)
        if ($Total -gt 0 -and ([DateTime]::UtcNow - $shown).TotalMilliseconds -ge 200) {
          $shown = [DateTime]::UtcNow
          Set-Progress ($done / $Total) ('ダウンロード中… {0} / {1} MB' -f
              [Math]::Floor($done / 1MB), [Math]::Floor($Total / 1MB))
        }
      }
      return $done
    } finally {
      $out.Dispose()
      $in.Dispose()
    }
  } finally {
    $response.Dispose()
  }
}

# --------------------------------------------------------------------------
# The check

# The same folder as SystemUtil::GetUserProfileDirectory():
# %USERPROFILE%\AppData\LocalLow\Mozlumi.
function Get-StateDir {
  $low = $null
  try {
    $folder = (New-Object -ComObject Shell.Application).NameSpace('shell:LocalAppDataLow')
    if ($null -ne $folder) {
      $low = $folder.Self.Path
    }
  } catch {
  }
  if ([string]::IsNullOrEmpty($low)) {
    $low = Join-Path $env:USERPROFILE 'AppData\LocalLow'
  }
  return (Join-Path $low 'Mozlumi')
}

# The installed version as @(major, minor, patch, build), from the file
# version of the installed converter (e.g. 1.3.0.36), or $null.
function Get-InstalledVersion {
  $exe = Join-Path $InstallDir 'mozlumi_server.exe'
  if (-not (Test-Path -LiteralPath $exe -PathType Leaf)) {
    return $null
  }
  $info = [Diagnostics.FileVersionInfo]::GetVersionInfo($exe)
  return @($info.FileMajorPart, $info.FileMinorPart, $info.FileBuildPart,
           $info.FilePrivatePart)
}

function Compare-Version([int[]]$A, [int[]]$B) {
  for ($i = 0; $i -lt 4; $i++) {
    if ($A[$i] -ne $B[$i]) {
      return [Math]::Sign($A[$i] - $B[$i])
    }
  }
  return 0
}

function Format-Label([int[]]$V) {
  return ('{0}.{1}.{2} Build {3}' -f $V[0], $V[1], $V[2], $V[3])
}

function Invoke-Check {
  $stateDir = Get-StateDir
  [void](New-Item -ItemType Directory -Path $stateDir -Force)
  $disabledFile = Join-Path $stateDir 'update_check_disabled'
  $lastCheckFile = Join-Path $stateDir 'update_last_check'
  $skipFile = Join-Path $stateDir 'update_skipped_version'
  $lockDir = Join-Path $stateDir 'update.lock'

  if ($Mode -eq 'Auto') {
    if (Test-Path -LiteralPath $disabledFile) {
      return 0
    }
    # At most once a day.
    if ((Test-Path -LiteralPath $lastCheckFile) -and
        (Get-Item -LiteralPath $lastCheckFile).LastWriteTimeUtc -gt
            [DateTime]::UtcNow.AddDays(-1)) {
      return 0
    }
  }
  if (-not [string]::IsNullOrEmpty($Tag) -and
      ($Mode -ne 'Manual' -or $Tag -notmatch $TagPattern)) {
    Write-Report 'リリースの指定が正しくありません。'
    return 1
  }

  try {
    [void](New-Item -ItemType Directory -Path $lockDir)
  } catch {
    # Another check is running. A lock older than a day is stale.
    if ((Test-Path -LiteralPath $lockDir) -and
        (Get-Item -LiteralPath $lockDir).LastWriteTimeUtc -lt
            [DateTime]::UtcNow.AddDays(-1)) {
      Remove-Item -LiteralPath $lockDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    return 0
  }
  $workDir = $null
  try {
    $current = Get-InstalledVersion
    if ($null -eq $current) {
      Write-Report 'Mozlumi がインストールされていません。'
      return 1
    }
    $currentLabel = Format-Label $current

    Enable-Tls
    $url = if ([string]::IsNullOrEmpty($Tag)) { "$ApiBase/latest" } else { "$ApiBase/tags/$Tag" }
    try {
      $release = (Get-WebText $url) | ConvertFrom-Json
    } catch {
      Write-Report 'アップデートを確認できませんでした。ネットワークの接続を確認してください。'
      return 1
    }
    [IO.File]::WriteAllText($lastCheckFile, [DateTime]::UtcNow.ToString('o'))

    $tagName = [string]$release.tag_name
    if ($tagName -notmatch $TagPattern -or
        (-not [string]::IsNullOrEmpty($Tag) -and $tagName -cne $Tag)) {
      Write-Report 'リリース情報を読み取れませんでした。'
      return 1
    }
    $new = @([int]$Matches[1], [int]$Matches[2], [int]$Matches[3], [int]$Matches[4])
    $newLabel = Format-Label $new
    if ((Compare-Version $new $current) -le 0) {
      Write-Report "Mozlumi は最新です（$currentLabel）。"
      return 0
    }
    if ($Mode -eq 'Auto' -and (Test-Path -LiteralPath $skipFile) -and
        ([IO.File]::ReadAllText($skipFile).Trim() -ceq $tagName)) {
      return 0
    }

    # Find the MSI and its checksum among the release assets. Accept only
    # the exact names and download URLs of this release.
    $version = '{0}.{1}.{2}' -f $new[0], $new[1], $new[2]
    $msiName = 'Mozlumi_v{0}_build{1}_windows_x64.msi' -f $version, $new[3]
    $shaName = $msiName + '.sha256'
    $downloadBase = "https://github.com/$Repo/releases/download/$tagName/"
    $msiAsset = $null
    $shaAsset = $null
    foreach ($asset in @($release.assets)) {
      if ([string]$asset.name -ceq $msiName -and
          [string]$asset.browser_download_url -ceq ($downloadBase + $msiName)) {
        $msiAsset = $asset
      } elseif ([string]$asset.name -ceq $shaName -and
                [string]$asset.browser_download_url -ceq ($downloadBase + $shaName)) {
        $shaAsset = $asset
      }
    }
    if ($null -eq $msiAsset -or $null -eq $shaAsset) {
      Write-Report "Mozlumi $newLabel が公開されていますが、Windows 用の更新ファイルが見つかりませんでした。"
      return 1
    }

    $answer = Show-UpdateQuestion ("Mozlumi $newLabel が公開されています（今は $currentLabel）。`n`n" +
        '更新しますか？ ダウンロードのあとで、管理者の確認が表示されます。')
    if ($answer -eq 'skip') {
      [IO.File]::WriteAllText($skipFile, $tagName)
      return 0
    }
    if ($answer -ne 'update') {
      return 0
    }

    $workDir = Join-Path ([IO.Path]::GetTempPath()) ('mozlumi-update-' + [guid]::NewGuid().ToString('N'))
    [void](New-Item -ItemType Directory -Path $workDir)
    $msi = Join-Path $workDir $msiName
    $sha = Join-Path $workDir $shaName
    $size = [long]$msiAsset.size
    Start-Progress $workDir
    try {
      $got = Save-WebFile ([string]$msiAsset.browser_download_url) $msi $size $size
      [void](Save-WebFile ([string]$shaAsset.browser_download_url) $sha 0 4096)
      if ($got -ne $size) {
        throw 'The download is incomplete.'
      }
    } catch {
      Show-Message 'ダウンロードに失敗しました。あとでもう一度お試しください。'
      return 1
    }
    Set-Progress -1 'ダウンロードしたファイルを確認しています…'
    $expected = ''
    $firstLine = ([IO.File]::ReadAllText($sha) -split "`n")[0].Trim()
    if ($firstLine -ne '') {
      $expected = ($firstLine -split '\s+')[0].ToLowerInvariant()
    }
    if ($expected -notmatch '^[0-9a-f]{64}$' -or $expected -ne (Get-Sha256 $msi)) {
      Show-Message 'ダウンロードしたファイルの確認（SHA-256）に失敗したため、更新を中止しました。'
      return 1
    }
    $msiVersion = '{0}.{1}.{2}.{3}' -f $new[0], $new[1], $new[2], $new[3]
    if (-not (Test-MozlumiMsi $msi $msiVersion)) {
      Show-Message 'ダウンロードしたファイルが Mozlumi の更新ではないため、更新を中止しました。'
      return 1
    }

    # The installer shows its own progress window.
    Stop-Progress
    $powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
    $arguments = @('-NoProfile', '-NonInteractive', '-ExecutionPolicy', 'Bypass',
                   '-WindowStyle', 'Hidden',
                   '-File', ('"' + (Join-Path $InstallDir 'mozlumi_update.ps1') + '"'),
                   '-Mode', 'Install',
                   '-Source', ('"' + $msi + '"'),
                   '-Sha256', $expected,
                   '-ExpectedVersion', $msiVersion) -join ' '
    try {
      $process = Start-Process -FilePath $powershell -ArgumentList $arguments `
          -Verb RunAs -WindowStyle Hidden -Wait -PassThru
    } catch {
      # The user declined the administrator approval.
      return 0
    }
    switch ($process.ExitCode) {
      0 {
        Remove-Item -LiteralPath $skipFile -Force -ErrorAction SilentlyContinue
        Show-Message ("Mozlumi を $newLabel に更新しました。`n`n" +
            'うまく切り替わらないときは、パソコンを再起動してください。')
        return 0
      }
      3010 {
        Remove-Item -LiteralPath $skipFile -Force -ErrorAction SilentlyContinue
        # Some files were in use; Windows replaces them at the next restart.
        Show-Message ("Mozlumi を $newLabel に更新しました。`n`n" +
            '一部のファイルは、パソコンを次に再起動したときに入れ替わります。')
        return 0
      }
      1602 {
        # The user canceled the installer.
        return 0
      }
      { $_ -eq $ExitHashChanged -or $_ -eq $ExitNotMozlumi } {
        Show-Message 'インストールの直前に確かめたところ、更新ファイルが変わっていたため、更新を中止しました。'
        return 1
      }
      default {
        Show-Message (("更新に失敗しました。（コード {0}）`n`n" +
            'インストールの詳しい記録: {1}') -f $process.ExitCode, $InstallLogPath)
        return 1
      }
    }
  } finally {
    Stop-Progress
    if ($null -ne $workDir) {
      Remove-Item -LiteralPath $workDir -Recurse -Force -ErrorAction SilentlyContinue
    }
    Remove-Item -LiteralPath $lockDir -Recurse -Force -ErrorAction SilentlyContinue
  }
}

# --------------------------------------------------------------------------

try {
  if ($Mode -eq 'Install') {
    exit (Invoke-Install)
  }
  Initialize-Ui
  Set-Location -LiteralPath ([IO.Path]::GetTempPath())
  exit (Invoke-Check)
} catch {
  if ($Mode -eq 'Manual') {
    try {
      Show-Message ("アップデートの確認中にエラーが起きました。`n`n" + $_.Exception.Message)
    } catch {
    }
  }
  exit 1
}
