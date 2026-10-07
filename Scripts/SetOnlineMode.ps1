param([Parameter(Mandatory=$true)][ValidateSet('Steam','LAN')][string]$Mode)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$ConfigPath = Join-Path $ProjectRoot 'Config\DefaultEngine.ini'
if (Test-Path (Join-Path $ProjectRoot 'Config\Windows\WindowsEngine.ini')) {
    throw 'WindowsEngine.ini exists. Remove its NetDriver overrides before switching modes.'
}
$Config = [IO.File]::ReadAllText($ConfigPath)
$Service = if ($Mode -eq 'Steam') { 'Steam' } else { 'Null' }
$Driver = if ($Mode -eq 'Steam') { '/Script/SteamSockets.SteamSocketsNetDriver' } else { '/Script/OnlineSubsystemUtils.IpNetDriver' }
$Config = [regex]::Replace($Config, '(?m)^DefaultPlatformService=.*$', "DefaultPlatformService=$Service")
# SteamSockets は既定の socket subsystem も切り替えるため、LAN では無効にする。
$SteamNetworking = if ($Mode -eq 'Steam') { 'true' } else { 'false' }
$Config = [regex]::Replace($Config, '(?m)^bUseSteamNetworking=.*$', "bUseSteamNetworking=$SteamNetworking")
# 接続方式の自動切替を避けるため、代替ドライバーにも同じものを指定する。
$Definition = '+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="' + $Driver + '",DriverClassNameFallback="' + $Driver + '")'
$Config = [regex]::Replace($Config, '(?m)^\+NetDriverDefinitions=\(DefName="GameNetDriver"[^\r\n]*', $Definition)
[IO.File]::WriteAllText($ConfigPath, $Config, [Text.UTF8Encoding]::new($false))
Write-Host "Mode: $Mode. Restart Unreal Editor and rebuild the package after changing modes."
