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
#include <QProgressDialog>
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
    const char* classListFilename = "anno_classes.json";
    const int fullnameRole = Qt::UserRole + 0;
    const QColor cleanColor = QColor(0, 255, 0, 64);
    const QColor ignoreColor = QColor(127, 127, 127, 128);
}

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    ui->menuFile->setToolTipsVisible(true);

    recentFoldersMenu = new QMenu(tr("&Recent folders"), this);
    ui->menuFile->insertMenu(ui->actionExit, recentFoldersMenu);

    setWindowTitle("anno");

    const QSettings settings(companyName, applicationName);
    reverseFileOrder = settings.value("reverseFileOrder").toBool();

    connect(ui->actionOpenFolder, SIGNAL(triggered()), this, SLOT(onOpenFolder()));
    connect(ui->actionExport, SIGNAL(triggered()), this, SLOT(onExport()));
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
    saveMaskIfDirty();

    QSettings settings(companyName, applicationName);
    settings.setValue("mainWindowGeometry", saveGeometry());
    settings.setValue("mainWindowState", saveState());
    settings.setValue("reverseFileOrder", reverseFileOrder);
    settings.setValue("annotateThings", annotateThings->isChecked());

    if (markingRadius) {
        settings.setValue("markingRadius", markingRadius->value());
    }

    if (allImageChannelsButton && allImageChannelsButton->isChecked()) {
        settings.setValue("channelSelection", "all");
    }
    else if (redChannelButton && redChannelButton->isChecked()) {
        settings.setValue("channelSelection", "red");
    }
    else if (greenChannelButton && greenChannelButton->isChecked()) {
        settings.setValue("channelSelection", "green");
    }
    else if (blueChannelButton && blueChannelButton->isChecked()) {
        settings.setValue("channelSelection", "blue");
    }
    else if (alphaChannelButton && alphaChannelButton->isChecked()) {
        settings.setValue("channelSelection", "alpha");
    }

    QMainWindow::closeEvent(event);
}

void MainWindow::init()
{
    initImageIO();
    createImageView();
    createToolList();
    createFileList();

    image->setMarkingRadius(markingRadius->value());

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

    /*const bool geometryRestored =*/ restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    /*const bool stateRestored =*/ restoreState(settings.value("mainWindowState").toByteArray());

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

    connect(image, SIGNAL(annotationUpdating()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(annotationUpdated()), this, SLOT(onAnnotationUpdated()));
    connect(image, SIGNAL(panned()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(zoomed()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(newMarkingRadius(int)), this, SLOT(onNewMarkingRadius(int)));
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
    connect(files, SIGNAL(currentItemChanged(QListWidgetItem*,QListWidgetItem*)), this, SLOT(onFileItemChanged(QListWidgetItem*,QListWidgetItem*)));
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
        markingRadius->setMaximum(200);
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

    QGroupBox* annotationModeWidget = new QGroupBox(tr("Annotation mode"));
    {
        auto* annotationModeLayout = new QHBoxLayout;

        annotateStuff = new QRadioButton("&Stuff", this);
        annotateThings = new QRadioButton("Thin&gs", this);

        layout->addWidget(annotateStuff);
        layout->addWidget(annotateThings);

        bool thingsMode = settings.value("annotateThings", 0).toBool();

        connect(annotateStuff, SIGNAL(toggled(bool)), this, SLOT(onAnnotateStuff(bool)));
        connect(annotateThings, SIGNAL(toggled(bool)), this, SLOT(onAnnotateThings(bool)));

        annotateStuff->setChecked(!thingsMode);
        annotateThings->setChecked(thingsMode);

        annotationModeLayout->addWidget(annotateStuff);
        annotationModeLayout->addWidget(annotateThings);

        annotationModeWidget->setLayout(annotationModeLayout);
    }

    {
        resultsVisible = new QCheckBox("&Results visible", this);
        resultsVisible->setChecked(true);

        connect(resultsVisible, SIGNAL(toggled(bool)), this, SLOT(onResultsVisible(bool)));
    }

    QGroupBox* imageChannels = nullptr;
    if (settings.value("channelSelectionEnabled", QVariant(true)).toBool()) {
        imageChannels = new QGroupBox(tr("Image channels"));
        QVBoxLayout* imageChannelsLayout = new QVBoxLayout;

        allImageChannelsButton = new QRadioButton(tr("All channels"));
        redChannelButton = new QRadioButton(tr("Red channel"));
        greenChannelButton = new QRadioButton(tr("Green channel"));
        blueChannelButton = new QRadioButton(tr("Blue channel"));
        alphaChannelButton = new QRadioButton(tr("Alpha channel"));

        imageChannelsLayout->addWidget(allImageChannelsButton);
        imageChannelsLayout->addWidget(redChannelButton);
        imageChannelsLayout->addWidget(greenChannelButton);
        imageChannelsLayout->addWidget(blueChannelButton);
        imageChannelsLayout->addWidget(alphaChannelButton);

        connect(allImageChannelsButton, SIGNAL(toggled(bool)), this, SLOT(onChannelSelectionToggled(bool)));
        connect(redChannelButton, SIGNAL(toggled(bool)), this, SLOT(onChannelSelectionToggled(bool)));
        connect(greenChannelButton, SIGNAL(toggled(bool)), this, SLOT(onChannelSelectionToggled(bool)));
        connect(blueChannelButton, SIGNAL(toggled(bool)), this, SLOT(onChannelSelectionToggled(bool)));
        connect(alphaChannelButton, SIGNAL(toggled(bool)), this, SLOT(onChannelSelectionToggled(bool)));

        imageChannels->setLayout(imageChannelsLayout);

        QString channelSelection = settings.value("channelSelection").toString();
        if (channelSelection == "red") {
            redChannelButton->setChecked(true);
        }
        else if (channelSelection == "green") {
            greenChannelButton->setChecked(true);
        }
        else if (channelSelection == "blue") {
            blueChannelButton->setChecked(true);
        }
        else if (channelSelection == "alpha") {
            alphaChannelButton->setChecked(true);
        }
        else {
            allImageChannelsButton->setChecked(true);
        }

        const bool allChannelsVisible = settings.value("allChannelsVisible", QVariant(true)).toBool();
        if (!allChannelsVisible) {
            allImageChannelsButton->setVisible(false);
        }

        const auto initChannelButton = [&settings](const QString& color, QRadioButton* button) {
            const bool channelVisible = settings.value(color + "ChannelVisible", QVariant(true)).toBool();
            if (!channelVisible) {
                button->setVisible(false);
            }
            else {
                QString channelLabel = settings.value(color + "ChannelLabel").toString();
                if (!channelLabel.isEmpty()) {
                    button->setText(channelLabel);
                }
            }
        };

        initChannelButton("red", redChannelButton);
        initChannelButton("green", greenChannelButton);
        initChannelButton("blue", blueChannelButton);
        initChannelButton("alpha", alphaChannelButton);
    }

    {
        yardstickVisible = new QCheckBox("&Yardstick visible", this);
        yardstickVisible->setChecked(true);

        connect(yardstickVisible, SIGNAL(toggled(bool)), this, SLOT(onYardstickVisible(bool)));
    }

    layout->addWidget(markingsVisible);
    layout->addSpacing(10);
    layout->addWidget(annotationModeWidget);
    layout->addSpacing(5);
    layout->addWidget(markingRadiusWidget);
    layout->addWidget(leftMouseButtonActions);
    layout->addSpacing(10);
    layout->addWidget(rightMouseButtonActions);
    layout->addSpacing(10);
    layout->addWidget(resultsVisible);
    if (imageChannels != nullptr) {
        layout->addSpacing(10);
        layout->addWidget(imageChannels);
    }
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

    QFileDialog dialog(this, "Select a folder containing some images (possibly in subfolders)");
    dialog.setDirectory(defaultDirectory);
    dialog.setFileMode(QFileDialog::Directory);
    dialog.setOption(QFileDialog::DontUseNativeDialog);
    dialog.setOption(QFileDialog::DontResolveSymlinks);
    dialog.setNameFilter("(*.jpg *.jpeg *.png)");

    if (dialog.exec() == QDialog::Accepted) {
        const QString dir = dialog.directory().path();
        if (!dir.isEmpty()) {
            settings.setValue("defaultDirectory", dir);
            openFolder(dir);
        }
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

    setWindowTitle(tr("anno @ %1").arg(dir));

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
        const QString filename = it.next();
        const auto isMaskFilename = [&]() { return filename.right(maskFilenameSuffix.length()) == maskFilenameSuffix; };
        const auto isInferenceResultFilename = [&]() { return filename.right(inferenceResultFilenameSuffix.length()) == inferenceResultFilenameSuffix; };
        if (!isMaskFilename() && !isInferenceResultFilename()) {
            const QString displayName = filename.mid(dir.length() + 1);
            QListWidgetItem* item = new QListWidgetItem(displayName, files);
            QFileInfo maskFileInfo(getMaskFilename(filename));
            QFileInfo thingAnnotationsFileInfo(getThingAnnotationsPathFilename(filename));
            const bool maskFileExists = maskFileInfo.exists() && maskFileInfo.isFile();
            const bool thingAnnotationsFileExists = thingAnnotationsFileInfo.exists() && thingAnnotationsFileInfo.isFile();
            if (maskFileExists || thingAnnotationsFileExists) {
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

    loadClassList();

    if (annotationClassItems.empty()) {
        // Add sample classes
        addNewClass(cleanClassLabel, cleanColor);
        addNewClass(tr("Minor defect"), QColor(255, 255, 0, 128));
        addNewClass(tr("Major defect"), QColor(255, 0,   0, 128));
    }

    if (annotateThings->isChecked()) {
        conditionallyChangeFirstClass(cleanClassLabel, cleanColor, ignoreClassLabel, ignoreColor);
    }
    else {
        conditionallyChangeFirstClass(ignoreClassLabel, ignoreColor, cleanClassLabel, cleanColor);
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

QString getClassListFilename(const QString& currentWorkingFolder)
{
    return currentWorkingFolder + "/" + classListFilename;
}

void MainWindow::onExport()
{
    QSettings settings(companyName, applicationName);
    QString defaultExportDirectory = settings.value("defaultExportDirectory").toString();
    if (defaultExportDirectory.isEmpty()) {
#ifdef _WIN32
        defaultExportDirectory = "C:\\";
#endif
    }

    const QString dir = QFileDialog::getExistingDirectory(this,
                                                          tr("Select a folder where to export the annotations and the corresponding images"),
                                                          defaultExportDirectory,
                                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        settings.setValue("defaultExportDirectory", dir);

        saveMaskIfDirty();

        QDir directory(dir);

        if (!directory.exists()) {
            QMessageBox::critical(this, tr("No such directory"), tr("Directory \"%1\" does not exist").arg(dir));
            return;
        }

        QDirIterator dirIterator(dir,
                                 QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot,
                                 QDirIterator::Subdirectories);

        if (dirIterator.hasNext()) {
            auto reply = QMessageBox::question(this,
                                               tr("Directory not empty"),
                                               tr("Directory %1 is not empty!\n\nProceed anyway?").arg(dir),
                                               QMessageBox::Yes | QMessageBox::No);
            if (reply != QMessageBox::Yes) {
                return;
            }
        }

        QApplication::setOverrideCursor(Qt::WaitCursor);

        std::deque<std::pair<QString, QString>> imagesWithAnnotations;

        const int total = files->count();
        for (int row = 0; row < total; ++row) {
            const auto* item = files->item(row);
            if (item->textColor() == Qt::black) {
                imagesWithAnnotations.push_back(std::make_pair(
                    item->text(),
                    item->data(fullnameRole).toString())
                );
            }
        }

        QApplication::restoreOverrideCursor();

        const int count = static_cast<int>(imagesWithAnnotations.size());

        QProgressDialog progress(tr("Exporting %1 images to %2 ...").arg(QString::number(count), dir), "Cancel", 0, count, this);

        progress.setWindowModality(Qt::WindowModal);

        size_t fileCount = 0;

        for (int i = 0; i < count; i++) {
            progress.setValue(i);

            if (progress.wasCanceled()) {
                break;
            }

            std::deque<std::pair<QString, QString>> allSourceFilesForThisImage;

            allSourceFilesForThisImage.push_back(imagesWithAnnotations[i]);

            {
                const auto maskFilename = getMaskFilename(imagesWithAnnotations[i].second);
                if (QFile().exists(maskFilename)) {
                    allSourceFilesForThisImage.push_back(std::make_pair(
                        getMaskFilename(imagesWithAnnotations[i].first),
                        getMaskFilename(imagesWithAnnotations[i].second)
                    ));
                }
            }

            {
                const auto thingAnnotationsPathFilename = getThingAnnotationsPathFilename(imagesWithAnnotations[i].second);
                if (QFile().exists(thingAnnotationsPathFilename)) {
                    allSourceFilesForThisImage.push_back(std::make_pair(
                        getThingAnnotationsPathFilename(imagesWithAnnotations[i].first),
                        getThingAnnotationsPathFilename(imagesWithAnnotations[i].second)
                    ));
                }
            }

            if (i == 0) { // it is enough to do this once
                if (QFile(getClassListFilename(currentWorkingFolder)).exists()) {
                    allSourceFilesForThisImage.push_back(std::make_pair(
                        classListFilename,
                        getClassListFilename(currentWorkingFolder)
                    ));
                }
            }

            for (int j = 0; j < allSourceFilesForThisImage.size(); ++j) {
                QString source = allSourceFilesForThisImage[j].first;
                QString sourceFull = allSourceFilesForThisImage[j].second;

                QString destination = dir + "/" + source;

                const auto last = destination.lastIndexOf("/");

                QString destinationDir = ".";

                if (last != -1) {
                    destinationDir = destination.left(last);

                    if (!QDir().exists(destinationDir)) {
                        if (!QDir().mkpath(destinationDir)) {
                            progress.setValue(count);
                            QMessageBox::critical(this, "Error creating directory", tr("Unable to create destination directory %1").arg(destinationDir));
                            return;
                        }
                    }
                }

                if (QFile().exists(destination)) {
                    if (!QFile().remove(destination)) {
                        progress.setValue(count);
                        QMessageBox::critical(this, "Error removing existing file", tr("Error removing existing file %1").arg(destination));
                        return;
                    }
                }

                if (!QFile().copy(sourceFull, destination)) {
                    progress.setValue(count);
                    QMessageBox::critical(this, "Error copying file", tr("Error copying file %1 to %2").arg(sourceFull, destinationDir));
                    return;
                }

                ++fileCount;
            }
        }

        if (!progress.wasCanceled()) {
            progress.setValue(count);
            QMessageBox::information(this,
                                     tr("Export complete"),
                                     tr("Exported %1 images (%2 files) to %3").arg(QString::number(count),
                                                                                   QString::number(fileCount),
                                                                                   dir));
        }
    }
}

void MainWindow::onFileClicked(QListWidgetItem* item)
{
    loadFile(item);
}

void MainWindow::onFileActivated(const QModelIndex& index)
{
    loadFile(files->item(index.row()));
}

void MainWindow::onFileItemChanged(QListWidgetItem* current, QListWidgetItem* previous)
{
    Q_UNUSED(previous);

    if (current) {
        loadFile(current);
    }
}

MainWindow::InferenceResults MainWindow::readResultsJSON(const QString& filename)
{
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
}

void MainWindow::loadFile(QListWidgetItem* item)
{
    if (item == currentImageFileItem) {
        return;
    }

    saveMaskIfDirty();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    currentImageFileItem = item;
    currentImageFile = item->data(fullnameRole).toString();

    QSettings settings(companyName, applicationName);
    settings.setValue("defaultFile", item->text());

    resetUndoBuffers();

    const auto readImage = [](const QString& filename) { return QImage(filename); };

    QFuture<QImage> imageFuture = QtConcurrent::run(readImage, currentImageFile);
    QFuture<QImage> maskFuture = QtConcurrent::run(readImage, getMaskFilename(currentImageFile));

    const auto readResults = [this](const QString& filename) {
        return readResultsJSON(filename);
    };

    auto thingAnnotationsFuture = QtConcurrent::run(readResults, getThingAnnotationsPathFilename(currentImageFile));
    auto resultsFuture = QtConcurrent::run(readResults, getInferenceResultPathFilename(currentImageFile));

    {
        QImage mask = maskFuture.result();
        currentMask = QPixmap::fromImage(mask);

        QResultImageView::DelayedRedrawToken delayedRedrawToken;

        originalImage = imageFuture.result();
        initCurrentImage(&delayedRedrawToken);
        image->setMask(mask, &delayedRedrawToken);

        currentThingAnnotations = thingAnnotationsFuture.result();
        currentResults = resultsFuture.result();

        if (!currentResults.error.isEmpty()) {
            QMessageBox::warning(nullptr, tr("Error"), currentResults.error);
        }

        if (currentThingAnnotations.error.isEmpty()) {
            for (auto& result : currentThingAnnotations.results) {
                result.pen.setWidth(2);
            }

            image->setThingAnnotations(currentThingAnnotations.results, &delayedRedrawToken);
        }
        else {
            QMessageBox::warning(nullptr, tr("Error"), currentThingAnnotations.error);
        }

        if (resultsVisible->isChecked()) {
            image->setResults(currentResults.results, &delayedRedrawToken);
        }
        resultsVisible->setEnabled(!currentResults.results.empty());
    }

    QApplication::restoreOverrideCursor();
}

void MainWindow::initCurrentImage(QResultImageView::DelayedRedrawToken* delayedRedrawToken)
{
    const bool channelSelectionsAvailable
            = allImageChannelsButton != nullptr
            && !originalImage.isGrayscale()
            && originalImage.depth() == 32;

    if (allImageChannelsButton) {
        allImageChannelsButton->setEnabled(channelSelectionsAvailable);
        redChannelButton->setEnabled(channelSelectionsAvailable);
        greenChannelButton->setEnabled(channelSelectionsAvailable);
        blueChannelButton->setEnabled(channelSelectionsAvailable);
        alphaChannelButton->setEnabled(channelSelectionsAvailable && originalImage.hasAlphaChannel());
    }

    if (!channelSelectionsAvailable || allImageChannelsButton->isChecked()) {
        image->setImage(originalImage, delayedRedrawToken);
    }
    else {
        const bool isRed = redChannelButton->isChecked();
        const bool isGreen = greenChannelButton->isChecked();
        const bool isBlue = blueChannelButton->isChecked();

        currentlyShownImage = originalImage.copy();

        const int rows = originalImage.height();
        const int cols = originalImage.width();

#pragma omp parallel for
        for (int row = 0; row < rows; ++row) {
            QRgb* rowPtr = reinterpret_cast<QRgb*>(currentlyShownImage.scanLine(row));
            for (int col = 0; col < cols; ++col) {
                QRgb& color = rowPtr[col];
                int value = 0;
                if (isRed) {
                    value = qRed(color);
                }
                else if (isGreen) {
                    value = qGreen(color);
                }
                else if (isBlue) {
                    value = qBlue(color);
                }
                else {
                    assert(alphaChannelButton->isChecked());
                    value = qAlpha(color);
                }
                color = qRgb(value, value, value);
            }
        }

        image->setImage(currentlyShownImage, delayedRedrawToken);
    }
}

bool MainWindow::conditionallyChangeFirstClass(const QString& oldName, QColor oldColor, const QString& newName, QColor newColor)
{
    if (!annotationClassItems.empty() && annotationClasses->count() > 0) {
        ClassItem& firstClass = annotationClassItems.front();
        if (firstClass.className == oldName && firstClass.color == oldColor) {
            firstClass.className = newName;
            firstClass.color = newColor;

            firstClass.listWidgetItem->setText(newName);
            setClassItemColor(firstClass.listWidgetItem, newColor);

            return true;
        }
    }

    return false;
}

void MainWindow::setClassItemColor(QListWidgetItem* listWidgetItem, QColor color)
{
    listWidgetItem->setBackgroundColor(color);

    const QColor hslColor = color.toHsl();

    listWidgetItem->setTextColor(hslColor.lightness() < 128 ? Qt::white : Qt::black);
}

void MainWindow::onAnnotateStuff(bool toggled)
{
    markingRadius->setEnabled(toggled);

    bucketFillCheckbox->setChecked(false); // Disable bucket fill, just to prevent accidents

    updateBucketFillCheckboxState();

    if (toggled) {
        image->setAnnotationMode(QResultImageView::AnnotationMode::Stuff);

        conditionallyChangeFirstClass(ignoreClassLabel, ignoreColor, cleanClassLabel, cleanColor);
    }
}

void MainWindow::onAnnotateThings(bool toggled)
{
    markingRadius->setEnabled(!toggled);

    updateBucketFillCheckboxState();

    if (toggled) {
        saveMaskIfDirty();

        image->setAnnotationMode(QResultImageView::AnnotationMode::Things);

        conditionallyChangeFirstClass(cleanClassLabel, cleanColor, ignoreClassLabel, ignoreColor);
    }

    updateUndoRedoMenuItemStatus();
}

void MainWindow::onPanButtonToggled(bool toggled)
{
    if (toggled) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Pan);
        bucketFillCheckbox->setChecked(false); // Disable bucket fill, just to prevent accidents
    }

    updateBucketFillCheckboxState();

    updateUndoRedoMenuItemStatus();
}


void MainWindow::updateBucketFillCheckboxState()
{
    bucketFillCheckbox->setEnabled(annotateStuff->isChecked() && !panButton->isChecked());
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

    image->setAnnotationsVisible(toggled);
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

void MainWindow::onChannelSelectionToggled(bool /*toggled*/)
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    initCurrentImage();
    QApplication::restoreOverrideCursor();
}

void MainWindow::onAnnotationUpdated()
{
    if (annotateThings->isChecked()) {
        annotationUndoBuffer.push_back(currentThingAnnotations.results);
        limitUndoOrRedoBufferSize(annotationUndoBuffer);

        currentThingAnnotations.results = image->getThingAnnotations();
        currentThingAnnotations.error.clear();

        annotationRedoBuffer.clear();

        updateUndoRedoMenuItemStatus();

        saveCurrentThingAnnotations();
    }

    if (annotateStuff->isChecked()) {
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
}

void MainWindow::saveCurrentThingAnnotations()
{
    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    {
        QJsonArray json;

        {
            for (const auto& annotationItem : currentThingAnnotations.results) {
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
            const QString filename = getThingAnnotationsPathFilename(currentImageFile);
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

    QApplication::restoreOverrideCursor();
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

QString MainWindow::getThingAnnotationsPathFilename(const QString& baseImageFilename)
{
    return baseImageFilename + "_annotation_paths.json";
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
            const QColor defaultColor(255, 255, 255, 128);
            const QColor color = QColorDialog::getColor(defaultColor, this, tr("Pick the color of the new class \"%1\"").arg(newClass), QColorDialog::ShowAlphaChannel);
            if (color.isValid()) {
                const auto minAlpha = 32;
                if (color.alpha() < minAlpha) {
                    QMessageBox::warning(this, tr("Invalid color"), tr("The alpha must be ≥ %1. (Now %2.)").arg(minAlpha, color.alpha()));
                }
                else {
                    QColor roundedColor = color;
                    const int alpha = color.alpha();

                    const auto roundComponent = [&](int component) {
                        // an experimental formula
                        return static_cast<int>(std::round(std::round(component * alpha / 255.0) * 255.0 / alpha));
                    };

                    roundedColor.setRed  (roundComponent(color.red()));
                    roundedColor.setGreen(roundComponent(color.green()));
                    roundedColor.setBlue (roundComponent(color.blue()));

                    if (roundedColor != color) {
                        const auto componentChangeAsString = [](int oldValue, int newValue) {
                            if (oldValue == newValue) {
                                return tr("No changes (still %1)").arg(QString::number(oldValue));
                            }
                            else {
                                return tr("%1 -> %2").arg(QString::number(oldValue), QString::number(newValue));
                            }
                        };

                        const auto title = tr("Need to round the new color");
                        const auto text = tr("We need to round the new color just a little:\n\n"
                                             "Red:\t%1\nGreen:\t%2\nBlue:\t%3\n\n"
                                             "Proceed?")
                                          .arg(componentChangeAsString(color.red(),   roundedColor.red()),
                                               componentChangeAsString(color.green(), roundedColor.green()),
                                               componentChangeAsString(color.blue(),  roundedColor.blue()));

                        const auto confirmation = QMessageBox::warning(this, title, text, QMessageBox::Yes | QMessageBox::No, QMessageBox::Yes);

                        if (confirmation != QMessageBox::Yes) {
                            return;
                        }
                    }

                    addNewClass(newClass, roundedColor);
                    saveClassList();
                }
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

    if (currentlySelectedAnnotationClassItem->text() == ignoreClassLabel) {
        QMessageBox::warning(this, "Error", tr("The special \"Ignore\" class cannot be renamed"));
        return;
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

    if (currentlySelectedAnnotationClassItem->text() == cleanClassLabel) {
        if (QMessageBox::warning(this, tr("Please confirm"), tr("It is not recommended to remove the special \"Clean\" class.\n\nProceed anyway?"),
                                 QMessageBox::Yes | QMessageBox::No, QMessageBox::No) != QMessageBox::Yes) {
            return;
        }
    }

    if (currentlySelectedAnnotationClassItem->text() == ignoreClassLabel) {
        QMessageBox::warning(this, "Error", tr("The special \"Ignore\" class cannot be removed"));
        return;
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

                if (row < annotationClassItems.size()) {
                    onAnnotationClassClicked(annotationClassItems[row].listWidgetItem);
                }
                else if (row - 1 < annotationClassItems.size()) {
                    onAnnotationClassClicked(annotationClassItems[row - 1].listWidgetItem);
                }
                panButton->setChecked(true);
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
    setClassItemColor(classItem.listWidgetItem, color);

    annotationClassItems.push_back(classItem);

    onAnnotationClassClicked(classItem.listWidgetItem);
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
            addNewClass(name == "<<ignore>>" ? ignoreClassLabel : name, color);
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
            if (classItem.className == ignoreClassLabel) {
                classObject["name"] = "<<ignore>>";
            }
            else {
                classObject["name"] = classItem.className;
            }
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
            const QString filename = files->item(row)->data(fullnameRole).toString();
            if (filename.length() > 0) {

                const auto maskFilename = getMaskFilename(filename);
                const auto thingAnnotationsPathFilename = getThingAnnotationsPathFilename(filename);

                const auto hasAnnotationFiles = [&]() {
                    if (files->item(row)->textColor() == Qt::gray) {
                        return false;
                    }

                    return QFile().exists(thingAnnotationsPathFilename)
                        || QFile().exists(maskFilename);
                };

                if (hasAnnotationFiles()) {
                    bool hasActualStuffAnnotations = false;
                    bool hasActualThingsAnnotations = false;

                    if (files->item(row)->textColor() != Qt::gray) {
                        QFuture<QImage> maskFuture;

                        if (QFile().exists(maskFilename)) {
                            const auto readImage = [](const QString& filename) { return QImage(filename); };
                            maskFuture = QtConcurrent::run(readImage, getMaskFilename(filename));
                        }

                        if (QFile().exists(thingAnnotationsPathFilename)) {
                            if (!readResultsJSON(thingAnnotationsPathFilename).results.empty()) {
                                hasActualThingsAnnotations = true;
                            }
                        }

                        if (QFile().exists(maskFilename)) {
                            QImage mask = maskFuture.result();
                            if (mask.height() > 0 && mask.width() > 0) {
                                if (mask.format() == QImage::Format_ARGB32) {
                                    QApplication::setOverrideCursor(Qt::WaitCursor);
                                    std::vector<uchar> emptyRow(mask.width() * 4);
                                    for (int row = 0, rows = mask.height(); row < rows; ++row) {
                                        const uchar* rowPtr = mask.scanLine(row);
                                        if (memcmp(rowPtr, emptyRow.data(), emptyRow.size()) != 0) {
                                            hasActualStuffAnnotations = true;
                                            break;
                                        }
                                    }
                                    QApplication::restoreOverrideCursor();
                                }
                                else {
                                    QMessageBox::warning(this,
                                                         tr("Unexpected mask image format"),
                                                         tr("Unexpected mask image format %1 in image %2").arg(QString::number(mask.format())), maskFilename);
                                }
                            }
                        }
                    }

                    const bool hasActualAnnotations = hasActualStuffAnnotations || hasActualThingsAnnotations;

                    const bool isOkToProceed = !hasActualAnnotations
                            || QMessageBox::Yes == QMessageBox::question(this, tr("Are you sure?"),
                                tr("You have actual annotations for image %1.\n\n"
                                   "Do you really want to delete them?\n\n"
                                   "This operation is irreversible.").arg(filename),
                                QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

                    if (isOkToProceed) {
                        const auto deleteAnnotationFile = [this](QString filename) {
                            if (QFile().exists(filename)) {
                                if (!QFile().remove(filename)) {
                                    QMessageBox::warning(this, tr("Error"), tr("Unable to remove file %1").arg(filename));
                                    return false;
                                }
                            }
                            return true;
                        };

                        const auto deleteAnnotations = [&]() {
                            if (!deleteAnnotationFile(thingAnnotationsPathFilename)) {
                                return false;
                            }
                            image->setThingAnnotations(QResultImageView::Results());

                            if (!deleteAnnotationFile(maskFilename)) {
                                return false;
                            }
                            image->setMask(QImage());

                            return true;
                        };

                        if (deleteAnnotations()) {
                            files->item(row)->setTextColor(Qt::gray);
                        }
                    }
                }
                else {
                    const auto removeFile = [event, this](const QString& filename) {

                        const auto confirmAndDeleteFile = [this](const QString& filename) {
                            const bool isOkToProceed
                                    = QMessageBox::Yes == QMessageBox::question(this, tr("Are you sure?"),
                                        tr("This will permanently delete the file:\n%1").arg(filename));
                            if (isOkToProceed) {
                                QFile::remove(filename);
                                return true;
                            }
                            else {
                                return false;
                            }
                        };

    #ifdef WIN32
                        if (event->modifiers() & Qt::ShiftModifier) {
                            return confirmAndDeleteFile(filename);
                        }
                        else {
                            std::vector<wchar_t> buffer(filename.length() + 1);
                            filename.toWCharArray(buffer.data());
                            buffer.back() = L'\0';
                            try {
                                return move_file_to_trash(buffer.data());
                            }
                            catch (std::exception& e) {
                                QString error = QString::fromLatin1(e.what());
                                error.replace(QString("Unable to move the file to the recycle bin; error code ="), tr("Unable to move file %1 to the recycle bin.\n\nError code =").arg(filename));
                                QMessageBox::warning(this, tr("Error"), error);
                                return false;
                            }
                        }
    #else // WIN32
                        return confirmAndDeleteFile(filename);
    #endif // WIN32
                    };

                    const auto removeImageFromList = [row, this]() {
                        QListWidgetItem* item = files->takeItem(row);
                        delete item;
                        loadFile(files->item(files->currentRow()));
                    };

                    if (removeFile(filename)) {
                        removeImageFromList();
                    }
                }
            }
        }
    }
}

void MainWindow::onUndo()
{
    if (annotateThings->isChecked() && !annotationUndoBuffer.empty()) {
        annotationRedoBuffer.push_back(currentThingAnnotations.results);
        limitUndoOrRedoBufferSize(annotationRedoBuffer);

        currentThingAnnotations.results = annotationUndoBuffer.back();

        annotationUndoBuffer.pop_back();

        image->setThingAnnotations(currentThingAnnotations.results);

        updateUndoRedoMenuItemStatus();

        saveCurrentThingAnnotations();
    }

    if (annotateStuff->isChecked() && !maskUndoBuffer.empty()) {
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
    if (annotateThings->isChecked() && !annotationRedoBuffer.empty()) {
        annotationUndoBuffer.push_back(currentThingAnnotations.results);
        limitUndoOrRedoBufferSize(annotationUndoBuffer);

        currentThingAnnotations.results = annotationRedoBuffer.back();

        annotationRedoBuffer.pop_back();

        image->setThingAnnotations(currentThingAnnotations.results);

        updateUndoRedoMenuItemStatus();

        saveCurrentThingAnnotations();
    }

    if (annotateStuff->isChecked() && !maskRedoBuffer.empty()) {
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
    if (annotateThings->isChecked()) {
        annotationUndoBuffer.clear();
        annotationRedoBuffer.clear();
    }
    if (annotateStuff->isChecked()) {
        maskUndoBuffer.clear();
        maskRedoBuffer.clear();
    }
    updateUndoRedoMenuItemStatus();
}

void MainWindow::updateUndoRedoMenuItemStatus()
{
    if (annotateThings->isChecked()) {
        ui->actionUndo->setEnabled(!annotationUndoBuffer.empty());
        ui->actionRedo->setEnabled(!annotationRedoBuffer.empty());
    }
    if (annotateStuff->isChecked()) {
        ui->actionUndo->setEnabled(!maskUndoBuffer.empty());
        ui->actionRedo->setEnabled(!maskRedoBuffer.empty());
    }
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

void MainWindow::limitUndoOrRedoBufferSize(std::deque<std::vector<QResultImageView::Result>>& buffer)
{
    const size_t maxBufferSize = 1024;
    while (buffer.size() > maxBufferSize) {
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
