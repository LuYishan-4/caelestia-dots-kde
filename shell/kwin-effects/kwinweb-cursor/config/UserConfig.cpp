#include "UserConfig.hpp"

#include "../lib/Quick/PluginPath/PluginPath.hpp"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

#include <algorithm>

namespace fs = std::filesystem;

namespace UltralightWebCursorM {

fs::path g_sdkInitialPath;
fs::path g_htmlInitialPath;

UserConfig* UserConfig::instance() {
    static UserConfig inst;
    return &inst;
}

UserConfig::UserConfig() {
    configPath_ = QString(QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation) +
                          QStringLiteral("/caelestia/shell.json"))
                      .toStdString();
}

void UserConfig::ensureInitialized() {
    if (!schema_.empty())
        return;
    const auto bundled = PluginPath::dataDir();
    g_sdkInitialPath = bundled;
    g_htmlInitialPath = bundled;
    schema_ = {
        { "width", "128",
            [this](const std::string& value) {
                values.width = std::max(1, std::stoi(value));
            } },
        { "height", "128",
            [this](const std::string& value) {
                values.height = std::max(1, std::stoi(value));
            } },
        { "enabled", "false",
            [this](const std::string& value) {
                values.enabled = value == "true";
            } },
        { "blacklist", "",
            [this](const std::string&) {
            } },
        { "selectTheme", "variant4-ciallo",
            [this](const std::string&) {
            } },
        { "themesDir", "",
            [this](const std::string&) {
            } },
    };
}

bool UserConfig::load() {
    ensureInitialized();
    data_.clear();
    values.blacklist.clear();
    values.configver = "caelestia";
    for (const auto& item : schema_)
        data_[item.key] = item.defaultValue;

    QFile file(QString::fromStdString(configPath_));
    if (file.open(QIODevice::ReadOnly)) {
        const auto root = QJsonDocument::fromJson(file.readAll()).object();
        const auto cursor =
            root.value(QStringLiteral("webCursor")).toObject().value(QStringLiteral("cursor")).toObject();
        if (!cursor.isEmpty()) {
            data_["enabled"] = cursor.value(QStringLiteral("enabled")).toBool(false) ? "true" : "false";
            data_["width"] = std::to_string(cursor.value(QStringLiteral("width")).toInt(128));
            data_["height"] = std::to_string(cursor.value(QStringLiteral("height")).toInt(128));
            data_["selectTheme"] =
                cursor.value(QStringLiteral("selectTheme")).toString(QStringLiteral("variant4-ciallo")).toStdString();
            data_["themesDir"] = cursor.value(QStringLiteral("themesDir")).toString().toStdString();
            for (const auto& item : cursor.value(QStringLiteral("blacklist")).toArray())
                if (!item.toString().isEmpty())
                    values.blacklist.push_back(item.toString().toStdString());
        }
    }

    for (const auto& item : schema_) {
        try {
            item.updater(data_[item.key]);
        } catch (...) {
            data_[item.key] = item.defaultValue;
            item.updater(item.defaultValue);
        }
    }

    const auto requestedTheme = QString::fromStdString(data_["selectTheme"]);
    const auto customRoot = data_["themesDir"].empty()
                                ? QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) +
                                      QStringLiteral("/caelestia/webcursor")
                                : QString::fromStdString(data_["themesDir"]);
    const auto customTheme = QDir(customRoot).filePath(requestedTheme);
    const auto bundledTheme =
        QString::fromStdString(PluginPath::dataDir().string()) + QLatin1Char('/') + requestedTheme;
    const auto themeDir =
        QFileInfo::exists(QDir(customTheme).filePath(QStringLiteral("index.html"))) ? customTheme : bundledTheme;
    values.html = QDir(themeDir).filePath(QStringLiteral("index.html")).toStdString();
    // Resources remain installed alongside the bundled effect; custom themes only
    // replace the HTML directory, never the Ultralight runtime resources.
    values.sdk = PluginPath::dataDir().string();
    g_htmlInitialPath = fs::path(themeDir.toStdString());
    return QFileInfo::exists(QString::fromStdString(values.html));
}

// The KWin plugin is deliberately read-only. Nexus owns shell.json persistence
// through WebCursorConfig, so no secondary UserConfig file can be created.
bool UserConfig::save() {
    return false;
}

void UserConfig::setKeyValue(const std::string& key, const std::string& value) {
    data_[key] = value;
}

std::string UserConfig::readKeyValue(const std::string& key) const {
    const auto it = data_.find(key);
    return it == data_.end() ? std::string{} : it->second;
}

std::vector<std::string> UserConfig::getBlacklist() const {
    return values.blacklist;
}

void UserConfig::appendBlacklist(const std::string&) {}

void UserConfig::removeBlacklist(const std::string&) {}

bool UserConfig::uploadTheme(const std::string&, const std::string&) {
    return false;
}

bool UserConfig::removeTheme(const std::string&) {
    return false;
}

void UserConfig::setTheme(const std::string&) {}

std::string UserConfig::currentTheme() const {
    return readKeyValue("selectTheme");
}

} // namespace UltralightWebCursorM
