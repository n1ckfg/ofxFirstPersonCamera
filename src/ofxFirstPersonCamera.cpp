#include "ofxFirstPersonCamera.h"

#ifdef OFX_FPC_EVDEV_MOUSE
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/input.h>
#include <cstring>

namespace {

// ofAppEGLWindow multiplies every relative motion count by its mouseScaleX/Y,
// which default to 2.0. Match it so that a given sensitivity feels the same
// whether we read the device ourselves or fall back to window mouse events.
const float kEglMouseScale = 2.0f;

constexpr int kBitsPerLong = sizeof(long) * 8;

constexpr int bitmapLongs(int bits)
{
  return (bits - 1) / kBitsPerLong + 1;
}

bool testBit(int bit, const unsigned long* bitmap)
{
  return (bitmap[bit / kBitsPerLong] >> (bit % kBitsPerLong)) & 1ul;
}

// A pointing device we can use is one that reports relative motion on both
// axes and has at least a left button: that rules out volume knobs, tilt
// wheels and the multi-touch/absolute devices that only speak EV_ABS.
bool isRelativePointer(int fd)
{
  unsigned long types[bitmapLongs(EV_MAX)];
  memset(types, 0, sizeof(types));
  if (ioctl(fd, EVIOCGBIT(0, sizeof(types)), types) < 0) return false;
  if (!testBit(EV_REL, types) || !testBit(EV_KEY, types)) return false;

  unsigned long axes[bitmapLongs(REL_MAX)];
  memset(axes, 0, sizeof(axes));
  if (ioctl(fd, EVIOCGBIT(EV_REL, sizeof(axes)), axes) < 0) return false;
  if (!testBit(REL_X, axes) || !testBit(REL_Y, axes)) return false;

  unsigned long keys[bitmapLongs(KEY_MAX)];
  memset(keys, 0, sizeof(keys));
  if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return false;

  return testBit(BTN_LEFT, keys);
}

} // namespace
#endif

namespace {

// Bindings are matched case insensitively. The ASCII path reports the
// character the keyboard produced, so tapping shift halfway through a
// movement turns 'w' into 'W': the release no longer matches the binding
// that the press did and the action stays latched on until the key is
// pressed and released again with the same modifiers. Folding both the
// incoming key and the binding to lower case keeps a press and its release
// looking alike. GLFW keycodes for letters are the upper case ASCII values
// and nothing else lives in the 'a'-'z' range, so this is a no-op there
// beyond letting users write their bindings either way.
int foldKey(int key)
{
  if (key >= 'A' && key <= 'Z') return key - 'A' + 'a';
  return key;
}

bool keyMatches(int key, int binding)
{
  return foldKey(key) == foldKey(binding);
}

#ifndef TARGET_GLFW_WINDOW
bool isUpperLetter(int key)
{
  return key >= 'A' && key <= 'Z';
}

bool isLetter(int key)
{
  return isUpperLetter(key) || (key >= 'a' && key <= 'z');
}
#endif

bool isShiftKey(int key)
{
#ifdef TARGET_GLFW_WINDOW
  return key == GLFW_KEY_LEFT_SHIFT || key == GLFW_KEY_RIGHT_SHIFT;
#else
  return key == OF_KEY_LEFT_SHIFT || key == OF_KEY_RIGHT_SHIFT;
#endif
}

// A binding can only name one of the two shift keys, but both of them should
// run. Any other binding is matched as usual.
bool runKeyMatches(int key, int binding)
{
  return isShiftKey(binding) ? isShiftKey(key) : keyMatches(key, binding);
}

// glm::normalize() is undefined on a zero length vector, and the projection
// below legitimately collapses to zero when looking straight up or down
glm::vec3 safeNormalize(const glm::vec3& v)
{
  const float len = glm::length(v);
  if (len < 1e-6f) return glm::vec3(0.0f);
  return v / len;
}

// Drops the component along axis (which must be a unit vector), leaving a
// direction that runs along the plane the axis is normal to
glm::vec3 flattenTo(const glm::vec3& v, const glm::vec3& axis)
{
  return safeNormalize(v - axis * glm::dot(v, axis));
}

bool isZero(const glm::vec3& v)
{
  return glm::dot(v, v) <= 0.0f;
}

// ofXml::set() formats through the default stream precision, which is six
// significant digits: enough to round a position like 12345.678 off to the
// nearest hundredth. Ask for a fixed number of decimals instead, so that a
// pose survives the round trip whatever the scale of the scene.
void setFloat(ofXml node, float value)
{
  node.set(ofToString(value, 6));
}

void appendVec3(ofXml parent, const std::string& name, const glm::vec3& v)
{
  ofXml node = parent.appendChild(name);
  setFloat(node.appendChild("X"), v.x);
  setFloat(node.appendChild("Y"), v.y);
  setFloat(node.appendChild("Z"), v.z);
}

// Missing tags fall back rather than reading as zero, so that a hand edited
// or older file still loads with something sane in the gaps
float childFloat(const ofXml& parent, const std::string& name, float fallback)
{
  if (!parent) return fallback;

  const ofXml node = parent.getChild(name);
  return node ? node.getFloatValue() : fallback;
}

glm::vec3 childVec3(const ofXml& parent, const std::string& name, const glm::vec3& fallback)
{
  const ofXml node = parent ? parent.getChild(name) : ofXml();
  if (!node) return fallback;

  return glm::vec3(childFloat(node, "X", fallback.x),
                   childFloat(node, "Y", fallback.y),
                   childFloat(node, "Z", fallback.z));
}

} // namespace

ofxFirstPersonCamera::ofxFirstPersonCamera()
{
  auto &events = ofEvents();
  ofAddListener(events.update      , this, &ofxFirstPersonCamera::update      , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.keyPressed  , this, &ofxFirstPersonCamera::keyPressed  , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.keyReleased , this, &ofxFirstPersonCamera::keyReleased , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.mouseMoved  , this, &ofxFirstPersonCamera::mouseMoved  , OF_EVENT_ORDER_BEFORE_APP);
  ofAddListener(events.mouseDragged, this, &ofxFirstPersonCamera::mouseDragged, OF_EVENT_ORDER_BEFORE_APP);
}

ofxFirstPersonCamera::~ofxFirstPersonCamera()
{
  // Catches a pose that was still moving when the app was closed. Quitting
  // mid-flight is otherwise the one way to lose the last few frames of it.
  if (autosavePosition && m_unsavedPosition) saveCameraPosition();

  auto &events = ofEvents();
  ofRemoveListener(events.update      , this, &ofxFirstPersonCamera::update      , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.keyPressed  , this, &ofxFirstPersonCamera::keyPressed  , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.keyReleased , this, &ofxFirstPersonCamera::keyReleased , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.mouseMoved  , this, &ofxFirstPersonCamera::mouseMoved  , OF_EVENT_ORDER_BEFORE_APP);
  ofRemoveListener(events.mouseDragged, this, &ofxFirstPersonCamera::mouseDragged, OF_EVENT_ORDER_BEFORE_APP);

#ifdef OFX_FPC_EVDEV_MOUSE
  closeMouseDevices();
#endif
}

bool ofxFirstPersonCamera::isControlled() const
{
  return m_isControlled;
}

void ofxFirstPersonCamera::toggleControl()
{
  m_isControlled ? disableControl() : enableControl();
}

void ofxFirstPersonCamera::enableControl()
{
#ifdef TARGET_GLFW_WINDOW
  auto win = dynamic_cast<ofAppGLFWWindow*>(ofGetWindowPtr());
  if (!win) {
    ofLogError("ofxFirstPersonCamera") << "needs an ofAppGLFWWindow, control not enabled";
    return;
  }

  m_glfwWindow = win->getGLFWWindow();
  glfwSetInputMode(m_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
#else
  ofHideCursor();
#endif

#ifdef OFX_FPC_EVDEV_MOUSE
  openMouseDevices();

  // Throw away whatever piled up in the device buffers while we were not
  // looking, otherwise the camera jumps on the first frame
  float discard_x;
  float discard_y;
  pollMouseDevices(discard_x, discard_y);
#endif

  // Ignore whatever the pointer was doing before we grabbed it
  m_isMouseInited = false;
  centerCursor();

  m_isControlled = true;
}

void ofxFirstPersonCamera::disableControl()
{
#ifdef TARGET_GLFW_WINDOW
  if (m_glfwWindow) {
    glfwSetInputMode(m_glfwWindow, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
  }
#else
  ofShowCursor();
#endif

#ifdef OFX_FPC_EVDEV_MOUSE
  closeMouseDevices();
#endif

  m_isControlled = false;
}

bool ofxFirstPersonCamera::hasUnsavedPosition() const
{
  return m_unsavedPosition;
}

bool ofxFirstPersonCamera::saveCameraPosition()
{
  return saveCameraPosition(cameraPositionFile);
}

// The layout follows the one ofxFPSCamera and ofxGameCamera write, with the
// orientation quaternion added: position, up and look on their own are a pose
// that has to be rebuilt through lookAt(), which cannot express roll and
// rounds the rest. The look target is still written for those older readers.
bool ofxFirstPersonCamera::saveCameraPosition(const std::string& file)
{
  const glm::vec3 pos = this->getPosition();
  const glm::quat rot = this->getOrientationQuat();

  ofXml xml;
  ofXml camera = xml.appendChild("camera");

  appendVec3(camera, "position", pos);

  ofXml orientation = camera.appendChild("orientation");
  setFloat(orientation.appendChild("X"), rot.x);
  setFloat(orientation.appendChild("Y"), rot.y);
  setFloat(orientation.appendChild("Z"), rot.z);
  setFloat(orientation.appendChild("W"), rot.w);

  appendVec3(camera, "up", upvector);
  appendVec3(camera, "look", pos + this->getLookAtDir());

  setFloat(camera.appendChild("FOV"), this->getFov());

  // pugixml refuses to write into a folder that is not there, and plenty of
  // projects have never needed a bin/data until now
  const std::string folder = ofFilePath::getEnclosingDirectory(ofToDataPath(file), false);
  if (!folder.empty() && !ofDirectory::doesDirectoryExist(folder, false)) {
    ofDirectory::createDirectory(folder, false, true);
  }

  if (!xml.save(file)) {
    ofLogError("ofxFirstPersonCamera") << "could not save the camera position to " << file;
    return false;
  }

  m_unsavedPosition = false;
  return true;
}

bool ofxFirstPersonCamera::loadCameraPosition()
{
  return loadCameraPosition(cameraPositionFile);
}

bool ofxFirstPersonCamera::loadCameraPosition(const std::string& file)
{
  ofXml xml;
  if (!xml.load(file)) {
    ofLogError("ofxFirstPersonCamera") << "could not load a camera position from " << file;
    return false;
  }

  const ofXml camera = xml.getChild("camera");
  if (!camera) {
    ofLogError("ofxFirstPersonCamera") << file << " has no <camera> in it";
    return false;
  }

  this->setPosition(childVec3(camera, "position", this->getPosition()));

  upvector = childVec3(camera, "up", glm::vec3(0.0f, 1.0f, 0.0f));

  const ofXml orientation = camera.getChild("orientation");
  if (orientation) {
    // Restores the pose as it was, roll included
    this->setOrientation(glm::quat(childFloat(orientation, "W", 1.0f),
                                   childFloat(orientation, "X", 0.0f),
                                   childFloat(orientation, "Y", 0.0f),
                                   childFloat(orientation, "Z", 0.0f)));
  }
  else if (camera.getChild("look")) {
    // A file from a writer that only knew about look targets, ofxFPSCamera
    // among them. Aiming at the target is the best that can be done with it.
    this->lookAt(childVec3(camera, "look", this->getPosition() + this->getLookAtDir()), upvector);
  }

  const ofXml fov = camera.getChild("FOV");
  if (fov) this->setFov(fov.getFloatValue());

  // The camera just moved, but to a pose that is already on disk
  m_lastpos = this->getPosition();
  m_lastrot = this->getOrientationQuat();
  m_unsavedPosition = false;

  return true;
}

// Watches the pose instead of flagging each place that writes one, so that
// moves the application makes itself count too. Autosaving then waits for the
// first frame that nothing moved: saving while the camera is still gliding
// would write the file every frame.
void ofxFirstPersonCamera::trackPoseChanges()
{
  const glm::vec3 pos = this->getPosition();
  const glm::quat rot = this->getOrientationQuat();

  if (pos != m_lastpos || rot != m_lastrot) {
    m_lastpos = pos;
    m_lastrot = rot;
    m_unsavedPosition = true;
    return;
  }

  if (m_unsavedPosition && autosavePosition) saveCameraPosition();
}

// Puts the cursor back in the middle of the window so that every mouse event
// can be read as a delta from the center. glfwGetWindowSize() reports screen
// coordinates, which is also what glfwSetCursorPos() expects.
void ofxFirstPersonCamera::centerCursor()
{
#ifdef TARGET_GLFW_WINDOW
  if (!m_glfwWindow) return;

  int win_w;
  int win_h;
  glfwGetWindowSize(m_glfwWindow, &win_w, &win_h);
  glfwSetCursorPos(m_glfwWindow, win_w / 2.0, win_h / 2.0);
#endif
}

#ifdef OFX_FPC_EVDEV_MOUSE

// The EGL window has no cursor to warp, so instead of reading a position and
// pushing it back to the center we read the raw motion counts the mice emit.
// Those have no bounds to begin with, which is exactly what we want.
void ofxFirstPersonCamera::openMouseDevices()
{
  closeMouseDevices();

  DIR* dir = opendir("/dev/input");
  if (!dir) {
    ofLogWarning("ofxFirstPersonCamera") << "cannot read /dev/input, falling back to window mouse events";
    return;
  }

  while (const dirent* entry = readdir(dir)) {
    if (strncmp(entry->d_name, "event", 5) != 0) continue;

    const std::string node = "/dev/input/" + std::string(entry->d_name);

    const int fd = open(node.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) continue;

    if (!isRelativePointer(fd)) {
      close(fd);
      continue;
    }

    // Exclusive access keeps ofAppEGLWindow (and every other reader) from
    // seeing the same motion, at the price of the app losing mouse buttons
    if (grabMouseDevice && ioctl(fd, EVIOCGRAB, 1) < 0) {
      ofLogWarning("ofxFirstPersonCamera") << "could not grab " << node;
    }

    ofLogVerbose("ofxFirstPersonCamera") << "reading raw mouse motion from " << node;
    m_mouseFds.push_back(fd);
  }

  closedir(dir);

  if (m_mouseFds.empty()) {
    ofLogWarning("ofxFirstPersonCamera")
      << "no readable mouse in /dev/input (is this user in the 'input' group?), "
      << "falling back to window mouse events, which stop at the window edges";
  }
}

void ofxFirstPersonCamera::closeMouseDevices()
{
  for (const int fd : m_mouseFds) {
    if (grabMouseDevice) ioctl(fd, EVIOCGRAB, 0);
    close(fd);
  }

  m_mouseFds.clear();
}

// Drains every pending event and sums the motion since the last frame.
// Returns false when there is no device to read, so that the caller can fall
// back to the window's own mouse events.
bool ofxFirstPersonCamera::pollMouseDevices(float& xdelta, float& ydelta)
{
  xdelta = 0.0f;
  ydelta = 0.0f;

  if (m_mouseFds.empty()) return false;

  for (const int fd : m_mouseFds) {
    input_event ev;

    while (read(fd, &ev, sizeof(ev)) == sizeof(ev)) {
      if (ev.type != EV_REL) continue;

      if      (ev.code == REL_X) xdelta += ev.value;
      else if (ev.code == REL_Y) ydelta += ev.value;
    }
  }

  return true;
}

// Deltas are physical mouse movement, already accumulated over the frame, so
// unlike the movement keys they must not be scaled by the frame time
void ofxFirstPersonCamera::applyRawRotation()
{
  float xdelta;
  float ydelta;
  if (!pollMouseDevices(xdelta, ydelta)) return;
  if (xdelta == 0.0f && ydelta == 0.0f) return;

  const float xdiff = -xdelta * kEglMouseScale * sensitivity;
  const float ydiff = -ydelta * kEglMouseScale * sensitivity;

  this->rotateDeg(ydiff, this->getSideDir());
  this->rotateDeg(xdiff, upvector);
}

#endif

void ofxFirstPersonCamera::update(ofEventArgs&)
{
  // Runs even when the camera is not being driven, so that a pose the
  // application set itself still gets picked up by the autosave
  trackPoseChanges();

  if (!m_isControlled) return;

#ifdef OFX_FPC_EVDEV_MOUSE
  applyRawRotation();
#endif

  // Keep movement frame rate independent, normalized to 60 fps. Guards
  // against the bogus frame times reported on the very first frames.
  float delta = ofGetLastFrameTime();
  if (delta <= 0.0f || delta > 0.25f) delta = 1.0f / 60.0f;
  const float step = delta * 60.0f;

  { // Roll
    const Actions doa = m_doa;
    const int rolldir = doa.RollLeft - doa.RollRight;

    if (rolldir) {
      this->rollDeg(rolldir * rollspeed * step);
      upvector = this->getUpDir();
    }

    if (doa.RollReset) {
      this->rollDeg(-this->getRollDeg());
      upvector = glm::vec3(0.0f, 1.0f, 0.0f);
    }
  }
  { // Position
    const Actions doa = m_doa;

    const float look = doa.Forward - doa.Backward;
    const float side = doa.Right   - doa.Left;
    const float up   = doa.Up      - doa.Down;

    if (look == 0 && side == 0 && up == 0)
    {
      // Standing still, so the next move eases in from a standstill again
      m_speedmod = 0.0f;
    }
    else
    {
      const float topspeed = movespeed * (doa.Run ? runspeed : 1.0f);

      if (!easein || easetime <= 0.0f) {
        m_speedmod = topspeed;
      } else {
        // Linear ramp that reaches top speed easetime seconds after the key
        // went down. The clamp works in both directions, so letting go of the
        // run key drops back to walking pace right away rather than coasting.
        m_speedmod += topspeed * delta / easetime;
        if (m_speedmod > topspeed) m_speedmod = topspeed;
      }

      glm::vec3 lookdir = this->getLookAtDir();
      glm::vec3 sidedir = this->getSideDir();
      glm::vec3 updir   = this->getUpDir();

      const glm::vec3 groundnormal = flymode ? glm::vec3(0.0f) : safeNormalize(upvector);

      if (!isZero(groundnormal)) {
        // Walking: take the vertical out of the forward and strafe axes so
        // that looking up or down stops lifting the camera off the ground
        // plane. Height is left to the up/down keys, which follow upvector
        // rather than wherever the camera is pointing.
        lookdir = flattenTo(lookdir, groundnormal);
        sidedir = flattenTo(sidedir, groundnormal);
        updir   = groundnormal;
      }

      const float speed = m_speedmod * step;
      this->move(lookdir * speed * look +
                 sidedir * speed * side +
                   updir * speed * up);
    }
  }
}

void ofxFirstPersonCamera::nodeRotate(ofMouseEventArgs& mouse)
{
  if (!m_isControlled) return;

#ifdef TARGET_GLFW_WINDOW
  if (!m_glfwWindow) return;
  const float win_center_x = ofGetWidth()  * 0.5f;
  const float win_center_y = ofGetHeight() * 0.5f;

  if (!m_isMouseInited) {
  // Swallows the first mouse move, which is a jump from wherever the
  // pointer happened to be rather than a real delta
    m_isMouseInited = true;
    centerCursor();
    return;
  }

  const float xdiff = (win_center_x - mouse.x) * sensitivity;
  const float ydiff = (win_center_y - mouse.y) * sensitivity;
#else
#ifdef OFX_FPC_EVDEV_MOUSE
  // update() is already turning the camera with the unclamped device deltas
  if (!m_mouseFds.empty()) return;
#endif

  if (!m_isMouseInited) {
    m_isMouseInited = true;
    return;
  }

  const float xdiff = (ofGetPreviousMouseX() - mouse.x) * sensitivity;
  const float ydiff = (ofGetPreviousMouseY() - mouse.y) * sensitivity;
#endif

  this->rotateDeg(ydiff, this->getSideDir());
  this->rotateDeg(xdiff, upvector);

  centerCursor();
}

void ofxFirstPersonCamera::mouseMoved(ofMouseEventArgs& mouse)
{
  nodeRotate(mouse);
}

void ofxFirstPersonCamera::mouseDragged(ofMouseEventArgs& mouse)
{
  nodeRotate(mouse);
}

void ofxFirstPersonCamera::setAction(int key, bool pressed)
{
  Actions doa = m_doa;
  bool ismovement = true;

  if      (keyMatches(key, keyUp       )) doa.Up       = pressed;
  else if (keyMatches(key, keyDown     )) doa.Down     = pressed;
  else if (keyMatches(key, keyLeft     )) doa.Left     = pressed;
  else if (keyMatches(key, keyRight    )) doa.Right    = pressed;
  else if (keyMatches(key, keyForward  )) doa.Forward  = pressed;
  else if (keyMatches(key, keyBackward )) doa.Backward = pressed;
  else {
    ismovement = false;

    if      (keyMatches(key, keyRollLeft )) doa.RollLeft  = pressed;
    else if (keyMatches(key, keyRollRight)) doa.RollRight = pressed;
    else if (keyMatches(key, keyRollReset)) doa.RollReset = pressed;

    else if (runKeyMatches(key, keyRun)) doa.Run = pressed;

    // Flip on the press that follows a release, never on the repeats a held
    // key produces. Left alone while control is disabled, so that a mode
    // cannot change behind the back of an app that is using the key itself.
    else if (keyMatches(key, keyToggleEase)) {
      if (pressed && !doa.EaseHeld && m_isControlled) easein = !easein;
      doa.EaseHeld = pressed;
    }
    else if (keyMatches(key, keyToggleFly)) {
      if (pressed && !doa.FlyHeld && m_isControlled) flymode = !flymode;
      doa.FlyHeld = pressed;
    }
  }

#ifndef TARGET_GLFW_WINDOW
  // ofAppEGLWindow never reports the shift keys themselves, all it does is
  // upper case the character they produce, so read the run state back off the
  // case of the movement keys: that is the only trace shift leaves there.
  // Caps lock looks the same as a held shift, which is the usual bargain.
  if (ismovement && isLetter(key)) doa.Run = isUpperLetter(key);
#else
  (void)ismovement;
#endif

  m_doa = doa;
}

void ofxFirstPersonCamera::keyPressed(ofKeyEventArgs& keys)
{
#ifdef TARGET_GLFW_WINDOW
  setAction(keys.keycode, true);
#else
  setAction(keys.key, true);
#endif
}

void ofxFirstPersonCamera::keyReleased(ofKeyEventArgs& keys)
{
#ifdef TARGET_GLFW_WINDOW
  setAction(keys.keycode, false);
#else
  setAction(keys.key, false);
#endif
}
