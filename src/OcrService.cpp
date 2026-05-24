#include "OcrService.h"

#include <QProcess>
#include <QTemporaryFile>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QStringList>
#include <QCoreApplication>
#include <QByteArray>
#include <QSettings>

OcrService::OcrService(QObject* parent) : QObject(parent) {}

QString OcrService::userTesseractPath() {
    QSettings s;
    return s.value(QStringLiteral("ocr/tesseract")).toString();
}

void OcrService::setUserTesseractPath(const QString& abs) {
    QSettings s;
    if (abs.isEmpty()) {
        s.remove(QStringLiteral("ocr/tesseract"));
    } else {
        s.setValue(QStringLiteral("ocr/tesseract"), abs);
    }
    s.sync();
}

QString OcrService::findTesseract() {
    // 1. User setting (QSettings)
    const QString user = userTesseractPath();
    if (!user.isEmpty() && QFileInfo::exists(user)) return user;

    // 2. Environment override
    const QByteArray env = qgetenv("KIMG_TESSERACT");
    if (!env.isEmpty()) {
        QString p = QString::fromLocal8Bit(env);
        if (QFileInfo::exists(p)) return p;
    }

    // 3. PATH lookup
    QString hit = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
    if (!hit.isEmpty()) return hit;

    // 4. Common install locations on Windows
    static const QStringList candidates = {
        QStringLiteral("C:/Program Files/Tesseract-OCR/tesseract.exe"),
        QStringLiteral("C:/Program Files (x86)/Tesseract-OCR/tesseract.exe"),
        QStringLiteral("D:/Program Files/Tesseract-OCR/tesseract.exe"),
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) return c;
    }
    return {};
}

QString OcrService::tesseractSource() {
    const QString user = userTesseractPath();
    if (!user.isEmpty() && QFileInfo::exists(user))
        return QStringLiteral("user-setting");

    const QByteArray env = qgetenv("KIMG_TESSERACT");
    if (!env.isEmpty() && QFileInfo::exists(QString::fromLocal8Bit(env)))
        return QStringLiteral("env");

    if (!QStandardPaths::findExecutable(QStringLiteral("tesseract")).isEmpty())
        return QStringLiteral("PATH");

    static const QStringList candidates = {
        QStringLiteral("C:/Program Files/Tesseract-OCR/tesseract.exe"),
        QStringLiteral("C:/Program Files (x86)/Tesseract-OCR/tesseract.exe"),
        QStringLiteral("D:/Program Files/Tesseract-OCR/tesseract.exe"),
    };
    for (const QString& c : candidates) {
        if (QFileInfo::exists(c)) return QStringLiteral("preset");
    }
    return {};
}

OcrService::Result OcrService::recognize(const QImage& img, const QString& langs) {
    Result r;
    if (img.isNull()) {
        r.error = tr("Empty image");
        return r;
    }

    const QString exe = findTesseract();
    if (exe.isEmpty()) {
        r.error = tr(
            "Tesseract OCR engine not found.\n\n"
            "To enable OCR:\n"
            "  - Menu \"Settings -> Tesseract Path...\" to point at tesseract.exe, or\n"
            "  - Install Tesseract (https://github.com/UB-Mannheim/tesseract/wiki),\n"
            "    add it to PATH, or set environment variable KIMG_TESSERACT.\n"
            "  - (Optional) Install Chinese data: chi_sim.traineddata\n"
        );
        return r;
    }

    // Save image to a temp PNG
    QString tmpDir = QDir::tempPath();
    QString stem = QStringLiteral("kimgedit_ocr_%1").arg(QCoreApplication::applicationPid());
    QString inPath = QDir(tmpDir).filePath(stem + QStringLiteral(".png"));
    QString outStem = QDir(tmpDir).filePath(stem + QStringLiteral("_out"));
    QString outPath = outStem + QStringLiteral(".txt");

    if (!img.save(inPath, "PNG")) {
        r.error = tr("Failed to write temp image: %1").arg(inPath);
        return r;
    }

    QProcess proc;
    QStringList args;
    args << inPath << outStem << QStringLiteral("-l") << langs;
    proc.start(exe, args);
    if (!proc.waitForStarted(3000)) {
        r.error = tr("Failed to start: %1").arg(exe);
        return r;
    }
    if (!proc.waitForFinished(30000)) {
        proc.kill();
        r.error = tr("Tesseract timed out.");
        return r;
    }

    if (proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0) {
        // Retry without language argument in case data files are missing
        QProcess proc2;
        QStringList args2;
        args2 << inPath << outStem;
        proc2.start(exe, args2);
        proc2.waitForFinished(30000);
        if (proc2.exitCode() != 0) {
            r.error = tr("Tesseract failed (exit=%1).\nstderr:\n%2")
                          .arg(proc.exitCode())
                          .arg(QString::fromLocal8Bit(proc.readAllStandardError()));
            QFile::remove(inPath);
            QFile::remove(outPath);
            return r;
        }
    }

    QFile out(outPath);
    if (!out.open(QIODevice::ReadOnly)) {
        r.error = tr("Could not read OCR output: %1").arg(outPath);
        QFile::remove(inPath);
        return r;
    }
    r.text = QString::fromUtf8(out.readAll()).trimmed();
    r.ok = true;

    out.close();
    QFile::remove(inPath);
    QFile::remove(outPath);
    return r;
}
