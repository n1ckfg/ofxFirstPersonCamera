# Architecture: ofxFirstPersonCamera

`ofxFirstPersonCamera` is a lightweight openFrameworks addon that provides a first-person camera class, replicating standard video game camera controls. The class extends `ofCamera` and relies on GLFW to capture and manipulate the mouse cursor natively, with automatic fallbacks for basic openFrameworks environments like `ofAppEGLWindow` (e.g. Raspberry Pi headless).

## Class: `ofxFirstPersonCamera`

Inherits from `ofCamera`.

### Overview
This class manages the view matrix for a first-person perspective. It handles keyboard and mouse events natively through openFrameworks event listeners (`ofEventArgs`, `ofKeyEventArgs`, `ofMouseEventArgs`). By default, it uses standard `WASD` controls for movement, `X`/`C` for rolling, `E`/`Q` for vertical translation, and the mouse for looking around. Holding `Shift` runs, and tapping `T` or `Z` toggles movement ease-in and fly mode respectively.

### Public Methods

- **`ofxFirstPersonCamera()`**
  Constructor. Initializes the camera and automatically registers the openFrameworks event listeners (update, mouse, and keyboard events).

- **`~ofxFirstPersonCamera()`**
  Destructor. Unregisters the openFrameworks event listeners to clean up nicely.

- **`bool isControlled() const`**
  Returns a boolean indicating whether the camera is currently in control mode (i.e., capturing the cursor and responding to input).

- **`void toggleControl()`**
  Toggles the control state of the camera. If currently controlled, it disables control; if disabled, it enables it.

- **`void enableControl()`**
  Enables camera control. This grabs the mouse cursor, hides it, and centers it in the window. The camera will now respond to movement keys and mouse rotation.

- **`void disableControl()`**
  Disables camera control. This releases the mouse cursor and makes it visible again. The camera will stop responding to input until re-enabled.

### Protected Methods (Event Handlers)

These methods are automatically bound to openFrameworks core events and generally do not need to be called manually.

- **`void update(ofEventArgs&)`**
  Called every frame. Applies the current movement velocities to the camera's position based on active key presses, scaled by the frame rate to ensure consistent movement speed. Also advances the ease-in ramp, applies the run multiplier, and constrains movement to the ground plane while `flymode` is off.

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
  Single place where a key maps to an action. Latches the movement, roll and run states, and flips `easein`/`flymode` on the leading edge of a toggle key press (ignoring the repeats a held key produces, and only while the camera is controlled). On the ASCII fallback path it also infers the run state from the case of movement letters, since `ofAppEGLWindow` never reports the shift keys themselves.

- **`void nodeRotate(ofMouseEventArgs&)`**
  Calculates pitch and yaw based on mouse deltas and applies rotation to the camera node.

- **`void centerCursor()`**
  When compiled for GLFW (`TARGET_GLFW_WINDOW`), directly accesses the GLFW window to reposition the cursor to the center of the screen, used to prevent the cursor from leaving the window boundaries while looking around. On EGL, this is a no-op as the camera relies on relative mouse position deltas instead.

### Public Properties

- **Key Bindings (int):**
  `keyUp`, `keyDown`, `keyLeft`, `keyRight`, `keyForward`, `keyBackward`, `keyRollLeft`, `keyRollRight`, `keyRollReset`, `keyRun`, `keyToggleEase`, `keyToggleFly`
  *(Mapped to `GLFW_KEY_*` constants on desktop, or standard ASCII characters and `OF_KEY_*` constants when falling back to EGL)*
  `keyRun` defaults to shift, and while it is bound to either shift key both of them run; rebinding it to anything else matches that one key exactly.

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

- **Vectors:**
  - `upvector`: A `glm::vec3` representing the camera's up vector (default: `{ 0.0f, 1.0f, 0.0f }`). Rolling updates it, so the walking plane rolls with the camera.
