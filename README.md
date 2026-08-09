# CS2 Simple Anti-Wallhack (SAWH)

![preview](https://github.com/umitc18/sawh/blob/main/assets/test.gif)

Simple Anti-Wallhack (SAWH) is a server-side Metamod:Source plugin designed for Counter-Strike 2. Its primary goal is to prevent Wallhack (ESP) cheats by transmitting player data only when players are physically visible to each other on the map. It utilizes precomputed visibility data (JSON) mapped to the game's radar coordinates to determine line-of-sight efficiently.

## Features
- Blocks ESP/Wallhacks by stopping the server from sending player data to clients who cannot see them.
- Uses highly optimized JSON-based visibility grids based on map radar coordinates.
- Prevents unnecessary data transmission, saving server bandwidth.

## Limitations
- **Map Support:** Currently requires a pre-generated visibility JSON file for every map played on the server (e.g., `de_dust2.json`).
- **Grid Resolution vs. RAM:** The accuracy of the Anti-Wallhack depends directly on the grid resolution of your JSON files (e.g., 32x32 vs 128x128). A higher grid resolution provides much better accuracy but will proportionally increase the server's RAM usage and JSON file size.
- **Platform:** This plugin has been developed and tested **exclusively on Linux**. Windows support is not guaranteed.
- **Dynamic Elements:** Since visibility is precomputed, dynamic map elements (like breaking doors or moving objects) are not natively accounted for unless baked into the JSON visibility map.
- **2D Visibility:** The visibility calculations are currently strictly 2D (X and Y axes). Height (Z-axis) is ignored, meaning multi-level areas (like Nuke or Vertigo) might have overlapping visibility issues.

## Installation (Pre-compiled Releases)
If you download the pre-compiled version from the **Releases** section, follow these steps:
1. Ensure your CS2 server has **Metamod:Source** installed and working.
2. Download the archive from the Releases page.
3. Extract the archive and copy the `addons` folder into your CS2 server's `game/csgo/` directory.
   - The plugin file should end up at: `game/csgo/addons/sawh/bin/linuxsteamrt64/sawh.so`
   - The map configs should end up at: `game/csgo/addons/sawh/configs/`
4. Restart your CS2 server.

## Manual Compilation

To compile the plugin manually from source, follow these steps:

### Prerequisites
You need the following tools and dependencies:
- GCC / Clang (C++17 or higher support)
- Python 3 (for AMBuild)
- Git

### Build Instructions
1. Clone the repository:
   ```bash
   git clone https://github.com/umitc18/sawh
   cd sawh
   ```
2. Download the required dependencies (`metamod-source` and `hl2sdk-cs2`). You can set them up in a `deps` folder:
   ```bash
   mkdir deps
   cd deps
   git clone https://github.com/alliedmodders/metamod-source.git
   git clone https://github.com/alliedmodders/hl2sdk.git hl2sdk-cs2 -b cs2
   git clone https://github.com/alliedmodders/hl2sdk-manifests.git
   cd ..
   ```
3. Install AMBuild (if you haven't already):
   ```bash
   pip3 install git+https://github.com/alliedmodders/ambuild
   ```
3. Configure the build environment:
   ```bash
   mkdir build
   cd build
   python3 ../configure.py --mms_path=../deps/metamod-source --hl2sdk-root=../deps --hl2sdk-manifests=../deps/hl2sdk-manifests --sdks=cs2
   ```
4. Compile the plugin:
   ```bash
   ambuild
   ```
5. If successful, the compiled binary `sawh.so` will be located in the `build/package/addons/sawh` directory.
