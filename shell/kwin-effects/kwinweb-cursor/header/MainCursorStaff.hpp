#pragma once

#include "../config/CursorJSON.hpp"
#include "../config/UserConfig.hpp"
#include "../lib/BlackList/BlacklistManager.hpp"
#include "../lib/CrashReport/CrashReport.hpp"
#include "MouseProvider.hpp"
#include "UltralightHtmlEffect.hpp"
#include <QDebug>
#include <QObject>
#include <QPointF>
#include <QRectF>
#include <filesystem>
#include <memory>
#include <string>

namespace UltralightWebCursorM {

class MainCursorStaff {

public:
  MainCursorStaff() = default;
  virtual ~MainCursorStaff() = default;

  bool isWindowBlacklisted(const std::string &windowClass) const {
    return m_blacklist.contains(windowClass);
  }

  QRectF getCursorRect(const QPointF &basePos) const {
    if (!m_html)
      return QRectF();
    qreal w = m_html->width();
    qreal h = m_html->height();
    return QRectF(basePos.x() - m_html->hotspotX(),
                  basePos.y() - m_html->hotspotY(), w, h);
  }

public:
  virtual void enable() {
    if (m_html)
      m_html->setEnabled(true);
  }

  virtual void disable() {
    if (m_html)
      m_html->setEnabled(false);
  }

  virtual void reloadHtml() {
    UltralightWebCursorM::UserConfig::instance()->load();
    UltralightWebCursorM::CursorJSON::instance()->load(UserConfigimp.html);
    if (m_html)
      m_html->reload(UserConfigimp, CursorJSONImp);
  }

protected:
  template <typename MouseProviderType> bool initializeCore() {
    static bool isPermanentlyDisabled = false;
    if (isPermanentlyDisabled)
      return false;
    UltralightWebCursorM::CrashHandler::registerHandler();
    try {
      qDebug() << "[UltralightCursorEffect] initializeCore: load user config";
      UltralightWebCursorM::UserConfig::instance()->load();
      qDebug() << "[UltralightCursorEffect] initializeCore: load cursor config";
      UltralightWebCursorM::CursorJSON::instance()->load(UserConfigimp.html);

      qDebug() << "[UltralightCursorEffect] initializeCore: create html effect";
      m_html = std::make_unique<UltralightWebCursorM::UltralightHtmlEffect>();
      qDebug()
          << "[UltralightCursorEffect] initializeCore: create mouse provider";
      m_mouseProvider = std::make_unique<MouseProviderType>();
      qDebug()
          << "[UltralightCursorEffect] initializeCore: init mouse provider";
      m_mouseProvider->initialize();
      qDebug() << "[UltralightCursorEffect] initializeCore: mouse provider "
                  "initialized";

      if (!m_html || !m_mouseProvider) {
        qDebug() << "[UltralightCursorEffect] deeeeeeeeee";
        isPermanentlyDisabled = true;
        return false;
      }

      qDebug() << "[UltralightCursorEffect] initializeCore: init html effect";
      if (!m_html->initialize(UserConfigimp, CursorJSONImp)) {
        qDebug() << "[UltralightCursorEffect] dccce";
        m_html.reset();
        m_mouseProvider.reset();
        isPermanentlyDisabled = true;
        return false;
      }
      qDebug()
          << "[UltralightCursorEffect] initializeCore: html effect initialized";
      m_blacklist.setBlacklist(
          UltralightWebCursorM::UserConfig::instance()->getBlacklist());
      qDebug() << "[UltralightCursorEffect] initializeCore: done";
      return true;
    } catch (const std::exception &e) {
      isPermanentlyDisabled = true;
      return false;
    } catch (...) {
      isPermanentlyDisabled = true;
      return false;
    }
  }

  virtual bool checkFullScreen() const { return false; }
  bool m_isIdleHidden = false;
  std::unique_ptr<UltralightWebCursorM::UltralightHtmlEffect> m_html;
  std::unique_ptr<UltralightWebCursorM::IMouseProvider> m_mouseProvider;
  UltralightWebCursorM::BlacklistManager m_blacklist;

  QPointF m_cursorPoint;
};

} // namespace UltralightWebCursorM
