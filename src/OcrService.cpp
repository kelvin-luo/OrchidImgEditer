#include "OcrService.h"

#include <QProcess>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>
#include <QProcessEnvironment>

OcrService::OcrService(QObject* parent) : QObject(parent) {}

QString OcrService::defaultTesseractRelPath() {
    return QStringLiteral("tesseract/tesseract.exe");
}

static QString appBaseDir() {
    return QDir::cleanPath(QCoreApplication::applicationDirPath());
}

static bool isBundledTesseractAbs(const QString& absPath) {
    const QFileInfo fi(QDir::cleanPath(absPath));
    return fi.fileName().compare(QStringLiteral("tesseract.exe"), Qt::CaseInsensitive) == 0
        && fi.dir().dirName().compare(QStringLiteral("tesseract"), Qt::CaseInsensitive) == 0;
}

static QString normalizeStoredRel(const QString& rel) {
    QString r = QDir::fromNativeSeparators(rel.trimmed());
    while (r.startsWith(QLatin1String("./")))
        r = r.mid(2);
    return r;
}

static QString toStoredPath(const QString& absOrRel) {
    const QString def = OcrService::defaultTesseractRelPath();
    if (absOrRel.isEmpty() || normalizeStoredRel(absOrRel) == def)
        return def;

    const QDir base(appBaseDir());
    const QFileInfo in(absOrRel);

    QString abs = in.isAbsolute()
        ? QDir::cleanPath(in.absoluteFilePath())
        : QDir::cleanPath(base.absoluteFilePath(normalizeStoredRel(absOrRel)));

    if (isBundledTesseractAbs(abs))
        return def;

    const QString baseAbs = base.absolutePath();
    if (abs.startsWith(baseAbs, Qt::CaseInsensitive)) {
        return normalizeStoredRel(base.relativeFilePath(abs));
    }

    // Outside app dir: keep as relative (may break if whole app is moved).
    return normalizeStoredRel(base.relativeFilePath(abs));
}

QString OcrService::resolveTesseractPath(const QString& storedRel) {
    const QString rel = normalizeStoredRel(
        storedRel.isEmpty() ? defaultTesseractRelPath() : storedRel);
    return QDir(appBaseDir()).absoluteFilePath(rel);
}

static void saveStoredPath(const QString& rel) {
    QSettings s;
    s.setValue(QStringLiteral("ocr/tesseract"), normalizeStoredRel(rel));
    s.sync();
}

QString OcrService::storedTesseractPath() {
    QSettings s;
    const QString key = QStringLiteral("ocr/tesseract");
    const QString def = defaultTesseractRelPath();

    if (!s.contains(key)) {
        saveStoredPath(def);
        return def;
    }

    QString stored = normalizeStoredRel(s.value(key).toString());
    if (stored.isEmpty())
        stored = def;

    bool changed = false;

    // Legacy: absolute path in settings -> normalize to app-relative.
    if (QFileInfo(stored).isAbsolute()) {
        stored = isBundledTesseractAbs(stored) ? def : toStoredPath(stored);
        changed = true;
    }

    // Legacy: stored path pointed at old install location (contains ..).
    if (stored.contains(QLatin1String(".."))) {
        if (QFileInfo::exists(resolveTesseractPath(def))) {
            stored = def;
            changed = true;
        }
    }

    // Resolved path missing (e.g. app folder moved) -> fall back to bundled default.
    if (!QFileInfo::exists(resolveTesseractPath(stored))
        && QFileInfo::exists(resolveTesseractPath(def))) {
        stored = def;
        changed = true;
    }

    if (changed)
        saveStoredPath(stored);

    return stored;
}

void OcrService::setUserTesseractPath(const QString& absOrRel) {
    saveStoredPath(toStoredPath(absOrRel));
}

void OcrService::resetTesseractPathToDefault() {
    saveStoredPath(defaultTesseractRelPath());
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
    const QString resolved = resolveTesseractPath(storedTesseractPath());
    if (QFileInfo::exists(resolved))
        return QDir::toNativeSeparators(resolved);
    return {};
}

QString OcrService::tesseractSource() {
    const QString stored = storedTesseractPath();
    if (!QFileInfo::exists(resolveTesseractPath(stored)))
        return {};
    return stored == defaultTesseractRelPath()
        ? QStringLiteral("default")
        : QStringLiteral("user-setting");
}

OcrService::Result OcrService::recognize(const QImage& img, const QString& langs) {
    Result r;
    if (img.isNull()) {
        r.error = tr("Empty image");
        return r;
    }

    const QString stored = storedTesseractPath();
    const QString exe = findTesseract();
    if (exe.isEmpty()) {
        r.error = tr(
            "Tesseract OCR engine not found.\n\n"
            "Stored (relative to app): %1\n"
            "App directory: %2\n"
            "Expected: %3\n\n"
            "Run code/scripts/setup_tesseract.bat to deploy tesseract/ beside kImgEdit.exe."
        ).arg(stored, appBaseDir(), resolveTesseractPath(stored));
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
