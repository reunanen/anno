#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSettings>
#include <QTimer>
#include <QTreeWidget>
#include <QPainter>
#include <QFileDialog>
#include <QDirIterator>
#include <QDockWidget>
#include <QSpinBox>
#include <QCheckBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFuture>
#include <QtConcurrent/QtConcurrentRun>
#include <QMessageBox>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
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
    createFileList();
    createToolList();
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

    files = new QTreeWidget(this);

    fileListDockWidget->setWidget(files);
    addDockWidget(Qt::LeftDockWidgetArea, fileListDockWidget);

    files->setColumnCount(1);
    files->setFont(QFont("Arial", 8, 0));

    QStringList columns;
    columns.append(tr("Name"));

    QTreeWidgetItem* headerItem = new QTreeWidgetItem(columns);
    headerItem->setTextAlignment(0, Qt::AlignLeft);
    files->setHeaderItem(headerItem);

    connect(files, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(onFileClicked(QTreeWidgetItem*,int)));
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

        markingsVisible = new QCheckBox("Markings visible", this);
        markingsVisible->setChecked(true);

        connect(markingsVisible, SIGNAL(toggled(bool)), this, SLOT(onMarkingsVisible(bool)));

        resultsVisible = new QCheckBox("Results visible", this);
        resultsVisible->setChecked(true);

        connect(resultsVisible, SIGNAL(toggled(bool)), this, SLOT(onResultsVisible(bool)));

        QHBoxLayout* markingRadiusLayout = new QHBoxLayout(markingRadiusWidget);
        markingRadiusLayout->addWidget(new QLabel(tr("Marking radius"), this));
        markingRadiusLayout->addWidget(markingRadius);
    }

    tools = new QTreeWidget(this);

    tools->setColumnCount(1);
    tools->setFont(QFont("Arial", 10, 0));

    QStringList columns;
    columns.append(tr(""));

    QTreeWidgetItem* headerItem = new QTreeWidgetItem(columns);
    headerItem->setTextAlignment(0, Qt::AlignLeft);
    tools->setHeaderItem(headerItem);

    QList<QTreeWidgetItem *> items;

    {
        columns[0] = "Pan";
        panToolItem = new QTreeWidgetItem(tools, columns);
        items.append(panToolItem);
    }

    {
        columns[0] = "Mark clean";
        markCleanToolItem = new QTreeWidgetItem(tools, columns);
        items.append(markCleanToolItem);
    }

    {
        columns[0] = settings.value("markDefectsLabel", "Mark defects").toString();
        markDefectsToolItem = new QTreeWidgetItem(tools, columns);
        items.append(markDefectsToolItem);
    }

    {
        columns[0] = "Erase markings";
        eraseMarkingsToolItem = new QTreeWidgetItem(tools, columns);
        items.append(eraseMarkingsToolItem);
    }

    tools->insertTopLevelItems(0, items);
    panToolItem->setSelected(true);

    connect(tools, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(onToolClicked(QTreeWidgetItem*,int)));

    layout->addWidget(markingsVisible);
    layout->addWidget(markingRadiusWidget);
    layout->addWidget(tools);
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

    QList<QTreeWidgetItem *> items;

    QStringList columns;
    while (columns.count() < files->columnCount()) {
        columns.append("");
    }

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
            columns[0] = filename;
            QTreeWidgetItem* item = new QTreeWidgetItem(files, columns);
            items.append(item);
        }
    }

    files->insertTopLevelItems(0, items);

    QApplication::restoreOverrideCursor();
}

void MainWindow::onFileClicked(QTreeWidgetItem* item, int column)
{
    loadFile(item->text(column));
}

void MainWindow::onFileActivated(const QModelIndex& index)
{
    loadFile(files->topLevelItem(index.row())->text(0));
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

    QApplication::restoreOverrideCursor();
}

void MainWindow::onToolClicked(QTreeWidgetItem* item, int column)
{
    if (item == panToolItem) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Pan);
    }
    else if (item == markDefectsToolItem) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::MarkDefect);
    }
    else if (item == markCleanToolItem) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::MarkClean);
    }
    else if (item == eraseMarkingsToolItem) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::EraseMarkings);
    }
}

void MainWindow::onMarkingRadiusChanged(int i)
{
    image->setMarkingRadius(i);
}

void MainWindow::onMarkingsVisible(bool toggled)
{
    image->setMaskVisible(toggled);
}

void MainWindow::onResultsVisible(bool toggled)
{
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
