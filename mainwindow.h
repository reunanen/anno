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
    void onAnnotateStuff(bool toggled);
    void onAnnotateThings(bool toggled);
    void onPanButtonToggled(bool toggled);
    void onAnnotateButtonToggled(bool toggled);
    void onBucketFillToggled(bool toggled);
    void onEraseAnnotationsButtonToggled(bool toggled);
    void onAnnotationClassClicked(QListWidgetItem* item);
    void onMarkingRadiusChanged(int i);
    void onMarkingsVisible(bool toggled);
    void onRightMousePanButtonToggled(bool toggled);
    void onRightMouseEraseAnnotationsButtonToggled(bool toggled);
    void onRightMouseResetViewButtonToggled(bool toggled);
    void onResultsVisible(bool toggled);
    void onYardstickVisible(bool toggled);
    void onAnnotationUpdated();
    void saveCurrentThingAnnotations();
    void onPostponeMaskUpdate();
    void onSaveMask();
    void onAddClass();
    void onRenameClass();
    void onRemoveClass();
    void onUndo();
    void onRedo();
    void onNewMarkingRadius(int newMarkingRadius);
    void onAnnotationsVisible(bool visible);
    void onRestoreDefaultWindowPositions();
    void onAbout();

private:
    void createFileList();
    void createToolList();
    void createImageView();

    void openFolder(const QString& dir);
    void addRecentFolderMenuItem(const QString& dir);
    void saveRecentFolders();
    void saveMaskIfDirty();
    void saveMask();

    void loadFile(QListWidgetItem* item);

    static QString getMaskFilenameSuffix();
    static QString getMaskFilename(const QString& baseImageFilename);

    static QString getThingAnnotationsPathFilename(const QString& baseImageFilename);

    static QString getInferenceResultFilenameSuffix();
    static QString getInferenceResultPathFilename(const QString& baseImageFilename);

    void addNewClass(const QString& className, QColor color);

    void loadClassList();
    void saveClassList() const;

    void resetUndoBuffers();
    void updateUndoRedoMenuItemStatus();
    void limitUndoOrRedoBufferSize(std::deque<QPixmap>& buffer);
    void limitUndoOrRedoBufferSize(std::deque<std::vector<QResultImageView::Result>>& buffer);

    void updateBucketFillCheckboxState();

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
    QCheckBox* bucketFillCheckbox = nullptr;
    QRadioButton* eraseAnnotationsButton = nullptr;

    QListWidget* annotationClasses = nullptr;
    QListWidgetItem* currentlySelectedAnnotationClassItem = nullptr;
    QColor currentAnnotationColor = Qt::transparent;

    std::vector<ClassItem> annotationClassItems;

    QSpinBox* markingRadius = nullptr;
    QCheckBox* markingsVisible = nullptr;

    QRadioButton* annotateStuff = nullptr;
    QRadioButton* annotateThings = nullptr;

    QPushButton* addClassButton = nullptr;
    QPushButton* renameClassButton = nullptr;
    QPushButton* removeClassButton = nullptr;

    QRadioButton* rightMousePanButton = nullptr;
    QRadioButton* rightMouseEraseAnnotationsButton = nullptr;
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
    InferenceResults currentThingAnnotations;
    std::vector<QResultImageView::Result> currentlyShownPaths;

    bool maskDirty = false;
    int saveMaskPendingCounter = 0;

    QMenu* recentFoldersMenu;

    QPixmap currentMask;
    std::deque<QPixmap> maskUndoBuffer;
    std::deque<QPixmap> maskRedoBuffer;

    std::deque<std::vector<QResultImageView::Result>> annotationUndoBuffer;
    std::deque<std::vector<QResultImageView::Result>> annotationRedoBuffer;

    QWidget* aboutDialog = nullptr;

    bool reverseFileOrder = false;

    QByteArray defaultGeometry;
    QByteArray defaultState;
};

#endif // MAINWINDOW_H
