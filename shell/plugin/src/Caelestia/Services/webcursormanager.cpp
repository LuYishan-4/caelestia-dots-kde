#include "webcursormanager.hpp"

#include "../Config/config.hpp"
#include "../Config/webcursorconfig.hpp"

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

config::WebCursorMain* cursorConfig() {
    return config::GlobalConfig::instance()->webCursor()->cursor();
}

bool isThemeDirectory(const QString& path) {
    const QDir dir(path);
    return dir.exists(QStringLiteral("CursorData.json")) && dir.exists(QStringLiteral("index.html"));
}

QString systemThemesDir() {
    return QStringLiteral("/usr/share/caelestia/webcursor");
}

QString userThemesDir() {
    return QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
           QStringLiteral("/caelestia/webcursor");
}

void ensureLinkedThemesDir() {
    const QString userDir = userThemesDir();
    const QString sysDir = systemThemesDir();

    QDir().mkpath(userDir);

    const QDir sys(sysDir);
    if (!sys.exists())
        return;

    const auto entries = sys.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
    for (const auto& name : entries) {
        const QString sysThemePath = sys.filePath(name);
        if (!isThemeDirectory(sysThemePath))
            continue;

        const QString linkPath = QDir(userDir).filePath(name);
        const QFileInfo linkInfo(linkPath);

        // Re-point stale links (e.g. after a package update replaced the dir)
        if (linkInfo.isSymLink()) {
            if (QFileInfo(linkInfo.symLinkTarget()).canonicalFilePath() == QFileInfo(sysThemePath).canonicalFilePath())
                continue;
            QFile::remove(linkPath);
        } else if (linkInfo.exists()) {
            continue;
        }

        QFile::link(sysThemePath, linkPath);
    }
}

bool linkThemeToSystem(const QString& themeName) {
    if (themeName.isEmpty() || themeName.contains(QLatin1Char('/')) || themeName.contains(QLatin1Char('\\')))
        return false;

    const QString target = QDir(systemThemesDir()).filePath(themeName);
    const QString source = QDir(userThemesDir()).filePath(themeName);

    const QString script = QStringLiteral("ln -sfn '%1' '%2'").arg(source, target);
    const int rc = QProcess::execute(QStringLiteral("pkexec"), { QStringLiteral("sh"), QStringLiteral("-c"), script });
    return rc == 0;
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
            if (!QDir().mkpath(target))
                return false;
        } else if (info.isFile()) {
            QDir().mkpath(QFileInfo(target).dir().path());
            QFile::remove(target);
            if (!QFile::copy(info.filePath(), target))
                return false;
        }
    }
    return true;
}

} // namespace

WebCursorManager::WebCursorManager(QObject* parent)
    : QObject(parent) {
    auto* cursor = cursorConfig();
    connect(cursor, &config::WebCursorMain::selectThemeChanged, this, &WebCursorManager::currentThemeChanged);
    cursor->set_themesDir(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                          QStringLiteral("/caelestia/webcursor"));
    config::GlobalConfig::instance()->save();
    ensureLinkedThemesDir();
    loadThemes();
}

QString WebCursorManager::statusMessage() const {
    return m_statusMessage;
}

QStringList WebCursorManager::themeList() const {
    return m_themeList;
}

QString WebCursorManager::currentTheme() const {
    return cursorConfig()->selectTheme();
}

void WebCursorManager::setStatusMessage(const QString& message) {
    if (m_statusMessage == message)
        return;
    m_statusMessage = message;
    emit statusMessageChanged();
}

QString WebCursorManager::themePath(const QString& name) const {
    if (name.isEmpty() || name.contains(QLatin1Char('/')) || name.contains(QLatin1Char('\\')))
        return {};
    const auto path = QDir(userThemesDir()).filePath(name);
    return isThemeDirectory(path) ? path : QString();
}

void WebCursorManager::loadThemes() {
    QStringList themes;
    const QDir dir(userThemesDir());
    for (const auto& name : dir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name))
        if (isThemeDirectory(dir.filePath(name)))
            themes.append(name);

    if (themes == m_themeList)
        return;
    m_themeList = themes;
    emit themeListChanged();
}

void WebCursorManager::save() {
    config::GlobalConfig::instance()->saveToFileSync();
    reconfigureKWin();
    setStatusMessage(tr("Saved"));
}

void WebCursorManager::reload() {
    loadThemes();
    emit currentThemeChanged();
    setStatusMessage(tr("Loaded"));
}

bool WebCursorManager::pathExists(const QString& path) const {
    return QFileInfo::exists(path);
}

bool WebCursorManager::uploadTheme(const QString& path) {
    const QDir source(QDir::cleanPath(path));

    if (!source.exists() || !QFileInfo(source.absolutePath()).isDir()) {
        setStatusMessage(tr("Invalid source directory"));
        return false;
    }
    const QDir destinationRoot(userThemesDir());
    const QDir destination = destinationRoot.filePath(source.dirName());
    if (destination.exists()) {
        // Never wipe a system theme (symlink) via upload.
        if (destination.isRoot() || QFileInfo(destination.path()).isSymLink()) {
            setStatusMessage(tr("Cannot overwrite a bundled theme"));
            return false;
        }
        if (!QDir(destination.absolutePath()).removeRecursively()) {
            setStatusMessage(tr("Failed to clear existing theme"));
            return false;
        }
    }

    if (!destinationRoot.mkpath(source.dirName())) {
        setStatusMessage(tr("Failed to create destination directory"));
        return false;
    }
    const QFileInfoList fileList = source.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo& fileInfo : fileList) {
        const QString targetPath = destination.filePath(fileInfo.fileName());

        if (fileInfo.isDir()) {
            if (!copyDirectory(fileInfo.absoluteFilePath(), targetPath)) {
                setStatusMessage(tr("Failed to copy directory: %1").arg(fileInfo.fileName()));
                return false;
            }
        } else {
            if (!QFile::copy(fileInfo.absoluteFilePath(), targetPath)) {
                setStatusMessage(tr("Failed to copy file: %1").arg(fileInfo.fileName()));
                return false;
            }
        }
    }
    loadThemes();
    if (!linkThemeToSystem(source.dirName())) {
        setStatusMessage(tr("Theme installed, but linking to system directory failed (run with elevated rights once)"));
        return true;
    }
    setStatusMessage(tr("Theme installed"));
    return true;
}

void WebCursorManager::useTheme(const QString& name) {
    if (themePath(name).isEmpty()) {
        setStatusMessage(tr("Theme not found"));
        return;
    }
    cursorConfig()->set_selectTheme(name);
    save();
    setStatusMessage(tr("Theme applied"));
}

bool WebCursorManager::removeTheme(const QString& name) {
    const auto path = QDir(userThemesDir()).filePath(name);
    // Only themes that live in the user dir (real dirs, not symlinks) can be removed.
    if (!isThemeDirectory(path) || QFileInfo(path).isSymLink() || !QDir(path).removeRecursively()) {
        setStatusMessage(tr("Only user-installed themes can be removed"));
        return false;
    }
    if (cursorConfig()->selectTheme() == name)
        useTheme(QStringLiteral("variant4-ciallo"));
    loadThemes();
    if (QFileInfo(QDir(systemThemesDir()).filePath(name)).isSymLink()) {
        const QString target = QDir(systemThemesDir()).filePath(name);
        QProcess::execute(QStringLiteral("pkexec"),
            { QStringLiteral("sh"), QStringLiteral("-c"), QStringLiteral("rm -f '%1'").arg(target) });
    }
    setStatusMessage(tr("Theme removed"));
    return true;
}

bool WebCursorManager::isUserTheme(const QString& name) const {
    const auto path = QDir(userThemesDir()).filePath(name);
    return isThemeDirectory(path) && !QFileInfo(path).isSymLink();
}

void WebCursorManager::openThemeFolder(const QString& name) {
    const auto path = themePath(name);
    if (!path.isEmpty())
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

QVariantMap WebCursorManager::getThemeDetails(const QString& name) const {
    QVariantMap details{ { QStringLiteral("iconPath"), QString() }, { QStringLiteral("author"), tr("Unknown") },
        { QStringLiteral("describe"), QString() }, { QStringLiteral("minWidth"), 128 },
        { QStringLiteral("minHeight"), 128 } };
    const auto path = themePath(name);
    QFile file(QDir(path).filePath(QStringLiteral("CursorData.json")));
    if (!file.open(QIODevice::ReadOnly))
        return details;
    const auto obj = QJsonDocument::fromJson(file.readAll()).object();
    const auto icon = obj.value(QStringLiteral("IconPath")).toString();
    if (!icon.isEmpty())
        details[QStringLiteral("iconPath")] = QUrl::fromLocalFile(QDir(path).filePath(icon)).toString();
    details[QStringLiteral("author")] =
        obj.value(QStringLiteral("Author")).toString(details.value(QStringLiteral("author")).toString());
    details[QStringLiteral("describe")] = obj.value(QStringLiteral("describe")).toString();
    details[QStringLiteral("minWidth")] = obj.value(QStringLiteral("minWidth")).toInt(128);
    details[QStringLiteral("minHeight")] = obj.value(QStringLiteral("minHeight")).toInt(128);
    return details;
}

void WebCursorManager::addBlacklist(const QString& app) {
    const auto trimmed = app.trimmed();
    auto list = cursorConfig()->blacklist();
    if (!trimmed.isEmpty() && !list.contains(trimmed)) {
        list.append(trimmed);
        cursorConfig()->set_blacklist(list);
        save();
    }
}

void WebCursorManager::removeBlacklist(const QString& app) {
    auto list = cursorConfig()->blacklist();
    if (list.removeAll(app)) {
        cursorConfig()->set_blacklist(list);
        save();
    }
}

void WebCursorManager::enable() {
    cursorConfig()->set_enabled(true);
    QProcess::execute(QStringLiteral("kwriteconfig6"),
        { QStringLiteral("--file"), QStringLiteral("kwinrc"), QStringLiteral("--group"), QStringLiteral("Plugins"),
            QStringLiteral("--key"), QStringLiteral("ultralightwebcursorEnabled"), QStringLiteral("true") });
    QProcess::startDetached(QStringLiteral("busctl"),
        { QStringLiteral("--user"), QStringLiteral("call"), QStringLiteral("org.kde.KWin"), QStringLiteral("/Effects"),
            QStringLiteral("org.kde.kwin.Effects"), QStringLiteral("loadEffect"), QStringLiteral("s"),
            QStringLiteral("ultralightwebcursor") });
    QProcess::startDetached(
        QStringLiteral("busctl"), { QStringLiteral("--user"), QStringLiteral("call"), QStringLiteral("org.kde.KWin"),
                                      QStringLiteral("/UltralightCursor"),
                                      QStringLiteral("org.kde.kwin.KWin.KwinCursorEffect"), QStringLiteral("enable") });
    save();
    setStatusMessage(tr("Enabled"));
}

void WebCursorManager::disable() {
    cursorConfig()->set_enabled(false);
    QProcess::execute(QStringLiteral("kwriteconfig6"),
        { QStringLiteral("--file"), QStringLiteral("kwinrc"), QStringLiteral("--group"), QStringLiteral("Plugins"),
            QStringLiteral("--key"), QStringLiteral("ultralightwebcursorEnabled"), QStringLiteral("false") });
    QProcess::startDetached(QStringLiteral("busctl"),
        { QStringLiteral("--user"), QStringLiteral("call"), QStringLiteral("org.kde.KWin"),
            QStringLiteral("/UltralightCursor"), QStringLiteral("org.kde.kwin.KWin.KwinCursorEffect"),
            QStringLiteral("disable") });
    save();
    setStatusMessage(tr("Disabled"));
}

void WebCursorManager::reconfigureKWin() {
    QProcess::startDetached(QStringLiteral("busctl"),
        { QStringLiteral("--user"), QStringLiteral("call"), QStringLiteral("org.kde.KWin"),
            QStringLiteral("/UltralightCursor"), QStringLiteral("org.kde.kwin.KWin.KwinCursorEffect"),
            QStringLiteral("reloadHtml") });
}

} // namespace caelestia::services
