#include "../header/KwinCursorEffect.hpp"
#include "../header/KwinMouseProvider.hpp"
#include "core/rendertarget.h"
#include "core/renderviewport.h"
#include "effect/effecthandler.h"
#include "opengl/glutils.h"
#include <QDBusConnection>
#include <QImage>
#include <qlogging.h>
// i know namespaces name is ever but i lazy
namespace {
const char *kDebugQuadVs = R"GLSL(
#version 320 es
precision highp float;
layout(location = 0) in vec2 inPos;
layout(location = 1) in vec2 inUv;
out vec2 vUv;
uniform mat4 uMvp;
void main() {
  vUv = inUv;
  gl_Position = uMvp * vec4(inPos, 0.0, 1.0);
}
)GLSL";

const char *kDebugQuadFs = R"GLSL(
#version 320 es
precision highp float;
precision highp int;
in vec2 vUv;
out vec4 fragColor;
uniform sampler2D uTex;
uniform int uDebugSolid;
uniform vec4 uSolidColor;
void main() {
  if (uDebugSolid != 0) {
    fragColor = uSolidColor;
  } else {
    vec4 tex = texture(uTex, vUv);
    fragColor = tex;
  }
}
)GLSL";

GLuint compileShader(GLenum type, const char *source) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &source, nullptr);
  glCompileShader(shader);

  GLint ok = GL_FALSE;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
  if (ok != GL_TRUE) {
    GLint logLen = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLen);
    QByteArray log(logLen > 1 ? logLen : 1, '\0');
    glGetShaderInfoLog(shader, log.size(), nullptr, log.data());
    qDebug() << "[UltralightCursorEffect] debug quad shader compile failed:"
             << log.constData();
  }

  return shader;
}

GLuint createDebugQuadProgram() {
  const GLuint vs = compileShader(GL_VERTEX_SHADER, kDebugQuadVs);
  const GLuint fs = compileShader(GL_FRAGMENT_SHADER, kDebugQuadFs);

  const GLuint program = glCreateProgram();
  glAttachShader(program, vs);
  glAttachShader(program, fs);
  glLinkProgram(program);

  GLint ok = GL_FALSE;
  glGetProgramiv(program, GL_LINK_STATUS, &ok);
  if (ok != GL_TRUE) {
    GLint logLen = 0;
    glGetProgramiv(program, GL_INFO_LOG_LENGTH, &logLen);
    QByteArray log(logLen > 1 ? logLen : 1, '\0');
    glGetProgramInfoLog(program, log.size(), nullptr, log.data());
    qDebug() << "[UltralightCursorEffect] debug quad program link failed:"
             << log.constData();
  }

  glDeleteShader(vs);
  glDeleteShader(fs);
  return program;
}
} // namespace

namespace KWin {

extern EffectsHandler *effects;

KWIN_EFFECT_FACTORY_SUPPORTED(KWin::KwinCursorEffect, "metadata.json",
                              return KWin::KwinCursorEffect::supported();)

KwinCursorEffect::KwinCursorEffect() {
  qDebug() << "[UltralightCursorEffect] KwinCursorEffect ctor: begin";
  if (!initializeCore<KwinMouseProvider>())
    return;
  qDebug()
      << "[UltralightCursorEffect] KwinCursorEffect ctor: core initialized";
  connect(effects, &EffectsHandler::windowActivated, this,
          &KwinCursorEffect::slotWindowStateChanged);
  qDebug() << "[UltralightCursorEffect] KwinCursorEffect ctor: windowActivated "
              "connected";
  m_mouseProvider->setCallback(
      [this](const UltralightWebCursorM::MousePoint &pt) {
        if (!m_html)
          return;
        QRect oldRect =
            getCursorRect(m_cursorPoint).toRect().adjusted(-20, -20, 20, 20);
        m_cursorPoint = QPointF(pt.x, pt.y);

        m_html->move(pt.x, pt.y, pt.pressed);

        QRect newRect =
            getCursorRect(m_cursorPoint).toRect().adjusted(-20, -20, 20, 20);
        effects->addRepaint(KWin::Rect(oldRect));
        effects->addRepaint(KWin::Rect(newRect));
      });
  qDebug()
      << "[UltralightCursorEffect] KwinCursorEffect ctor: mouse callback set";
  QDBusConnection::sessionBus().registerObject(
      QStringLiteral("/UltralightCursor"), this,
      QDBusConnection::ExportAllSlots);
  qDebug() << "[UltralightCursorEffect] KwinCursorEffect ctor: dbus object "
              "registered";
  // m_html->update();
}

KwinCursorEffect::~KwinCursorEffect() {
  if (m_mouseProvider) {
    m_mouseProvider->setCallback(nullptr);
    m_mouseProvider.reset();
  }
  if (m_debugQuadVbo) {
    glDeleteBuffers(1, &m_debugQuadVbo);
  }
  if (m_debugQuadVao) {
    glDeleteVertexArrays(1, &m_debugQuadVao);
  }
  if (m_debugQuadProgram) {
    glDeleteProgram(m_debugQuadProgram);
  }
  m_cursorTexture.reset();
}

bool KwinCursorEffect::supported() { return effects->isOpenGLCompositing(); }

void KwinCursorEffect::enable() {
  UltralightWebCursorM::MainCursorStaff::enable();
  effects->addRepaintFull();
}

void KwinCursorEffect::disable() {
  UltralightWebCursorM::MainCursorStaff::disable();
  m_cursorTexture.reset();
  effects->addRepaintFull();
}

void KwinCursorEffect::reloadHtml() {
  UltralightWebCursorM::MainCursorStaff::reloadHtml();
  effects->addRepaintFull();
}

bool KwinCursorEffect::isBlacklisted() const {
  auto window = effects->activeWindow();
  if (!window)
    return false;
  return isWindowBlacklisted(window->windowClass().toStdString());
}
GLTexture *KwinCursorEffect::ensureCursorTexture() {
  static bool logged = false;
  if (!logged) {
    qDebug() << "[UltralightCursorEffect] ensureCursorTexture() entered";
    logged = true;
  }
  if (!m_html || !m_html->isEnabled() || m_isIdleHidden)
    return nullptr;

  static bool first_focus_done = false;
  if (!first_focus_done && m_html->view()) {
    m_html->view()->Focus();
    first_focus_done = true;
  }

  GLint native_draw_fbo = 0;
  GLint native_read_fbo = 0;
  GLint native_active_program = 0;
  GLint native_vertex_array = 0;
  GLint native_array_buffer = 0;
  GLint native_active_texture = 0;
  GLint native_texture_bindings_2d[3] = {0, 0, 0};
  GLint native_viewport[4] = {0, 0, 0, 0};
  GLint native_scissor_box[4] = {0, 0, 0, 0};

  GLboolean native_blend = glIsEnabled(GL_BLEND);
  GLboolean native_scissor = glIsEnabled(GL_SCISSOR_TEST);
  GLboolean native_depth_test = glIsEnabled(GL_DEPTH_TEST);
  GLboolean native_cull_face = glIsEnabled(GL_CULL_FACE);
  GLboolean native_stencil_test = glIsEnabled(GL_STENCIL_TEST);
  GLboolean native_color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};

  GLint native_depth_func = GL_LESS;
  GLint native_blend_src_rgb = GL_ONE;
  GLint native_blend_dst_rgb = GL_ZERO;
  GLint native_blend_src_alpha = GL_ONE;
  GLint native_blend_dst_alpha = GL_ZERO;
  GLint native_blend_equation_rgb = GL_FUNC_ADD;
  GLint native_blend_equation_alpha = GL_FUNC_ADD;
  GLint native_unpack_alignment = 4;
  GLint native_unpack_row_length = 0;

  glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &native_draw_fbo);
  glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &native_read_fbo);
  glGetIntegerv(GL_CURRENT_PROGRAM, &native_active_program);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &native_vertex_array);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &native_array_buffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &native_active_texture);
  glGetIntegerv(GL_VIEWPORT, native_viewport);
  glGetIntegerv(GL_SCISSOR_BOX, native_scissor_box);
  glGetBooleanv(GL_COLOR_WRITEMASK, native_color_mask);

  for (int unit = 0; unit < 3; ++unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &native_texture_bindings_2d[unit]);
  }
  glActiveTexture(native_active_texture);

  glGetIntegerv(GL_DEPTH_FUNC, &native_depth_func);
  glGetIntegerv(GL_BLEND_SRC_RGB, &native_blend_src_rgb);
  glGetIntegerv(GL_BLEND_DST_RGB, &native_blend_dst_rgb);
  glGetIntegerv(GL_BLEND_SRC_ALPHA, &native_blend_src_alpha);
  glGetIntegerv(GL_BLEND_DST_ALPHA, &native_blend_dst_alpha);
  glGetIntegerv(GL_BLEND_EQUATION_RGB, &native_blend_equation_rgb);
  glGetIntegerv(GL_BLEND_EQUATION_ALPHA, &native_blend_equation_alpha);
  glGetIntegerv(GL_UNPACK_ALIGNMENT, &native_unpack_alignment);
  glGetIntegerv(GL_UNPACK_ROW_LENGTH, &native_unpack_row_length);

  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
  m_html->update();

  glBindFramebuffer(GL_DRAW_FRAMEBUFFER, native_draw_fbo);
  glBindFramebuffer(GL_READ_FRAMEBUFFER, native_read_fbo);

  glUseProgram(native_active_program);
  glBindVertexArray(native_vertex_array);
  glBindBuffer(GL_ARRAY_BUFFER, native_array_buffer);

  for (int unit = 0; unit < 3; ++unit) {
    glActiveTexture(GL_TEXTURE0 + unit);
    glBindTexture(GL_TEXTURE_2D, native_texture_bindings_2d[unit]);
  }
  glActiveTexture(native_active_texture);

  glViewport(native_viewport[0], native_viewport[1], native_viewport[2],
             native_viewport[3]);
  glScissor(native_scissor_box[0], native_scissor_box[1], native_scissor_box[2],
            native_scissor_box[3]);

  if (native_blend)
    glEnable(GL_BLEND);
  else
    glDisable(GL_BLEND);

  if (native_scissor)
    glEnable(GL_SCISSOR_TEST);
  else
    glDisable(GL_SCISSOR_TEST);

  if (native_depth_test)
    glEnable(GL_DEPTH_TEST);
  else
    glDisable(GL_DEPTH_TEST);

  if (native_cull_face)
    glEnable(GL_CULL_FACE);
  else
    glDisable(GL_CULL_FACE);

  if (native_stencil_test)
    glEnable(GL_STENCIL_TEST);
  else
    glDisable(GL_STENCIL_TEST);

  glDepthFunc(native_depth_func);
  glBlendFuncSeparate(native_blend_src_rgb, native_blend_dst_rgb,
                      native_blend_src_alpha, native_blend_dst_alpha);
  glBlendEquationSeparate(native_blend_equation_rgb,
                          native_blend_equation_alpha);
  glColorMask(native_color_mask[0], native_color_mask[1], native_color_mask[2],
              native_color_mask[3]);
  glPixelStorei(GL_UNPACK_ALIGNMENT, native_unpack_alignment);
  glPixelStorei(GL_UNPACK_ROW_LENGTH, native_unpack_row_length);

  int w = m_html->width();
  int h = m_html->height();
  if (w <= 0 || h <= 0)
    return nullptr;

  if (m_html->view() && m_html->view()->needs_paint()) {
    QRect repaintRect =
        getCursorRect(effects->cursorPos()).toRect().adjusted(-20, -20, 20, 20);
    effects->addRepaint(KWin::Rect(repaintRect));
  }

  unsigned int gpuTexId = m_html->textureId();
  if (gpuTexId != 0) {
    m_lastGpuTexId = gpuTexId;
    return nullptr;
  }

  if (m_cursorTexture && !m_html->hasNewFrame())
    return m_cursorTexture.get();

  const uint8_t *pixels = m_html->pixels();
  if (!pixels)
    return nullptr;
  if (m_cursorTexture &&
      (m_cursorTexture->width() != w || m_cursorTexture->height() != h))
    m_cursorTexture.reset();
  if (!m_cursorTexture) {
    QImage wrapperImage(const_cast<uint8_t *>(pixels), w, h, m_html->stride(),
                        QImage::Format_ARGB32_Premultiplied);
    m_cursorTexture = GLTexture::upload(wrapperImage);
    if (!m_cursorTexture)
      return nullptr;
    m_cursorTexture->setWrapMode(GL_CLAMP_TO_EDGE);
    m_html->clearNewFrame();
    return m_cursorTexture.get();
  }

  if (m_html->hasNewFrame()) {
    m_cursorTexture->bind();
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE,
                    pixels);
    m_cursorTexture->unbind();
    m_html->clearNewFrame();
  }
  return m_cursorTexture.get();
}
void KwinCursorEffect::ensureDebugQuadResources() {
  if (m_debugQuadProgram != 0 && m_debugQuadVao != 0 && m_debugQuadVbo != 0)
    return;

  m_debugQuadProgram = createDebugQuadProgram();
  if (!m_debugQuadProgram)
    return;

  constexpr float kQuadVertices[] = {
      0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f,
      0.0f, 1.0f, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f,
  };

  glGenVertexArrays(1, &m_debugQuadVao);
  glGenBuffers(1, &m_debugQuadVbo);
  glBindVertexArray(m_debugQuadVao);
  glBindBuffer(GL_ARRAY_BUFFER, m_debugQuadVbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices,
               GL_STATIC_DRAW);
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<const void *>(0));
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float),
                        reinterpret_cast<const void *>(2 * sizeof(float)));
  glBindBuffer(GL_ARRAY_BUFFER, 0);
  glBindVertexArray(0);
}

void KwinCursorEffect::renderGpuTextureDirect(unsigned int gpuTexId,
                                              const QMatrix4x4 &mvp,
                                              float width, float height,
                                              bool debugSolidColor) {
  ensureDebugQuadResources();
  if (!m_debugQuadProgram || !m_debugQuadVao || !m_debugQuadVbo)
    return;

  GLint prevProgram = 0;
  GLint prevVao = 0;
  GLint prevArrayBuffer = 0;
  GLint prevActiveTexture = 0;
  GLint prevTexture = 0;
  GLboolean blendEnabled = glIsEnabled(GL_BLEND);

  glGetIntegerv(GL_CURRENT_PROGRAM, &prevProgram);
  glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prevVao);
  glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &prevArrayBuffer);
  glGetIntegerv(GL_ACTIVE_TEXTURE, &prevActiveTexture);
  glActiveTexture(GL_TEXTURE0);
  glGetIntegerv(GL_TEXTURE_BINDING_2D, &prevTexture);

  QMatrix4x4 model;
  model.scale(width, height);
  const QMatrix4x4 finalMvp = mvp * model;

  glEnable(GL_BLEND);
  glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
  glUseProgram(m_debugQuadProgram);

  const GLint mvpLoc = glGetUniformLocation(m_debugQuadProgram, "uMvp");
  const GLint texLoc = glGetUniformLocation(m_debugQuadProgram, "uTex");
  const GLint debugSolidLoc =
      glGetUniformLocation(m_debugQuadProgram, "uDebugSolid");
  const GLint solidColorLoc =
      glGetUniformLocation(m_debugQuadProgram, "uSolidColor");
  glUniformMatrix4fv(mvpLoc, 1, GL_FALSE, finalMvp.constData());
  glUniform1i(texLoc, 0);
  glUniform1i(debugSolidLoc, debugSolidColor ? 1 : 0);
  glUniform4f(solidColorLoc, 1.0f, 0.0f, 0.0f, 0.9f);

  glBindVertexArray(m_debugQuadVao);
  glBindTexture(GL_TEXTURE_2D, gpuTexId);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

  glBindTexture(GL_TEXTURE_2D, prevTexture);
  glActiveTexture(prevActiveTexture);
  glBindBuffer(GL_ARRAY_BUFFER, prevArrayBuffer);
  glBindVertexArray(prevVao);
  glUseProgram(prevProgram);

  if (!blendEnabled)
    glDisable(GL_BLEND);
}

void KwinCursorEffect::paintScreen(const RenderTarget &renderTarget,
                                   const RenderViewport &viewport, int mask,
                                   const Region &region,
                                   LogicalOutput *screen) {
  effects->paintScreen(renderTarget, viewport, mask, region, screen);
  if (!m_html || !m_html->isEnabled() || m_isIdleHidden)
    return;

  GLTexture *texture = ensureCursorTexture();
  const unsigned int gpuTexId = m_html->textureId();
  if (gpuTexId == 0 && !texture) {
    effects->addRepaintFull();
    return;
  }
  const int w = m_html->width();
  const int h = m_html->height();
  {
    static int scanCounter = 0;
    scanCounter++;
    if (scanCounter % 60 == 0 && gpuTexId != 0 && glIsTexture(gpuTexId)) {
      GLint prevFbo = 0;
      glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);

      GLuint tempFbo = 0;
      glGenFramebuffers(1, &tempFbo);
      glBindFramebuffer(GL_FRAMEBUFFER, tempFbo);
      glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                             GL_TEXTURE_2D, gpuTexId, 0);

      if (glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
        std::vector<uint8_t> buf(w * h * 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, buf.data());

        int nonZeroAlphaCount = 0;
        uint8_t maxAlpha = 0;
        int minX = w, maxX = 0, minY = h, maxY = 0;
        uint8_t sampleR = 0, sampleG = 0, sampleB = 0, sampleA = 0;

        for (int y = 0; y < h; ++y) {
          for (int x = 0; x < w; ++x) {
            int i = (y * w + x) * 4;
            uint8_t a = buf[i + 3];
            if (a > 0) {
              nonZeroAlphaCount++;
              if (a > maxAlpha) {
                maxAlpha = a;
                sampleR = buf[i];
                sampleG = buf[i + 1];
                sampleB = buf[i + 2];
                sampleA = a;
              }
              if (x < minX)
                minX = x;
              if (x > maxX)
                maxX = x;
              if (y < minY)
                minY = y;
              if (y > maxY)
                maxY = y;
            }
          }
        }

        qDebug() << "[UltralightCursorEffect] [Texture Scan] gpuTexId:"
                 << gpuTexId << "| nonZeroAlphaPixels:" << nonZeroAlphaCount
                 << "/" << (w * h) << "| bbox:"
                 << (nonZeroAlphaCount ? QStringLiteral("(%1,%2)-(%3,%4)")
                                             .arg(minX)
                                             .arg(minY)
                                             .arg(maxX)
                                             .arg(maxY)
                                       : QStringLiteral("none"))
                 << "| sampleRGBA:" << sampleR << sampleG << sampleB << sampleA;
      }

      glBindFramebuffer(GL_FRAMEBUFFER, prevFbo);
      glDeleteFramebuffers(1, &tempFbo);
    }
  }

  QPointF hotspot(m_html->hotspotX(), m_html->hotspotY());
  QPointF pos = effects->cursorPos() - screen->geometry().topLeft() - hotspot;

  auto scale = viewport.scale();
  QMatrix4x4 mvp = viewport.projectionMatrix();
  mvp.translate(pos.x() * scale, pos.y() * scale);

  constexpr bool kForceSolidQuadDebug = false;
  if (gpuTexId != 0 && glIsTexture(gpuTexId)) {
    renderGpuTextureDirect(gpuTexId, mvp, w * scale, h * scale,
                           kForceSolidQuadDebug);
  } else if (texture) {
    ShaderBinder binder(ShaderTrait::MapTexture);
    GLShader *shader = binder.shader();
    if (!shader)
      return;
    shader->setUniform(GLShader::Mat4Uniform::ModelViewProjectionMatrix, mvp);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    texture->render(QSizeF(w, h) * scale);
    glDisable(GL_BLEND);
  }

  static int positionDebugCounter = 0;
  positionDebugCounter++;
  if (positionDebugCounter % 60 == 0) {
    const QRectF drawRect(pos.x(), pos.y(), w, h);
    const unsigned int gpuTexId = m_html->textureId();
    GLint queriedWidth = 0;
    GLint queriedHeight = 0;
    GLint queriedInternalFormat = 0;
    GLint queriedMinFilter = 0;
    GLint queriedMagFilter = 0;
    GLint queriedWrapS = 0;
    GLint queriedWrapT = 0;
    GLint previousTexture = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &previousTexture);
    if (gpuTexId != 0 && glIsTexture(gpuTexId)) {
      glBindTexture(GL_TEXTURE_2D, gpuTexId);
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH,
                               &queriedWidth);
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT,
                               &queriedHeight);
      glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_INTERNAL_FORMAT,
                               &queriedInternalFormat);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                          &queriedMinFilter);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                          &queriedMagFilter);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, &queriedWrapS);
      glGetTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, &queriedWrapT);
      glBindTexture(GL_TEXTURE_2D, previousTexture);
    }
  }

  if (m_html->view() && m_html->view()->needs_paint()) {
    QRect repaintRect =
        getCursorRect(effects->cursorPos()).toRect().adjusted(-20, -20, 20, 20);
    effects->addRepaint(KWin::Rect(repaintRect));
  }
}

bool KwinCursorEffect::isActive() const { return m_html != nullptr; }

bool KwinCursorEffect::checkFullScreen() const {
  if (EffectWindow *activeWin = effects->activeWindow()) {
    return activeWin->isFullScreen();
  }
  return false;
}

void KwinCursorEffect::slotWindowStateChanged(EffectWindow *w) {
  Q_UNUSED(w);
  if (!checkFullScreen()) {
    if (m_isIdleHidden) {
      m_isIdleHidden = false;
      if (m_html)
        m_html->setEnabled(true);
      effects->addRepaintFull();
    }
  } else {
    m_isIdleHidden = true;
    if (m_html)
      m_html->setEnabled(false);
    effects->addRepaintFull();
  }
}

} // namespace KWin

#include "main.moc"
