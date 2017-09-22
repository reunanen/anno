#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QTreeWidget;
class QTreeWidgetItem;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event);

private slots:
    void init();
    void createFileList();
    void onOpenFolder();
    void onFileClicked(QTreeWidgetItem* item, int column);

private:
    Ui::MainWindow* ui;
    QTreeWidget* files = nullptr;
};

#endif // MAINWINDOW_H
