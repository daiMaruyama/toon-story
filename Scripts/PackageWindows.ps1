param([Parameter(Mandatory=$true)][string]$EngineRoot)
$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path $PSScriptRoot -Parent
$Project = Join-Path $ProjectRoot 'ToonStory.uproject'
$UAT = Join-Path $EngineRoot 'Engine\Build\BatchFiles\RunUAT.bat'
$Destination = Join-Path $ProjectRoot 'Builds\Windows'
if (!(Test-Path $UAT)) { throw "RunUAT.bat not found: $UAT" }
if (!(Test-Path (Join-Path $ProjectRoot 'Content\Maps\Arena.umap'))) { throw 'Create and save /Game/Maps/Arena in the editor first.' }
& $UAT BuildCookRun "-project=$Project" -noP4 -platform=Win64 -clientconfig=Development -build -cook -stage -pak -archive "-archivedirectory=$Destination" -map=/Game/Maps/Arena
if ($LASTEXITCODE -ne 0) { throw "Packaging failed with code $LASTEXITCODE. Read the first UAT error." }
if ([IO.File]::ReadAllText((Join-Path $ProjectRoot 'Config\DefaultEngine.ini')) -match 'DefaultPlatformService=Steam') {
    # 開発テスト専用。後々消す予定。Steamで出すときには含めないようにする。
    $ExecutableDirectories = Get-ChildItem -LiteralPath $Destination -Filter 'ToonStory*.exe' -File -Recurse |
        Select-Object -ExpandProperty DirectoryName -Unique
    foreach ($Directory in $ExecutableDirectories) {
        [IO.File]::WriteAllText((Join-Path $Directory 'steam_appid.txt'), "480`n", [Text.Encoding]::ASCII)
    }
}
Write-Host "Packaged output: $Destination"
