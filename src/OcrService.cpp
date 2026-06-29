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
#include <QProcessEnvironment>

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

static QString bundledTesseractExe() {
    const QString appDir = QCoreApplication::applicationDirPath();
    const QString exe = QDir(appDir).filePath(QStringLiteral("tesseract/tesseract.exe"));
    if (QFileInfo::exists(exe))
        return QDir::toNativeSeparators(exe);
    return {};
}

static QString tessdataPrefixForExe(const QString& exePath);

QString OcrService::bundledTessdataPrefix() {
    const QString exe = bundledTesseractExe();
    if (exe.isEmpty())
        return {};
    return tessdataPrefixForExe(exe);
}

static QString tessdataPrefixForExe(const QString& exePath) {
    const QFileInfo fi(exePath);
    const QDir binDir = fi.absoluteDir();

    // Windows Tesseract 5.x: TESSDATA_PREFIX must be the tessdata directory itself
    // (not its parent) when running OCR with -l; see --list-langs vs recognize behavior.
    const QDir tessdataDir(binDir.filePath(QStringLiteral("tessdata")));
    if (tessdataDir.exists(QStringLiteral("eng.traineddata"))
        || tessdataDir.exists(QStringLiteral("chi_sim.traineddata"))) {
        return QDir::toNativeSeparators(tessdataDir.absolutePath());
    }

    // Project layout: msvc_release/models/tessdata
    const QDir modelsTessdata(
        QCoreApplication::applicationDirPath() + QStringLiteral("/models/tessdata"));
    if (modelsTessdata.exists(QStringLiteral("eng.traineddata"))
        || modelsTessdata.exists(QStringLiteral("chi_sim.traineddata"))) {
        return QDir::toNativeSeparators(modelsTessdata.absolutePath());
    }

    // Linux packages: <prefix>/share/tessdata
    const QDir shareTessdata(binDir.filePath(QStringLiteral("share/tessdata")));
    if (shareTessdata.exists(QStringLiteral("eng.traineddata"))) {
        return QDir::toNativeSeparators(shareTessdata.absolutePath());
    }

    // Fallback: parent of exe (legacy installs); may still work for --list-langs.
    if (tessdataDir.exists())
        return QDir::toNativeSeparators(binDir.absolutePath());

    return QDir::toNativeSeparators(binDir.absolutePath());
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

    // 3. Bundled next to kImgEdit.exe: msvc_release/tesseract/tesseract.exe
    const QString bundled = bundledTesseractExe();
    if (!bundled.isEmpty()) return bundled;

    // 4. PATH lookup
    QString hit = QStandardPaths::findExecutable(QStringLiteral("tesseract"));
    if (!hit.isEmpty()) return hit;

    // 5. Common install locations on Windows
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

    if (!bundledTesseractExe().isEmpty())
        return QStringLiteral("bundled");

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
            "  - Run: code\\scripts\\setup_tesseract.ps1  (downloads engine + models)\n"
            "  - Menu \"Settings -> Tesseract Path...\" to point at tesseract.exe, or\n"
            "  - Install Tesseract (https://github.com/UB-Mannheim/tesseract/wiki),\n"
            "    add it to PATH, or set environment variable KIMG_TESSERACT.\n"
            "  - Chinese data: chi_sim.traineddata (included by setup script)\n"
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

    auto runTesseract = [&](const QStringList& args, int timeoutMs) -> QPair<int, QString> {
        QProcess proc;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        const QString prefix = tessdataPrefixForExe(exe);
        if (!prefix.isEmpty())
            env.insert(QStringLiteral("TESSDATA_PREFIX"), prefix);
        proc.setProcessEnvironment(env);
        proc.start(exe, args);
        if (!proc.waitForStarted(3000))
            return { -1, tr("Failed to start: %1").arg(exe) };
        if (!proc.waitForFinished(timeoutMs)) {
            proc.kill();
            return { -2, tr("Tesseract timed out.") };
        }
        const QString err = QString::fromLocal8Bit(proc.readAllStandardError());
        return { proc.exitCode(), err };
    };

    QStringList args;
    args << inPath << outStem << QStringLiteral("-l") << langs;
    auto first = runTesseract(args, 30000);
    if (first.first == -1) {
        r.error = first.second;
        QFile::remove(inPath);
        return r;
    }
    if (first.first == -2) {
        r.error = first.second;
        QFile::remove(inPath);
        return r;
    }

    if (first.first != 0) {
        // Retry without language argument in case data files are missing
        QStringList args2;
        args2 << inPath << outStem;
        auto second = runTesseract(args2, 30000);
        if (second.first != 0) {
            r.error = tr("Tesseract failed (exit=%1).\nstderr:\n%2")
                          .arg(first.first)
                          .arg(first.second.isEmpty() ? second.second : first.second);
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
