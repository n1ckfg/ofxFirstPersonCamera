#pragma once

#include "ofMain.h"

#ifndef TARGET_GLFW_WINDOW
  #error "ofxFirstPersonCamera needs an ofAppGLFWWindow (TARGET_GLFW_WINDOW)."
#endif

// ofMain.h has already pulled in GLEW, so it is safe to let GLFW bring in
// whatever GL headers it wants here. Exposed so that users can reassign the
// key bindings below with GLFW_KEY_* constants.
#include <GLFW/glfw3.h>

class ofxFirstPersonCamera : public ofCamera
{
  public:

    ofxFirstPersonCamera();
   ~ofxFirstPersonCamera();

    bool isControlled() const;
    void toggleControl();
    void enableControl();
    void disableControl();

    // GLFW_KEY_* keycodes, independent of keyboard layout and modifiers
    int keyUp         = GLFW_KEY_E;
    int keyDown       = GLFW_KEY_C;
    int keyLeft       = GLFW_KEY_A;
    int keyRight      = GLFW_KEY_D;
    int keyForward    = GLFW_KEY_W;
    int keyBackward   = GLFW_KEY_S;
    int keyRollLeft   = GLFW_KEY_Q;
    int keyRollRight  = GLFW_KEY_R;
    int keyRollReset  = GLFW_KEY_F;

    float movespeed   = 1.00f;
    float rollspeed   = 1.00f;
    float sensitivity = 0.10f;

    glm::vec3 upvector { 0.0f, 1.0f, 0.0f };

  protected:

    void update(ofEventArgs&);

    void keyPressed(ofKeyEventArgs&);
    void keyReleased(ofKeyEventArgs&);
    void mouseMoved(ofMouseEventArgs&);
    void mouseDragged(ofMouseEventArgs&);

  private:

    void nodeRotate(ofMouseEventArgs&);
    void centerCursor();

    GLFWwindow* m_glfwWindow = nullptr;

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
