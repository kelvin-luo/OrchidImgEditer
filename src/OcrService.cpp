#include "OcrService.h"

#include <QProcess>
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

QString OcrService::defaultTesseractRelPath() {
    return QStringLiteral("tesseract/tesseract.exe");
}

static QString appBaseDir() {
    return QCoreApplication::applicationDirPath();
}

static QString toStoredPath(const QString& absOrRel) {
    if (absOrRel.isEmpty())
        return OcrService::defaultTesseractRelPath();

    const QDir base(appBaseDir());
    QFileInfo fi(absOrRel);
    const QString abs = fi.isAbsolute()
        ? QDir::cleanPath(fi.absoluteFilePath())
        : QDir::cleanPath(base.absoluteFilePath(absOrRel));

    QString rel = base.relativeFilePath(abs);
    if (rel.startsWith(QLatin1String("..")))
        return rel;  // outside app dir: still store as relative
    return QDir::fromNativeSeparators(rel);
}

QString OcrService::storedTesseractPath() {
    QSettings s;
    const QString key = QStringLiteral("ocr/tesseract");
    if (!s.contains(key)) {
        s.setValue(key, defaultTesseractRelPath());
        s.sync();
        return defaultTesseractRelPath();
    }

    QString stored = s.value(key).toString();
    if (stored.isEmpty())
        stored = defaultTesseractRelPath();

    // Migrate legacy absolute paths to relative.
    if (QFileInfo(stored).isAbsolute()) {
        stored = toStoredPath(stored);
        s.setValue(key, stored);
        s.sync();
    }
    return stored;
}

QString OcrService::resolveTesseractPath(const QString& storedRel) {
    QString rel = storedRel.isEmpty() ? storedTesseractPath() : storedRel;
    rel = QDir::fromNativeSeparators(rel);
    return QDir::toNativeSeparators(QDir(appBaseDir()).absoluteFilePath(rel));
}

void OcrService::setUserTesseractPath(const QString& absOrRel) {
    QSettings s;
    s.setValue(QStringLiteral("ocr/tesseract"), toStoredPath(absOrRel));
    s.sync();
}

void OcrService::resetTesseractPathToDefault() {
    setUserTesseractPath(defaultTesseractRelPath());
}

static QString tessdataPrefixForExe(const QString& exePath);

QString OcrService::bundledTessdataPrefix() {
    const QString exe = resolveTesseractPath(defaultTesseractRelPath());
    if (!QFileInfo::exists(exe))
        return {};
    return tessdataPrefixForExe(exe);
}

static QString tessdataDirForExe(const QString& exePath) {
    const QFileInfo fi(exePath);
    const QDir binDir = fi.absoluteDir();

    const QDir tessdataDir(binDir.filePath(QStringLiteral("tessdata")));
    if (tessdataDir.exists(QStringLiteral("eng.traineddata"))
        || tessdataDir.exists(QStringLiteral("chi_sim.traineddata"))) {
        return QDir::toNativeSeparators(tessdataDir.absolutePath());
    }

    const QDir modelsTessdata(
        QCoreApplication::applicationDirPath() + QStringLiteral("/models/tessdata"));
    if (modelsTessdata.exists(QStringLiteral("eng.traineddata"))
        || modelsTessdata.exists(QStringLiteral("chi_sim.traineddata"))) {
        return QDir::toNativeSeparators(modelsTessdata.absolutePath());
    }

    const QDir shareTessdata(binDir.filePath(QStringLiteral("share/tessdata")));
    if (shareTessdata.exists(QStringLiteral("eng.traineddata"))) {
        return QDir::toNativeSeparators(shareTessdata.absolutePath());
    }

    return {};
}

static QString tessdataPrefixForExe(const QString& exePath) {
    const QString dir = tessdataDirForExe(exePath);
    if (!dir.isEmpty())
        return dir;
    return QDir::toNativeSeparators(QFileInfo(exePath).absolutePath());
}

QString OcrService::findTesseract() {
    // 1. Saved setting (relative -> absolute)
    const QString stored = storedTesseractPath();
    const QString fromSettings = resolveTesseractPath(stored);
    if (QFileInfo::exists(fromSettings))
        return fromSettings;

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
    const QString stored = storedTesseractPath();
    const QString resolved = resolveTesseractPath(stored);
    if (QFileInfo::exists(resolved)) {
        if (stored == defaultTesseractRelPath())
            return QStringLiteral("default");
        return QStringLiteral("user-setting");
    }

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
            "Default path (relative): %1\n"
            "Resolved: %2\n\n"
            "To enable OCR:\n"
            "  - Run: code/scripts/setup_tesseract.bat\n"
            "  - Menu \"Settings -> Tesseract Path...\" to choose tesseract.exe\n"
            "  - Or set environment variable KIMG_TESSERACT\n"
        ).arg(defaultTesseractRelPath(), resolveTesseractPath());
        return r;
    }

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
        const QFileInfo exeInfo(exe);
        const QString tessDir = tessdataDirForExe(exe);

        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove(QStringLiteral("TESSDATA_PREFIX"));
        if (!tessDir.isEmpty())
            env.insert(QStringLiteral("TESSDATA_PREFIX"), tessDir);
        proc.setProcessEnvironment(env);
        proc.setWorkingDirectory(exeInfo.absolutePath());

        QStringList cmd = args;
        if (!tessDir.isEmpty()) {
            cmd << QStringLiteral("--tessdata-dir") << tessDir;
        }

        proc.start(exe, cmd);
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
