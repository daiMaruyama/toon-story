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
# Use the same known driver as fallback; fail visibly instead of silently changing transport.
$Definition = '+NetDriverDefinitions=(DefName="GameNetDriver",DriverClassName="' + $Driver + '",DriverClassNameFallback="' + $Driver + '")'
$Config = [regex]::Replace($Config, '(?m)^\+NetDriverDefinitions=\(DefName="GameNetDriver"[^\r\n]*', $Definition)
[IO.File]::WriteAllText($ConfigPath, $Config, [Text.UTF8Encoding]::new($false))
Write-Host "Mode: $Mode. Restart Unreal Editor and rebuild the package after changing modes."
