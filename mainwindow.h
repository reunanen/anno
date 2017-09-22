#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QTreeWidget;
class QTreeWidgetItem;
class QResultImageView;

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
    void openFolder(const QString& dir);

    Ui::MainWindow* ui;
    QTreeWidget* files = nullptr;
    QResultImageView* image = nullptr;
};

#endif // MAINWINDOW_H
