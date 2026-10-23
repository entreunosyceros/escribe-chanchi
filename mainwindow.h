#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTextEdit>
#include <QWebEngineView>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QStringList>
#include <QList>
#include <QLocalServer>

class QCloseEvent;

//class MarkdownHighlighter;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionOpen_triggered();
    void on_actionSave_triggered();
    void on_actionSave_As_triggered();
    void on_actionNew_triggered();
    void on_actionExit_triggered();
    void on_actionAbout_triggered();
    void on_actionToggle_Preview_triggered();
    void onTextChanged();
    void openRecentFile();
    void updateRecentFileActions();
    void handleNewConnection();
    void restoreFromTray();

private:
    QTextEdit *textEdit;
    QWebEngineView *webView;
    QSplitter *splitter;
    //MarkdownHighlighter *highlighter;
    QString currentFilePath;
    bool isModified;
    
    // Funcionalidad de archivos recientes
    QStringList recentFiles;
    QList<QAction*> recentFileActions;
    QMenu *recentFilesMenu;
    static const int MaxRecentFiles = 5;
    
    void setupUI();
    void setupMenuBar();
    void setupRecentFilesMenu();
    void updatePreview();
    void loadRecentFiles();
    void saveRecentFiles();
    void addToRecentFiles(const QString &fileName);
    void openFile(const QString &fileName);
    void saveFile(const QString &fileName);
    void newFile();
    bool maybeSave();
    void setCurrentFile(const QString &fileName);
    void updateWindowTitle();
    void setupSingleInstance();

    QLocalServer *localServer;

protected:
    void closeEvent(QCloseEvent *event) override;
};

#endif // MAINWINDOW_H