# Packages a release build into out/Skhroma-<version>.zip with a Data-relative
# layout (SKSE/Plugins/...), ready for MO2/Vortex and Nexus Mods.
# Run after: cmake --build --preset release   (or any preset that uses build/)
param(
	[string]$BuildDir = "$PSScriptRoot/../build/RelWithDebInfo"
)
$ErrorActionPreference = 'Stop'
$root = Resolve-Path "$PSScriptRoot/.."

$cmake = Get-Content "$root/CMakeLists.txt" -Raw
if ($cmake -notmatch 'VERSION\s+(\d+\.\d+\.\d+)') { throw 'Version not found in CMakeLists.txt' }
$version = $Matches[1]

$dll = Join-Path $BuildDir 'Skhroma.dll'
$pdb = Join-Path $BuildDir 'Skhroma.pdb'
if (-not (Test-Path $dll)) { throw "Build output not found: $dll" }

$files = [ordered]@{
	'SKSE/Plugins/Skhroma.dll' = $dll
	'SKSE/Plugins/Skhroma.ini' = "$root/dist/SKSE/Plugins/Skhroma.ini"
}
if (Test-Path $pdb) { $files['SKSE/Plugins/Skhroma.pdb'] = $pdb }  # lets crash loggers resolve Skhroma frames

New-Item -ItemType Directory -Force "$root/out" | Out-Null
$zip = Join-Path $root "out/Skhroma-$version.zip"
if (Test-Path $zip) { Remove-Item $zip -Force }

# Compress-Archive in Windows PowerShell 5.1 writes '\' separators; use the .NET API for '/'.
Add-Type -AssemblyName System.IO.Compression, System.IO.Compression.FileSystem
$archive = [IO.Compression.ZipFile]::Open($zip, 'Create')
try {
	foreach ($entry in $files.GetEnumerator()) {
		[IO.Compression.ZipFileExtensions]::CreateEntryFromFile($archive, (Resolve-Path $entry.Value).Path, $entry.Key, 'Optimal') | Out-Null
	}
} finally {
	$archive.Dispose()
}

Write-Host "Packaged $zip"
