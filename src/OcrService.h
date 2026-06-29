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

    // Stored in QSettings as path relative to applicationDirPath() only.
    static QString storedTesseractPath();

    // Resolve stored relative path against current applicationDirPath().
    static QString resolveTesseractPath(const QString& storedRel = QString());

    // Uses stored relative path only (no system PATH / preset fallback).
    static QString findTesseract();
    static bool    tesseractAvailable() { return !findTesseract().isEmpty(); }

    static QString bundledTessdataPrefix();

    // Save as relative path (empty resets to default).
    static QString userTesseractPath() { return storedTesseractPath(); }
    static void    setUserTesseractPath(const QString& absOrRel);
    static void    resetTesseractPathToDefault();

    // Return "default" or "user-setting", or empty if not found.
    static QString tesseractSource();
};
