# Pokemon Pack Simulator Packaging Script
# This script creates a distributable package for the Pokemon Pack Simulator

# Create package directory
$packageDir = "PokemonPackSimulator"
if (Test-Path $packageDir) {
    Remove-Item -Recurse -Force $packageDir
}
New-Item -ItemType Directory -Path $packageDir

# Copy executable and DLLs
Copy-Item "x64/Debug/base_freeglut.exe" -Destination "$packageDir/PokemonPackSimulator.exe"
Copy-Item "x64/Debug/freeglut.dll" -Destination $packageDir
Copy-Item "x64/Debug/cpr.dll" -Destination $packageDir
Copy-Item "x64/Debug/libcurl-d.dll" -Destination $packageDir
Copy-Item "x64/Debug/zlibd1.dll" -Destination $packageDir
Copy-Item "x64/Debug/SDL2d.dll" -Destination $packageDir

# Copy assets
# Textures
New-Item -ItemType Directory -Path "$packageDir/textures"
Copy-Item -Recurse "textures/pack" -Destination "$packageDir/textures"
Copy-Item -Recurse "textures/cards" -Destination "$packageDir/textures"
Copy-Item -Recurse "textures/pokemon" -Destination "$packageDir/textures"
Copy-Item -Recurse "textures/pack_images" -Destination "$packageDir/textures"

# Models
New-Item -ItemType Directory -Path "$packageDir/models"
Copy-Item "models/pack.obj" -Destination "$packageDir/models"
Copy-Item "models/pack.glb" -Destination "$packageDir/models"
Copy-Item "models/cube.obj" -Destination "$packageDir/models"

# Shaders
New-Item -ItemType Directory -Path "$packageDir/shaders"
Copy-Item "shaders/*.glsl" -Destination "$packageDir/shaders"

# Create batch file to run the app
$batchContent = @"
@echo off
echo Starting Pokemon Pack Simulator...
start PokemonPackSimulator.exe
"@
$batchContent | Out-File -FilePath "$packageDir/Run_Pokemon_Pack_Simulator.bat" -Encoding ascii

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
$readmeContent | Out-File -FilePath "$packageDir/README.txt" -Encoding ascii

# Create a zip file
Compress-Archive -Path $packageDir -DestinationPath "PokemonPackSimulator.zip" -Force

Write-Host "Package created successfully! The distributable zip file is PokemonPackSimulator.zip" 