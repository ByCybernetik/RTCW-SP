# Return to Castle Wolfenstein - Modern Arch Linux x64 Port
# Build Instructions

## Prerequisites

Install required dependencies on Arch Linux:

```bash
sudo pacman -S base-devel cmake sdl2 sdl2_image openal-alsa libpng libjpeg-turbo zlib git
```

Optional dependencies for enhanced features:
```bash
sudo pacman -S freetype2 glew
```

## Building

### Quick Build

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
```

### Build Options

You can customize the build with these CMake options:

- `-DBUILD_CLIENT=ON/OFF` - Build client executable (default: ON)
- `-DBUILD_SERVER=ON/OFF` - Build dedicated server (default: ON)
- `-DBUILD_GAME_MODULE=ON/OFF` - Build game module as .so (default: ON)
- `-DBUILD_CGAME_MODULE=ON/OFF` - Build cgame module as .so (default: ON)
- `-DBUILD_UI_MODULE=ON/OFF` - Build UI module as .so (default: ON)
- `-DUSE_SDL2=ON/OFF` - Use SDL2 for input/audio/window (default: ON)
- `-DUSE_OPENAL=ON/OFF` - Use OpenAL for audio (default: ON)
- `-DUSE_OPENGL=ON/OFF` - Use OpenGL for rendering (default: ON)

Example with custom options:
```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SERVER=OFF
```

### Debug Build

```bash
mkdir build-debug && cd build-debug
cmake .. -DCMAKE_BUILD_TYPE=Debug
make -j$(nproc)
```

## Installation

After building, binaries are located in `build/bin/`:

- `wolf.x64` - Client executable
- `wolfded.x64` - Dedicated server executable
- `game.so` - Game logic module
- `cgame.so` - Client game module  
- `ui.so` - User interface module

To install system-wide:
```bash
sudo make install
```

## Running

Copy your original RTCW .pk3 files to the base directory:
```
cp /path/to/rtcw/Main/*.pk3 ./base/
```

Run the client:
```bash
cd build/bin
./wolf.x64 +set fs_basepath /path/to/rtcw
```

Or run the dedicated server:
```bash
./wolfded.x64 +set dedicated 1 +exec server.cfg
```

## Key Changes from Original Code

1. **Native .so Modules**: Game, cgame, and UI modules are compiled as native shared libraries (.so) instead of QVM bytecode
2. **SDL2 Integration**: All input, audio, and window management through SDL2
3. **x64 Optimized**: Compiled with -m64 flag for native 64-bit performance
4. **Modern CMake**: Uses CMake build system instead of old Makefiles
5. **No QVM**: Removed virtual machine overhead by using direct native code

## Troubleshooting

### Missing .pk3 files error
Ensure you have copied the original game data files (.pk3) from your RTCW installation to the `base/` directory.

### Audio issues
Verify OpenAL is properly installed: `pacman -Qs openal`

### OpenGL errors
Make sure you have proper graphics drivers installed for your GPU.

## License

This project is licensed under GPL v3. See COPYING.txt for details.

Original RTCW source copyright © 1999-2010 id Software LLC.
