#pragma once

#include <QObject>
#include <QString>
#include <QImage>

// Lightweight wrapper for OCR.
//
// Implementation strategy:
//   1. If "tesseract" is on PATH (or env KIMG_TESSERACT points to a binary),
//      use QProcess to run it on a temporary PNG and parse the text output.
//   2. Otherwise, return an informative error string that explains how to
//      enable OCR locally.
class OcrService : public QObject {
    Q_OBJECT
public:
    explicit OcrService(QObject* parent = nullptr);

    struct Result {
        bool ok = false;
        QString text;
        QString error;
    };

    // Synchronous (runs tesseract); returns recognized text.
    // 'langs' example: "chi_sim+eng" or "eng".
    Result recognize(const QImage& img, const QString& langs = QStringLiteral("chi_sim+eng"));

    // Resolve order: QSettings -> env KIMG_TESSERACT -> bundled (next to exe)
    //              -> PATH lookup -> common install locations.
    static QString findTesseract();
    static bool    tesseractAvailable() { return !findTesseract().isEmpty(); }

    // Parent directory of tessdata/ when using the bundled layout (exe dir / tesseract).
    static QString bundledTessdataPrefix();

    // Persistent override (saved in QSettings, group "ocr", key "tesseract").
    // Pass an empty string to clear.
    static QString userTesseractPath();
    static void    setUserTesseractPath(const QString& abs);

    // Return one of: "user-setting", "env", "bundled", "PATH", "preset", or empty.
    static QString tesseractSource();
};
