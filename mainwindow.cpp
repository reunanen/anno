#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QSettings>
#include <QTimer>
#include <QTreeWidget>
#include <QPainter>
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

    QSettings settings(companyName, applicationName);
    restoreGeometry(settings.value("mainWindowGeometry").toByteArray());
    restoreState(settings.value("mainWindowState").toByteArray());

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

    QMainWindow::closeEvent(event);
}

void MainWindow::init()
{
}
