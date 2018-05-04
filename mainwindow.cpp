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
    const char* applicationName = "anno-single-rectangle-per-image";
    const int fullnameRole = Qt::UserRole + 0;
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
    reverseFileOrder = settings.value("reverseFileOrder").toBool();

    connect(ui->actionOpenFolder, SIGNAL(triggered()), this, SLOT(onOpenFolder()));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));
    connect(ui->actionUndo, SIGNAL(triggered()), this, SLOT(onUndo()));
    connect(ui->actionRedo, SIGNAL(triggered()), this, SLOT(onRedo()));
    connect(ui->actionRestoreDefaultWindowPositions, SIGNAL(triggered()), this, SLOT(onRestoreDefaultWindowPositions()));
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
    QSettings settings(companyName, applicationName);
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    settings.setValue("reverseFileOrder", reverseFileOrder);

    QMainWindow::closeEvent(event);
}

void MainWindow::init()
{
    initImageIO();
    createToolList();
    createFileList();
    createImageView();

    const QSettings settings(companyName, applicationName);
    const QString defaultDirectory = settings.value("defaultDirectory").toString();
    if (!defaultDirectory.isEmpty()) {
        openFolder(defaultDirectory);
    }
    else {
        onOpenFolder();
    }

    showMaximized();

    if (files->count() > 0) {
        const QString defaultFile = settings.value("defaultFile").toString();
        bool defaultFileFound = false;

        if (!defaultFile.isEmpty()) {
            for (int i = 0, end = files->count(); i < end; ++i) {
                QListWidgetItem* file = files->item(i);
                if (file->text() == defaultFile) {
                    defaultFileFound = true;
                    files->scrollToItem(file, QListWidget::EnsureVisible);
                    file->setSelected(true);
                    onFileClicked(file);
                    break;
                }
            }
        }

        if (!defaultFileFound) {
            QListWidgetItem* firstFile = files->item(0);
            firstFile->setSelected(true);
            onFileClicked(firstFile);
        }
    }

    defaultGeometry = saveGeometry();
    defaultState = saveState();

    const bool geometryRestored = restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    const bool stateRestored = restoreState(settings.value("mainWindowState").toByteArray());

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

    setCentralWidget(image);

    //connect(image, SIGNAL(annotationUpdating()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(annotationUpdated()), this, SLOT(onAnnotationUpdated()));
    connect(image, SIGNAL(makeAnnotationsVisible(bool)), this, SLOT(onAnnotationsVisible(bool)));
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

    {
        annotationsVisible = new QCheckBox("Annotations &visible", this);
        annotationsVisible->setChecked(true);

        connect(annotationsVisible, SIGNAL(toggled(bool)), this, SLOT(onAnnotationsVisible(bool)));
    }

    eraseAnnotationsButton = new QPushButton(tr("&Remove current annotation"));
    connect(eraseAnnotationsButton, SIGNAL(clicked()), this, SLOT(onEraseAnnotationsButtonClicked()));

    QGroupBox* leftMouseButtonActions = new QGroupBox(tr("Left mouse button actions"));
    {
        QGridLayout* leftMouseButtonActionsLayout = new QGridLayout;

        panButton = new QRadioButton(tr("&Pan"));
        annotateButton = new QRadioButton(tr("&Annotate"));

        annotateButton->setChecked(true);

        int row = 0;
        leftMouseButtonActionsLayout->addWidget(panButton, row++, 0, 1, 2);
        leftMouseButtonActionsLayout->addWidget(annotateButton, row++, 0, 1, 1);

        connect(panButton, SIGNAL(toggled(bool)), this, SLOT(onPanButtonToggled(bool)));
        connect(annotateButton, SIGNAL(toggled(bool)), this, SLOT(onAnnotateButtonToggled(bool)));

        leftMouseButtonActions->setLayout(leftMouseButtonActionsLayout);
    }

    QGroupBox* rightMouseButtonActions = new QGroupBox(tr("Right mouse button actions"));
    {
        QGridLayout* rightMouseButtonActionsLayout = new QGridLayout;

        rightMousePanButton = new QRadioButton(tr("Pa&n"));
        rightMouseResetViewButton = new QRadioButton(tr("Reset vie&w"));

        rightMouseResetViewButton->setChecked(true);

        int row = 0;
        rightMouseButtonActionsLayout->addWidget(rightMousePanButton, row++, 0, 1, 1);
        rightMouseButtonActionsLayout->addWidget(rightMouseResetViewButton, row++, 0, 1, 1);

        connect(rightMousePanButton, SIGNAL(toggled(bool)), this, SLOT(onRightMousePanButtonToggled(bool)));
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

    layout->addWidget(annotationsVisible);
    layout->addSpacing(10);
    layout->addWidget(leftMouseButtonActions);
    layout->addSpacing(10);
    layout->addWidget(rightMouseButtonActions);
    layout->addSpacing(10);
    layout->addWidget(eraseAnnotationsButton);
    layout->addSpacing(10);
    layout->addWidget(resultsVisible);
    layout->addSpacing(10);
    layout->addStretch(1);
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

    QDirIterator it(dir, QStringList() << "*.jpg" << "*.jpeg" << "*.png", QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString filename = it.next();
        const bool isResultImage = filename.length() > 11 && filename.right(11) == "_result.png";
        const bool isMaskImage = filename.length() > 9 && filename.right(9) == "_mask.png";
        if (!isResultImage && !isMaskImage){
            const QString displayName = filename.mid(dir.length() + 1);
            QListWidgetItem* item = new QListWidgetItem(displayName, files);
            QFileInfo annotationFileInfo(getAnnotationPathFilename(filename));
            if (annotationFileInfo.exists() && annotationFileInfo.isFile()) {
                item->setTextColor(Qt::black);
            }
            else {
                item->setTextColor(Qt::gray);
            }
            item->setData(fullnameRole, filename);
        }
    }

    files->sortItems(reverseFileOrder ? Qt::DescendingOrder : Qt::AscendingOrder);

    currentWorkingFolder = dir;

    addRecentFolderMenuItem(dir);

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
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    currentImageFileItem = item;
    currentImageFile = item->data(fullnameRole).toString();

    QSettings settings(companyName, applicationName);
    settings.setValue("defaultFile", item->text());

    resetUndoBuffers();

    const auto readImage = [](const QString& filename) { return QImage(filename); };

    QFuture<QImage> imageFuture = QtConcurrent::run(readImage, currentImageFile);

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
            result.pen.setWidth(2);

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

    auto annotationsFuture = QtConcurrent::run(readResults, getAnnotationPathFilename(currentImageFile));
    auto resultsFuture = QtConcurrent::run(readResults, getInferenceResultPathFilename(currentImageFile));

    {
        QResultImageView::DelayedRedrawToken delayedRedrawToken;

        image->setImage(imageFuture.result(), &delayedRedrawToken);

        currentAnnotations = annotationsFuture.result();
        currentResults = resultsFuture.result();

        if (!currentAnnotations.error.isEmpty()) {
            QMessageBox::warning(nullptr, tr("Error"), currentAnnotations.error);
        }
        if (!currentResults.error.isEmpty()) {
            QMessageBox::warning(nullptr, tr("Error"), currentResults.error);
        }

        updateResults(&delayedRedrawToken);
    }

    QApplication::restoreOverrideCursor();
}

void MainWindow::onPanButtonToggled(bool toggled)
{
    if (toggled) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Pan);
    }
}

void MainWindow::onAnnotateButtonToggled(bool toggled)
{
    if (toggled) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Annotate);
    }
}

void MainWindow::onEraseAnnotationsButtonClicked()
{
    annotationUndoBuffer.push_back(currentAnnotations.results);
    limitUndoOrRedoBufferSize(annotationUndoBuffer);

    currentAnnotations.results.clear();
    image->setAnnotations(currentAnnotations.results);

    annotationRedoBuffer.clear();

    updateUndoRedoMenuItemStatus();

    updateResults();

    saveCurrentAnnotation();
}

void MainWindow::onRightMousePanButtonToggled(bool toggled)
{
    if (toggled) {
        image->setRightMouseMode(QResultImageView::RightMouseMode::Pan);
    }
}

void MainWindow::onRightMouseResetViewButtonToggled(bool toggled)
{
    if (toggled) {
        image->setRightMouseMode(QResultImageView::RightMouseMode::ResetView);
    }
}

void MainWindow::onMakeAnnotationsVisible(bool visible)
{
    disconnect(annotationsVisible, SIGNAL(toggled(bool)), this, SLOT(onAnnotationsVisible(bool)));
    annotationsVisible->setChecked(visible);
    connect(annotationsVisible, SIGNAL(toggled(bool)), this, SLOT(onAnnotationsVisible(bool)));
}

void MainWindow::onAnnotationsVisible(bool toggled)
{
    updateResults();
}

void MainWindow::onResultsVisible(bool toggled)
{
    updateResults();
}

void MainWindow::updateResults(QResultImageView::DelayedRedrawToken* delayedRedrawToken)
{
    currentlyShownPaths.clear();
    if (annotationsVisible->isChecked()) {
        std::copy(currentAnnotations.results.begin(), currentAnnotations.results.end(), std::back_inserter(currentlyShownPaths));
    }
    if (resultsVisible->isChecked()) {
        std::copy(currentResults.results.begin(), currentResults.results.end(), std::back_inserter(currentlyShownPaths));
    }
    image->setResults(currentlyShownPaths, delayedRedrawToken);

    annotationsVisible->setEnabled(!currentAnnotations.results.empty());
    resultsVisible->setEnabled(!currentResults.results.empty());

    eraseAnnotationsButton->setEnabled(!currentAnnotations.results.empty());
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

void MainWindow::onAnnotationUpdated()
{
    annotationUndoBuffer.push_back(currentAnnotations.results);
    limitUndoOrRedoBufferSize(annotationUndoBuffer);

    const QRectF annotation = image->getAnnotatedSourceRect();

    currentAnnotations.results.resize(1);
    currentAnnotations.results[0].pen = QPen(Qt::darkGreen);
    currentAnnotations.results[0].pen.setWidth(2);
    currentAnnotations.results[0].contour.resize(4);
    currentAnnotations.results[0].contour[0] = annotation.topLeft();
    currentAnnotations.results[0].contour[1] = annotation.topRight();
    currentAnnotations.results[0].contour[2] = annotation.bottomRight();
    currentAnnotations.results[0].contour[3] = annotation.bottomLeft();

    annotationRedoBuffer.clear();

    updateUndoRedoMenuItemStatus();

    updateResults();

    saveCurrentAnnotation();
}

void MainWindow::saveCurrentAnnotation()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    if (!currentAnnotations.results.empty()) {
        QJsonArray json;

        {
            for (const auto& annotationItem : currentAnnotations.results) {
                QJsonObject colorObject;
                colorObject["r"] = annotationItem.pen.color().red();
                colorObject["g"] = annotationItem.pen.color().green();
                colorObject["b"] = annotationItem.pen.color().blue();
                colorObject["a"] = annotationItem.pen.color().alpha();
                QJsonArray colorPathsArray;
                QJsonArray colorPathArray;
                for (const QPointF& point : annotationItem.contour) {
                    QJsonObject pointObject;
                    pointObject["x"] = point.x();
                    pointObject["y"] = point.y();
                    colorPathArray.append(pointObject);
                }
                colorPathsArray.append(colorPathArray);
                QJsonObject annotationObject;
                annotationObject["color"] = colorObject;
                annotationObject["color_paths"] = colorPathsArray;
                json.append(annotationObject);
            }
        }

        if (!currentImageFile.isEmpty()) {
            const QString filename = getAnnotationPathFilename(currentImageFile);
            QFile file(filename);

            if (file.open(QIODevice::WriteOnly)) {
                file.write(QJsonDocument(json).toJson());
            }
            else {
                const QString text = tr("Couldn't open file \"%1\" for writing").arg(filename);
                QMessageBox::warning(nullptr, tr("Error"), text);
            }
        }

        if (currentImageFileItem != nullptr) {
            currentImageFileItem->setTextColor(Qt::black); // now we will have an annotation file
        }
    }
    else {
        const QString filename = getAnnotationPathFilename(currentImageFile);
        std::vector<wchar_t> buffer(filename.length() + 1);
        filename.toWCharArray(buffer.data());
        buffer.back() = L'\0';
        move_file_to_trash(buffer.data());

        if (currentImageFileItem != nullptr) {
            currentImageFileItem->setTextColor(Qt::gray); // we no longer have an annotation file
        }
    }

    QApplication::restoreOverrideCursor();
}

QString MainWindow::getAnnotationPathFilename(const QString& baseImageFilename)
{
    return baseImageFilename + "_annotation_path.json";
}

QString MainWindow::getInferenceResultPathFilename(const QString& baseImageFilename)
{
    return baseImageFilename + "_result_path.json";
}

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    const int key = event->key();
    if (key == Qt::Key_Space) {
        annotationsVisible->toggle();
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
    else if (key == Qt::Key_V) {
        annotationsVisible->toggle();
    }
    else if (key == Qt::Key_R) {
        resultsVisible->toggle();
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
        //if (image) {
        //    yardstickVisible->toggle();
        //}
    }
    else if (key == Qt::Key_Delete) {
        if (files->hasFocus()) {
            const int row = files->currentRow();
            const QString filename = files->item(row)->data(fullnameRole).toString();
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
    else if (key == Qt::Key_Up) {
        const int currentRow = files->currentRow();
        const int newRow = currentRow - 1;
        if (newRow >= 0 && newRow < files->count()) {
            files->setCurrentRow(newRow);
            loadFile(files->item(newRow));
        }
    }
    else if (key == Qt::Key_Down) {
        const int currentRow = files->currentRow();
        const int newRow = currentRow + 1;
        if (newRow >= 0 && newRow < files->count()) {
            files->setCurrentRow(newRow);
            loadFile(files->item(newRow));
        }
    }
}

void MainWindow::onUndo()
{
    if (!annotationUndoBuffer.empty()) {
        annotationRedoBuffer.push_back(currentAnnotations.results);
        limitUndoOrRedoBufferSize(annotationRedoBuffer);

        currentAnnotations.results = annotationUndoBuffer.back();
        updateResults();

        annotationUndoBuffer.pop_back();

        updateUndoRedoMenuItemStatus();

        saveCurrentAnnotation();
    }
}

void MainWindow::onRedo()
{
    if (!annotationRedoBuffer.empty()) {
        annotationUndoBuffer.push_back(currentAnnotations.results);
        limitUndoOrRedoBufferSize(annotationUndoBuffer);

        currentAnnotations.results = annotationRedoBuffer.back();
        updateResults();

        annotationRedoBuffer.pop_back();

        updateUndoRedoMenuItemStatus();

        saveCurrentAnnotation();
    }
}

void MainWindow::resetUndoBuffers()
{
    annotationUndoBuffer.clear();
    annotationRedoBuffer.clear();
    updateUndoRedoMenuItemStatus();
}

void MainWindow::updateUndoRedoMenuItemStatus()
{
    ui->actionUndo->setEnabled(!annotationUndoBuffer.empty());
    ui->actionRedo->setEnabled(!annotationRedoBuffer.empty());
}

void MainWindow::limitUndoOrRedoBufferSize(std::deque<std::vector<QResultImageView::Result>>& buffer)
{
    const size_t maxBufferSize = 1024;
    while (buffer.size() > maxBufferSize) {
        buffer.pop_front();
    }
}

void MainWindow::onRestoreDefaultWindowPositions()
{
    restoreGeometry(defaultGeometry);
    restoreState(defaultState);
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
