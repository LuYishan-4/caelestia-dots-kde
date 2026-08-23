#include "GPUContextGL.h"
#include "GPUDriverGL.h"
#include <EGL/egl.h>
#include <QDebug>
#include <epoxy/gl.h>
#include <cstdio>
namespace ultralight {
GPUContextGL::GPUContextGL(bool enable_vsync, bool enable_msaa)
    : GPUContextGL(Mode::OwnedOffscreen, enable_vsync, enable_msaa) {}
GPUContextGL::GPUContextGL(Mode mode, bool enable_vsync, bool enable_msaa)
    : msaa_enabled_(enable_msaa), mode_(mode) {
#if !defined(_WIN32)
  (void)enable_vsync;
#endif
  if (mode_ == Mode::OwnedOffscreen) {
#if defined(_WIN32)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
    if (enable_msaa) {
      glfwWindowHint(GLFW_SAMPLES, 4);
    }
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    GLFWwindow *win = glfwCreateWindow(10, 10, "", NULL, NULL);
    window_ = win;
    if (!window_) {
      glfwTerminate();
      exit(EXIT_FAILURE);
    }
    glfwMakeContextCurrent(window_);
    // Windows still needs glad since there's no epoxy equivalent readily
    // available there; left as-is, out of scope for this change.
    gladLoadGLLoader((GLADloadproc)glfwGetProcAddress);
    glfwSwapInterval(enable_vsync ? 1 : 0);
    int samples = 4;
    glGetIntegerv(GL_SAMPLES, &samples);
    if (!samples) {
      msaa_enabled_ = false;
    }
    if (msaa_enabled_) {
      glEnable(GL_MULTISAMPLE);
    }
#else
    // ExternalCurrent-style reuse of KWin's own already-current EGL/GL(ES)
    // context. No loader call needed at all: epoxy resolves every GL
    // symbol transparently on first use against whatever context is
    // current at call time, unlike glad which requires an explicit
    // gladLoadXXXLoader() step before any GL call is safe to make.
    external_context_token_ = this;
    msaa_enabled_ = false;
#endif
  }
  driver_.reset(new ultralight::GPUDriverGL(this));
}
bool GPUContextGL::has_current_context() const {
#if defined(_WIN32)
  return glfwGetCurrentContext() != nullptr;
#else
  // With epoxy there's no separate "loader" readiness step — the only
  // thing that matters is whether a context is actually bound right now.
  return eglGetCurrentContext() != EGL_NO_CONTEXT;
#endif
}
bool GPUContextGL::is_glad_ready() const {
#if defined(_WIN32)
  return glad_glGetString != nullptr && glad_glBindTexture != nullptr;
#else
  // Kept for interface compatibility with callers (e.g.
  // UltralightHtmlEffect::ensureInitialized() logs off of this). epoxy
  // symbols are always "ready" as soon as a context is current, so this
  // just re-checks the same condition as has_current_context().
  return eglGetCurrentContext() != EGL_NO_CONTEXT;
#endif
}

bool GPUContextGL::supports_required_gles_version() const {
#if defined(_WIN32)
  // The standalone GLFW path explicitly requests desktop OpenGL 3.2.
  return true;
#else
  if (!has_current_context())
    return false;

  const auto *version = reinterpret_cast<const char *>(glGetString(GL_VERSION));
  GLint major = 0;
  GLint minor = 0;
  // KWin may expose an ES 2 context. GL_MAJOR_VERSION and GL_MINOR_VERSION
  // are invalid there, so parse GL_VERSION instead of leaving a GL error in
  // KWin's current context.
  const bool is_gles = version &&
      std::sscanf(version, "OpenGL ES %d.%d", &major, &minor) == 2;
  const bool meets_minimum = major > 3 || (major == 3 && minor >= 2);
  if (!is_gles || !meets_minimum) {
    if (!unsupported_context_reported_) {
      qWarning().noquote()
          << "[UltralightCursorEffect] Unsupported KWin GL context:"
          << (version ? version : "unknown")
          << "(requires OpenGL ES 3.2 or newer)";
      unsupported_context_reported_ = true;
    }
    return false;
  }
  unsupported_context_reported_ = false;
  return true;
#endif
}
void *GPUContextGL::current_context_token() const {
  if (mode_ == Mode::ExternalCurrent) {
    return reinterpret_cast<void *>(eglGetCurrentContext());
  }
#if defined(_WIN32)
  return reinterpret_cast<void *>(glfwGetCurrentContext());
#else
  return const_cast<GPUContextGL *>(this);
#endif
}
} // namespace ultralight
