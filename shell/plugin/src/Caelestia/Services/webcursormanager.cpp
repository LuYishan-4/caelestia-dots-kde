#include "webcursormanager.hpp"

#include "../Config/config.hpp"
#include "../Config/webcursorconfig.hpp"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

namespace caelestia::services {
namespace {
config::WebCursorMain* cursorConfig() { return config::GlobalConfig::instance()->webCursor()->cursor(); }

bool isThemeDirectory(const QString& path) {
    const QDir dir(path);
    return dir.exists(QStringLiteral("CursorData.json")) && dir.exists(QStringLiteral("index.html"));
}

bool copyDirectory(const QString& source, const QString& destination) {
    QDir().mkpath(destination);
    QDirIterator it(source, QDir::NoDotAndDotDot | QDir::AllEntries, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        const QFileInfo info = it.fileInfo();
        const auto relative = QDir(source).relativeFilePath(info.filePath());
        const auto target = QDir(destination).filePath(relative);
        if (info.isDir()) {
            if (!QDir().mkpath(target)) return false;
        } else if (info.isFile()) {
            QDir().mkpath(QFileInfo(target).dir().path());
            QFile::remove(target);
            if (!QFile::copy(info.filePath(), target)) return false;
        }
    }
    return true;
}
} // namespace

WebCursorManager::WebCursorManager(QObject* parent) : QObject(parent) {
    auto* cursor = cursorConfig();
    connect(cursor, &config::WebCursorMain::selectThemeChanged, this, &WebCursorManager::currentThemeChanged);
    connect(cursor, &config::WebCursorMain::themesDirChanged, this, &WebCursorManager::loadThemes);
    loadThemes();
}

QString WebCursorManager::statusMessage() const { return m_statusMessage; }
QStringList WebCursorManager::themeList() const { return m_themeList; }
QString WebCursorManager::currentTheme() const { return cursorConfig()->selectTheme(); }

void WebCursorManager::setStatusMessage(const QString& message) {
    if (m_statusMessage == message) return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

QString WebCursorManager::bundledThemesDir() const {
    return QStandardPaths::locate(QStandardPaths::GenericDataLocation,
        QStringLiteral("kwin/effects/ultralightwebcursor"), QStandardPaths::LocateDirectory);
}

QString WebCursorManager::themePath(const QString& name) const {
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\'))) return {};
    const auto custom = QDir(cursorConfig()->themesDir()).filePath(name);
    if (isThemeDirectory(custom)) return custom;
    const auto bundled = QDir(bundledThemesDir()).filePath(name);
    return isThemeDirectory(bundled) ? bundled : QString();
}

void WebCursorManager::loadThemes() {
    QStringList themes;
    const auto appendThemes = [&themes](const QString& root) {
        const QDir dir(root);
        for (const auto& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
            if (isThemeDirectory(dir.filePath(name)) && !themes.contains(name)) themes.append(name);
    };
    appendThemes(bundledThemesDir());
    appendThemes(cursorConfig()->themesDir());
    if (themes == m_themeList) return;
    m_themeList = themes;
    emit themeListChanged();
}

void WebCursorManager::save() {
    config::GlobalConfig::instance()->save();
    reconfigureKWin();
    setStatusMessage(tr("Saved"));
}

void WebCursorManager::reload() {
    config::GlobalConfig::instance()->reload();
    loadThemes();
    emit currentThemeChanged();
    setStatusMessage(tr("Loaded"));
}

bool WebCursorManager::pathExists(const QString& path) const { return QFileInfo::exists(path); }

bool WebCursorManager::uploadTheme(const QString& path) {
    const QDir source(QDir::cleanPath(path));
    if (!isThemeDirectory(source.absolutePath())) {
        setStatusMessage(tr("A theme must contain CursorData.json and index.html"));
        return false;
    }
    const auto destination = QDir(cursorConfig()->themesDir()).filePath(source.dirName());
    if (QFileInfo(destination).absoluteFilePath() == source.absolutePath()) {
        setStatusMessage(tr("Theme is already installed"));
        return false;
    }
    QDir(destination).removeRecursively();
    if (!copyDirectory(source.absolutePath(), destination)) {
        setStatusMessage(tr("Could not install theme"));
        return false;
    }
    loadThemes();
    setStatusMessage(tr("Theme installed"));
    return true;
}

void WebCursorManager::useTheme(const QString& name) {
    if (themePath(name).isEmpty()) { setStatusMessage(tr("Theme not found")); return; }
    cursorConfig()->set_selectTheme(name);
    save();
    setStatusMessage(tr("Theme applied"));
}

bool WebCursorManager::removeTheme(const QString& name) {
    const auto path = QDir(cursorConfig()->themesDir()).filePath(name);
    if (!isThemeDirectory(path) || !QDir(path).removeRecursively()) {
        setStatusMessage(tr("Only user-installed themes can be removed"));
        return false;
    }
    if (cursorConfig()->selectTheme() == name) useTheme(QStringLiteral("variant4-ciallo"));
    loadThemes();
    setStatusMessage(tr("Theme removed"));
    return true;
}

bool WebCursorManager::isUserTheme(const QString& name) const {
    return isThemeDirectory(QDir(cursorConfig()->themesDir()).filePath(name));
}

void WebCursorManager::openThemeFolder(const QString& name) {
    const auto path = themePath(name);
    if (!path.isEmpty()) QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QVariantMap WebCursorManager::getThemeDetails(const QString& name) const {
    QVariantMap details{{QStringLiteral("iconPath"), QString()}, {QStringLiteral("author"), tr("Unknown")},
        {QStringLiteral("describe"), QString()}, {QStringLiteral("minWidth"), 128}, {QStringLiteral("minHeight"), 128}};
    const auto path = themePath(name);
    QFile file(QDir(path).filePath(QStringLiteral("CursorData.json")));
    if (!file.open(QIODevice::ReadOnly)) return details;
    const auto obj = QJsonDocument::fromJson(file.readAll()).object();
    const auto icon = obj.value(QStringLiteral("IconPath")).toString();
    if (!icon.isEmpty()) details[QStringLiteral("iconPath")] = QUrl::fromLocalFile(QDir(path).filePath(icon)).toString();
    details[QStringLiteral("author")] = obj.value(QStringLiteral("Author")).toString(details.value(QStringLiteral("author")).toString());
    details[QStringLiteral("describe")] = obj.value(QStringLiteral("describe")).toString();
    details[QStringLiteral("minWidth")] = obj.value(QStringLiteral("minWidth")).toInt(128);
    details[QStringLiteral("minHeight")] = obj.value(QStringLiteral("minHeight")).toInt(128);
    return details;
}

void WebCursorManager::addBlacklist(const QString& app) {
    const auto trimmed = app.trimmed();
    auto list = cursorConfig()->blacklist();
    if (!trimmed.isEmpty() && !list.contains(trimmed)) { list.append(trimmed); cursorConfig()->set_blacklist(list); save(); }
}

void WebCursorManager::removeBlacklist(const QString& app) {
    auto list = cursorConfig()->blacklist();
    if (list.removeAll(app)) { cursorConfig()->set_blacklist(list); save(); }
}

void WebCursorManager::enable() {
    cursorConfig()->set_enabled(true);
    QProcess::execute(QStringLiteral("kwriteconfig6"), {QStringLiteral("--file"), QStringLiteral("kwinrc"), QStringLiteral("--group"), QStringLiteral("Plugins"), QStringLiteral("--key"), QStringLiteral("ultralightwebcursorEnabled"), QStringLiteral("true")});
    save(); setStatusMessage(tr("Enabled"));
}

void WebCursorManager::disable() {
    cursorConfig()->set_enabled(false);
    QProcess::execute(QStringLiteral("kwriteconfig6"), {QStringLiteral("--file"), QStringLiteral("kwinrc"), QStringLiteral("--group"), QStringLiteral("Plugins"), QStringLiteral("--key"), QStringLiteral("ultralightwebcursorEnabled"), QStringLiteral("false")});
    save(); setStatusMessage(tr("Disabled"));
}

void WebCursorManager::reconfigureKWin() {
    QDBusInterface effect(QStringLiteral("org.kde.KWin"), QStringLiteral("/UltralightCursor"), QStringLiteral("org.kde.kwin.KWin.UltralightCursorEffect"), QDBusConnection::sessionBus());
    effect.call(QStringLiteral("reloadHtml"));
    QDBusInterface kwin(QStringLiteral("org.kde.KWin"), QStringLiteral("/KWin"), QStringLiteral("org.kde.KWin"), QDBusConnection::sessionBus());
    kwin.call(QStringLiteral("reconfigure"));
}

} // namespace caelestia::services
