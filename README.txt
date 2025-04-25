# Pokemon Pack Simulator

A 3D OpenGL-based Pokemon card pack opening simulator that renders interactive card animations and holographic effects.

## Prerequisites

### Windows
- Visual Studio 2019 or later
- [Git](https://git-scm.com/downloads)
- [vcpkg](https://github.com/Microsoft/vcpkg) package manager

### Linux
- GCC 8.0 or later (C++17 support required)
- Required packages:
  ```bash
  # Debian/Ubuntu
  sudo apt install build-essential libglm-dev freeglut3-dev

  # Arch/Manjaro
  sudo pacman -Sy base-devel glm freeglut
  ```

## Installation

### Windows Setup

1. Clone the repository:
   ```cmd
   git clone <repository-url>
   cd pokemon-pack-sim
   ```

2. Install vcpkg if you haven't already:
   ```cmd
   cd vcpkg
   .\bootstrap-vcpkg.bat or .\bootstrap-vcpkg.sh
   .\vcpkg integrate install
   ```

3. Install required dependencies:
   ```cmd
   .\vcpkg install cpr:x64-windows nlohmann-json:x64-windows
   or for mac

   ```

4. Open `base_freeglut.sln` in Visual Studio

5. Build and run the project (F5 or select Debug > Start Debugging)

### Linux Setup

1. Clone the repository:
   ```bash
   git clone <repository-url>
   cd pokemon-pack-sim
   ```

2. Install vcpkg:
   ```bash
   git clone https://github.com/Microsoft/vcpkg.git
   cd vcpkg
   ./bootstrap-vcpkg.sh
   ./vcpkg integrate install
   ```

3. Install dependencies through vcpkg:
   ```bash
   ./vcpkg install cpr:x64-linux nlohmann-json:x64-linux
   ```

4. Build the project:
   ```bash
   make
   ```

5. Run the application:
   ```bash
   ./base_freeglut
   ```

## Project Structure

```
pokemon-pack-sim/
├── src/             # Source code files
├── shaders/         # GLSL shader files
├── models/          # 3D model assets
├── textures/        # Texture images
├── include/         # Header files
└── lib/            # Library dependencies
```

## Troubleshooting

### Common Issues

1. **Missing DLL errors on Windows**
   - Make sure you've installed the dependencies through vcpkg
   - Check that you're building for the correct platform (x64)

2. **OpenGL/GLUT errors**
   - Ensure your graphics drivers are up to date
   - On Linux, verify freeglut is properly installed

3. **Build errors related to C++17**
   - Make sure you're using a compatible compiler version
   - On Windows: Set the C++ Language Standard to C++17 in project properties
   - On Linux: Add `-std=c++17` to CXXFLAGS if needed

### Linux-Specific

If you encounter permission issues running the executable:
```bash
chmod +x base_freeglut
```

### Windows-Specific

If Visual Studio can't find the dependencies:
1. Make sure vcpkg integration is installed
2. Check that the platform toolset matches your Visual Studio version
3. Verify the project is set to x64 platform

## License

[Your license information here]

## Contributing

[Your contribution guidelines here]
