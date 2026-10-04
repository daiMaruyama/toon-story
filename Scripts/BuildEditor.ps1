param([Parameter(Mandatory=$true)][string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Project = Join-Path $ProjectRoot 'ToonStory.uproject'
$Build = Join-Path $EngineRoot 'Engine\Build\BatchFiles\Build.bat'
if (!(Test-Path $Build)) { throw "Build.bat not found under EngineRoot: $EngineRoot" }
if (!(Test-Path $Project)) { throw "ToonStory.uproject not found: $Project" }
& $Build ToonStoryEditor Win64 Development "-Project=$Project" -WaitMutex
if ($LASTEXITCODE -ne 0) { throw "Unreal build failed with code $LASTEXITCODE. Read the first error in the build output." }
Write-Host 'Editor build succeeded. Open ToonStory.uproject and play TB_KidsRoom or TB_ArchViz. Use Arena for rule checks.'
