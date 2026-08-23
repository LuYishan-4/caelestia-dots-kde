#include "PluginPath.hpp"
#include <QCoreApplication>
#include <QDir>
#include <QStandardPaths>

namespace UltralightWebCursorM {

std::filesystem::path PluginPath::dataDir() {
  QString path;
  path =
      QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                             QStringLiteral("kwin/effects/ultralightwebcursor"),
                             QStandardPaths::LocateDirectory);
  if (path.isEmpty()) {
    path = QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                                  QStringLiteral("ultralightwebcursor"),
                                  QStandardPaths::LocateDirectory);
    if (path.isEmpty()) {
      path = QStandardPaths::writableLocation(
                 QStandardPaths::GenericDataLocation) +
             QStringLiteral("/ultralightwebcursor");
    }
  }

  return std::filesystem::path(path.toStdString());
}

} // namespace UltralightWebCursorM
