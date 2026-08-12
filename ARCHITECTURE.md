# Architecture: ofxFirstPersonCamera

`ofxFirstPersonCamera` is a lightweight openFrameworks addon that provides a first-person camera class, replicating standard video game camera controls. The class extends `ofCamera` and relies on GLFW to capture and manipulate the mouse cursor.

## Class: `ofxFirstPersonCamera`

Inherits from `ofCamera`.

### Overview
This class manages the view matrix for a first-person perspective. It handles keyboard and mouse events natively through openFrameworks event listeners (`ofEventArgs`, `ofKeyEventArgs`, `ofMouseEventArgs`). By default, it uses standard `WASD` controls for movement, `X`/`C` for rolling, `E`/`Q` for vertical translation, and the mouse for looking around.

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
  Called every frame. Applies the current movement velocities to the camera's position based on active key presses, scaled by the frame rate to ensure consistent movement speed.

- **`void keyPressed(ofKeyEventArgs&)`**
  Called when a key is pressed. Updates internal boolean states for active movement actions (e.g., setting "Forward" to true).

- **`void keyReleased(ofKeyEventArgs&)`**
  Called when a key is released. Reverts internal boolean states for movement actions.

- **`void mouseMoved(ofMouseEventArgs&)`**
  Called when the mouse is moved. Delegates to `nodeRotate()` if the camera is controlled.

- **`void mouseDragged(ofMouseEventArgs&)`**
  Called when the mouse is dragged (moved while clicked). Also delegates to `nodeRotate()` if the camera is controlled.

### Private Methods

- **`void nodeRotate(ofMouseEventArgs&)`**
  Calculates pitch and yaw based on mouse deltas and applies rotation to the camera node.

- **`void centerCursor()`**
  Directly accesses the GLFW window to reposition the cursor to the center of the screen, used to prevent the cursor from leaving the window boundaries while looking around.

### Public Properties

- **Key Bindings (int):**
  `keyUp`, `keyDown`, `keyLeft`, `keyRight`, `keyForward`, `keyBackward`, `keyRollLeft`, `keyRollRight`, `keyRollReset`
  *(Mapped to GLFW_KEY_* constants)*

- **Settings (float):**
  - `movespeed`: Units to move per frame.
  - `rollspeed`: Degrees to roll per frame.
  - `sensitivity`: Degrees of camera rotation per pixel of mouse movement.

- **Vectors:**
  - `upvector`: A `glm::vec3` representing the camera's up vector (default: `{ 0.0f, 1.0f, 0.0f }`).
