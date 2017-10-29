#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QTreeWidget;
class QTreeWidgetItem;
class QSpinBox;
class QCheckBox;
class QPushButton;

#include "QResultImageView/QResultImageView.h"

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
    void initImageIO();
    void onOpenFolder();
    void onFileClicked(QTreeWidgetItem* item, int column);
    void onFileActivated(const QModelIndex& index);
    void onToolClicked(QTreeWidgetItem* item, int column);
    void onMarkingRadiusChanged(int i);
    void onMarkingsVisible(bool toggled);
    void onResultsVisible(bool toggled);
    void onMaskUpdated();
    void onPostponeMaskUpdate();
    void onSaveMask();
    void onAddClass();
    void onRemoveClass();

private:
    void createFileList();
    void createToolList();
    void createImageView();

    void openFolder(const QString& dir);
    void saveMaskIfDirty();
    void saveMask();

    void loadFile(const QString& filename);

    static QString getMaskFilenameSuffix();
    static QString getMaskFilename(const QString& baseImageFilename);

    static QString getInferenceResultFilenameSuffix();
    static QString getInferenceResultPathFilename(const QString& baseImageFilename);

    Ui::MainWindow* ui;
    QTreeWidget* files = nullptr;
    QTreeWidget* tools = nullptr;
    QResultImageView* image = nullptr;

    QTreeWidgetItem* panToolItem = nullptr;
    QTreeWidgetItem* markDefectsToolItem = nullptr;
    QTreeWidgetItem* markCleanToolItem = nullptr;
    QTreeWidgetItem* eraseMarkingsToolItem = nullptr;

    QSpinBox* markingRadius = nullptr;
    QCheckBox* markingsVisible = nullptr;
    QPushButton* addClassButton = nullptr;
    QPushButton* removeClassButton = nullptr;
    QCheckBox* resultsVisible = nullptr;

    QString currentImageFile;
    std::vector<QResultImageView::Result> currentResults;

    bool maskDirty = false;
    int saveMaskPendingCounter = 0;
};

#endif // MAINWINDOW_H
