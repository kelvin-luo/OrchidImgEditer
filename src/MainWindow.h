#pragma once

#include "Tools.h"

#include <QMainWindow>
#include <QLabel>
#include <QSpinBox>
#include <QComboBox>
#include <QToolButton>
#include <QActionGroup>
#include <QPointer>

class ImageCanvas;
class OcrService;
class QToolBar;
class QStatusBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow() override;

    void openInitial(const QString& path);

private slots:
    void onOpen();
    void onSave();
    void onSaveAs();
    void onPickColor();
    void onToolTriggered(QAction* a);
    void onCursorPos(const QPoint& p, const QColor& c);
    void onImageLoaded(const QSize& s);
    void onScaleChanged(double s);
    void onOcrRegion(QRect r);
    void onStatusMsg(const QString& m);
    void onSetTesseractPath();
    void onClearTesseractPath();
    void onShowTesseractStatus();

private:
    void buildActions();
    void buildToolBars();
    void buildStatusBar();
    void applyStyle();

    QString currentImageDir() const;

    ImageCanvas* m_canvas    = nullptr;
    OcrService*  m_ocr       = nullptr;

    QActionGroup* m_toolGroup = nullptr;

    // Actions
    QAction* m_actOpen   = nullptr;
    QAction* m_actSave   = nullptr;
    QAction* m_actSaveAs = nullptr;
    QAction* m_actUndo   = nullptr;
    QAction* m_actFit    = nullptr;

    QAction* m_actLine   = nullptr;
    QAction* m_actRect   = nullptr;
    QAction* m_actCircle = nullptr;
    QAction* m_actArrow  = nullptr;
    QAction* m_actPencil = nullptr;
    QAction* m_actMosaic = nullptr;
    QAction* m_actOcr    = nullptr;
    QAction* m_actText   = nullptr;
    QAction* m_actCrop   = nullptr;

    QAction* m_actColor  = nullptr;

    // Settings actions
    QAction* m_actSetTess   = nullptr;
    QAction* m_actClearTess = nullptr;
    QAction* m_actTessInfo  = nullptr;

    // Toolbar widgets
    QSpinBox* m_spinWidth  = nullptr;
    QSpinBox* m_spinMosaic = nullptr;
    QSpinBox* m_spinFont   = nullptr;

    // Status bar
    QLabel* m_lblPath  = nullptr;
    QLabel* m_lblSize  = nullptr;
    QLabel* m_lblPos   = nullptr;
    QLabel* m_lblScale = nullptr;

    QString m_lastPath;
};
