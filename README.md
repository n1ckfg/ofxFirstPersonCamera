ofxFirstPersonCamera
====================


Lightweight ofCamera class that replicates camera controls of first person video games. It uses mouse to look around, hides cursor on activation and has reassignable keys which by default set to `WASD` for moving the camera, `E` and `C` to move camera up and down, `Q` and `R` to roll left and right and `F` to reset camera's up vector.

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


Examples
--------

### [example](example)

Bundled with this addon. Open `example/example.xcodeproj` (macOS) or run `make`
in the `example` folder.
