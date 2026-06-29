#pragma once

#include <QObject>
#include <QString>
#include <QImage>

// Lightweight wrapper for OCR (calls tesseract.exe via QProcess).
class OcrService : public QObject {
    Q_OBJECT
public:
    explicit OcrService(QObject* parent = nullptr);

    struct Result {
        bool ok = false;
        QString text;
        QString error;
    };

    // Default relative path under msvc_release: tesseract/tesseract.exe
    static QString defaultTesseractRelPath();

    // Synchronous (runs tesseract); returns recognized text.
    // 'langs' example: "chi_sim+eng" or "eng".
    Result recognize(const QImage& img, const QString& langs = QStringLiteral("chi_sim+eng"));

    // Stored in QSettings as a path relative to applicationDirPath().
    static QString storedTesseractPath();

    // Absolute path used to launch tesseract.exe.
    static QString resolveTesseractPath(const QString& storedRel = QString());

    // Resolve order: QSettings (relative) -> env KIMG_TESSERACT -> PATH -> presets.
    static QString findTesseract();
    static bool    tesseractAvailable() { return !findTesseract().isEmpty(); }

    static QString bundledTessdataPrefix();

    // Save as relative path (empty resets to default).
    static QString userTesseractPath() { return storedTesseractPath(); }
    static void    setUserTesseractPath(const QString& absOrRel);
    static void    resetTesseractPathToDefault();

    // Return one of: "default", "user-setting", "env", "PATH", "preset", or empty.
    static QString tesseractSource();
};
