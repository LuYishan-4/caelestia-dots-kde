#pragma once

#include <qfile.h>
#include <qhash.h>
#include <qnetworkaccessmanager.h>
#include <qnetworkreply.h>
#include <qobject.h>
#include <qpointer.h>
#include <qqmlengine.h>
#include <qtimer.h>

namespace caelestia {

class Requests : public QObject {
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Requests(QObject* parent = nullptr);

    /// Basic HTTP GET. Returns a request ID usable with cancel().
    /// @param url        Target URL.
    /// @param callback   function(responseText, statusCode)
    /// @param onError    function(errorString, statusCode)   [optional]
    /// @param headers    JS object of header name → value    [optional]
    /// @param timeoutMs  Abort after this many milliseconds; 0 = no timeout.
    Q_INVOKABLE int get(const QUrl& url, QJSValue callback, QJSValue onError = {},
                        QJSValue headers = {}, int timeoutMs = 0);

    /// HTTP POST with a raw body.
    /// @param body         Raw bytes to send.
    /// @param contentType  Value for the Content-Type header (e.g. "application/json").
    /// Callback signatures match get().
    Q_INVOKABLE int post(const QUrl& url, const QByteArray& body, const QString& contentType,
                         QJSValue callback, QJSValue onError = {}, QJSValue headers = {},
                         int timeoutMs = 0);

    /// Download a URL straight to a local file, with progress callbacks.
    /// @param destPath    Absolute path of the output file (overwritten if it exists).
    /// @param onComplete  function(filePath, statusCode)
    /// @param onProgress  function(bytesReceived, bytesTotal)   [optional]
    /// @param onError     function(errorString, statusCode)     [optional]
    Q_INVOKABLE int download(const QUrl& url, const QString& destPath, QJSValue onComplete,
                             QJSValue onProgress = {}, QJSValue onError = {},
                             QJSValue headers = {}, int timeoutMs = 0);

    /// Abort an in-flight request. No-ops for already-finished / unknown IDs.
    Q_INVOKABLE void cancel(int requestId);

    /// Parse a JSON string → JS object/array, or null on failure.
    Q_INVOKABLE QJSValue parseJson(const QString& text) const;

    /// Serialise a JS value to its compact JSON representation.
    Q_INVOKABLE QString toJson(const QJSValue& value) const;

    /// Replace the cookie jar with a fresh empty one.
    Q_INVOKABLE void resetCookies();

signals:
    /// Emitted for every active download so QML can drive a global progress bar.
    void downloadProgress(int requestId, qint64 bytesReceived, qint64 bytesTotal);

private:
    struct ActiveRequest {
        QPointer<QNetworkReply> reply;
        QTimer* timeoutTimer = nullptr;
        bool isDownload = false;
        QFile* destFile = nullptr;
        QString destPath;
        QJSValue onProgress;
        QJSValue onComplete;
        QJSValue onError;
    };

    /// Shared reply-setup: connect finished/error, install timeout, stash in m_activeRequests.
    int registerReply(QNetworkReply* reply, QJSValue callback, QJSValue onError, int timeoutMs);
    void cleanupRequest(int requestId);
    int nextRequestId();

    void abortAndFail(int requestId, const QString& errorMessage, bool removeDestFile);

    QNetworkAccessManager* m_manager;
    QHash<int, ActiveRequest> m_activeRequests;
    int m_nextRequestId = 1;
};

} // namespace caelestia
