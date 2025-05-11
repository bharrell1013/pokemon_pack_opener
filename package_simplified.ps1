# Pokemon Pack Simulator Packaging Script (Simplified Version)
# This script creates a distributable package for the Pokemon Pack Simulator

# Configuration
$packageDir = "PokemonPackSimulator"
$outputZip = "PokemonPackSimulator.zip"
$executablePath = "x64/Debug/base_freeglut.exe"
$newExecutableName = "PokemonPackSimulator.exe"

# Function to log messages
function Write-Log {
    param (
        [string]$Message,
        [string]$Type = "INFO"
    )
    
    $timestamp = Get-Date -Format "yyyy-MM-dd HH:mm:ss"
    Write-Host "[$timestamp] [$Type] $Message"
}

# Function to copy file with error handling
function Copy-FileWithErrorHandling {
    param (
        [string]$Source,
        [string]$Destination
    )
    
    if (Test-Path $Source) {
        try {
            Copy-Item $Source -Destination $Destination -Force
            Write-Log "Copied: $Source -> $Destination"
            return $true
        }
        catch {
            Write-Log "Failed to copy: $Source -> $Destination" "ERROR"
            Write-Log $_.Exception.Message "ERROR"
            return $false
        }
    }
    else {
        Write-Log "File not found: $Source" "WARNING"
        return $false
    }
}

# Start packaging
Write-Log "Starting packaging process for Pokemon Pack Simulator"

# Create or clean package directory
if (Test-Path $packageDir) {
    try {
        Remove-Item -Recurse -Force $packageDir
        Write-Log "Cleaned existing package directory"
    }
    catch {
        Write-Log "Failed to clean existing package directory" "ERROR"
        exit 1
    }
}

try {
    New-Item -ItemType Directory -Path $packageDir | Out-Null
    Write-Log "Created package directory: $packageDir"
}
catch {
    Write-Log "Failed to create package directory" "ERROR"
    exit 1
}

# Copy executable
$executableCopied = Copy-FileWithErrorHandling $executablePath "$packageDir/$newExecutableName"
if (-not $executableCopied) {
    Write-Log "Critical file missing: $executablePath. Aborting packaging." "ERROR"
    exit 1
}

# Copy required DLLs
$requiredDlls = @(
    "x64/Debug/freeglut.dll",
    "x64/Debug/cpr.dll",
    "x64/Debug/libcurl-d.dll",
    "x64/Debug/zlibd1.dll",
    "x64/Debug/SDL2d.dll"
)

$missingDlls = 0
foreach ($dll in $requiredDlls) {
    $dllCopied = Copy-FileWithErrorHandling $dll $packageDir
    if (-not $dllCopied) {
        $missingDlls++
    }
}

if ($missingDlls -gt 0) {
    Write-Log "$missingDlls DLL files are missing. Package may not work correctly." "WARNING"
}

# Copy assets
# Create directories
$directories = @(
    "$packageDir/textures",
    "$packageDir/textures/pack",
    "$packageDir/textures/cards",
    "$packageDir/textures/pokemon",
    "$packageDir/textures/pack_images",
    "$packageDir/models",
    "$packageDir/shaders"
)

foreach ($dir in $directories) {
    try {
        if (-not (Test-Path $dir)) {
            New-Item -ItemType Directory -Path $dir | Out-Null
            Write-Log "Created directory: $dir"
        }
    }
    catch {
        Write-Log "Failed to create directory: $dir" "ERROR"
    }
}

# Copy texture directories
$textureDirectories = @(
    @{ Source = "textures/pack"; Destination = "$packageDir/textures" },
    @{ Source = "textures/cards"; Destination = "$packageDir/textures" },
    @{ Source = "textures/pokemon"; Destination = "$packageDir/textures" },
    @{ Source = "textures/pack_images"; Destination = "$packageDir/textures" }
)

foreach ($dir in $textureDirectories) {
    if (Test-Path $dir.Source) {
        try {
            Copy-Item -Recurse -Force $dir.Source $dir.Destination
            Write-Log "Copied directory: $($dir.Source) -> $($dir.Destination)"
        }
        catch {
            Write-Log "Failed to copy directory: $($dir.Source)" "ERROR"
        }
    }
    else {
        Write-Log "Directory not found: $($dir.Source)" "WARNING"
    }
}

# Copy model files
$modelFiles = @(
    "models/pack.obj",
    "models/pack.glb",
    "models/cube.obj"
)

foreach ($model in $modelFiles) {
    Copy-FileWithErrorHandling $model "$packageDir/models"
}

# Copy shader files
if (Test-Path "shaders") {
    try {
        Copy-Item "shaders/*.glsl" -Destination "$packageDir/shaders"
        Write-Log "Copied shader files"
    }
    catch {
        Write-Log "Failed to copy shader files" "ERROR"
    }
}
else {
    Write-Log "Shader directory not found" "WARNING"
}

# Create batch file to run the app
$batchContent = @"
@echo off
echo Starting Pokemon Pack Simulator...
start PokemonPackSimulator.exe
"@

try {
    $batchContent | Out-File -FilePath "$packageDir/Run_Pokemon_Pack_Simulator.bat" -Encoding ascii
    Write-Log "Created launcher batch file"
}
catch {
    Write-Log "Failed to create launcher batch file" "ERROR"
}

# Create README file
$readmeContent = @"
# Pokemon Pack Simulator

This is a standalone version of the Pokemon Pack Simulator that allows users to simulate opening Pokemon card packs in 3D.

## Running the Application

1. Simply double-click the 'Run_Pokemon_Pack_Simulator.bat' file
2. Alternatively, you can run 'PokemonPackSimulator.exe' directly

## Controls

- Left mouse button: Select/interact with card packs
- Right mouse button: Rotate the view
- Mouse wheel: Zoom in/out
- W, A, S, D: Move the camera
- R: Reset view
- ESC: Exit application

## Requirements

- Windows 10 or newer
- Graphics card with OpenGL 3.3 support

## Troubleshooting

If the application doesn't start:
1. Make sure all DLL files are in the same directory as the executable
2. Check that your graphics card supports OpenGL 3.3
3. Make sure all the texture, model and shader directories are present

For more information, visit: [Your Website URL]
"@

try {
    $readmeContent | Out-File -FilePath "$packageDir/README.txt" -Encoding ascii
    Write-Log "Created README file"
}
catch {
    Write-Log "Failed to create README file" "ERROR"
}

# Create zip file
try {
    if (Test-Path $outputZip) {
        Remove-Item -Force $outputZip
        Write-Log "Removed existing zip file"
    }
    
    Compress-Archive -Path $packageDir -DestinationPath $outputZip -Force
    Write-Log "Created package zip file: $outputZip"
}
catch {
    Write-Log "Failed to create zip file" "ERROR"
    Write-Log $_.Exception.Message "ERROR"
    exit 1
}

Write-Log "Package created successfully!" "SUCCESS"
Write-Log "You can now upload $outputZip to your website for distribution." 