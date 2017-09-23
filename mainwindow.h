#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QTreeWidget;
class QTreeWidgetItem;
class QSpinBox;
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
    void onOpenFolder();
    void onFileClicked(QTreeWidgetItem* item, int column);
    void onToolClicked(QTreeWidgetItem* item, int column);
    void onMarkingRadiusChanged(int i);
    void onMaskUpdated();
    void onSaveMask();

private:
    void createFileList();
    void createToolList();
    void createImageView();

    void openFolder(const QString& dir);
    void saveMaskIfDirty();
    void saveMask();

    Ui::MainWindow* ui;
    QTreeWidget* files = nullptr;
    QTreeWidget* tools = nullptr;
    QResultImageView* image = nullptr;

    QTreeWidgetItem* panToolItem = nullptr;
    QTreeWidgetItem* markDefectsToolItem = nullptr;
    QTreeWidgetItem* eraseToolItem = nullptr;

    QSpinBox* markingRadius = nullptr;

    QString currentImageFile;

    bool maskDirty = false;
    int saveMaskPendingCounter = 0;
};

#endif // MAINWINDOW_H
