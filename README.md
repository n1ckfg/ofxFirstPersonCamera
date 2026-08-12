ofxFirstPersonCamera
====================


Lightweight ofCamera class that replicates camera controls of first person video games. It uses mouse to look around, hides cursor on activation and has reassignable keys which by default set to `WASD` for moving the camera, `E` and `C` to move camera up and down, `Q` and `R` to roll left and right and `F` to reset camera's up vector. Hold `Shift` to run, tap `T` for eased-in movement and `Z` to switch between free flight and walking.

Tested on Linux, Windows and macOS.


Dependencies
------------

#### 1. openFrameworks 0.12.1

#### 2. An `ofAppGLFWWindow`

The addon talks to GLFW directly to grab and re-center the cursor, so the app
has to run on the default GLFW window backend.

Compiling
---------

For [openFrameworks](https://github.com/openframeworks/openFrameworks):

[See wiki](https://github.com/ofnode/of/wiki/Compiling-ofApp-with-vanilla-openFrameworks)

For [CMake-based openFrameworks](https://github.com/ofnode/of):

Add this repo as a git submodule to your [ofApp](https://github.com/ofnode/ofApp) folder and use `ofxaddon` command in `CMakeLists.txt`.


How to use
----------

Include header file in `ofApp.h`, add an instance of `ofxFirstPersonCamera`:

```cpp
#pragma once

#include "ofMain.h"

#include "ofxFirstPersonCamera.h"

class ofApp : public ofBaseApp
{
  public:
    void mousePressed(int x, int y, int button);
    void draw();

    ofxFirstPersonCamera cam;
};
```

`ofxFirstPersonCamera` is an `ofCamera`, so it is used like any other one. Call
`enableControl()` (or `toggleControl()`) to grab the cursor and start driving it:

```cpp
#include "ofApp.h"

void ofApp::mousePressed(int x, int y, int button)
{
  cam.toggleControl();
}

void ofApp::draw()
{
  cam.begin();

    ofDrawGrid(10.0f, 10, true);

  cam.end();
}
```

Key bindings are `GLFW_KEY_*` keycodes and can all be reassigned, e.g.
`cam.keyForward = GLFW_KEY_UP;`. Speeds are expressed per frame at 60 fps and
are scaled by the actual frame time, so movement stays consistent:

```cpp
cam.movespeed   = 1.00f;   // units per frame
cam.rollspeed   = 1.00f;   // degrees per frame
cam.sensitivity = 0.10f;   // degrees per pixel of mouse movement
```


Movement modes
--------------

Holding `keyRun` (either `Shift` by default) multiplies `movespeed` by
`runspeed` for as long as it is down.

Two modes are toggled by tapping a key while the camera is being controlled,
and both start out matching the camera's traditional behaviour, so nothing
changes until a key is pressed or a flag is set:

| Key | Flag | Default | What the other setting does |
| --- | --- | --- | --- |
| `T` | `easein`  | `false` | Movement accelerates from a standstill to full speed over `easetime` seconds instead of starting at full pace. Stopping is still immediate. |
| `Z` | `flymode` | `true`  | Walking instead of flying: forward/backward and strafing stay on the plane that `upvector` is normal to, so looking up or down no longer lifts the camera off it. `keyUp`/`keyDown` are what change height. |

```cpp
cam.runspeed = 2.00f;   // movespeed multiplier while keyRun is held
cam.easetime = 0.25f;   // seconds from a standstill to full speed
cam.easein   = true;    // start eased, without waiting for a T
cam.flymode  = false;   // start walking, without waiting for a Z
```

The toggle keys are ignored while control is disabled, so they stay free to be
application shortcuts whenever the camera is not driving.


Examples
--------

### [example](example)

Bundled with this addon. Open `example/example.xcodeproj` (macOS) or run `make`
in the `example` folder.
