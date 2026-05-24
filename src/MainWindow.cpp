#include "MainWindow.h"
#include "ImageCanvas.h"
#include "OcrService.h"

#include <QFileDialog>
#include <QColorDialog>
#include <QMessageBox>
#include <QToolBar>
#include <QStatusBar>
#include <QLabel>
#include <QApplication>
#include <QStyle>
#include <QIcon>
#include <QFileInfo>
#include <QDir>
#include <QKeySequence>
#include <QFontDatabase>
#include <QTextEdit>
#include <QDialog>
#include <QVBoxLayout>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QPushButton>
#include <QMenu>
#include <QMenuBar>
#include <QProcess>
#include <QEvent>
#include <QDragEnterEvent>
#include <QDragMoveEvent>
#include <QDragLeaveEvent>
#include <QDropEvent>

namespace {
QIcon icon(const QString& name) {
    return QIcon(QStringLiteral(":/icons/%1.svg").arg(name));
}
} // anon

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("kImgEdit"));
    setWindowIcon(QIcon(QStringLiteral(":/icons/app.ico")));
    if (windowIcon().isNull())
        setWindowIcon(QIcon(QStringLiteral(":/icons/app.svg")));
    resize(1280, 820);

    m_canvas = new ImageCanvas(this);
    setCentralWidget(m_canvas);

    m_ocr = new OcrService(this);

    buildActions();
    buildToolBars();
    buildStatusBar();
    applyStyle();

    connect(m_canvas, &ImageCanvas::cursorImagePosChanged,
            this, &MainWindow::onCursorPos);
    connect(m_canvas, &ImageCanvas::imageLoaded,
            this, &MainWindow::onImageLoaded);
    connect(m_canvas, &ImageCanvas::scaleChanged,
            this, &MainWindow::onScaleChanged);
    connect(m_canvas, &ImageCanvas::undoAvailable,
            m_actUndo, &QAction::setEnabled);
    connect(m_canvas, &ImageCanvas::statusMessage,
            this, &MainWindow::onStatusMsg);
    connect(m_canvas, &ImageCanvas::ocrRegionSelected,
            this, &MainWindow::onOcrRegion);

    enableDragDropOnChildren();

    m_actUndo->setEnabled(false);
}

MainWindow::~MainWindow() = default;

void MainWindow::openInitial(const QString& path) {
    openImagePath(path);
}

bool MainWindow::openImagePath(const QString& path) {
    if (path.isEmpty() || !QFileInfo::exists(path))
        return false;
    if (!m_canvas->loadImage(path))
        return false;
    m_lastPath = path;
    return true;
}

QString MainWindow::currentImageDir() const {
    if (m_lastPath.isEmpty()) return QDir::homePath();
    return QFileInfo(m_lastPath).absolutePath();
}

void MainWindow::buildActions() {
    m_actOpen   = new QAction(icon(QStringLiteral("open")),   tr("&Open..."), this);
    m_actSave   = new QAction(icon(QStringLiteral("save")),   tr("&Save"),    this);
    m_actSaveAs = new QAction(icon(QStringLiteral("save-as")),tr("Save &As..."), this);
    m_actUndo   = new QAction(icon(QStringLiteral("undo")),   tr("&Undo"),    this);
    m_actFit    = new QAction(icon(QStringLiteral("fit")),    tr("Fit to &View"), this);

    m_actOpen  ->setShortcut(QKeySequence::Open);
    m_actSave  ->setShortcut(QKeySequence::Save);
    m_actSaveAs->setShortcut(QKeySequence::SaveAs);
    m_actUndo  ->setShortcut(QKeySequence::Undo);
    m_actFit   ->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));

    connect(m_actOpen,   &QAction::triggered, this, &MainWindow::onOpen);
    connect(m_actSave,   &QAction::triggered, this, &MainWindow::onSave);
    connect(m_actSaveAs, &QAction::triggered, this, &MainWindow::onSaveAs);
    connect(m_actUndo,   &QAction::triggered, m_canvas, &ImageCanvas::undo);
    connect(m_actFit,    &QAction::triggered, m_canvas, &ImageCanvas::zoomToFit);

    // Tools
    auto makeTool = [&](const QString& iconName, const QString& tip) {
        QAction* a = new QAction(icon(iconName), tip, this);
        a->setCheckable(true);
        a->setToolTip(tip);
        return a;
    };

    m_actLine   = makeTool(QStringLiteral("line"),   tr("Line"));
    m_actRect   = makeTool(QStringLiteral("rect"),   tr("Rectangle"));
    m_actCircle = makeTool(QStringLiteral("circle"), tr("Circle/Ellipse"));
    m_actArrow  = makeTool(QStringLiteral("arrow"),  tr("Arrow"));
    m_actPencil = makeTool(QStringLiteral("pencil"), tr("Pencil"));
    m_actMosaic = makeTool(QStringLiteral("mosaic"), tr("Mosaic"));
    m_actOcr    = makeTool(QStringLiteral("ocr"),    tr("OCR (extract text)"));
    m_actText   = makeTool(QStringLiteral("text"),   tr("Add Text"));
    m_actCrop   = makeTool(QStringLiteral("crop"),   tr("Crop"));

    m_actLine  ->setData(int(kimg::Tool::Line));
    m_actRect  ->setData(int(kimg::Tool::Rectangle));
    m_actCircle->setData(int(kimg::Tool::Circle));
    m_actArrow ->setData(int(kimg::Tool::Arrow));
    m_actPencil->setData(int(kimg::Tool::Pencil));
    m_actMosaic->setData(int(kimg::Tool::Mosaic));
    m_actOcr   ->setData(int(kimg::Tool::Ocr));
    m_actText  ->setData(int(kimg::Tool::Text));
    m_actCrop  ->setData(int(kimg::Tool::Crop));

    m_toolGroup = new QActionGroup(this);
    m_toolGroup->setExclusive(true);
    for (QAction* a : { m_actLine, m_actRect, m_actCircle, m_actArrow,
                        m_actPencil, m_actMosaic, m_actOcr,
                        m_actText, m_actCrop })
        m_toolGroup->addAction(a);
    connect(m_toolGroup, &QActionGroup::triggered, this, &MainWindow::onToolTriggered);

    m_actColor = new QAction(icon(QStringLiteral("color")), tr("Color"), this);
    connect(m_actColor, &QAction::triggered, this, &MainWindow::onPickColor);

    // Settings actions
    m_actSetTess   = new QAction(icon(QStringLiteral("settings")), tr("Tesseract Path..."), this);
    m_actClearTess = new QAction(tr("Clear Tesseract Override"), this);
    m_actTessInfo  = new QAction(tr("Tesseract Status..."), this);
    connect(m_actSetTess,   &QAction::triggered, this, &MainWindow::onSetTesseractPath);
    connect(m_actClearTess, &QAction::triggered, this, &MainWindow::onClearTesseractPath);
    connect(m_actTessInfo,  &QAction::triggered, this, &MainWindow::onShowTesseractStatus);

    // Menus
    QMenu* mFile = menuBar()->addMenu(tr("&File"));
    mFile->addAction(m_actOpen);
    mFile->addAction(m_actSave);
    mFile->addAction(m_actSaveAs);
    mFile->addSeparator();
    mFile->addAction(tr("E&xit"), this, &QWidget::close);

    QMenu* mEdit = menuBar()->addMenu(tr("&Edit"));
    mEdit->addAction(m_actUndo);

    QMenu* mView = menuBar()->addMenu(tr("&View"));
    mView->addAction(m_actFit);

    QMenu* mSettings = menuBar()->addMenu(tr("&Settings"));
    mSettings->addAction(m_actSetTess);
    mSettings->addAction(m_actClearTess);
    mSettings->addSeparator();
    mSettings->addAction(m_actTessInfo);

    QMenu* mHelp = menuBar()->addMenu(tr("&Help"));
    mHelp->addAction(tr("About"), [this]{
        QMessageBox::about(this, tr("About kImgEdit"),
            tr("<h3>kImgEdit  v1.0</h3>"
               "<p>A lightweight image annotation tool.</p>"
               "<p>Built with Qt %1 and OpenCV.</p>"
               "<p>Hotkeys: <code>Ctrl+O</code> open  <code>Ctrl+S</code> save  "
               "<code>Ctrl+Z</code> undo  <code>Ctrl+0</code> fit  "
               "Mouse wheel: zoom around cursor  Right-drag: pan</p>")
                .arg(QStringLiteral(QT_VERSION_STR)));
    });
}

void MainWindow::buildToolBars() {
    QToolBar* tb = addToolBar(tr("Main"));
    tb->setIconSize(QSize(22, 22));
    tb->setObjectName(QStringLiteral("mainToolbar"));
    tb->setMovable(false);

    tb->addAction(m_actOpen);
    tb->addAction(m_actSave);
    tb->addAction(m_actSaveAs);
    tb->addSeparator();
    tb->addAction(m_actUndo);
    tb->addAction(m_actFit);
    tb->addSeparator();

    tb->addAction(m_actLine);
    tb->addAction(m_actRect);
    tb->addAction(m_actCircle);
    tb->addAction(m_actArrow);
    tb->addAction(m_actPencil);
    tb->addSeparator();

    tb->addAction(m_actMosaic);
    tb->addAction(m_actText);
    tb->addAction(m_actOcr);
    tb->addAction(m_actCrop);
    tb->addSeparator();

    tb->addAction(m_actColor);
    tb->addAction(m_actSetTess);

    // Width selector
    auto* lblW = new QLabel(QStringLiteral("  ") + tr("Width:"));
    tb->addWidget(lblW);
    m_spinWidth = new QSpinBox(this);
    m_spinWidth->setRange(1, 60);
    m_spinWidth->setValue(m_canvas->strokeWidth());
    m_spinWidth->setSuffix(QStringLiteral(" px"));
    connect(m_spinWidth, qOverload<int>(&QSpinBox::valueChanged),
            m_canvas, [this](int v){ m_canvas->setStrokeWidth(v); });
    tb->addWidget(m_spinWidth);

    // Mosaic block size
    auto* lblM = new QLabel(QStringLiteral("  ") + tr("Mosaic:"));
    tb->addWidget(lblM);
    m_spinMosaic = new QSpinBox(this);
    m_spinMosaic->setRange(2, 80);
    m_spinMosaic->setValue(m_canvas->mosaicBlock());
    m_spinMosaic->setSuffix(QStringLiteral(" px"));
    connect(m_spinMosaic, qOverload<int>(&QSpinBox::valueChanged),
            m_canvas, [this](int v){ m_canvas->setMosaicBlock(v); });
    tb->addWidget(m_spinMosaic);

    // Font size
    auto* lblF = new QLabel(QStringLiteral("  ") + tr("Font:"));
    tb->addWidget(lblF);
    m_spinFont = new QSpinBox(this);
    m_spinFont->setRange(8, 200);
    m_spinFont->setValue(m_canvas->textFontSize());
    m_spinFont->setSuffix(QStringLiteral(" px"));
    connect(m_spinFont, qOverload<int>(&QSpinBox::valueChanged),
            m_canvas, [this](int v){ m_canvas->setTextFontSize(v); });
    tb->addWidget(m_spinFont);
}

void MainWindow::buildStatusBar() {
    m_lblPath  = new QLabel(QStringLiteral("—"), this);
    m_lblSize  = new QLabel(QStringLiteral("—"), this);
    m_lblPos   = new QLabel(QStringLiteral("—"), this);
    m_lblScale = new QLabel(QStringLiteral("100%"), this);

    m_lblPath ->setMinimumWidth(200);
    m_lblSize ->setMinimumWidth(120);
    m_lblPos  ->setMinimumWidth(220);
    m_lblScale->setMinimumWidth(80);

    statusBar()->addPermanentWidget(m_lblPath, 1);
    statusBar()->addPermanentWidget(m_lblSize);
    statusBar()->addPermanentWidget(m_lblPos);
    statusBar()->addPermanentWidget(m_lblScale);
}

void MainWindow::applyStyle() {
    // A subtle modern flat look. Uses Qt stylesheets only.
    static const char* kQss = R"(
        QMainWindow { background: #1f2228; }
        QMenuBar    { background: #2a2f37; color: #e4e7ea; }
        QMenuBar::item:selected { background: #3a4250; }
        QMenu       { background: #2a2f37; color: #e4e7ea; border: 1px solid #3a4250; }
        QMenu::item:selected   { background: #3a4250; }
        QToolBar    { background: #2a2f37; border: 0; padding: 4px; spacing: 4px; }
        QToolButton { padding: 4px; border-radius: 6px; color: #e4e7ea; }
        QToolButton:hover    { background: #3a4250; }
        QToolButton:checked  { background: #4a6fa5; color: #ffffff; }
        QStatusBar  { background: #2a2f37; color: #d0d3d8; }
        QLabel      { color: #d0d3d8; }
        QSpinBox    {
            background: #1f2228; color: #e4e7ea;
            border: 1px solid #3a4250; border-radius: 4px;
            padding: 2px 6px; min-width: 56px;
        }
        QSpinBox::up-button, QSpinBox::down-button { width: 14px; }
        QToolTip    {
            background-color: #2a2f37; color: #e4e7ea;
            border: 1px solid #4a6fa5; padding: 4px;
        }
    )";
    setStyleSheet(QString::fromLatin1(kQss));
}

void MainWindow::enableDragDropOnChildren() {
    setAcceptDrops(true);
    installEventFilter(this);

    const auto widgets = findChildren<QWidget*>();
    for (QWidget* w : widgets) {
        w->setAcceptDrops(true);
        w->installEventFilter(this);
    }
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event) {
    switch (event->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        auto* e = static_cast<QDragMoveEvent*>(event);
        if (ImageCanvas::imagePathFromMime(e->mimeData()).isEmpty())
            break;
        e->acceptProposedAction();
        const bool onCanvas = (watched == m_canvas);
        m_canvas->setDropHighlight(onCanvas);
        return true;
    }
    case QEvent::DragLeave:
        m_canvas->setDropHighlight(false);
        break;
    case QEvent::Drop: {
        auto* e = static_cast<QDropEvent*>(event);
        const QString path = ImageCanvas::imagePathFromMime(e->mimeData());
        m_canvas->setDropHighlight(false);
        if (path.isEmpty())
            break;
        if (openImagePath(path)) {
            e->acceptProposedAction();
            statusBar()->showMessage(tr("Opened: %1").arg(path), 3000);
            return true;
        }
        break;
    }
    default:
        break;
    }
    return QMainWindow::eventFilter(watched, event);
}

void MainWindow::onOpen() {
    QString path = QFileDialog::getOpenFileName(
        this, tr("Open Image"), currentImageDir(),
        tr("Images (*.png *.jpg *.jpeg *.bmp *.tif *.tiff *.webp);;All files (*.*)"));
    if (path.isEmpty()) return;
    openImagePath(path);
}

void MainWindow::onSave() {
    if (!m_canvas->hasImage()) return;
    if (m_lastPath.isEmpty()) { onSaveAs(); return; }
    if (!m_canvas->saveImage(m_lastPath))
        QMessageBox::warning(this, tr("Save failed"), tr("Could not save to %1").arg(m_lastPath));
    else
        statusBar()->showMessage(tr("Saved: %1").arg(m_lastPath), 3000);
}

void MainWindow::onSaveAs() {
    if (!m_canvas->hasImage()) return;
    QString suggestion = m_lastPath.isEmpty()
        ? QDir(currentImageDir()).filePath(QStringLiteral("untitled.png"))
        : QFileInfo(m_lastPath).absolutePath() + QStringLiteral("/")
              + QFileInfo(m_lastPath).completeBaseName()
              + QStringLiteral("_edit.png");
    QString path = QFileDialog::getSaveFileName(
        this, tr("Save Image As"), suggestion,
        tr("PNG (*.png);;JPEG (*.jpg *.jpeg);;BMP (*.bmp);;All files (*.*)"));
    if (path.isEmpty()) return;
    if (!m_canvas->saveImage(path)) {
        QMessageBox::warning(this, tr("Save failed"), tr("Could not save to %1").arg(path));
        return;
    }
    m_lastPath = path;
    statusBar()->showMessage(tr("Saved: %1").arg(path), 3000);
    onImageLoaded(m_canvas->image().size());
}

void MainWindow::onPickColor() {
    QColor c = QColorDialog::getColor(m_canvas->strokeColor(), this, tr("Pick stroke color"));
    if (!c.isValid()) return;
    m_canvas->setStrokeColor(c);
    QPixmap pm(20, 20);
    pm.fill(c);
    m_actColor->setIcon(QIcon(pm));
}

void MainWindow::onToolTriggered(QAction* a) {
    if (!a) return;
    auto t = static_cast<kimg::Tool>(a->data().toInt());
    m_canvas->setTool(t);
    statusBar()->showMessage(tr("Tool: %1").arg(kimg::toolName(t)), 2000);
}

void MainWindow::onCursorPos(const QPoint& p, const QColor& c) {
    if (p.x() < 0) {
        m_lblPos->setText(QStringLiteral("—"));
    } else {
        m_lblPos->setText(tr("Pos: (%1, %2)  RGBA(%3,%4,%5,%6)")
                              .arg(p.x()).arg(p.y())
                              .arg(c.red()).arg(c.green()).arg(c.blue()).arg(c.alpha()));
    }
}

void MainWindow::onImageLoaded(const QSize& s) {
    if (m_lastPath.isEmpty())
        m_lblPath->setText(QStringLiteral("—"));
    else
        m_lblPath->setText(m_lastPath);
    m_lblSize->setText(tr("Image: %1 x %2").arg(s.width()).arg(s.height()));
    setWindowTitle(QStringLiteral("kImgEdit — %1")
                       .arg(m_lastPath.isEmpty() ? tr("(unsaved)") : QFileInfo(m_lastPath).fileName()));
}

void MainWindow::onScaleChanged(double s) {
    m_lblScale->setText(QStringLiteral("%1%").arg(int(s * 100.0)));
}

void MainWindow::onStatusMsg(const QString& m) {
    statusBar()->showMessage(m, 4000);
}

void MainWindow::onSetTesseractPath() {
    // Suggest the currently resolved path (if any) as starting location
    QString current = OcrService::userTesseractPath();
    if (current.isEmpty()) current = OcrService::findTesseract();

    QString startDir;
    if (!current.isEmpty()) {
        startDir = QFileInfo(current).absolutePath();
    } else {
#ifdef Q_OS_WIN
        startDir = QStringLiteral("C:/Program Files/Tesseract-OCR");
        if (!QDir(startDir).exists()) startDir = QStringLiteral("C:/");
#endif
    }

    QString path = QFileDialog::getOpenFileName(
        this, tr("Select tesseract.exe"), startDir,
#ifdef Q_OS_WIN
        tr("Executable (tesseract.exe);;All files (*.*)")
#else
        tr("Executable (tesseract);;All files (*)")
#endif
    );
    if (path.isEmpty()) return;

    if (!QFileInfo(path).isExecutable() && !QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("Invalid path"),
                             tr("Not a valid executable: %1").arg(path));
        return;
    }

    // Probe by running "<exe> --version"
    QProcess proc;
    proc.start(path, { QStringLiteral("--version") });
    proc.waitForFinished(4000);
    QString verOut = QString::fromLocal8Bit(proc.readAllStandardOutput())
                     + QString::fromLocal8Bit(proc.readAllStandardError());
    if (proc.exitStatus() != QProcess::NormalExit
        || !verOut.contains(QStringLiteral("tesseract"), Qt::CaseInsensitive))
    {
        auto ret = QMessageBox::question(
            this, tr("Cannot verify Tesseract"),
            tr("Could not run '%1 --version' successfully.\n\nUse it anyway?")
                .arg(path),
            QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
        if (ret != QMessageBox::Yes) return;
    }

    OcrService::setUserTesseractPath(path);
    statusBar()->showMessage(
        tr("Tesseract set: %1").arg(path), 5000);

    QString ver = verOut.section(QChar('\n'), 0, 0).trimmed();
    QMessageBox::information(this, tr("Tesseract configured"),
        tr("<b>Saved.</b><br><br>Path: <code>%1</code><br>Version: %2")
            .arg(path)
            .arg(ver.isEmpty() ? tr("(unknown)") : ver));
}

void MainWindow::onClearTesseractPath() {
    QString cur = OcrService::userTesseractPath();
    if (cur.isEmpty()) {
        QMessageBox::information(this, tr("Tesseract"),
            tr("No user override is set."));
        return;
    }
    auto ret = QMessageBox::question(this, tr("Clear override?"),
        tr("Remove the saved Tesseract path?\n\nCurrent: %1").arg(cur),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (ret == QMessageBox::Yes) {
        OcrService::setUserTesseractPath(QString());
        statusBar()->showMessage(tr("Tesseract override cleared"), 4000);
    }
}

void MainWindow::onShowTesseractStatus() {
    const QString resolved = OcrService::findTesseract();
    const QString source   = OcrService::tesseractSource();
    const QString user     = OcrService::userTesseractPath();

    QString version = tr("(not probed)");
    if (!resolved.isEmpty()) {
        QProcess proc;
        proc.start(resolved, { QStringLiteral("--version") });
        if (proc.waitForFinished(4000)) {
            QString out = QString::fromLocal8Bit(proc.readAllStandardOutput())
                        + QString::fromLocal8Bit(proc.readAllStandardError());
            version = out.section(QChar('\n'), 0, 0).trimmed();
            if (version.isEmpty()) version = tr("(unknown)");
        }
    }

    QString html;
    html += QStringLiteral("<h3>%1</h3>").arg(tr("Tesseract Status"));
    if (resolved.isEmpty()) {
        html += QStringLiteral("<p><b style='color:#c64'>%1</b></p>")
                    .arg(tr("Tesseract not found."));
    } else {
        html += QStringLiteral("<p><b style='color:#4a8'>%1</b></p>")
                    .arg(tr("Available."));
        html += QStringLiteral("<p>%1 <code>%2</code></p>")
                    .arg(tr("Path:")).arg(resolved);
        html += QStringLiteral("<p>%1 %2</p>")
                    .arg(tr("Source:")).arg(source);
        html += QStringLiteral("<p>%1 %2</p>")
                    .arg(tr("Version:")).arg(version);
    }
    if (!user.isEmpty()) {
        html += QStringLiteral("<hr><p>%1 <code>%2</code></p>")
                    .arg(tr("User override:")).arg(user);
    }
    QMessageBox box(this);
    box.setWindowTitle(tr("Tesseract Status"));
    box.setTextFormat(Qt::RichText);
    box.setText(html);
    box.setIcon(resolved.isEmpty() ? QMessageBox::Warning : QMessageBox::Information);
    box.exec();
}

void MainWindow::onOcrRegion(QRect r) {
    if (!m_canvas->hasImage() || r.isEmpty()) return;
    QImage roi = m_canvas->image().copy(r);

    statusBar()->showMessage(tr("Running OCR..."), 2000);
    QApplication::setOverrideCursor(Qt::WaitCursor);
    OcrService::Result res = m_ocr->recognize(roi);
    QApplication::restoreOverrideCursor();

    QDialog dlg(this);
    dlg.setWindowTitle(res.ok ? tr("OCR Result") : tr("OCR"));
    dlg.resize(560, 360);
    auto* lay = new QVBoxLayout(&dlg);
    auto* edit = new QTextEdit(&dlg);
    edit->setReadOnly(false);
    if (res.ok) {
        edit->setPlainText(res.text);
    } else {
        edit->setPlainText(res.error);
    }
    lay->addWidget(edit);

    auto* box = new QDialogButtonBox(QDialogButtonBox::Ok, &dlg);
    auto* btnCopy = box->addButton(tr("Copy"), QDialogButtonBox::ActionRole);
    connect(btnCopy, &QPushButton::clicked, &dlg, [edit]{
        QGuiApplication::clipboard()->setText(edit->toPlainText());
    });
    connect(box, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    lay->addWidget(box);
    dlg.exec();
}
