#pragma once

#include "ofMain.h"

#ifdef TARGET_GLFW_WINDOW
// ofMain.h has already pulled in GLEW, so it is safe to let GLFW bring in
// whatever GL headers it wants here. Exposed so that users can reassign the
// key bindings below with GLFW_KEY_* constants.
#include <GLFW/glfw3.h>
#elif defined(TARGET_LINUX)
// ofAppEGLWindow (Raspberry Pi and friends) keeps its own cursor position and
// clamps it to the window, so the pointer runs out of room to move. Read the
// relative motion straight off the evdev devices instead to get the same
// unbounded looking that GLFW_CURSOR_DISABLED gives us on desktop.
#define OFX_FPC_EVDEV_MOUSE
#endif

class ofxFirstPersonCamera : public ofCamera
{
  public:

    ofxFirstPersonCamera();
   ~ofxFirstPersonCamera();

    bool isControlled() const;
    void toggleControl();
    void enableControl();
    void disableControl();

    // Position, orientation, up vector and field of view, written as XML
    // through the core ofXml so that the addon stays dependency free. The
    // orientation is stored as a quaternion, so roll and the exact pose come
    // back rather than being rebuilt from a look target.
    bool saveCameraPosition();
    bool saveCameraPosition(const std::string& file);
    bool loadCameraPosition();
    bool loadCameraPosition(const std::string& file);

    // True between a move and the next successful save
    bool hasUnsavedPosition() const;

#ifdef TARGET_GLFW_WINDOW
    // GLFW_KEY_* keycodes, independent of keyboard layout and modifiers
    int keyUp         = GLFW_KEY_E;
    int keyDown       = GLFW_KEY_Q;
    int keyLeft       = GLFW_KEY_A;
    int keyRight      = GLFW_KEY_D;
    int keyForward    = GLFW_KEY_W;
    int keyBackward   = GLFW_KEY_S;
    int keyRollLeft   = GLFW_KEY_X;
    int keyRollRight  = GLFW_KEY_C;
    int keyRollReset  = GLFW_KEY_F;
    // Held down rather than tapped. Either shift key runs while this is bound
    // to one of them; rebinding it to anything else matches that key exactly.
    int keyRun        = GLFW_KEY_LEFT_SHIFT;
    // Tapped rather than held. Ignored while control is disabled, so that
    // they stay usable as application shortcuts when the camera is not
    // driving.
    int keyToggleEase   = GLFW_KEY_T;
    int keyToggleFly    = GLFW_KEY_Z;
    int keyLoadPosition = GLFW_KEY_I;
    int keySavePosition = GLFW_KEY_O;
#else
    int keyUp         = 'e';
    int keyDown       = 'q';
    int keyLeft       = 'a';
    int keyRight      = 'd';
    int keyForward    = 'w';
    int keyBackward   = 's';
    int keyRollLeft   = 'x';
    int keyRollRight  = 'c';
    int keyRollReset  = 'f';
    int keyRun          = OF_KEY_LEFT_SHIFT;
    int keyToggleEase   = 't';
    int keyToggleFly    = 'z';
    int keyLoadPosition = 'i';
    int keySavePosition = 'o';
#endif

    float movespeed   = 1.00f;
    float rollspeed   = 1.00f;
    float sensitivity = 0.10f;

    // movespeed is multiplied by this while keyRun is held
    float runspeed    = 2.00f;

    // Seconds spent accelerating from a standstill to full speed when easein
    // is on. Zero or less makes movement start at full speed as if it was off.
    float easetime    = 0.25f;

    // Ramp the movement speed up instead of starting at full pace (key: T)
    bool easein  = false;

    // Free flight: forward/backward follow wherever the camera is looking.
    // Turn it off (key: Z) to walk instead, where forward/backward stay on the
    // plane that upvector is normal to and only keyUp/keyDown change height.
    bool flymode = true;

    glm::vec3 upvector { 0.0f, 1.0f, 0.0f };

    // Where saveCameraPosition()/loadCameraPosition() go when they are called
    // without a file name. Relative paths land in bin/data, as everywhere else
    // in openFrameworks.
    std::string cameraPositionFile = "ofxFirstPersonCamera.xml";

    // Writes the pose out on the first frame after it stops changing, and once
    // more from the destructor if anything is still unsaved
    bool autosavePosition = false;

    // Reads cameraPositionFile back on the first frame, so the camera starts
    // where it was left. Nothing happens when the file is not there yet, and
    // calling loadCameraPosition() yourself first takes precedence.
    bool loadPositionOnStartup = true;

#ifdef OFX_FPC_EVDEV_MOUSE
    // Take the mouse away from the rest of the system while control is
    // enabled, so that ofAppEGLWindow stops drawing/moving its own cursor.
    // The app will not receive mouse button events either, so this is off by
    // default. Set it before calling enableControl().
    bool grabMouseDevice = false;
#endif

  protected:

    void update(ofEventArgs&);

    void keyPressed(ofKeyEventArgs&);
    void keyReleased(ofKeyEventArgs&);
    void mouseMoved(ofMouseEventArgs&);
    void mouseDragged(ofMouseEventArgs&);

  private:

    void setAction(int key, bool pressed);
    void startupLoad();
    void trackPoseChanges();
    void nodeRotate(ofMouseEventArgs&);
    void centerCursor();

#ifdef TARGET_GLFW_WINDOW
    GLFWwindow* m_glfwWindow = nullptr;
#endif

#ifdef OFX_FPC_EVDEV_MOUSE
    void openMouseDevices();
    void closeMouseDevices();
    bool pollMouseDevices(float& xdelta, float& ydelta);
    void applyRawRotation();

    std::vector<int> m_mouseFds;
#endif

    struct Actions {
      bool Up        = false;
      bool Down      = false;
      bool Left      = false;
      bool Right     = false;
      bool Forward   = false;
      bool Backward  = false;
      bool RollLeft  = false;
      bool RollRight = false;
      bool RollReset = false;
      bool Run       = false;
      // The tapped keys repeat while held, so remember whether the last
      // event already did the deed
      bool EaseHeld  = false;
      bool FlyHeld   = false;
      bool LoadHeld  = false;
      bool SaveHeld  = false;
    } m_doa;

    // Movement speed the ease in has reached so far, in units per frame at
    // 60 fps, just like movespeed
    float m_speedmod = 0.0f;

    // Pose as of the last frame, so that a change can be spotted wherever it
    // came from, this class or the application moving the camera itself
    glm::vec3 m_lastpos { 0.0f, 0.0f, 0.0f };
    glm::quat m_lastrot { 1.0f, 0.0f, 0.0f, 0.0f };

    bool m_unsavedPosition = false;
    bool m_didStartupLoad  = false;
    bool m_isControlled    = false;
    bool m_isMouseInited   = false;
};
