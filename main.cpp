#include <QApplication>
#include <QSharedMemory>
#include <QLocalSocket>
#include <QDataStream>
#include <QFileInfo>
#include <QThread>
#include "markdow_editor.h"

namespace {

QStringList fileArguments(int argc, char *argv[])
{
    QStringList files;
    for (int i = 1; i < argc; ++i) {
        const QString argument = QString::fromLocal8Bit(argv[i]);
        if (argument.startsWith(QLatin1Char('-'))) {
            continue;
        }

        const QFileInfo info(argument);
        if (info.exists() && info.isFile()) {
            QString path = info.canonicalFilePath();
            if (path.isEmpty()) {
                path = info.absoluteFilePath();
            }
            files.append(path);
        }
    }
    return files;
}

bool notifyRunningInstance(const QStringList &files)
{
    for (int attempt = 0; attempt < 25; ++attempt) {
        QLocalSocket socket;
        socket.connectToServer(QLatin1String(MarkdownEditor::singleInstanceServerName()));
        if (!socket.waitForConnected(150)) {
            QThread::msleep(100);
            continue;
        }

        QByteArray block;
        QDataStream out(&block, QIODevice::WriteOnly);
        out.setVersion(QDataStream::Qt_5_15);
        for (const QString &file : files) {
            out << QStringLiteral("OPEN") << file;
        }
        out << QStringLiteral("SHOW") << QString();

        const qint64 written = socket.write(block);
        if (written != block.size()) {
            socket.disconnectFromServer();
            continue;
        }
        socket.flush();
        // Si el envío cabe en el búfer, bytesToWrite() ya es 0 y waitForBytesWritten falla.
        if (socket.bytesToWrite() > 0 && !socket.waitForBytesWritten(2000)) {
            socket.disconnectFromServer();
            continue;
        }
        socket.disconnectFromServer();
        if (socket.state() != QLocalSocket::UnconnectedState) {
            socket.waitForDisconnected(1000);
        }
        return true;
    }
    return false;
}

} // namespace

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    const QStringList files = fileArguments(argc, argv);

    // Sistema de instancia única. Si ya hay una ventana, el documento se abre ahí.
    QSharedMemory sharedMemory(QLatin1String(MarkdownEditor::singleInstanceKey()));
    if (!sharedMemory.create(1)) {
        if (notifyRunningInstance(files)) {
            return 0;
        }

        // Segmento huérfano de una ejecución anterior: liberarlo y continuar.
        if (sharedMemory.attach()) {
            sharedMemory.detach();
        }
        if (!sharedMemory.create(1) && notifyRunningInstance(files)) {
            return 0;
        }
    }

    app.setOrganizationName("EscribeChanchi");
    app.setApplicationName("Escribe Chanchi");
    app.setApplicationVersion("1.6");

    MarkdownEditor window;
    for (const QString &file : files) {
        window.openFilePath(file);
    }
    window.showMaximized();

    return app.exec();
}
