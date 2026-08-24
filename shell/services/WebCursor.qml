pragma Singleton

import QtQuick
import Quickshell
import Quickshell.Io
import Caelestia.Config

Singleton {
    id: root

    signal themeUploadFinished(bool success, string error)
    signal themeRemoveFinished(bool success, string error)

    function ensureInitialized(): void {
        Quickshell.execDetached(["mkdir", "-p", GlobalConfig.webcursor.sdkThemesDir, GlobalConfig.webcursor.htmlThemesDir]);
    }
    function uploadTheme(srcPath: string, themeName: string): void {
        const src = String(srcPath || "").replace(/^file:\/\//, "");
        if (!src || !themeName) {
            root.themeUploadFinished(false, qsTr("Invalid folder or theme name"));
            return;
        }

        const dst = `${GlobalConfig.webcursor.sdkThemesDir}/${themeName}`;
        const script =
            'src="$1"; dst="$2"; ' +
            '[ -d "$src" ] || { echo "source is not a directory" >&2; exit 1; }; ' +
            'rm -rf -- "$dst" || exit 1; ' +
            'mkdir -p -- "$dst" || exit 1; ' +
            'cp -r -- "$src"/. "$dst"/ || exit 1';

        uploadProc.command = ["sh", "-c", script, "--", src, dst];
        uploadProc._themeName = themeName;
        uploadProc.running = true;
    }

    Process {
        id: uploadProc
        property string _themeName: ""
        stderr: StdioCollector { id: uploadStderr }
        onExited: code => {
            if (code === 0) {
                root.setTheme(uploadProc._themeName);
                root.themeUploadFinished(true, "");
            } else {
                root.themeUploadFinished(false, (uploadStderr.text || "").trim() || qsTr("Upload failed"));
            }
        }
    }
    function setTheme(themeName: string): void {
        GlobalConfig.webcursor.currentTheme = themeName;
        GlobalConfig.webcursor.html = `${GlobalConfig.webcursor.htmlThemesDir}/${themeName}/index.html`;
    }
    function removeTheme(themeName: string): void {
        if (!themeName) {
            root.themeRemoveFinished(false, qsTr("Empty theme name"));
            return;
        }
        const dst = `${GlobalConfig.webcursor.sdkThemesDir}/${themeName}`;
        const script = '[ -e "$1" ] || exit 1; rm -rf -- "$1"';
        removeProc.command = ["sh", "-c", script, "--", dst];
        removeProc._themeName = themeName;
        removeProc.running = true;
    }

    Process {
        id: removeProc
        property string _themeName: ""
        onExited: code => {
            if (code === 0) {
                if (GlobalConfig.webcursor.selectTheme === removeProc._themeName)
                    GlobalConfig.webcursor.selectTheme = "";
                root.themeRemoveFinished(true, "");
            } else {
                root.themeRemoveFinished(false, qsTr("Theme not found"));
            }
        }
    }

    Component.onCompleted: ensureInitialized()
}
