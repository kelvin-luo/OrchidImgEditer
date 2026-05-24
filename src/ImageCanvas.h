#pragma once

#include "Tools.h"

#include <QWidget>
#include <QImage>
#include <QColor>
#include <QPointF>
#include <QVector>
#include <QStack>
#include <QRect>

class QMimeData;

// ROI padding: top, bottom, left, right (positive = inward, negative = outward).
struct RoiPadding {
    int top = 0;
    int bottom = 0;
    int left = 0;
    int right = 0;
};

class ImageCanvas : public QWidget {
    Q_OBJECT

public:
    explicit ImageCanvas(QWidget* parent = nullptr);

    bool loadImage(const QString& path);
    bool saveImage(const QString& path) const;
    bool hasImage() const { return !m_image.isNull(); }
    QImage image() const { return m_image; }

    void setTool(kimg::Tool t);
    kimg::Tool currentTool() const { return m_tool; }

    void setStrokeColor(const QColor& c)   { m_strokeColor = c; }
    QColor strokeColor() const             { return m_strokeColor; }

    void setStrokeWidth(int w)             { m_strokeWidth = qMax(1, w); }
    int  strokeWidth() const               { return m_strokeWidth; }

    void setMosaicBlock(int px)            { m_mosaicBlock = qMax(2, px); }
    int  mosaicBlock() const               { return m_mosaicBlock; }

    void setTextFontSize(int px)           { m_textFontSize = qMax(6, px); }
    int  textFontSize() const              { return m_textFontSize; }

    bool canUndo() const                   { return !m_undo.isEmpty(); }

    void setDropHighlight(bool on);

    // Returns the first local image file path from a drag payload, or empty.
    static QString imagePathFromMime(const QMimeData* mime);

    // ROI = full image rect adjusted by padding (positive shrinks, negative expands).
    static QRect roiRectFromPadding(const QSize& imageSize, const RoiPadding& pad);

    void setRoiPaddingPreview(const RoiPadding& pad);
    void clearRoiPaddingPreview();
    bool extractRoiByPadding(const RoiPadding& pad);

public slots:
    void undo();
    void resetView();
    void zoomToFit();

signals:
    void imageLoaded(const QSize& sz);
    void cursorImagePosChanged(const QPoint& imgPos, const QColor& color);
    void scaleChanged(double scale);
    void undoAvailable(bool yes);
    void statusMessage(const QString& msg);
    void ocrRegionSelected(QRect imgRect);

protected:
    void paintEvent(QPaintEvent* ev) override;
    void mousePressEvent(QMouseEvent* ev) override;
    void mouseMoveEvent(QMouseEvent* ev) override;
    void mouseReleaseEvent(QMouseEvent* ev) override;
    void wheelEvent(QWheelEvent* ev) override;
    void resizeEvent(QResizeEvent* ev) override;
    void leaveEvent(QEvent* ev) override;

private:
    // Coordinate conversion (widget <-> image pixel)
    QPointF widgetToImage(const QPointF& widgetPos) const;
    QPointF imageToWidget(const QPointF& imgPos) const;

    void pushUndoSnapshot();
    void applyZoomAround(double newScale, const QPointF& widgetAnchor);

    // Commit helpers: paint final shape onto m_image
    void commitLine(const QPointF& a, const QPointF& b);
    void commitRect(const QRectF& r);
    void commitCircle(const QRectF& r);
    void commitArrow(const QPointF& a, const QPointF& b);
    void commitPencil(const QVector<QPointF>& pts);
    void commitMosaic(const QRectF& r);
    void commitText(const QPointF& pos, const QString& text);
    void commitCrop(const QRectF& r);

    // Preview drawing on widget
    void drawPreview(QPainter& painter);
    void drawRoiPaddingPreview(QPainter& painter);

    QPen makePen() const;

    QRectF normalizedImageRect(const QPointF& a, const QPointF& b) const;

    // --- State ---
    QImage     m_image;          // current image (RGBA8888 internal)
    QImage     m_checker;        // background checker pattern
    QStack<QImage> m_undo;       // undo snapshots
    int        m_undoLimit = 30;

    double     m_scale  = 1.0;
    QPointF    m_offset {0, 0};  // top-left of image (in widget coords)

    kimg::Tool m_tool   = kimg::Tool::None;
    QColor     m_strokeColor = QColor(220, 30, 30);
    int        m_strokeWidth = 3;
    int        m_mosaicBlock = 14;
    int        m_textFontSize = 24;

    // Mouse interaction
    bool       m_drawing   = false;
    bool       m_panning   = false;
    QPointF    m_dragStartWidget;     // widget coords (for panning)
    QPointF    m_startImg;            // image coords at press
    QPointF    m_curImg;              // image coords at move
    QVector<QPointF> m_pencilPts;     // image coords pencil trace

    QPointF    m_offsetAtPanStart;

    bool       m_dropHighlight = false;

    bool       m_roiPreviewActive = false;
    RoiPadding m_roiPreview;
};
