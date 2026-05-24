#include "ImageCanvas.h"

#include <QPainter>
#include <QPainterPath>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QInputDialog>
#include <QFontMetrics>
#include <QtMath>
#include <QFileInfo>
#include <QMimeData>
#include <QUrl>

#include <opencv2/core.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/imgcodecs.hpp>

#include <algorithm>

namespace {

// Convert QImage (Format_ARGB32 / RGBA8888) to cv::Mat (BGR or BGRA, no copy share)
cv::Mat qimageToMat(const QImage& src) {
    QImage img = src.convertToFormat(QImage::Format_RGBA8888);
    cv::Mat tmp(img.height(), img.width(), CV_8UC4,
                const_cast<uchar*>(img.constBits()),
                static_cast<size_t>(img.bytesPerLine()));
    cv::Mat out;
    cv::cvtColor(tmp, out, cv::COLOR_RGBA2BGRA);
    return out;
}

QImage matToQImage(const cv::Mat& src) {
    cv::Mat tmp;
    if (src.channels() == 4) {
        cv::cvtColor(src, tmp, cv::COLOR_BGRA2RGBA);
    } else if (src.channels() == 3) {
        cv::cvtColor(src, tmp, cv::COLOR_BGR2RGBA);
    } else {
        cv::cvtColor(src, tmp, cv::COLOR_GRAY2RGBA);
    }
    QImage out(tmp.data, tmp.cols, tmp.rows,
               static_cast<int>(tmp.step), QImage::Format_RGBA8888);
    return out.copy();
}

QImage makeChecker(int tile = 16) {
    QImage img(tile * 2, tile * 2, QImage::Format_RGB32);
    img.fill(QColor(230, 230, 230));
    QPainter p(&img);
    p.fillRect(0,     0,    tile, tile, QColor(200, 200, 200));
    p.fillRect(tile, tile, tile, tile, QColor(200, 200, 200));
    return img;
}

bool isImageExtension(const QString& ext) {
    static const QStringList kExts = {
        QStringLiteral("png"),  QStringLiteral("jpg"),  QStringLiteral("jpeg"),
        QStringLiteral("bmp"),  QStringLiteral("tif"),  QStringLiteral("tiff"),
        QStringLiteral("webp"), QStringLiteral("gif"),  QStringLiteral("ico"),
    };
    return kExts.contains(ext.toLower());
}

} // anon

ImageCanvas::ImageCanvas(QWidget* parent)
    : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setMinimumSize(400, 300);
    setAcceptDrops(true);
    m_checker = makeChecker(12);
}

QString ImageCanvas::imagePathFromMime(const QMimeData* mime) {
    if (!mime || !mime->hasUrls()) return {};

    for (const QUrl& url : mime->urls()) {
        if (!url.isLocalFile()) continue;
        const QString path = url.toLocalFile();
        if (isImageExtension(QFileInfo(path).suffix()))
            return path;
    }
    return {};
}

void ImageCanvas::setDropHighlight(bool on) {
    if (m_dropHighlight == on) return;
    m_dropHighlight = on;
    update();
}

QRect ImageCanvas::roiRectFromPadding(const QSize& imageSize, const RoiPadding& pad) {
    if (imageSize.isEmpty()) return {};
    const int w = imageSize.width()  - pad.left - pad.right;
    const int h = imageSize.height() - pad.top  - pad.bottom;
    return QRect(pad.left, pad.top, w, h);
}

void ImageCanvas::setRoiPaddingPreview(const RoiPadding& pad) {
    m_roiPreview = pad;
    m_roiPreviewActive = true;
    update();
}

void ImageCanvas::clearRoiPaddingPreview() {
    if (!m_roiPreviewActive) return;
    m_roiPreviewActive = false;
    update();
}

bool ImageCanvas::extractRoiByPadding(const RoiPadding& pad) {
    if (m_image.isNull()) return false;

    const QRect src = roiRectFromPadding(m_image.size(), pad);
    if (src.width() <= 0 || src.height() <= 0) {
        emit statusMessage(tr("Invalid ROI padding: result size %1 x %2")
                               .arg(src.width()).arg(src.height()));
        return false;
    }

    pushUndoSnapshot();

    QImage out(src.width(), src.height(), QImage::Format_RGBA8888);
    out.fill(Qt::transparent);

    const QRect inside = src.intersected(m_image.rect());
    if (!inside.isEmpty()) {
        QPainter p(&out);
        p.drawImage(inside.translated(-src.topLeft()), m_image, inside);
    }

    m_image = out;
    m_roiPreviewActive = false;
    emit imageLoaded(m_image.size());
    emit statusMessage(tr("ROI extract: %1 x %2  (padding T/B/L/R = %3/%4/%5/%6)")
                           .arg(out.width()).arg(out.height())
                           .arg(pad.top).arg(pad.bottom)
                           .arg(pad.left).arg(pad.right));
    zoomToFit();
    update();
    return true;
}

bool ImageCanvas::loadImage(const QString& path) {
    QImage img;
    if (!img.load(path)) {
        emit statusMessage(tr("Failed to load: %1").arg(path));
        return false;
    }
    m_image = img.convertToFormat(QImage::Format_RGBA8888);
    m_undo.clear();
    emit undoAvailable(false);
    clearModified();
    emit imageLoaded(m_image.size());
    emit statusMessage(tr("Loaded %1  (%2x%3)")
                       .arg(QFileInfo(path).fileName())
                       .arg(m_image.width()).arg(m_image.height()));
    zoomToFit();
    update();
    return true;
}

bool ImageCanvas::saveImage(const QString& path) const {
    if (m_image.isNull()) return false;
    return m_image.save(path);
}

void ImageCanvas::setTool(kimg::Tool t) {
    m_tool = t;
    if (t == kimg::Tool::Crop)
        setCursor(Qt::CrossCursor);
    else if (t == kimg::Tool::Pencil)
        setCursor(Qt::CrossCursor);
    else if (t != kimg::Tool::None)
        setCursor(Qt::CrossCursor);
    else
        setCursor(Qt::ArrowCursor);
    update();
}

void ImageCanvas::undo() {
    if (m_undo.isEmpty()) return;
    m_image = m_undo.pop();
    emit undoAvailable(!m_undo.isEmpty());
    emit imageLoaded(m_image.size());
    emit statusMessage(tr("Undo (remaining %1)").arg(m_undo.size()));
    update();
}

void ImageCanvas::resetView() {
    m_scale = 1.0;
    m_offset = QPointF(0, 0);
    emit scaleChanged(m_scale);
    update();
}

void ImageCanvas::zoomToFit() {
    if (m_image.isNull()) return;
    const double sx = double(width())  / m_image.width();
    const double sy = double(height()) / m_image.height();
    m_scale = std::min(sx, sy) * 0.97;
    if (m_scale <= 0.0) m_scale = 1.0;
    const double imgW = m_image.width()  * m_scale;
    const double imgH = m_image.height() * m_scale;
    m_offset = QPointF((width() - imgW) * 0.5, (height() - imgH) * 0.5);
    emit scaleChanged(m_scale);
    update();
}

QPointF ImageCanvas::widgetToImage(const QPointF& w) const {
    return QPointF((w.x() - m_offset.x()) / m_scale,
                   (w.y() - m_offset.y()) / m_scale);
}

QPointF ImageCanvas::imageToWidget(const QPointF& p) const {
    return QPointF(p.x() * m_scale + m_offset.x(),
                   p.y() * m_scale + m_offset.y());
}

void ImageCanvas::pushUndoSnapshot() {
    if (m_image.isNull()) return;
    if (m_undo.size() >= m_undoLimit) {
        m_undo.remove(0);
    }
    m_undo.push(m_image.copy());
    emit undoAvailable(true);
    markModified();
}

void ImageCanvas::markModified() {
    if (m_modified) return;
    m_modified = true;
    emit modifiedChanged(true);
}

void ImageCanvas::clearModified() {
    if (!m_modified) return;
    m_modified = false;
    emit modifiedChanged(false);
}

void ImageCanvas::applyZoomAround(double newScale, const QPointF& widgetAnchor) {
    newScale = std::clamp(newScale, 0.05, 40.0);
    const QPointF imgAnchor = widgetToImage(widgetAnchor);
    m_scale = newScale;
    // After zoom, recompute offset so the same image point stays under cursor
    m_offset = widgetAnchor - imgAnchor * m_scale;
    emit scaleChanged(m_scale);
    update();
}

QPen ImageCanvas::makePen() const {
    QPen pen(m_strokeColor);
    pen.setWidthF(std::max(1, m_strokeWidth));
    pen.setCapStyle(Qt::RoundCap);
    pen.setJoinStyle(Qt::RoundJoin);
    return pen;
}

QRectF ImageCanvas::normalizedImageRect(const QPointF& a, const QPointF& b) const {
    return QRectF(QPointF(std::min(a.x(), b.x()), std::min(a.y(), b.y())),
                  QPointF(std::max(a.x(), b.x()), std::max(a.y(), b.y())));
}

// --- Event handlers --------------------------------------------------------

void ImageCanvas::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.fillRect(rect(), QColor(45, 48, 56));

    if (m_image.isNull()) {
        p.setPen(QColor(180, 180, 180));
        p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 14));
        p.drawText(rect(), Qt::AlignCenter,
                   tr("Use [Open] or drag & drop an image here\n"
                      "Wheel: zoom around cursor   Right-drag: pan   Ctrl+Z: undo"));
        if (m_dropHighlight) {
            p.fillRect(rect(), QColor(74, 111, 165, 72));
            p.setPen(QPen(QColor(120, 170, 255), 2, Qt::DashLine));
            p.setBrush(Qt::NoBrush);
            p.drawRect(rect().adjusted(6, 6, -6, -6));
            p.setPen(QColor(220, 230, 255));
            p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 15, QFont::Bold));
            p.drawText(rect(), Qt::AlignCenter, tr("Drop image to open"));
        }
        return;
    }

    // Visible image rect (in widget coords)
    const QRectF imgRectW(m_offset,
                          QSizeF(m_image.width()  * m_scale,
                                 m_image.height() * m_scale));

    // Checker background under the image
    p.save();
    p.setClipRect(imgRectW);
    QBrush checkerBrush(m_checker);
    p.fillRect(imgRectW, checkerBrush);
    p.restore();

    // Image
    p.setRenderHint(QPainter::SmoothPixmapTransform, m_scale < 4.0);
    p.drawImage(imgRectW, m_image, QRectF(QPointF(0, 0), QSizeF(m_image.size())));

    // Frame
    p.setPen(QPen(QColor(80, 80, 80), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRect(imgRectW.adjusted(-0.5, -0.5, 0.5, 0.5));

    // Preview of current operation
    if (m_drawing && m_tool != kimg::Tool::None) {
        drawPreview(p);
    }

    if (m_roiPreviewActive && !m_image.isNull()) {
        drawRoiPaddingPreview(p);
    }

    if (m_dropHighlight) {
        p.fillRect(rect(), QColor(74, 111, 165, 72));
        p.setPen(QPen(QColor(120, 170, 255), 2, Qt::DashLine));
        p.setBrush(Qt::NoBrush);
        p.drawRect(rect().adjusted(6, 6, -6, -6));
        p.setPen(QColor(220, 230, 255));
        p.setFont(QFont(QStringLiteral("Microsoft YaHei"), 15, QFont::Bold));
        p.drawText(rect(), Qt::AlignCenter, tr("Drop image to open"));
    }
}

void ImageCanvas::drawPreview(QPainter& p) {
    p.setRenderHint(QPainter::Antialiasing, true);
    QPen pen = makePen();
    pen.setWidthF(std::max<double>(1.0, pen.widthF() * m_scale));
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    const QPointF a = imageToWidget(m_startImg);
    const QPointF b = imageToWidget(m_curImg);

    switch (m_tool) {
        case kimg::Tool::Line:
            p.drawLine(a, b);
            break;
        case kimg::Tool::Rectangle:
            p.drawRect(QRectF(a, b).normalized());
            break;
        case kimg::Tool::Circle: {
            QRectF r(a, b);
            r = r.normalized();
            p.drawEllipse(r);
            break;
        }
        case kimg::Tool::Arrow: {
            p.drawLine(a, b);
            // Arrow head
            const double ang = std::atan2(b.y() - a.y(), b.x() - a.x());
            const double head = std::max(10.0, m_strokeWidth * 3.0 * m_scale);
            const double dA = ang + M_PI - M_PI / 7.0;
            const double dB = ang + M_PI + M_PI / 7.0;
            QPolygonF tri;
            tri << b
                << b + QPointF(std::cos(dA) * head, std::sin(dA) * head)
                << b + QPointF(std::cos(dB) * head, std::sin(dB) * head);
            QBrush fill(m_strokeColor);
            p.setBrush(fill);
            p.drawPolygon(tri);
            break;
        }
        case kimg::Tool::Pencil: {
            QPolygonF poly;
            for (const auto& pt : m_pencilPts) poly << imageToWidget(pt);
            p.drawPolyline(poly);
            break;
        }
        case kimg::Tool::Mosaic:
        case kimg::Tool::Crop:
        case kimg::Tool::Ocr: {
            QPen dashed = pen;
            dashed.setStyle(Qt::DashLine);
            dashed.setColor(QColor(255, 220, 60));
            dashed.setWidthF(std::max(1.5, 1.5));
            p.setPen(dashed);
            QRectF r(a, b);
            p.drawRect(r.normalized());
            p.fillRect(r.normalized(), QColor(255, 220, 60, 40));
            break;
        }
        default: break;
    }
}

void ImageCanvas::drawRoiPaddingPreview(QPainter& p) {
    const QRect roi = roiRectFromPadding(m_image.size(), m_roiPreview);
    if (roi.isEmpty()) return;

    p.setRenderHint(QPainter::Antialiasing, true);

    // Full expanded bounds (may extend outside image when padding is negative).
    const QRectF roiW(
        imageToWidget(QPointF(roi.x(), roi.y())),
        imageToWidget(QPointF(roi.x() + roi.width(), roi.y() + roi.height())));

    QPen outer(QColor(255, 180, 60), 2, Qt::DashLine);
    p.setPen(outer);
    p.setBrush(QColor(255, 180, 60, 35));
    p.drawRect(roiW.normalized());

    // Visible image area for reference.
    const QRectF imgW(m_offset,
                      QSizeF(m_image.width() * m_scale, m_image.height() * m_scale));
    const QRectF visible = roiW.normalized().intersected(imgW);
    if (!visible.isEmpty()) {
        QPen inner(QColor(80, 220, 120), 2, Qt::SolidLine);
        p.setPen(inner);
        p.setBrush(Qt::NoBrush);
        p.drawRect(visible);
    }
}

void ImageCanvas::mousePressEvent(QMouseEvent* ev) {
    if (m_image.isNull()) return;

    if (ev->button() == Qt::RightButton) {
        m_panning = true;
        m_dragStartWidget = ev->position();
        m_offsetAtPanStart = m_offset;
        setCursor(Qt::ClosedHandCursor);
        return;
    }

    if (ev->button() != Qt::LeftButton) return;
    if (m_tool == kimg::Tool::None) return;

    m_drawing = true;
    m_startImg = widgetToImage(ev->position());
    m_curImg   = m_startImg;
    if (m_tool == kimg::Tool::Pencil) {
        m_pencilPts.clear();
        m_pencilPts.push_back(m_startImg);
    } else if (m_tool == kimg::Tool::Text) {
        // Text uses single click; commit immediately
        m_drawing = false;
        bool ok = false;
        QString text = QInputDialog::getText(this, tr("Insert Text"),
                                             tr("Text:"), QLineEdit::Normal,
                                             QString(), &ok);
        if (ok && !text.isEmpty()) {
            pushUndoSnapshot();
            commitText(m_startImg, text);
            update();
        }
    }
    update();
}

void ImageCanvas::mouseMoveEvent(QMouseEvent* ev) {
    if (m_panning) {
        const QPointF delta = ev->position() - m_dragStartWidget;
        m_offset = m_offsetAtPanStart + delta;
        update();
        return;
    }

    const QPointF imgP = widgetToImage(ev->position());

    // Emit cursor info
    if (!m_image.isNull()) {
        const QPoint ip(int(std::floor(imgP.x())), int(std::floor(imgP.y())));
        QColor c;
        if (ip.x() >= 0 && ip.y() >= 0 &&
            ip.x() < m_image.width() && ip.y() < m_image.height()) {
            c = m_image.pixelColor(ip);
        }
        emit cursorImagePosChanged(ip, c);
    }

    if (!m_drawing) return;

    m_curImg = imgP;
    if (m_tool == kimg::Tool::Pencil) {
        m_pencilPts.push_back(imgP);
    }
    update();
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent* ev) {
    if (ev->button() == Qt::RightButton && m_panning) {
        m_panning = false;
        setCursor(m_tool == kimg::Tool::None ? Qt::ArrowCursor : Qt::CrossCursor);
        return;
    }

    if (ev->button() != Qt::LeftButton) return;
    if (!m_drawing) return;
    m_drawing = false;

    // Commit
    switch (m_tool) {
        case kimg::Tool::Line:
            pushUndoSnapshot();
            commitLine(m_startImg, m_curImg);
            break;
        case kimg::Tool::Rectangle:
            pushUndoSnapshot();
            commitRect(normalizedImageRect(m_startImg, m_curImg));
            break;
        case kimg::Tool::Circle:
            pushUndoSnapshot();
            commitCircle(normalizedImageRect(m_startImg, m_curImg));
            break;
        case kimg::Tool::Arrow:
            pushUndoSnapshot();
            commitArrow(m_startImg, m_curImg);
            break;
        case kimg::Tool::Pencil:
            if (m_pencilPts.size() >= 2) {
                pushUndoSnapshot();
                commitPencil(m_pencilPts);
            }
            m_pencilPts.clear();
            break;
        case kimg::Tool::Mosaic: {
            QRectF r = normalizedImageRect(m_startImg, m_curImg);
            if (r.width() >= 2 && r.height() >= 2) {
                pushUndoSnapshot();
                commitMosaic(r);
            }
            break;
        }
        case kimg::Tool::Crop: {
            QRectF r = normalizedImageRect(m_startImg, m_curImg);
            if (r.width() >= 2 && r.height() >= 2) {
                pushUndoSnapshot();
                commitCrop(r);
                zoomToFit();
            }
            break;
        }
        case kimg::Tool::Ocr: {
            QRectF r = normalizedImageRect(m_startImg, m_curImg);
            QRect ir = r.toRect().intersected(m_image.rect());
            if (!ir.isEmpty()) {
                emit statusMessage(tr("OCR region: (%1, %2) %3x%4")
                                   .arg(ir.left()).arg(ir.top())
                                   .arg(ir.width()).arg(ir.height()));
                emit ocrRegionSelected(ir);
            }
            break;
        }
        default: break;
    }

    update();
}

void ImageCanvas::wheelEvent(QWheelEvent* ev) {
    if (m_image.isNull()) return;
    const double steps = ev->angleDelta().y() / 120.0;
    const double factor = std::pow(1.18, steps);
    applyZoomAround(m_scale * factor, ev->position());
    ev->accept();
}

void ImageCanvas::resizeEvent(QResizeEvent*) {
    // Keep the view roughly stable on resize: do nothing here so user keeps
    // their pan/zoom. zoomToFit() is invoked on load only.
}

void ImageCanvas::leaveEvent(QEvent*) {
    emit cursorImagePosChanged(QPoint(-1, -1), QColor());
}

// --- Commit functions ------------------------------------------------------

void ImageCanvas::commitLine(const QPointF& a, const QPointF& b) {
    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(makePen());
    p.drawLine(a, b);
}

void ImageCanvas::commitRect(const QRectF& r) {
    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(makePen());
    p.setBrush(Qt::NoBrush);
    p.drawRect(r);
}

void ImageCanvas::commitCircle(const QRectF& r) {
    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(makePen());
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(r);
}

void ImageCanvas::commitArrow(const QPointF& a, const QPointF& b) {
    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen = makePen();
    p.setPen(pen);
    p.setBrush(QBrush(m_strokeColor));
    p.drawLine(a, b);

    const double ang = std::atan2(b.y() - a.y(), b.x() - a.x());
    const double head = std::max<double>(10.0, m_strokeWidth * 3.0);
    const double dA = ang + M_PI - M_PI / 7.0;
    const double dB = ang + M_PI + M_PI / 7.0;
    QPolygonF tri;
    tri << b
        << b + QPointF(std::cos(dA) * head, std::sin(dA) * head)
        << b + QPointF(std::cos(dB) * head, std::sin(dB) * head);
    p.drawPolygon(tri);
}

void ImageCanvas::commitPencil(const QVector<QPointF>& pts) {
    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(makePen());
    QPainterPath path;
    path.moveTo(pts.front());
    for (int i = 1; i < pts.size(); ++i) path.lineTo(pts[i]);
    p.drawPath(path);
}

void ImageCanvas::commitMosaic(const QRectF& r) {
    QRect ir = r.toRect().intersected(m_image.rect());
    if (ir.isEmpty()) return;

    // Use OpenCV pixelation
    QImage region = m_image.copy(ir);
    cv::Mat m = qimageToMat(region);
    const int block = std::clamp(m_mosaicBlock, 2, std::max(2, std::min(m.cols, m.rows)));

    cv::Mat down, up;
    cv::resize(m, down,
               cv::Size(std::max(1, m.cols / block), std::max(1, m.rows / block)),
               0, 0, cv::INTER_AREA);
    cv::resize(down, up, m.size(), 0, 0, cv::INTER_NEAREST);

    QImage pixel = matToQImage(up);
    QPainter p(&m_image);
    p.drawImage(ir.topLeft(), pixel);
}

void ImageCanvas::commitText(const QPointF& pos, const QString& text) {
    QPainter p(&m_image);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f(QStringLiteral("Microsoft YaHei"));
    f.setPixelSize(m_textFontSize);
    f.setBold(true);
    p.setFont(f);
    p.setPen(m_strokeColor);
    QFontMetrics fm(f);
    // Position: pos as left baseline
    p.drawText(pos + QPointF(0, fm.ascent()), text);
}

void ImageCanvas::commitCrop(const QRectF& r) {
    QRect ir = r.toRect().intersected(m_image.rect());
    if (ir.isEmpty()) return;
    m_image = m_image.copy(ir);
    emit imageLoaded(m_image.size());
}
