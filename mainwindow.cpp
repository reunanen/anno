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
#include <assert.h>

#include "QResultImageView/QResultImageView.h"

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
    openFolder(defaultDirectory);

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
        columns[0] = "Mark defect";
        markDefectsToolItem = new QTreeWidgetItem(tools, columns);
        items.append(markDefectsToolItem);
    }

    {
        columns[0] = "Erase";
        eraseToolItem = new QTreeWidgetItem(tools, columns);
        items.append(eraseToolItem);
    }

    tools->insertTopLevelItems(0, items);
    panToolItem->setSelected(true);

    connect(tools, SIGNAL(itemClicked(QTreeWidgetItem*,int)), this, SLOT(onToolClicked(QTreeWidgetItem*,int)));

    layout->addWidget(markingsVisible);
    layout->addWidget(markingRadiusWidget);
    layout->addWidget(tools);
}

void MainWindow::onOpenFolder()
{
    QSettings settings(companyName, applicationName);
    const QString defaultDirectory = settings.value("defaultDirectory").toString();
    const QString dir = QFileDialog::getExistingDirectory(this, tr("Open Folder"), defaultDirectory,
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
    saveMaskIfDirty();

    QApplication::setOverrideCursor(Qt::WaitCursor);
    QApplication::processEvents(); // actually update the cursor

    currentImageFile = item->text(column);

    const auto readImage = [](const QString& filename) { return QImage(filename); };

    QFuture<QImage> imageFuture = QtConcurrent::run(readImage, currentImageFile);
    QFuture<QImage> maskFuture = QtConcurrent::run(readImage, getMaskFilename(currentImageFile));

    image->setImageAndMask(imageFuture.result(), maskFuture.result());

    QApplication::restoreOverrideCursor();
}

void MainWindow::onToolClicked(QTreeWidgetItem* item, int column)
{
    if (item == panToolItem) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Pan);
    }
    else if (item == markDefectsToolItem) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Mark);
    }
    else if (item == eraseToolItem) {
        image->setLeftMouseMode(QResultImageView::LeftMouseMode::Erase);
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
