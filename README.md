# Console Ray Tracer by Eodum

A sophisticated real-time ray tracing system implemented entirely in the Windows console, demonstrating advanced light simulation and material interactions in an ASCII art environment.


## Key Features

### Advanced Light Simulation
- **Real-time ray tracing** with dynamic light propagation
- **Multiple light sources** supported simultaneously:
  - Point lights with radial emission
  - Spotlights with adjustable cone and falloff
  - Laser beams with precise directional control
- **Spectrum-based light system** accurately simulating wavelengths and color dispersion

### Sophisticated Material System
- **Physically-based material interactions:**
  - Perfect mirrors with accurate reflection
  - Matte surfaces with realistic diffuse scattering
  - Absorbing materials for light elimination
  - Prisms with wavelength-dependent refraction
- **Subsurface scattering** simulation for realistic material appearance
- **Spectral response** modeling for each material type

### Interactive Console Interface
- **Real-time manipulation** of scene objects and light sources
- **Intuitive controls** for object placement and modification
- **Live updates** with instant visual feedback
- **ASCII art rendering** with color support
- **Double-buffered console output** for smooth rendering

### Technical Highlights
- **Efficient ray-geometry intersection** algorithms
- **Vector mathematics** optimized for 2D space
- **Color space handling** with RGB and spectral conversions
- **Memory-efficient design** suitable for console environment
- **Modular architecture** with clear separation of concerns

## Getting Started

### Prerequisites
- Windows operating system
- G++ compiler
- Git (optional, for version control)

### Building the Project
1. Clone the repository:
```bash
git clone https://github.com/0xEodum/CPPConsoleRayTracer.git
```

2. Navigate to the project directory:
```bash
cd console-ray-tracer
```

3. Run the build script:
```bash
build.bat
```

4. Find the executable in the `bin` directory:
```bash
./bin/RayTracer.exe
```

### Controls
- **Left Mouse Click**: Place a rectangle
- **Right Mouse Click**: Delete object
- **L**: Create point light
- **S**: Create spotlight
- **Z**: Create laser (click again to set direction)
- **D**: Delete light
- **+/-**: Adjust light intensity
- **Up/Down**: Change material type
- **ESC**: Exit application

## Technical Design

### Architecture
The project follows a modular design with clear separation of concerns:
- Core mathematical components (vectors, colors, spectra)
- Material system for light interaction
- Light sources with different behaviors
- Shape system for geometry handling
- Console rendering system
- Input handling and scene management

