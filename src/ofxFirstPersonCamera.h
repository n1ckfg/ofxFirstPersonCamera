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
#endif

    float movespeed   = 1.00f;
    float rollspeed   = 1.00f;
    float sensitivity = 0.10f;

    glm::vec3 upvector { 0.0f, 1.0f, 0.0f };

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
    } m_doa;

    bool m_isControlled  = false;
    bool m_isMouseInited = false;
};
