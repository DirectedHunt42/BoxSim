# Box Physics Simulator - GUI Edition

A real-time 2D physics simulator with Windows native GUI rendering.

## Features

- **Visual box rendering** - Watch boxes fall and collide in real-time
- **Click-to-add boxes** - Click and drag to create boxes of any size
- **Play/Pause control** - Toggle simulation with Space or Play button
- **Physics simulation** - Gravity, restitution, friction, and wall collisions
- **Clear all** - Remove all boxes with one click
- **No external dependencies** - Uses Windows native APIs

## Build Instructions

### Prerequisites

- MinGW (g++) with Windows SDK (should be included by default)
- Already installed with MSYS2

### Compile

```powershell
cd "c:\Users\jackp\Github Repositories\BoxSim"
g++ -std=c++20 boxsim.cpp -o boxsim.exe -luser32 -lgdi32 -lkernel32
```

Or use VS Code build task: `Ctrl+Shift+B`

### Run

```powershell
.\boxsim.exe
```

## Controls

| Action | Method |
|--------|--------|
| **Add Box** | Click & drag on canvas (below top bar) - green preview appears |
| **Play/Pause** | Press Space or click PAUSE/RESUME button |
| **Clear All** | Press C or click CLEAR button |

## How It Works

1. **Click and drag** to define box size - a green preview shows as you drag
2. **Release** to create the box - it will immediately fall under gravity
3. **Press Space** to pause/resume the simulation
4. **Press C** or click CLEAR to remove all boxes

Boxes bounce on the floor with configurable restitution and friction, collide with walls elastically.

