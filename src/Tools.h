#pragma once

#include <QString>

namespace kimg {

enum class Tool {
    None,
    Line,
    Rectangle,
    Circle,
    Arrow,
    Pencil,
    Mosaic,
    Ocr,
    Text,
    Crop
};

inline QString toolName(Tool t) {
    switch (t) {
        case Tool::None:      return QStringLiteral("None");
        case Tool::Line:      return QStringLiteral("Line");
        case Tool::Rectangle: return QStringLiteral("Rectangle");
        case Tool::Circle:    return QStringLiteral("Circle");
        case Tool::Arrow:     return QStringLiteral("Arrow");
        case Tool::Pencil:    return QStringLiteral("Pencil");
        case Tool::Mosaic:    return QStringLiteral("Mosaic");
        case Tool::Ocr:       return QStringLiteral("OCR");
        case Tool::Text:      return QStringLiteral("Text");
        case Tool::Crop:      return QStringLiteral("Crop");
    }
    return QStringLiteral("Unknown");
}

} // namespace kimg
