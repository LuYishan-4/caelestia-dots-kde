#pragma once
#include "GPUDriverImpl.h"
#include <Ultralight/platform/Config.h>
#include <Ultralight/platform/GPUDriver.h>
#include <memory>
#if defined(_WIN32)
typedef struct GLFWwindow GLFWwindow;
#endif
#define ENABLE_OFFSCREEN_GL 0
namespace ultralight {

class GPUContextGL {
public:
  enum class Mode {
    OwnedOffscreen,
    ExternalCurrent,
  };

protected:
  std::unique_ptr<ultralight::GPUDriverImpl> driver_;
#if defined(_WIN32)
  GLFWwindow *window_ = nullptr;
  GLFWwindow *active_window_ = nullptr;
#endif
  bool msaa_enabled_;
  Mode mode_;
  void *external_context_token_ = nullptr;
  mutable bool unsupported_context_reported_ = false;

public:
  GPUContextGL(bool enable_vsync, bool enable_msaa);
  GPUContextGL(Mode mode, bool enable_vsync, bool enable_msaa);

  virtual ~GPUContextGL() {}

  virtual ultralight::GPUDriverImpl *driver() const { return driver_.get(); }

  virtual ultralight::FaceWinding face_winding() const {
    return ultralight::FaceWinding::CounterClockwise;
  }

  virtual void BeginDrawing() {}

  virtual void EndDrawing() {}

  virtual bool msaa_enabled() const { return msaa_enabled_; }
  virtual Mode mode() const { return mode_; }
  virtual bool owns_context() const { return mode_ == Mode::OwnedOffscreen; }
  virtual bool uses_external_context() const {
    return mode_ == Mode::ExternalCurrent;
  }
  virtual bool has_current_context() const;
  virtual bool is_glad_ready() const;
  // KWin 6.8 and newer render effects with GLES.  Ultralight's generated
  // shaders use GLSL ES 3.20, so accepting a desktop GL 3.1 context here
  // would only defer the failure to shader compilation.
  virtual bool supports_required_gles_version() const;
  virtual void set_external_context_token(void *token) {
    external_context_token_ = token;
  }
  virtual void *current_context_token() const;

  // An offscreen window dedicated to maintaining the OpenGL context.
  // All other windows created during lifetime of the app share this context.

#if defined(_WIN32)
  virtual GLFWwindow *window() { return window_; }

  // FBOs are not shared across contexts in OpenGL 3.2 (AFAIK), we luckily
  // don't need to share them across multiple windows anyways so we temporarily
  // set the active GL context to the "active window" when creating FBOs.
  virtual void set_active_window(GLFWwindow *win) { active_window_ = win; }

  virtual GLFWwindow *active_window() { return active_window_; }
#endif
};

} // namespace ultralight
