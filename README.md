# CS2 Simple Anti-Wallhack (SAWH)

![preview](https://github.com/umitc18/sawh/blob/main/assets/test.gif)

Simple Anti-Wallhack (SAWH) is a server-side Metamod:Source plugin designed for Counter-Strike 2. Its primary goal is to prevent Wallhack (ESP) cheats by transmitting player data only when players are physically visible to each other on the map. It utilizes precomputed visibility data mapped to the game's radar coordinates to determine line-of-sight efficiently.

## Features
- **Blocks ESP/Wallhacks** by stopping the server from sending player data to clients who cannot see them.
- **Interactive Map Editor:** Easily visualize and manually punch holes or create solid walls in the PVS grid using the included Python GUI.
- Prevents unnecessary data transmission, saving server bandwidth.

## Limitations
- **Map Support:** Requires a pre-generated visibility `.bin` file for every map played on the server (e.g., `de_dust2.bin`).
- **Platform:** This plugin has been developed and tested **exclusively on Linux**. Windows support is not guaranteed.
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

## Map Visualizer & Editor

![preview](https://github.com/umitc18/sawh/blob/main/assets/visualizer.png)

The repository includes a highly optimized Python-based visualizer and map editor. You can use it to generate `.bin` visibility files from map radar images and manually edit the solid walls.

### Prerequisites for Visualizer
You need Python 3 to install requirements:
```bash
cd visualizer && pip install -r requirements.txt
```

### Usage Instructions
1. Navigate to the visualizer folder and run the GUI:
   ```bash
   cd visualizer
   python3 gui.py
   ```
2. **Load Image:** Click "Load Map Image" and select a map radar overview (e.g., `de_dust2_radar.png`).
3. **Configure Settings:**
   - **Grid Size:** The base resolution of the map (e.g., 64x64, 128x128).
   - **Max Node Size:** Controls how aggressively the Quadtree merges empty space(lowest values more accurate).
   - **Radar Coordinates:** Enter `pos_x`, `pos_y`, and `scale` from the map's metadata so the plugin can translate in-game coordinates to the grid(you can find in: `resource/overviews/de_dust2.txt`).
4. **Generate:** Click `REBUILD FROM IMAGE` to process the map
5. **Interactive Editing:**
   - **Left Click:** Select a node to see what it can see (Green nodes are visible).
   - **Right Click:** Manually toggle a node between Solid (Red) and Empty (Grey).
6. **Update:** If you made manual edits (Right Clicks), click `REBUILD FROM EDITS` to recalculate the raycaster without losing your custom edits.
7. **Export:** Click `EXPORT MAP` and place the generated file in your server's `addons/sawh/configs/` directory.
