#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <messaging/claim/PostOffice.h>

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
#include <future>

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
    void onExport();
    void onFileClicked(QListWidgetItem* item);
    void onFileActivated(const QModelIndex& index);
    void onFileItemChanged(QListWidgetItem* current, QListWidgetItem* previous);
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
    void onChannelSelectionToggled(bool toggled);
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
    void onIdle();
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
    void initCurrentImage(QResultImageView::DelayedRedrawToken* delayedRedrawToken = nullptr);

    static QString getMaskFilenameSuffix();
    static QString getMaskFilename(const QString& baseImageFilename);

    static QString getThingAnnotationsPathFilenameSuffix();
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

    bool conditionallyChangeFirstClass(const QString& oldName, QColor oldColor, const QString& newName, QColor newColor);
    static void setClassItemColor(QListWidgetItem* listWidgetItem, QColor color);

    void markMissingFilesRed(const std::chrono::milliseconds& maxDuration);

    struct InferenceResults
    {
        std::vector<QResultImageView::Result> results;
        QString error;
    };

    InferenceResults readResultsJSON(const QString& filename);

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

    QRadioButton* allImageChannelsButton = nullptr;
    QRadioButton* redChannelButton = nullptr;
    QRadioButton* greenChannelButton = nullptr;
    QRadioButton* blueChannelButton = nullptr;
    QRadioButton* alphaChannelButton = nullptr;

    QString currentWorkingFolder;
    QListWidgetItem* currentImageFileItem = nullptr;
    QString currentImageFile;

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

    const QString cleanClassLabel = tr("Clean");
    const QString ignoreClassLabel = tr("Ignore");

    QImage originalImage;
    QImage currentlyShownImage;

    claim::PostOffice postOffice;
};

#endif // MAINWINDOW_H
