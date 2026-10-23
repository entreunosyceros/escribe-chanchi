#include "mainwindow.h"
//#include "markdownhighlighter.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QTextStream>
#include <QCloseEvent>
#include <QSettings>
#include <QFileInfo>
#include <QSplitter>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QWebEngineView>
#include <QMenuBar>
#include <QStatusBar>
#include <QApplication>
#include <QRegularExpression>
#include <QLocalSocket>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , currentFilePath("")
    , isModified(false)
    , localServer(nullptr)
{
    setupSingleInstance();
    setupUI();
    setupMenuBar();
    setupRecentFilesMenu();
    loadRecentFiles();
    updateRecentFileActions();
    
    // Configurar ventana
    setWindowTitle("Escribe Chanchi - Editor de Markdown");
    resize(1200, 800);
    
    // Crear nuevo archivo al inicio
    newFile();
}

MainWindow::~MainWindow()
{
    saveRecentFiles();
    if (localServer) {
        localServer->close();
        delete localServer;
    }
}

void MainWindow::setupUI()
{
    // Crear widget central
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Crear splitter principal
    splitter = new QSplitter(Qt::Horizontal, this);
    
    // Crear editor de texto
    textEdit = new QTextEdit(this);
    textEdit->setPlaceholderText("Escribe tu markdown aquí...");
    
    // Crear vista web
    webView = new QWebEngineView(this);
    
    // Agregar widgets al splitter
    splitter->addWidget(textEdit);
    splitter->addWidget(webView);
    splitter->setSizes({600, 600});
    
    // Layout principal
    QHBoxLayout *layout = new QHBoxLayout(centralWidget);
    layout->addWidget(splitter);
    layout->setContentsMargins(0, 0, 0, 0);
    
    // Conectar cambios de texto
    connect(textEdit, &QTextEdit::textChanged, this, &MainWindow::onTextChanged);
    connect(textEdit, &QTextEdit::textChanged, this, &MainWindow::updatePreview);
}

void MainWindow::setupMenuBar()
{
    // Menú Archivo
    QMenu *fileMenu = menuBar()->addMenu("&Archivo");
    
    QAction *newAction = fileMenu->addAction("&Nuevo");
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &MainWindow::on_actionNew_triggered);
    
    QAction *openAction = fileMenu->addAction("&Abrir");
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MainWindow::on_actionOpen_triggered);
    
    QAction *saveAction = fileMenu->addAction("&Guardar");
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MainWindow::on_actionSave_triggered);
    
    QAction *saveAsAction = fileMenu->addAction("Guardar &como...");
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MainWindow::on_actionSave_As_triggered);
    
    fileMenu->addSeparator();
    
    QAction *exitAction = fileMenu->addAction("&Salir");
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MainWindow::on_actionExit_triggered);
    
    // Menú Ver
    QMenu *viewMenu = menuBar()->addMenu("&Ver");
    
    QAction *togglePreviewAction = viewMenu->addAction("&Alternar Vista Previa");
    togglePreviewAction->setShortcut(QKeySequence("F12"));
    connect(togglePreviewAction, &QAction::triggered, this, &MainWindow::on_actionToggle_Preview_triggered);
    
    // Menú Ayuda
    QMenu *helpMenu = menuBar()->addMenu("A&yuda");
    
    QAction *aboutAction = helpMenu->addAction("&Acerca de");
    connect(aboutAction, &QAction::triggered, this, &MainWindow::on_actionAbout_triggered);
}

void MainWindow::setupRecentFilesMenu()
{
    // Buscar el menú Archivo
    QMenu *fileMenu = nullptr;
    foreach (QAction *action, menuBar()->actions()) {
        if (action->menu() && action->text().contains("Archivo")) {
            fileMenu = action->menu();
            break;
        }
    }
    
    // Si no existe, buscar por nombre en inglés o crear nuevo
    if (!fileMenu) {
        foreach (QAction *action, menuBar()->actions()) {
            if (action->menu() && action->text().contains("File")) {
                fileMenu = action->menu();
                break;
            }
        }
    }
    
    // Si aún no existe, crear el menú
    if (!fileMenu) {
        fileMenu = menuBar()->addMenu("&Archivo");
    }
    
    // Agregar separador antes del menú de archivos recientes
    fileMenu->addSeparator();
    
    // Crear menú de archivos recientes
    recentFilesMenu = fileMenu->addMenu("Archivos &Recientes");
    
    // Crear acciones para archivos recientes
    for (int i = 0; i < MaxRecentFiles; ++i) {
        QAction *action = new QAction(this);
        action->setVisible(false);
        connect(action, &QAction::triggered, this, &MainWindow::openRecentFile);
        recentFileActions.append(action);
        recentFilesMenu->addAction(action);
    }
    
    // Agregar separador y acción para limpiar lista
    recentFilesMenu->addSeparator();
    QAction *clearAction = recentFilesMenu->addAction("&Limpiar Lista");
    connect(clearAction, &QAction::triggered, [this]() {
        recentFiles.clear();
        saveRecentFiles();
        updateRecentFileActions();
    });
}

void MainWindow::loadRecentFiles()
{
    QSettings settings;
    recentFiles = settings.value("recentFiles").toStringList();
    
    // Limpiar archivos que ya no existen
    for (int i = recentFiles.size() - 1; i >= 0; --i) {
        if (!QFileInfo::exists(recentFiles.at(i))) {
            recentFiles.removeAt(i);
        }
    }
}

void MainWindow::saveRecentFiles()
{
    QSettings settings;
    settings.setValue("recentFiles", recentFiles);
}

void MainWindow::addToRecentFiles(const QString &fileName)
{
    recentFiles.removeAll(fileName);
    recentFiles.prepend(fileName);
    
    while (recentFiles.size() > MaxRecentFiles) {
        recentFiles.removeLast();
    }
    
    saveRecentFiles();
    updateRecentFileActions();
}

void MainWindow::updateRecentFileActions()
{
    for (int i = 0; i < MaxRecentFiles; ++i) {
        if (i < recentFiles.size()) {
            QString fileName = recentFiles.at(i);
            QString text = QString("&%1 %2").arg(i + 1).arg(QFileInfo(fileName).fileName());
            recentFileActions.at(i)->setText(text);
            recentFileActions.at(i)->setData(fileName);
            recentFileActions.at(i)->setVisible(true);
        } else {
            recentFileActions.at(i)->setVisible(false);
        }
    }
    
    // Mostrar/ocultar el menú según si hay archivos recientes
    recentFilesMenu->setEnabled(!recentFiles.isEmpty());
}

void MainWindow::openRecentFile()
{
    QAction *action = qobject_cast<QAction*>(sender());
    if (action) {
        QString fileName = action->data().toString();
        if (QFileInfo::exists(fileName)) {
            if (maybeSave()) {
                openFile(fileName);
            }
        } else {
            QMessageBox::warning(this, "Archivo no encontrado", 
                               "El archivo ya no existe en la ubicación especificada.");
            recentFiles.removeAll(fileName);
            saveRecentFiles();
            updateRecentFileActions();
        }
    }
}

void MainWindow::openFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", 
                           QString("No se pudo abrir el archivo:\n%1").arg(fileName));
        return;
    }
    
    QTextStream in(&file);
    textEdit->setPlainText(in.readAll());
    file.close();
    
    setCurrentFile(fileName);
    addToRecentFiles(fileName);
    
    statusBar()->showMessage("Archivo cargado", 2000);
}

void MainWindow::on_actionOpen_triggered()
{
    if (maybeSave()) {
        QString fileName = QFileDialog::getOpenFileName(this,
            "Abrir archivo Markdown", "", 
            "Archivos Markdown (*.md *.markdown *.txt);;Todos los archivos (*)");
        
        if (!fileName.isEmpty()) {
            openFile(fileName);
        }
    }
}

void MainWindow::on_actionSave_triggered()
{
    if (currentFilePath.isEmpty()) {
        on_actionSave_As_triggered();
    } else {
        saveFile(currentFilePath);
    }
}

void MainWindow::on_actionSave_As_triggered()
{
    QString fileName = QFileDialog::getSaveFileName(this,
        "Guardar archivo", "", 
        "Archivos Markdown (*.md);;Archivos de texto (*.txt);;Todos los archivos (*)");
    
    if (!fileName.isEmpty()) {
        saveFile(fileName);
    }
}

void MainWindow::saveFile(const QString &fileName)
{
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", 
                           QString("No se pudo guardar el archivo:\n%1").arg(fileName));
        return;
    }
    
    QTextStream out(&file);
    out << textEdit->toPlainText();
    file.close();
    
    setCurrentFile(fileName);
    addToRecentFiles(fileName);
    
    statusBar()->showMessage("Archivo guardado", 2000);
}

void MainWindow::on_actionNew_triggered()
{
    if (maybeSave()) {
        newFile();
    }
}

void MainWindow::newFile()
{
    textEdit->clear();
    setCurrentFile("");
}

void MainWindow::on_actionExit_triggered()
{
    close();
}

void MainWindow::on_actionAbout_triggered()
{
    QMessageBox::about(this, "Acerca de Escribe Chanchi", 
        "Escribe Chanchi - Editor de Markdown\n\n"
        "Versión 1.0\n\n"
        "Un editor simple y potente para archivos Markdown\n"
        "con vista previa en tiempo real.\n\n"
        "© 2024");
}

void MainWindow::on_actionToggle_Preview_triggered()
{
    if (webView->isVisible()) {
        webView->hide();
    } else {
        webView->show();
        updatePreview();
    }
}

void MainWindow::onTextChanged()
{
    if (!isModified) {
        isModified = true;
        updateWindowTitle();
    }
    updatePreview();
}

void MainWindow::updatePreview()
{
    QString markdown = textEdit->toPlainText();
    
    // Convertir markdown básico a HTML
    QString html = "<html><head><style>";
    html += "body { font-family: Arial, sans-serif; margin: 20px; line-height: 1.6; }";
    html += "h1, h2, h3, h4, h5, h6 { color: #333; }";
    html += "code { background-color: #f4f4f4; padding: 2px 4px; border-radius: 3px; }";
    html += "pre { background-color: #f4f4f4; padding: 10px; border-radius: 5px; overflow-x: auto; }";
    html += "blockquote { border-left: 4px solid #ddd; margin: 0; padding: 0 15px; color: #777; }";
    html += "</style></head><body>";
    
    // Procesamiento básico de markdown usando QRegularExpression
    QRegularExpression h1Regex("^# (.*)$", QRegularExpression::MultilineOption);
    markdown = markdown.replace(h1Regex, "<h1>\\1</h1>");
    
    QRegularExpression h2Regex("^## (.*)$", QRegularExpression::MultilineOption);
    markdown = markdown.replace(h2Regex, "<h2>\\1</h2>");
    
    QRegularExpression h3Regex("^### (.*)$", QRegularExpression::MultilineOption);
    markdown = markdown.replace(h3Regex, "<h3>\\1</h3>");
    
    QRegularExpression boldRegex("\\*\\*([^*]+)\\*\\*");
    markdown = markdown.replace(boldRegex, "<strong>\\1</strong>");
    
    QRegularExpression italicRegex("\\*([^*]+)\\*");
    markdown = markdown.replace(italicRegex, "<em>\\1</em>");
    
    QRegularExpression codeRegex("`([^`]+)`");
    markdown = markdown.replace(codeRegex, "<code>\\1</code>");
    
    markdown = markdown.replace("\n\n", "</p><p>");
    
    html += "<p>" + markdown + "</p>";
    html += "</body></html>";
    
    webView->setHtml(html);
}

bool MainWindow::maybeSave()
{
    if (isModified) {
        QMessageBox::StandardButton ret;
        ret = QMessageBox::warning(this, "Escribe Chanchi",
                     "El documento ha sido modificado.\n"
                     "¿Desea guardar los cambios?",
                     QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (ret == QMessageBox::Save)
            return on_actionSave_triggered(), true;
        else if (ret == QMessageBox::Cancel)
            return false;
    }
    return true;
}

void MainWindow::setCurrentFile(const QString &fileName)
{
    currentFilePath = fileName;
    isModified = false;
    updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    QString title = "Escribe Chanchi";
    
    if (!currentFilePath.isEmpty()) {
        title += " - " + QFileInfo(currentFilePath).fileName();
    } else {
        title += " - Nuevo documento";
    }
    
    if (isModified) {
        title += " *";
    }
    
    setWindowTitle(title);
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (maybeSave()) {
        saveRecentFiles();
        event->accept();
    } else {
        event->ignore();
    }
}

void MainWindow::setupSingleInstance()
{
    localServer = new QLocalServer(this);
    connect(localServer, &QLocalServer::newConnection, this, &MainWindow::handleNewConnection);
    
    if (!localServer->listen("EscribeChanchi_Server")) {
        // Si no puede crear el servidor, puede ser que ya exista
        localServer->removeServer("EscribeChanchi_Server");
        localServer->listen("EscribeChanchi_Server");
    }
}

void MainWindow::handleNewConnection()
{
    QLocalSocket *socket = localServer->nextPendingConnection();
    if (socket) {
        connect(socket, &QLocalSocket::readyRead, [this, socket]() {
            QByteArray data = socket->readAll();
            if (data == "SHOW_WINDOW") {
                restoreFromTray();
            }
            socket->disconnectFromServer();
        });
    }
}

void MainWindow::restoreFromTray()
{
    show();
    raise();
    activateWindow();
    setWindowState(windowState() & ~Qt::WindowMinimized);
}