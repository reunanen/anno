#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
class MainWindow;
}

class QListWidget;
class QListWidgetItem;
class QSpinBox;
class QCheckBox;
class QRadioButton;
class QPushButton;

#include "QResultImageView/QResultImageView.h"
#include <deque>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void init();
    void initImageIO();
    void onOpenFolder();
    void onOpenRecentFolder();
    void onFileClicked(QListWidgetItem* item);
    void onFileActivated(const QModelIndex& index);
    void onPanButtonToggled(bool toggled);
    void onAnnotateButtonToggled(bool toggled);
    void onEraseAnnotationsButtonClicked();
    void onRightMousePanButtonToggled(bool toggled);
    void onRightMouseResetViewButtonToggled(bool toggled);
    void onMakeAnnotationsVisible(bool visible);
    void onAnnotationsVisible(bool toggled);
    void onResultsVisible(bool toggled);
    void onYardstickVisible(bool toggled);
    void onAnnotationUpdated();
    void onUndo();
    void onRedo();
    void onAbout();

private:
    void createFileList();
    void createToolList();
    void createImageView();

    void updateResults(QResultImageView::DelayedRedrawToken* delayedRedrawToken = nullptr);

    void openFolder(const QString& dir);
    void addRecentFolderMenuItem(const QString& dir);
    void saveRecentFolders();
    void saveCurrentAnnotation();

    void loadFile(QListWidgetItem* item);

    static QString getAnnotationPathFilename(const QString& baseImageFilename);
    static QString getInferenceResultPathFilename(const QString& baseImageFilename);

    void resetUndoBuffers();
    void updateUndoRedoMenuItemStatus();
    void limitUndoOrRedoBufferSize(std::deque<std::vector<QResultImageView::Result>>& buffer);

    Ui::MainWindow* ui;
    QListWidget* files = nullptr;
    QResultImageView* image = nullptr;

    struct ClassItem {
        QString className;
        QColor color;
        QListWidgetItem* listWidgetItem = nullptr;
    };

    QRadioButton* panButton = nullptr;
    QRadioButton* annotateButton = nullptr;
    QPushButton* eraseAnnotationsButton = nullptr;

    QCheckBox* annotationsVisible = nullptr;

    QRadioButton* rightMousePanButton = nullptr;
    QRadioButton* rightMouseResetViewButton = nullptr;

    QCheckBox* resultsVisible = nullptr;
    QCheckBox* yardstickVisible = nullptr;

    QString currentWorkingFolder;
    QListWidgetItem* currentImageFileItem = nullptr;
    QString currentImageFile;

    struct InferenceResults
    {
        std::vector<QResultImageView::Result> results;
        QString error;
    };

    InferenceResults currentResults;
    InferenceResults currentAnnotations;
    std::vector<QResultImageView::Result> currentlyShownPaths;

    QMenu* recentFoldersMenu;

    std::deque<std::vector<QResultImageView::Result>> annotationUndoBuffer;
    std::deque<std::vector<QResultImageView::Result>> annotationRedoBuffer;

    QWidget* aboutDialog = nullptr;

    bool reverseFileOrder = false;
};

#endif // MAINWINDOW_H
