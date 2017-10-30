#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSettings>
#include <QTimer>
#include <QListWidget>
#include <QPainter>
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

    setWindowTitle("anno");

    const QSettings settings(companyName, applicationName);
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

    connect(ui->actionOpenFolder, SIGNAL(triggered()), this, SLOT(onOpenFolder()));
    connect(ui->actionExit, SIGNAL(triggered()), this, SLOT(close()));

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

    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

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

    setCentralWidget(image);

    connect(image, SIGNAL(maskUpdated()), this, SLOT(onMaskUpdated()));
    connect(image, SIGNAL(panned()), this, SLOT(onPostponeMaskUpdate()));
    connect(image, SIGNAL(zoomed()), this, SLOT(onPostponeMaskUpdate()));
}

void MainWindow::createFileList()
{
    QDockWidget* fileListDockWidget = new QDockWidget(tr("Files"), this);
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

        removeClassButton = new QPushButton(tr("Remove selected class"), this);
        connect(removeClassButton, SIGNAL(clicked()), this, SLOT(onRemoveClass()));

        QVBoxLayout* classButtonsLayout = new QVBoxLayout(classButtonsWidget);
        classButtonsLayout->addWidget(addClassButton);
        classButtonsLayout->addWidget(renameClassButton);
        classButtonsLayout->addWidget(removeClassButton);
    }

    QGroupBox* leftMouseButtonActions = new QGroupBox(tr("Left mouse button actions"));
    {
        QVBoxLayout* leftMouseButtonActionsLayout = new QVBoxLayout;

        panButton = new QRadioButton(tr("&Pan"));
        annotateButton = new QRadioButton(tr("&Annotate"));
        eraseAnnotationsButton = new QRadioButton(tr("&Erase annotations"));

        annotationClasses = new QListWidget(this);
        annotationClasses->setFont(QFont("Arial", 10, 0));

        panButton->setChecked(true);

        leftMouseButtonActionsLayout->addWidget(panButton);
        leftMouseButtonActionsLayout->addWidget(annotateButton);
        leftMouseButtonActionsLayout->addWidget(annotationClasses);
        leftMouseButtonActionsLayout->addWidget(classButtonsWidget);
        leftMouseButtonActionsLayout->addWidget(eraseAnnotationsButton);

        connect(panButton, SIGNAL(toggled(bool)), this, SLOT(onPanButtonToggled(bool)));
        connect(eraseAnnotationsButton, SIGNAL(toggled(bool)), this, SLOT(onEraseAnnotationsButtonToggled(bool)));
        connect(annotateButton, SIGNAL(toggled(bool)), this, SLOT(onAnnotateButtonToggled(bool)));
        connect(annotationClasses, SIGNAL(itemClicked(QListWidgetItem*)), this, SLOT(onAnnotationClassClicked(QListWidgetItem*)));

        leftMouseButtonActions->setLayout(leftMouseButtonActionsLayout);
    }

    {
        resultsVisible = new QCheckBox("&Results visible", this);
        resultsVisible->setChecked(true);

        connect(resultsVisible, SIGNAL(toggled(bool)), this, SLOT(onResultsVisible(bool)));
    }

    layout->addWidget(markingsVisible);
    layout->addWidget(markingRadiusWidget);
    layout->addWidget(leftMouseButtonActions);
    layout->addSpacing(10);
    layout->addWidget(resultsVisible);
}

void MainWindow::onOpenFolder()
{
    QSettings settings(companyName, applicationName);
    const QString defaultDirectory = settings.value("defaultDirectory").toString();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Select a folder containing some images"), defaultDirectory,
                                                          QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    if (!dir.isEmpty()) {
        settings.setValue("defaultDirectory", dir);

        openFolder(dir);
    }
}

void MainWindow::openFolder(const QString& dir)
{
    if (!files) {
        createFileList();
    }

    files->clear();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

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

    currentWorkingFolder = dir;

    loadClassList();

    if (annotationClassItems.empty()) {
        // Add sample classes
        addNewClass("Clean", QColor(0, 255, 0, 64));
        addNewClass("Defect", QColor(255, 0, 0, 128));
    }

    QApplication::restoreOverrideCursor();
}

void MainWindow::onFileClicked(QListWidgetItem* item)
{
    loadFile(item->text());
}

void MainWindow::onFileActivated(const QModelIndex& index)
{
    loadFile(files->item(index.row())->text());
}

void MainWindow::loadFile(const QString& filename)
{
    saveMaskIfDirty();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    currentImageFile = filename;

    const auto readImage = [](const QString& filename) { return QImage(filename); };

    QFuture<QImage> imageFuture = QtConcurrent::run(readImage, currentImageFile);
    QFuture<QImage> maskFuture = QtConcurrent::run(readImage, getMaskFilename(currentImageFile));

    const auto readResults = [](const QString& filename) {
        std::vector<QResultImageView::Result> results;

        QFile file;
        file.setFileName(filename);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
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
            for (int j = 0, end = paths.size(); j < end; ++j) {
                const QJsonArray path = paths[j].toArray();
                for (int k = 0, end = path.size(); k < end; ++k) {
                    const QJsonObject point = path[k].toObject();
                    result.contour.push_back(QPointF(point.value("x").toDouble(), point.value("y").toDouble()));

                }
                results.push_back(result);
                result.contour.clear();
            }
        }

        return results;
    };

    auto resultsFuture = QtConcurrent::run(readResults, getInferenceResultPathFilename(filename));

    image->setImageAndMask(imageFuture.result(), maskFuture.result());

    currentResults = resultsFuture.result();
    if (resultsVisible->isChecked()) {
        image->setResults(currentResults);
    }
    resultsVisible->setEnabled(!currentResults.empty());

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

void MainWindow::onResultsVisible(bool toggled)
{
    onPostponeMaskUpdate();

    if (toggled) {
        image->setResults(currentResults);
    }
    else {
        std::vector<QResultImageView::Result> emptyResults;
        image->setResults(emptyResults);
    }
}

void MainWindow::onMaskUpdated()
{
    maskDirty = true;

    ++saveMaskPendingCounter;

    QTimer::singleShot(10000, this, SLOT(onSaveMask()));
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
                annotationClasses->takeItem(row);
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
}
