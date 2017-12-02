#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "version.h"

#include "cpp-move-file-to-trash/move-file-to-trash.h"

#include <QSettings>
#include <QTimer>
#include <QListWidget>
#include <QFileDialog>
#include <QDirIterator>
#include <QDockWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QGroupBox>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QLabel>
#include <QInputDialog>
#include <QColorDialog>
#include <QFuture>
#include <QtConcurrent/QtConcurrentRun>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QBuffer>
#include <QKeyEvent>
#include <QtUiTools>
#include <assert.h>

namespace {
    const char* companyName = "Tomaattinen";
    const char* applicationName = "anno";
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    recentFoldersMenu = new QMenu(tr("&Recent folders"), this);
    ui->menuFile->insertMenu(ui->actionExit, recentFoldersMenu);

    setWindowTitle("anno");

    const QSettings settings(companyName, applicationName);
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());
    reverseFileOrder = settings.value("reverseFileOrder").toBool();

    connect(ui->actionOpenFolder, SIGNAL(triggered()), this, SLOT(onOpenFolder()));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionUndo, SIGNAL(triggered()), this, SLOT(onUndo()));
    connect(ui->actionRedo, SIGNAL(triggered()), this, SLOT(onRedo()));
    connect(ui->actionAbout, SIGNAL(triggered()), this, SLOT(onAbout()));

    QStringList recentFolders = settings.value("recentFolders").toStringList();
    for (int i = recentFolders.size() - 1; i >= 0; --i) { // load in reverse order
        addRecentFolderMenuItem(recentFolders[i]);
    }

    QTimer::singleShot(0, this, SLOT(init()));
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    saveMaskIfDirty();

    QSettings settings(companyName, applicationName);
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    settings.setValue("reverseFileOrder", reverseFileOrder);

    if (markingRadius) {
        settings.setValue("markingRadius", markingRadius->value());
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::init()
{
    initImageIO();
    createToolList();
    createFileList();
    createImageView();

    image->setMarkingRadius(markingRadius->value());

    const QSettings settings(companyName, applicationName);
    const QString defaultDirectory = settings.value("defaultDirectory").toString();
    if (!defaultDirectory.isEmpty()) {
        openFolder(defaultDirectory);
    }
    else {
        onOpenFolder();
    }

    const bool geometryRestored = restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    const bool stateRestored = restoreState(settings.value("mainWindowState").toByteArray());

    if (!geometryRestored && !stateRestored) {
        showMaximized();

        if (files->count() > 0) {
            QListWidgetItem* firstFile = files->item(0);
            firstFile->setSelected(true);
            onFileClicked(firstFile);
        }
    }

    setFocusPolicy(Qt::StrongFocus);
}

void MainWindow::initImageIO()
{
    // see https://stackoverflow.com/a/26809360/19254
    QImage image(10, 10, QImage::Format_RGB32);
    QBuffer buffer;
    buffer.open(QBuffer::WriteOnly);
    image.save(&buffer, "JPG", 85);
    buffer.close();
}

void MainWindow::createImageView()
{
    image = new QResultImageView(this);
    onYardstickVisible(true);

    QPixmap bucketCursorPixmap = QPixmap(":/resources/cursor_bucket.png");
    QCursor bucketCursor(bucketCursorPixmap, 7, 22);
    image->setBucketCursor(bucketCursor);

    setCentralWidget(image);

    connect(image, SIGNAL(maskUpdating()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(maskUpdated()), this, SLOT(onMaskUpdated()));
    connect(image, SIGNAL(panned()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(zoomed()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(newMarkingRadius(int)), this, SLOT(onNewMarkingRadius(int)));
    connect(image, SIGNAL(annotationsVisible(bool)), this, SLOT(onAnnotationsVisible(bool)));
}

void MainWindow::createFileList()
{
    QDockWidget* fileListDockWidget = new QDockWidget(tr("Files"), this);
    fileListDockWidget->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable); // no close button
    fileListDockWidget->setObjectName("Files");

    files = new QListWidget(this);

    fileListDockWidget->setWidget(files);
    addDockWidget(Qt::LeftDockWidgetArea, fileListDockWidget);

    files->setFont(QFont("Arial", 8, 0));

    QStringList columns;
    columns.append(tr("Name"));

    QListWidgetItem* headerItem = new QListWidgetItem(files);
    headerItem->setTextAlignment(Qt::AlignLeft);

    connect(files, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onFileClicked(QListWidgetItem*)));
    connect(files, SIGNAL(activated(const QModelIndex&)), this, SLOT(onFileActivated(const QModelIndex&)));
}

void MainWindow::createToolList()
{
    const QSettings settings(companyName, applicationName);

    QDockWidget* dockWidget = new QDockWidget(tr("Tools"), this);
    dockWidget->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable); // no close button
    dockWidget->setObjectName("Tools");

    QWidget* widget = new QWidget(this);

    dockWidget->setWidget(widget);
    addDockWidget(Qt::RightDockWidgetArea, dockWidget);

    QVBoxLayout* layout = new QVBoxLayout(widget);
    layout->setSpacing(0);

    {
        markingsVisible = new QCheckBox("Annotations &visible", this);
        markingsVisible->setChecked(true);

        connect(markingsVisible, SIGNAL(toggled(bool)), this, SLOT(onMarkingsVisible(bool)));
    }

    bool radiusOk = false;
    int radius = settings.value("markingRadius").toInt(&radiusOk);
    if (!radiusOk) {
        radius = 10;
    }

    QWidget* markingRadiusWidget = new QWidget(this);
    {
        markingRadius = new QSpinBox(this);
        markingRadius->setMinimum(1);
        markingRadius->setMaximum(100);
        markingRadius->setValue(radius);
        markingRadius->setToolTip(tr("Tip: You can change the radius quickly by Ctrl + mouse wheel, when in annotation mode."));

        connect(markingRadius, SIGNAL(valueChanged(int)), this, SLOT(onMarkingRadiusChanged(int)));

        QHBoxLayout* markingRadiusLayout = new QHBoxLayout(markingRadiusWidget);
        markingRadiusLayout->addWidget(new QLabel(tr("Annotation radius"), this));
        markingRadiusLayout->addWidget(markingRadius);
    }

    QWidget* classButtonsWidget = new QWidget(this);
    {
        addClassButton = new QPushButton(tr("Add new class ..."), this);
        connect(addClassButton, SIGNAL(clicked()), this, SLOT(onAddClass()));

        renameClassButton = new QPushButton(tr("Rename selected class ..."), this);
        connect(renameClassButton, SIGNAL(clicked()), this, SLOT(onRenameClass()));

        removeClassButton = new QPushButton(tr("Remove selected class ..."), this);
        connect(removeClassButton, SIGNAL(clicked()), this, SLOT(onRemoveClass()));

        QVBoxLayout* classButtonsLayout = new QVBoxLayout(classButtonsWidget);
        classButtonsLayout->addWidget(addClassButton);
        classButtonsLayout->addWidget(renameClassButton);
        classButtonsLayout->addWidget(removeClassButton);
    }

    QGroupBox* leftMouseButtonActions = new QGroupBox(tr("Left mouse button actions"));
    {
        QGridLayout* leftMouseButtonActionsLayout = new QGridLayout;

        panButton = new QRadioButton(tr("&Pan"));
        annotateButton = new QRadioButton(tr("&Annotate"));
        bucketFillCheckbox = new QCheckBox(tr("&Bucket fill"), this);
        eraseAnnotationsButton = new QRadioButton(tr("&Erase annotations"));

        annotationClasses = new QListWidget(this);
        annotationClasses->setFont(QFont("Arial", 10, 0));

        panButton->setChecked(true);
        bucketFillCheckbox->setEnabled(false);

        int row = 0;
        leftMouseButtonActionsLayout->addWidget(panButton, row++, 0, 1, 2);
        leftMouseButtonActionsLayout->addWidget(annotateButton, row, 0, 1, 1);
        leftMouseButtonActionsLayout->addWidget(bucketFillCheckbox, row++, 1, 1, 1);
        leftMouseButtonActionsLayout->addWidget(annotationClasses, row++, 0, 1, 2);
        leftMouseButtonActionsLayout->addWidget(classButtonsWidget, row++, 0, 1, 2);
        leftMouseButtonActionsLayout->addWidget(eraseAnnotationsButton, row++, 0, 1, 2);

        connect(panButton, SIGNAL(toggled(bool)), this, SLOT(onPanButtonToggled(bool)));
        connect(eraseAnnotationsButton, SIGNAL(toggled(bool)), this, SLOT(onEraseAnnotationsButtonToggled(bool)));
        connect(annotateButton, SIGNAL(toggled(bool)), this, SLOT(onAnnotateButtonToggled(bool)));
        connect(bucketFillCheckbox, SIGNAL(toggled(bool)), this, SLOT(onBucketFillToggled(bool)));
        connect(annotationClasses, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onAnnotationClassClicked(QListWidgetItem*)));

        leftMouseButtonActions->setLayout(leftMouseButtonActionsLayout);
    }

    QGroupBox* rightMouseButtonActions = new QGroupBox(tr("Right mouse button actions"));
    {
        QGridLayout* rightMouseButtonActionsLayout = new QGridLayout;

        rightMousePanButton = new QRadioButton(tr("Pa&n"));
        rightMouseEraseAnnotationsButton = new QRadioButton(tr("Erase anno&tations"));
        rightMouseResetViewButton = new QRadioButton(tr("Reset vie&w"));

        rightMouseResetViewButton->setChecked(true);

        int row = 0;
        rightMouseButtonActionsLayout->addWidget(rightMousePanButton, row++, 0, 1, 1);
        rightMouseButtonActionsLayout->addWidget(rightMouseEraseAnnotationsButton, row++, 0, 1, 1);
        rightMouseButtonActionsLayout->addWidget(rightMouseResetViewButton, row++, 0, 1, 1);

        connect(rightMousePanButton, SIGNAL(toggled(bool)), this, SLOT(onRightMousePanButtonToggled(bool)));
        connect(rightMouseEraseAnnotationsButton, SIGNAL(toggled(bool)), this, SLOT(onRightMouseEraseAnnotationsButtonToggled(bool)));
        connect(rightMouseResetViewButton, SIGNAL(toggled(bool)), this, SLOT(onRightMouseResetViewButtonToggled(bool)));

        rightMouseButtonActions->setLayout(rightMouseButtonActionsLayout);
    }

    {
        resultsVisible = new QCheckBox("&Results visible", this);
        resultsVisible->setChecked(true);

        connect(resultsVisible, SIGNAL(toggled(bool)), this, SLOT(onResultsVisible(bool)));
    }

    {
        yardstickVisible = new QCheckBox("&Yardstick visible", this);
        yardstickVisible->setChecked(true);

        connect(yardstickVisible, SIGNAL(toggled(bool)), this, SLOT(onYardstickVisible(bool)));
    }

    layout->addWidget(markingsVisible);
    layout->addWidget(markingRadiusWidget);
    layout->addWidget(leftMouseButtonActions);
    layout->addSpacing(10);
    layout->addWidget(rightMouseButtonActions);
    layout->addSpacing(10);
    layout->addWidget(resultsVisible);
    layout->addSpacing(10);
    layout->addWidget(yardstickVisible);
}

void MainWindow::onOpenFolder()
{
    QSettings settings(companyName, applicationName);
    QString defaultDirectory = settings.value("defaultDirectory").toString();
    if (defaultDirectory.isEmpty()) {
#ifdef _WIN32
        defaultDirectory = "C:\\";
#endif
    }
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select a folder containing some images"), defaultDirectory,
                                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        settings.setValue("defaultDirectory", dir);
        openFolder(dir);
    }
}

void MainWindow::onOpenRecentFolder()
{
    const QAction *action = qobject_cast<QAction *>(sender());
    if (action) {
        const QString dir = action->data().toString();
        if (!dir.isEmpty()) {
            QSettings settings(companyName, applicationName);
            settings.setValue("defaultDirectory", dir);
            openFolder(dir);
        }
    }
}

void MainWindow::openFolder(const QString& dir)
{
    saveMaskIfDirty();

    if (!files) {
        createFileList();
    }

    files->clear();
    currentImageFileItem = nullptr;

    resetUndoBuffers();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    image->setImage(QImage());
    image->resetZoomAndPan();

    const QString maskFilenameSuffix = getMaskFilenameSuffix();
    const QString inferenceResultFilenameSuffix = getInferenceResultFilenameSuffix();

    QDirIterator it(dir, QStringList() << "*.jpg" << "*.jpeg" << "*.png", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        QString filename = it.next();
        const auto isMaskFilename = [&]() { return filename.right(maskFilenameSuffix.length()) == maskFilenameSuffix; };
        const auto isInferenceResultFilename = [&]() { return filename.right(inferenceResultFilenameSuffix.length()) == inferenceResultFilenameSuffix; };
        if (!isMaskFilename() && !isInferenceResultFilename()) {
            QListWidgetItem* item = new QListWidgetItem(filename, files);
            QFileInfo maskFileInfo(getMaskFilename(filename));
            if (maskFileInfo.exists() && maskFileInfo.isFile()) {
                item->setTextColor(Qt::black);
            }
            else {
                item->setTextColor(Qt::gray);
            }
        }
    }

    files->sortItems(reverseFileOrder ? Qt::DescendingOrder : Qt::AscendingOrder);

    currentWorkingFolder = dir;

    addRecentFolderMenuItem(dir);

    loadClassList();

    if (annotationClassItems.empty()) {
        // Add sample classes
        addNewClass("Clean",        QColor(0,   255, 0,  64));
        addNewClass("Minor defect", QColor(255, 255, 0, 128));
        addNewClass("Major defect", QColor(255, 0,   0, 128));
    }

    QApplication::restoreOverrideCursor();
}

void MainWindow::addRecentFolderMenuItem(const QString& dir)
{
    QList<QAction*> currentRecentFoldersActions = recentFoldersMenu->actions();

    // First see if we already have it
    for (QAction* currentAction : currentRecentFoldersActions) {
        if (currentAction->data().toString() == dir) {
            QAction* firstAction = currentRecentFoldersActions.front();
            if (currentAction != firstAction) {
                recentFoldersMenu->removeAction(currentAction);
                recentFoldersMenu->insertAction(firstAction, currentAction);
            }
            return;
        }
    }

    // Decide where to insert the new item
    QAction* newRecentFolderAction = nullptr;
    if (!currentRecentFoldersActions.isEmpty()) {
        newRecentFolderAction = new QAction(dir, recentFoldersMenu);
        recentFoldersMenu->insertAction(currentRecentFoldersActions.front(), newRecentFolderAction);
    }
    else {
        newRecentFolderAction = recentFoldersMenu->addAction(dir);
    }

    newRecentFolderAction->setData(dir);
    connect(newRecentFolderAction, SIGNAL(triggered()), this, SLOT(onOpenRecentFolder()));

    // Remove old items, if applicable
    const int maxRecentFolders = 10;
    while (currentRecentFoldersActions.size() >= maxRecentFolders) {
        QAction* actionToRemove = currentRecentFoldersActions.back();
        disconnect(actionToRemove, SIGNAL(triggered()), this, SLOT(onOpenRecentFolder()));
        recentFoldersMenu->removeAction(actionToRemove);
        delete actionToRemove;
        currentRecentFoldersActions.pop_back();
    }

    saveRecentFolders();
}

void MainWindow::saveRecentFolders()
{
    QSettings settings(companyName, applicationName);
    QStringList recentFolders;
    for (QAction* action : recentFoldersMenu->actions()) {
        const QString dir = action->data().toString();
        recentFolders.append(dir);
    }
    settings.setValue("recentFolders", recentFolders);
}

void MainWindow::onFileClicked(QListWidgetItem* item)
{
    loadFile(item);
}

void MainWindow::onFileActivated(const QModelIndex& index)
{
    loadFile(files->item(index.row()));
}

void MainWindow::loadFile(QListWidgetItem* item)
{
    saveMaskIfDirty();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    currentImageFileItem = item;
    currentImageFile = item->text();

    resetUndoBuffers();

    const auto readImage = [](const QString& filename) { return QImage(filename); };

    QFuture<QImage> imageFuture = QtConcurrent::run(readImage, currentImageFile);
    QFuture<QImage> maskFuture = QtConcurrent::run(readImage, getMaskFilename(currentImageFile));

    const auto readResults = [](const QString& filename) {
        InferenceResults results;

        QFile file;
        file.setFileName(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            return results;
        }

        if (file.size() > 100e6) {
            results.error = tr("The inference results JSON file is insanely large, so we're really not even trying to parse it.\n\nFor your reference, the file is:\n%1").arg(filename);
            return results;
        }

        QString json = file.readAll();
        QJsonDocument document = QJsonDocument::fromJson(json.toUtf8());

        const QJsonArray colors = document.array();

        for (int i = 0, end = colors.size(); i < end; ++i) {
            const QJsonObject colorAndPaths = colors[i].toObject();
            QResultImageView::Result result;
            const QJsonObject color = colorAndPaths.value("color").toObject();

            result.pen = QPen(QColor(
                color.value("r").toInt(),
                color.value("g").toInt(),
                color.value("b").toInt()
            ));

            const QJsonArray paths = colorAndPaths.value("color_paths").toArray();
            results.results.reserve(paths.size());

            for (int j = 0, end = paths.size(); j < end; ++j) {
                const QJsonArray path = paths[j].toArray();
                result.contour.reserve(path.size());
                for (int k = 0, end = path.size(); k < end; ++k) {
                    const QJsonObject point = path[k].toObject();
                    result.contour.push_back(QPointF(point.value("x").toDouble(), point.value("y").toDouble()));

                }
                results.results.push_back(result);
                result.contour.clear();
            }
        }

        return results;
    };

    auto resultsFuture = QtConcurrent::run(readResults, getInferenceResultPathFilename(currentImageFile));

    {
        QImage mask = maskFuture.result();
        currentMask = QPixmap::fromImage(mask);

        QResultImageView::DelayedRedrawToken delayedRedrawToken;

        image->setImage(imageFuture.result(), &delayedRedrawToken);
        image->setMask(mask, &delayedRedrawToken);

        currentResults = resultsFuture.result();

        if (!currentResults.error.isEmpty()) {
            QMessageBox::warning(nullptr, tr("Error"), currentResults.error);
        }

        if (resultsVisible->isChecked()) {
            image->setResults(currentResults.results, &delayedRedrawToken);
        }
        resultsVisible->setEnabled(!currentResults.results.empty());
    }

    QApplication::restoreOverrideCursor();
}

void MainWindow::onPanButtonToggled(bool toggled)
{
    if (toggled) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Pan);
        bucketFillCheckbox->setChecked(false); // Disable bucket fill, just to prevent accidents
    }

    bucketFillCheckbox->setEnabled(!toggled);
}

void MainWindow::onAnnotateButtonToggled(bool toggled)
{
    if (toggled) {
        if (annotationClasses->count() == 0) {
            QMessageBox::warning(this, tr("Error"), tr("Add a class first"));
        }
        else {
            if (currentlySelectedAnnotationClassItem == nullptr) {
                QListWidgetItem* firstItem = annotationClasses->item(0);
                firstItem->setSelected(true);
                onAnnotationClassClicked(firstItem);
            }

            image->setLeftMouseMode(QResultImageView::LeftMouseMode::Annotate);
        }
    }
}

void MainWindow::onBucketFillToggled(bool toggled)
{
    image->setFloodFillMode(toggled);
}

void MainWindow::onEraseAnnotationsButtonToggled(bool toggled)
{
    if (toggled) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::EraseAnnotations);
    }
}

void MainWindow::onAnnotationClassClicked(QListWidgetItem* item)
{
    annotateButton->setChecked(true);

    for (const ClassItem& classItem : annotationClassItems) {
        if (item == classItem.listWidgetItem) {
            currentlySelectedAnnotationClassItem = item;
            currentAnnotationColor = classItem.color;
            image->setAnnotationColor(currentAnnotationColor);
            return;
        }
    }

    panButton->setChecked(true);
    QMessageBox::warning(this, tr("Error"), tr("No annotation class item found"));
}

void MainWindow::onMarkingRadiusChanged(int i)
{
    image->setMarkingRadius(i);
}

void MainWindow::onMarkingsVisible(bool toggled)
{
    onPostponeMaskUpdate();

    image->setMaskVisible(toggled);
}

void MainWindow::onRightMousePanButtonToggled(bool toggled)
{
    if (toggled) {
        image->setRightMouseMode(QResultImageView::RightMouseMode::Pan);
    }
}

void MainWindow::onRightMouseEraseAnnotationsButtonToggled(bool toggled)
{
    if (toggled) {
        image->setRightMouseMode(QResultImageView::RightMouseMode::EraseAnnotations);
    }
}

void MainWindow::onRightMouseResetViewButtonToggled(bool toggled)
{
    if (toggled) {
        image->setRightMouseMode(QResultImageView::RightMouseMode::ResetView);
    }
}

void MainWindow::onResultsVisible(bool toggled)
{
    onPostponeMaskUpdate();

    if (toggled) {
        image->setResults(currentResults.results);
    }
    else {
        std::vector<QResultImageView::Result> emptyResults;
        image->setResults(emptyResults);
    }
}

void MainWindow::onYardstickVisible(bool toggled)
{
    if (toggled) {
        image->setPixelSize(1, "px", false);
    }
    else {
        image->setPixelSize(std::numeric_limits<double>::quiet_NaN(), "", false);
    }
}

void MainWindow::onMaskUpdated()
{
    maskDirty = true;

    ++saveMaskPendingCounter;

    QTimer::singleShot(10000, this, SLOT(onSaveMask()));

    if (currentImageFileItem != nullptr) {
        currentImageFileItem->setTextColor(Qt::black); // now we will have a mask file
    }

    maskUndoBuffer.push_back(currentMask);
    limitUndoOrRedoBufferSize(maskUndoBuffer);

    currentMask = image->getMask();

    maskRedoBuffer.clear();

    updateUndoRedoMenuItemStatus();
}

void MainWindow::onPostponeMaskUpdate()
{
    if (maskDirty) {
        ++saveMaskPendingCounter;
        QTimer::singleShot(10000, this, SLOT(onSaveMask()));
    }
}

void MainWindow::onSaveMask()
{
    if (saveMaskPendingCounter > 0) {
        Q_ASSERT(maskDirty);

        --saveMaskPendingCounter;

        if (saveMaskPendingCounter == 0) {
            saveMask();
        }
    }
}

void MainWindow::saveMaskIfDirty()
{
    if (maskDirty) {
        saveMask();
    }
}

void MainWindow::saveMask()
{
    assert(maskDirty);

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    QFile file(getMaskFilename(currentImageFile));
    file.open(QIODevice::WriteOnly);
    image->getMask().save(&file, "PNG");

    QApplication::restoreOverrideCursor();

    maskDirty = false;
    saveMaskPendingCounter = 0;
}

QString MainWindow::getMaskFilenameSuffix()
{
    return "_mask.png";
}

QString MainWindow::getMaskFilename(const QString& baseImageFilename)
{
    return baseImageFilename + getMaskFilenameSuffix();
}

QString MainWindow::getInferenceResultFilenameSuffix()
{
    return "_result.png";
}

QString MainWindow::getInferenceResultPathFilename(const QString& baseImageFilename)
{
    return baseImageFilename + "_result_path.json";
}

void MainWindow::onAddClass()
{
    if (currentWorkingFolder.isEmpty()) {
        QMessageBox::warning(this, "Error", "Open some folder first");
        return;
    }

    bool ok = false;
    QString newClass = QInputDialog::getText(this, tr("Add new class"), tr("Enter the name of the new class"), QLineEdit::Normal, QString(), &ok);

    if (ok) {
        if (newClass.isEmpty()) {
            QMessageBox::critical(this, tr("Error"), tr("The class name cannot be empty."));
        }
        else {
            const QColor color = QColorDialog::getColor(Qt::white, this, tr("Pick the color of the new class \"%1\"").arg(newClass), QColorDialog::ShowAlphaChannel);
            if (color.isValid()) {
                addNewClass(newClass, color);
                saveClassList();
            }
        }
    }
}

void MainWindow::onRenameClass()
{
    if (currentWorkingFolder.isEmpty()) {
        QMessageBox::warning(this, "Error", "Open some folder first");
        return;
    }

    if (currentlySelectedAnnotationClassItem == nullptr) {
        QMessageBox::warning(this, "Error", "No class selected");
    }

    int row = 0, rowCount = annotationClasses->count();
    for (auto i = annotationClassItems.begin(), end = annotationClassItems.end(); i != end && row < rowCount; ++i, ++row) {
        ClassItem& classItem = *i;
        if (currentlySelectedAnnotationClassItem == classItem.listWidgetItem) {
            bool ok = false;
            const QString newName = QInputDialog::getText(this, tr("Enter new name"),
                                                          tr("Please enter a new name for class \"%1\":").arg(classItem.className),
                                                          QLineEdit::Normal, classItem.className, &ok);
            if (ok) {
                if (newName.isEmpty()) {
                    QMessageBox::critical(this, tr("Error"), tr("The class name cannot be empty."));
                }
                else {
                    annotationClasses->item(row)->setText(newName);
                    classItem.className = newName;
                    saveClassList();
                }
            }
            return;
        }
    }

    panButton->setChecked(true);
    QMessageBox::warning(this, tr("Error"), tr("No annotation class item found"));
}

void MainWindow::onRemoveClass()
{
    if (currentWorkingFolder.isEmpty()) {
        QMessageBox::warning(this, "Error", "Open some folder first");
        return;
    }

    if (currentlySelectedAnnotationClassItem == nullptr) {
        QMessageBox::warning(this, "Error", "No class selected");
    }

    int row = 0, rowCount = annotationClasses->count();
    for (auto i = annotationClassItems.begin(), end = annotationClassItems.end(); i != end && row < rowCount; ++i, ++row) {
        const ClassItem& classItem = *i;
        if (currentlySelectedAnnotationClassItem == classItem.listWidgetItem) {
            if (QMessageBox::question(this, tr("Please confirm"), tr("Are you sure you want to remove class \"%1\"?").arg(classItem.className)) == QMessageBox::Yes) {
                QListWidgetItem* item = annotationClasses->takeItem(row);
                delete item;
                annotationClassItems.erase(i);
                saveClassList();
            }
            return;
        }
    }

    panButton->setChecked(true);
    QMessageBox::warning(this, tr("Error"), tr("No annotation class item found"));
}

void MainWindow::addNewClass(const QString& className, QColor color)
{
    QStringList columns;
    columns.append(className);

    ClassItem classItem;
    classItem.className = className;
    classItem.color = color;
    classItem.listWidgetItem = new QListWidgetItem(className, annotationClasses);
    classItem.listWidgetItem->setBackgroundColor(color);

    const QColor hslColor = color.toHsl();

    if (hslColor.lightness() < 128) {
        classItem.listWidgetItem->setTextColor(Qt::white);
    }

    if (annotationClassItems.empty()) {
        classItem.listWidgetItem->setSelected(true);
    }

    annotationClassItems.push_back(classItem);
}

QString getClassListFilename(const QString& currentWorkingFolder)
{
    return currentWorkingFolder + "/anno_classes.json";
}

void MainWindow::loadClassList()
{
    const QString filename = getClassListFilename(currentWorkingFolder);
    QFile file(filename);

    if (file.open(QIODevice::ReadOnly)) {

        annotationClasses->clear();
        annotationClassItems.clear();

        const QJsonDocument doc(QJsonDocument::fromJson(file.readAll()));
        const QJsonArray classArray = doc.object()["anno_classes"].toArray();
        const int classCount = classArray.size();
        for (int i = 0; i < classCount; ++i) {
            const QJsonObject classObject = classArray[i].toObject();
            const QJsonObject colorObject = classObject["color"].toObject();
            const QString name = classObject["name"].toString();
            const QColor color(
                colorObject["red"].toInt(),
                colorObject["green"].toInt(),
                colorObject["blue"].toInt(),
                colorObject["alpha"].toInt()
            );
            addNewClass(name, color);
        }
    }
}

void MainWindow::saveClassList() const
{
    QJsonObject json;

    {
        QJsonArray classArray;
        for (const ClassItem& classItem : annotationClassItems) {
            QJsonObject classObject;
            classObject["name"] = classItem.className;
            QJsonObject colorObject;
            colorObject["red"] = classItem.color.red();
            colorObject["green"] = classItem.color.green();
            colorObject["blue"] = classItem.color.blue();
            colorObject["alpha"] = classItem.color.alpha();
            classObject["color"] = colorObject;
            classArray.append(classObject);
        }
        json["anno_classes"] = classArray;
    }

    const QString filename = getClassListFilename(currentWorkingFolder);
    QFile file(filename);

    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(json).toJson());
    }
    else {
        const QString text = tr("Couldn't open file \"%1\" for writing").arg(filename);
        QMessageBox::warning(nullptr, tr("Error"), text);
    }
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();
    if (key == Qt::Key_Space) {
        markingsVisible->toggle();
        resultsVisible->toggle();
        //yardstickVisible->toggle();
    }
    else if (key == Qt::Key_Escape || key == Qt::Key_P) {
        panButton->setChecked(true);
    }
    else if (key == Qt::Key_A) {
        annotateButton->setChecked(true);
    }
    else if (key == Qt::Key_E) {
        eraseAnnotationsButton->setChecked(true);
    }
    else if (key >= Qt::Key_0 && key <= Qt::Key_9) {
        int index = key - Qt::Key_0;
        if (index >= 0 && index < annotationClassItems.size()) {
            QListWidgetItem* itemToSelect = annotationClassItems[index].listWidgetItem;
            itemToSelect->setSelected(true);
            onAnnotationClassClicked(itemToSelect);
        }
    }
    else if (key == Qt::Key_V) {
        markingsVisible->toggle();
    }
    else if (key == Qt::Key_R) {
        resultsVisible->toggle();
    }
    else if (key == Qt::Key_B) {
        bucketFillCheckbox->toggle();
    }
    else if (key == Qt::Key_S) {
        reverseFileOrder = !reverseFileOrder;
        if (files) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
            files->sortItems(reverseFileOrder ? Qt::DescendingOrder : Qt::AscendingOrder);
            auto* currentItem = files->currentItem();
            if (currentItem) {
                files->scrollToItem(currentItem, QListWidget::EnsureVisible);
            }
            QApplication::restoreOverrideCursor();
        }
    }
    else if (key == Qt::Key_F5) {
        openFolder(currentWorkingFolder);
    }
    else if (key == Qt::Key_Z) {
        if (image) {
            image->resetZoomAndPan();
        }
    }
    else if (key == Qt::Key_Y) {
        if (image) {
            yardstickVisible->toggle();
        }
    }
    else if (key == Qt::Key_Delete) {
        if (files->hasFocus()) {
            const int row = files->currentRow();
            const QString filename = files->item(row)->text();
            if (filename.length() > 0) {

                const auto confirmAndDeleteFile = [&]() {
                    const bool isOkToProceed
                            = QMessageBox::Yes == QMessageBox::question(this, tr("Are you sure?"),
                                tr("This will permanently delete the file:\n%1").arg(filename));
                    if (isOkToProceed) {
                        QFile::remove(filename);
                        QListWidgetItem* item = files->takeItem(row);
                        delete item;
                        loadFile(files->item(files->currentRow()));
                        return true;
                    }
                    else {
                        return false;
                    }
                };

                // TODO: Delete also the JSON result files, etc.

#ifdef WIN32
                if (event->modifiers() & Qt::ShiftModifier) {
                    confirmAndDeleteFile();
                }
                else {
                    std::vector<wchar_t> buffer(filename.length() + 1);
                    filename.toWCharArray(buffer.data());
                    buffer.back() = L'\0';
                    try {
                        move_file_to_trash(buffer.data());
                        QListWidgetItem* item = files->takeItem(row);
                        delete item;
                        loadFile(files->item(files->currentRow()));
                    }
                    catch (std::exception& e) {
                        QString error = QString::fromLatin1(e.what());
                        error.replace(QString("Unable to move the file to the recycle bin; error code ="), tr("Unable to move file %1 to the recycle bin.\n\nError code =").arg(filename));
                        QMessageBox::warning(this, tr("Error"), error);
                    }
                }
#else // WIN32
                confirmAndDeleteFile();
#endif // WIN32
            }
        }
    }
}

void MainWindow::onUndo()
{
    if (!maskUndoBuffer.empty()) {
        const bool requireWaitCursor = currentMask.size().width() * currentMask.size().height() > 1024 * 1024;
        if (requireWaitCursor) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
        }

        maskRedoBuffer.push_back(currentMask);
        limitUndoOrRedoBufferSize(maskRedoBuffer);

        currentMask = maskUndoBuffer.back();
        image->setMask(currentMask.toImage());
        maskUndoBuffer.pop_back();

        updateUndoRedoMenuItemStatus();

        maskDirty = true;
        ++saveMaskPendingCounter;
        QTimer::singleShot(10000, this, SLOT(onSaveMask()));

        if (requireWaitCursor) {
            QApplication::restoreOverrideCursor();
        }
    }
}

void MainWindow::onRedo()
{
    if (!maskRedoBuffer.empty()) {
        const bool requireWaitCursor = currentMask.size().width() * currentMask.size().height() > 1024 * 1024;
        if (requireWaitCursor) {
            QApplication::setOverrideCursor(Qt::WaitCursor);
        }

        maskUndoBuffer.push_back(currentMask);
        limitUndoOrRedoBufferSize(maskUndoBuffer);

        currentMask = maskRedoBuffer.back();
        image->setMask(currentMask.toImage());
        maskRedoBuffer.pop_back();

        updateUndoRedoMenuItemStatus();

        maskDirty = true;
        ++saveMaskPendingCounter;
        QTimer::singleShot(10000, this, SLOT(onSaveMask()));

        if (requireWaitCursor) {
            QApplication::restoreOverrideCursor();
        }
    }
}

void MainWindow::resetUndoBuffers()
{
    maskUndoBuffer.clear();
    maskRedoBuffer.clear();
    updateUndoRedoMenuItemStatus();
}

void MainWindow::updateUndoRedoMenuItemStatus()
{
    ui->actionUndo->setEnabled(!maskUndoBuffer.empty());
    ui->actionRedo->setEnabled(!maskRedoBuffer.empty());
}

void MainWindow::limitUndoOrRedoBufferSize(std::deque<QPixmap>& buffer)
{
    const size_t maxBufferSizeInPixels = 256 * 1024 * 1024; // 256 Mpixels ought to lead to a buffer size of around 1 GB
    size_t bufferSizeInPixels = 0;
    for (const QPixmap& item : buffer) {
        bufferSizeInPixels += item.size().width() * item.size().height();
    }
    while (buffer.size() > 1 && bufferSizeInPixels > maxBufferSizeInPixels) {
        const QPixmap& itemToRemove = buffer.front();
        bufferSizeInPixels -= itemToRemove.size().width() * itemToRemove.size().height();
        buffer.pop_front();
    }
}

void MainWindow::onNewMarkingRadius(int newMarkingRadius)
{
    markingRadius->setValue(newMarkingRadius);
}

void MainWindow::onAnnotationsVisible(bool visible)
{
    disconnect(markingsVisible, SIGNAL(toggled(bool)), this, SLOT(onMarkingsVisible(bool)));
    markingsVisible->setChecked(visible);
    connect(markingsVisible, SIGNAL(toggled(bool)), this, SLOT(onMarkingsVisible(bool)));
}

void MainWindow::onAbout()
{
    if (!aboutDialog) {
        QUiLoader loader;

        QFile file(":/about.ui");
        file.open(QFile::ReadOnly);

        aboutDialog = loader.load(&file, this);

        // remove the question mark from the title bar
        aboutDialog->setWindowFlags(aboutDialog->windowFlags() & ~Qt::WindowContextHelpButtonHint);

        // set the anno version number
        QLabel* versionNumber = aboutDialog->findChild<QLabel*>("versionNumber");
        versionNumber->setText(version);
    }

    aboutDialog->show();
}
