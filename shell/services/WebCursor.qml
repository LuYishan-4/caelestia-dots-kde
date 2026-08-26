pragma Singleton
import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia.Config

Singleton {
    id: root
    signal themeUploadFinished(bool success, string error)
    signal themeRemoveFinished(bool success, string error)

    property var themeList: []
    property string statusMessage: ""
    readonly property string currentTheme: GlobalConfig.webCursor.cursor.selectTheme

    readonly property string systemThemesDir: "/usr/share/caelestia/webcursor"

    readonly property string userThemesDir: GlobalConfig.webCursor.cursor.themesDir

    function ensureInitialized(): void {
        linkProc.running = true;
    }


    Process {
        id: linkProc
        command: ["sh", "-c",
            'user="$1"; sys="$2"; ' +
            'mkdir -p "$user" || exit 1; ' +
            'for f in "$user"/*; do ' +
            '  [ -e "$f" ] && continue; ' +
            '  [ -L "$f" ] && rm -f -- "$f"; ' +
            'done; ' +

            '[ -d "$sys" ] || exit 0; ' +
            'for d in "$sys"/*/; do ' +
            '  [ -d "$d" ] || continue; ' +
            '  n=$(basename "$d"); ' +
            '  [ -f "$d/CursorData.json" ] && [ -f "$d/index.html" ] || continue; ' +
            '  target="$user/$n"; ' +
            '  [ -e "$target" ] || [ -L "$target" ] || ln -s -- "$d" "$target"; ' +
            'done',
            "--", root.userThemesDir, root.systemThemesDir]
        onExited: code => {
            root.refreshThemes();
        }
    }

    function refreshThemes(): void {
        listProc.running = true;
    }

    Process {
        id: listProc
        command: ["sh", "-c",
            'dir="$1"; [ -d "$dir" ] || exit 0; ' +
            'for d in "$dir"/*/; do ' +
            '  [ -d "$d" ] || continue; ' +
            '  n=$(basename "$d"); ' +
            '  if [ -f "$d/CursorData.json" ] && [ -f "$d/index.html" ]; then echo "$n"; fi; ' +
            'done', "--", root.userThemesDir]
        stdout: StdioCollector {
            id: listStdout
            onStreamFinished: {
                root.themeList = (listStdout.text || "")
                    .split("\n").map(s => s.trim()).filter(s => s.length > 0);
            }
        }
    }

    function themePath(name: string): string {
        if (!name || name.indexOf("/") !== -1 || name.indexOf("\\") !== -1)
            return "";
        return `${root.userThemesDir}/${name}`;
    }

    property var _symlinkCache: ({})

    function isUserTheme(name: string): bool {

        if (Object.prototype.hasOwnProperty.call(root._symlinkCache, name))
            return !root._symlinkCache[name];

        checkSymlinkProc.command = ["sh", "-c", '[ -L "$1" ] && echo yes || echo no', "--", root.themePath(name)];
        checkSymlinkProc._name = name;
        checkSymlinkProc.running = true;
        return false;
    }

    Process {
        id: checkSymlinkProc
        property string _name: ""
        stdout: StdioCollector {
            id: checkSymlinkStdout
            onStreamFinished: {
                const isSymlink = (checkSymlinkStdout.text || "").trim() === "yes";
                root._symlinkCache[checkSymlinkProc._name] = isSymlink;

                root.themeListChanged();
            }
        }
    }

    function getThemeDetails(name: string): var {
        const details = { iconPath: "", author: qsTr("Unknown"), describe: "", minWidth: 128, minHeight: 128 };
        const path = root.themePath(name);
        if (!path) return details;
        try {
            const xhr = new XMLHttpRequest();
            xhr.open("GET", "file://" + path + "/CursorData.json", false);
            xhr.send();
            if (xhr.status !== 200 && xhr.status !== 0) return details;
            const obj = JSON.parse(xhr.responseText);
            if (obj.IconPath) details.iconPath = "file://" + path + "/" + obj.IconPath;
            details.author = obj.Author || details.author;
            details.describe = obj.describe || "";
            details.minWidth = obj.minWidth || 128;
            details.minHeight = obj.minHeight || 128;
        } catch (e) {
            console.warn("Failed to read theme data:", name, e);
        }
        return details;
    }

    function openThemeFolder(name: string): void {
        const path = root.themePath(name);
        if (path) Quickshell.execDetached(["xdg-open", path]);
    }

    function uploadTheme(srcPath: string, themeName: string): void {
        const src = String(srcPath || "").replace(/^file:\/\//, "");
        if (!src) {
            root.statusMessage = qsTr("Invalid folder");
            root.themeUploadFinished(false, root.statusMessage);
            return;
        }
        let name = String(themeName || "").trim();
        if (!name) {
            const parts = src.split("/").filter(p => p.length > 0);
            name = parts.length > 0 ? parts[parts.length - 1] : "";
        }
        name = name.replace(/[\/\\]/g, ""); // prevent path traversal
        if (!name) {
            root.statusMessage = qsTr("Invalid folder or theme name");
            root.themeUploadFinished(false, root.statusMessage);
            return;
        }

        const dst = `${root.userThemesDir}/${name}`;
        const script =
            'src="$1"; dst="$2"; ' +
            '[ -d "$src" ] || { echo "source is not a directory" >&2; exit 1; }; ' +
            '[ -f "$src/CursorData.json" ] && [ -f "$src/index.html" ] || { echo "selected folder is not a valid cursor theme" >&2; exit 1; }; ' +
            '[ -L "$dst" ] && rm -f -- "$dst"; ' +
            'rm -rf -- "$dst" || exit 1; mkdir -p -- "$dst" || exit 1; ' +
            'cp -r -- "$src"/. "$dst"/ || exit 1';
        uploadProc.command = ["sh", "-c", script, "--", src, dst];
        uploadProc._themeName = name;
        uploadProc.running = true;
    }

    Process {
        id: uploadProc
        property string _themeName: ""
        stderr: StdioCollector { id: uploadStderr }
        onExited: code => {
            if (code === 0) {
                delete root._symlinkCache[uploadProc._themeName];
                root.refreshThemes();
                root.setTheme(uploadProc._themeName);
                root.statusMessage = qsTr("Theme uploaded successfully");
                root.themeUploadFinished(true, "");
            } else {
                root.statusMessage = (uploadStderr.text || "").trim() || qsTr("Theme upload failed");
                root.themeUploadFinished(false, root.statusMessage);
            }
        }
    }

    function setTheme(themeName: string): void {
        GlobalConfig.webCursor.cursor.selectTheme = themeName;
    }

    function useTheme(themeName: string): void {
        root.setTheme(themeName);
        root.statusMessage = qsTr("Theme applied successfully");
    }

    function removeTheme(themeName: string): void {
        if (!themeName) {
            root.themeRemoveFinished(false, qsTr("Empty theme name"));
            return;
        }
        const dst = `${root.userThemesDir}/${themeName}`;
        const script =
            'dst="$1"; ' +
            '[ -e "$dst" ] || [ -L "$dst" ] || exit 2; ' +
            '[ -L "$dst" ] && { echo "cannot remove a built-in theme" >&2; exit 3; }; ' +
            'rm -rf -- "$dst"';
        removeProc.command = ["sh", "-c", script, "--", dst];
        removeProc._themeName = themeName;
        removeProc.running = true;
    }

    Process {
        id: removeProc
        property string _themeName: ""
        stderr: StdioCollector { id: removeStderr }
        onExited: code => {
            if (code === 0) {
                if (GlobalConfig.webCursor.cursor.selectTheme === removeProc._themeName)
                    GlobalConfig.webCursor.cursor.selectTheme = "";
                delete root._symlinkCache[removeProc._themeName];
                root.refreshThemes();
                root.statusMessage = qsTr("Theme removed successfully");
                root.themeRemoveFinished(true, "");
            } else if (code === 3) {
                root.statusMessage = qsTr("Built-in themes cannot be removed");
                root.themeRemoveFinished(false, root.statusMessage);
            } else {
                root.statusMessage = qsTr("Theme not found");
                root.themeRemoveFinished(false, root.statusMessage);
            }
        }
    }

    function reload(): void {
        root.refreshThemes();
        root.statusMessage = qsTr("Reloaded successfully");
    }

    function enable(): void {
        GlobalConfig.webCursor.cursor.enabled = true;
        kwinToggleProc.command = ["sh", "-c",
            'busctl --user call org.kde.KWin /Effects org.kde.kwin.Effects loadEffect s ultralightwebcursor 2>/dev/null; ' +
            'busctl --user call org.kde.KWin /UltralightCursor org.kde.kwin.KWin.KwinCursorEffect enable 2>/dev/null; ' +
            'true'];
        kwinToggleProc.running = true;
        root.save();
        root.statusMessage = qsTr("Enabled");
    }

    function disable(): void {
        GlobalConfig.webCursor.cursor.enabled = false;
        kwinToggleProc.command = ["sh", "-c",
            'busctl --user call org.kde.KWin /UltralightCursor org.kde.kwin.KWin.KwinCursorEffect disable 2>/dev/null; ' +
            'true'];
        kwinToggleProc.running = true;
        root.save();
        root.statusMessage = qsTr("Disabled");
    }

    Process { id: kwinToggleProc }

    // ---- Blacklist ----
    function addBlacklist(app: string): void {
        const trimmed = String(app || "").trim();
        if (!trimmed) return;
        const list = GlobalConfig.webCursor.cursor.blacklist || [];
        if (list.indexOf(trimmed) === -1) {
            GlobalConfig.webCursor.cursor.blacklist = [...list, trimmed];
            root.save();
        }
    }

    function removeBlacklist(app: string): void {
        const list = GlobalConfig.webCursor.cursor.blacklist || [];
        const next = list.filter(a => a !== app);
        if (next.length !== list.length) {
            GlobalConfig.webCursor.cursor.blacklist = next;
            root.save();
        }
    }

    // ---- Apply size / config changes to KWin ----
    // Only reloadHtml: /KWin reconfigure makes KWin re-read kwinrc and
    // unload/reload effect plugins, which destroys and re-creates the
    // Ultralight renderer and crashes KWin.
    function save(): void {
        reconfigureProc.running = true;
        root.statusMessage = qsTr("Saved");
    }

    Process {
        id: reconfigureProc
        command: ["sh", "-c",
            'busctl --user call org.kde.KWin /UltralightCursor org.kde.kwin.KWin.KwinCursorEffect reloadHtml 2>/dev/null; ' +
            'true']
    }

    Component.onCompleted: ensureInitialized()
}
