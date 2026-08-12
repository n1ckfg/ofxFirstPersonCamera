# Architecture: ofxFirstPersonCamera

`ofxFirstPersonCamera` is a lightweight openFrameworks addon that provides a first-person camera class, replicating standard video game camera controls. The class extends `ofCamera` and relies on GLFW to capture and manipulate the mouse cursor natively, with automatic fallbacks for basic openFrameworks environments like `ofAppEGLWindow` (e.g. Raspberry Pi headless).

## Class: `ofxFirstPersonCamera`

Inherits from `ofCamera`.

### Overview
This class manages the view matrix for a first-person perspective. It handles keyboard and mouse events natively through openFrameworks event listeners (`ofEventArgs`, `ofKeyEventArgs`, `ofMouseEventArgs`). By default, it uses standard `WASD` controls for movement, `X`/`C` for rolling, `E`/`Q` for vertical translation, and the mouse for looking around. Holding `Shift` runs, tapping `T` or `Z` toggles movement ease-in and fly mode, and `O`/`I` save and load the camera pose. Poses are persisted as XML through the core `ofXml`, so the addon has no addon dependencies.

### Public Methods

- **`ofxFirstPersonCamera()`**
  Constructor. Initializes the camera and automatically registers the openFrameworks event listeners (update, mouse, and keyboard events).

- **`~ofxFirstPersonCamera()`**
  Destructor. Saves the pose first if `autosavePosition` is on and something is still unsaved, which catches an application that quit while the camera was still moving. Then unregisters the openFrameworks event listeners to clean up nicely.

- **`bool isControlled() const`**
  Returns a boolean indicating whether the camera is currently in control mode (i.e., capturing the cursor and responding to input).

- **`void toggleControl()`**
  Toggles the control state of the camera. If currently controlled, it disables control; if disabled, it enables it.

- **`void enableControl()`**
  Enables camera control. This grabs the mouse cursor, hides it, and centers it in the window. The camera will now respond to movement keys and mouse rotation.

- **`void disableControl()`**
  Disables camera control. This releases the mouse cursor and makes it visible again. The camera will stop responding to input until re-enabled.

- **`bool saveCameraPosition()` / `bool saveCameraPosition(const std::string& file)`**
  Writes the pose to `cameraPositionFile`, or to the given file without changing that setting. Saves position, orientation (as a quaternion), `upvector`, a look target, and the field of view. Returns whether the write succeeded. Relative paths resolve under `bin/data`, and the enclosing folder is created if it is missing. Bound to `O`.

- **`bool loadCameraPosition()` / `bool loadCameraPosition(const std::string& file)`**
  Reads a pose back and applies it. Returns false (and logs) when the file is missing, unparseable, or has no `<camera>` root, leaving the camera untouched. Tags that are absent keep their current value rather than resetting to zero. Bound to `I`. Calling this suppresses the automatic startup load, so an application that loads a pose of its own in `setup()` keeps it.

- **`bool hasUnsavedPosition() const`**
  True from the moment the pose changes until the next successful save.

### Protected Methods (Event Handlers)

These methods are automatically bound to openFrameworks core events and generally do not need to be called manually.

- **`void update(ofEventArgs&)`**
  Called every frame. Applies the current movement velocities to the camera's position based on active key presses, scaled by the frame rate to ensure consistent movement speed. Also advances the ease-in ramp, applies the run multiplier, and constrains movement to the ground plane while `flymode` is off. Runs the startup load on the first frame and the pose tracking on every frame, both of which happen even while control is disabled.

- **`void keyPressed(ofKeyEventArgs&)`**
  Called when a key is pressed. Forwards the keycode (GLFW) or character (ASCII fallback) to `setAction()`.

- **`void keyReleased(ofKeyEventArgs&)`**
  Called when a key is released. Forwards to `setAction()` the same way.

- **`void mouseMoved(ofMouseEventArgs&)`**
  Called when the mouse is moved. Delegates to `nodeRotate()` if the camera is controlled.

- **`void mouseDragged(ofMouseEventArgs&)`**
  Called when the mouse is dragged (moved while clicked). Also delegates to `nodeRotate()` if the camera is controlled.

### Private Methods

- **`void setAction(int key, bool pressed)`**
  Single place where a key maps to an action. Latches the movement, roll and run states, and acts on the leading edge of a tapped key: `keyToggleEase` and `keyToggleFly` flip their modes, `keySavePosition` and `keyLoadPosition` write and read the pose file. The repeats a held key produces are ignored, and tapped keys do nothing while the camera is not controlled. On the ASCII fallback path it also infers the run state from the case of movement letters, since `ofAppEGLWindow` never reports the shift keys themselves.

- **`void startupLoad()`**
  Runs once, on the first `update()` rather than from the constructor, so that the application's `setup()` has already had its say over `cameraPositionFile` and over the pose that acts as the fallback. Loads `cameraPositionFile` when `loadPositionOnStartup` is on and the file exists; a missing file is a silent no-op, since that is just a first run.

- **`void trackPoseChanges()`**
  Compares the pose against the previous frame's rather than flagging every place that writes one, so that moves the application makes itself are noticed too. Marks the pose unsaved while it is changing, and triggers the autosave on the first frame it stops, which is what keeps `autosavePosition` from writing the file every frame of a move.

- **`void nodeRotate(ofMouseEventArgs&)`**
  Calculates pitch and yaw based on mouse deltas and applies rotation to the camera node.

- **`void centerCursor()`**
  When compiled for GLFW (`TARGET_GLFW_WINDOW`), directly accesses the GLFW window to reposition the cursor to the center of the screen, used to prevent the cursor from leaving the window boundaries while looking around. On EGL, this is a no-op as the camera relies on relative mouse position deltas instead.

### Public Properties

- **Key Bindings (int):**
  `keyUp`, `keyDown`, `keyLeft`, `keyRight`, `keyForward`, `keyBackward`, `keyRollLeft`, `keyRollRight`, `keyRollReset`, `keyRun`, `keyToggleEase`, `keyToggleFly`, `keyLoadPosition`, `keySavePosition`
  *(Mapped to `GLFW_KEY_*` constants on desktop, or standard ASCII characters and `OF_KEY_*` constants when falling back to EGL)*
  `keyRun` defaults to shift, and while it is bound to either shift key both of them run; rebinding it to anything else matches that one key exactly.
  `keyLoadPosition` (`I`) and `keySavePosition` (`O`) are tapped, like the two mode toggles: they act once per press and are ignored while control is disabled.

- **Settings (float):**
  - `movespeed`: Units to move per frame.
  - `runspeed`: Multiplier applied to `movespeed` while `keyRun` is held (default `2.0`).
  - `rollspeed`: Degrees to roll per frame.
  - `sensitivity`: Degrees of camera rotation per pixel of mouse movement.
  - `easetime`: Seconds spent accelerating from a standstill to full speed while `easein` is on (default `0.25`). Zero or less behaves as if ease-in were off.

- **Modes (bool):**
  - `easein` (key `T`, default `false`): Ramps movement speed up linearly over `easetime` instead of starting at full pace. Stopping stays immediate, and the ramp restarts once no movement key is held. Releasing the run key drops the speed back to walking pace right away rather than coasting.
  - `flymode` (key `Z`, default `true`): Free flight, where forward/backward follow wherever the camera is looking. Turning it off walks instead: the forward and strafe axes are projected onto the plane that `upvector` is normal to, so looking up or down no longer lifts the camera off it, and `keyUp`/`keyDown` become the only way to change height (moving along `upvector` rather than the camera's tilted up direction). Looking exactly along `upvector` collapses the forward axis, in which case forward/backward simply does not move.

  Both default to the camera's behaviour before these modes existed, so an existing app is unaffected until a key is tapped or a flag is set. The toggle keys are ignored while control is disabled.

- **Saving and Loading:**
  - `cameraPositionFile` (`std::string`, default `"ofxFirstPersonCamera.xml"`): Where the save/load methods go when called without a file name. Relative paths resolve under `bin/data`.
  - `autosavePosition` (`bool`, default `false`): Writes the pose on the first frame after it stops changing, and once more from the destructor if anything is still unsaved. Waiting for the pose to settle is what keeps a move from writing the file every frame.
  - `loadPositionOnStartup` (`bool`, default `true`): Reads `cameraPositionFile` back on the first frame, so the camera starts where it was left. Does nothing when the file does not exist yet, and steps aside if `loadCameraPosition()` has already been called.

- **Vectors:**
  - `upvector`: A `glm::vec3` representing the camera's up vector (default: `{ 0.0f, 1.0f, 0.0f }`). Rolling updates it, so the walking plane rolls with the camera.

### On-Disk Format

Written with the core `ofXml`, in the layout `ofxFPSCamera` and `ofxGameCamera` use, plus an `orientation` block:

```xml
<camera>
  <position><X/><Y/><Z/></position>
  <orientation><X/><Y/><Z/><W/></orientation>
  <up><X/><Y/><Z/></up>
  <look><X/><Y/><Z/></look>
  <FOV/>
</camera>
```

The reference addons store only position, up and look, and rebuild the orientation with `lookAt()` on load. That cannot express roll, which this camera has, so the orientation quaternion is written too and is what loading prefers. `look` is still written for older readers, and is what loading falls back to when a file has no `orientation` block, which is how a file written by `ofxFPSCamera` still loads. Values are written with six decimals rather than the default six *significant* digits, so a position out at scene scale survives the round trip.
