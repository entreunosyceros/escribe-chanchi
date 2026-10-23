#ifndef MARKDOWN_EDITOR_H
#define MARKDOWN_EDITOR_H

#include <QWidget>
#include <QTextEdit>
#include <QTextBrowser>
#include <QPushButton>
#include <QSplitter>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolBar>
#include <QAction>
#include <QFileDialog>
#include <QTextStream>
#include <QPrinter>
#include <QMenuBar>
#include <QMainWindow>
#include <QMenu>
#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QPainter>
#include <QTextBlock>
#include <QInputDialog>
#include <QRegularExpression>
#include <QTabWidget>
#include <QMessageBox>
#include <QFileInfo>
#include <QCloseEvent>
#include <QLineEdit>
#include <QSystemTrayIcon>
#include <QSpinBox>
#include <QFormLayout>
#include <QStandardPaths>
#include <QDir>
#include <QListWidget>
#include <QLocalServer>
#include <QLocalSocket>
#include <QByteArray>
#include "markdown_formatter.h"
#include "template_manager.h"

// Estructura para manejar cada documento/pestaña
struct TabDocument {
    QTextEdit *editor;
    QTextBrowser *browser;
    QSplitter *splitter;
    QString filePath;
    QString fileName;
    bool isModified;
    bool isNewFile;
    
    TabDocument() : editor(nullptr), browser(nullptr), splitter(nullptr), 
                   isModified(false), isNewFile(true) {}
};

class MarkdownEditor : public QMainWindow {
    Q_OBJECT

public:
    explicit MarkdownEditor(QWidget *parent = nullptr);
    bool openFilePath(const QString &filePath);

    static const char *singleInstanceServerName() { return "EscribeChanchi_Server"; }
    static const char *singleInstanceKey() { return "EscribeChanchi_SingleInstance"; }

private slots:
    void toggleDarkMode();
    void setBold();
    void setItalic();
    void setCode();
    void setHeader();
    void setStrikethrough();    // Nuevo
    void setBlockquote();       // Nuevo
    void setUnorderedList();    // Nuevo
    void setOrderedList();      // Nuevo
    void setLink();             // Nuevo
    void setImage();            // Nuevo
    void setHorizontalRule();   // Nuevo
    void setCodeBlock();        // Nuevo
    void insertTable();         // Nuevo - Insertar tabla con asistente
    void renderMarkdown();      // Declaración de la función
    void saveToFile();
    void loadFromFile();
    void exportToHTML();       // Declaración de la función
    void exportToPDF();        // Declaración de la función
    void showManual();         // Mostrar manual de Markdown
    void showAbout();          // Mostrar ventana Acerca de
    void toggleLineNumbers();  // Alternar numeración de líneas
    void updateLineNumbers();  // Actualizar numeración de líneas
    void showTemplates();      // Mostrar plantillas
    void createTemplate();     // Crear nueva plantilla
    void insertEmoji();        // Insertar emoji
    
    // Slots para sistema de búsqueda y bandeja del sistema
    void setupSystemTray();
    void createTrayIcon();
    void showFromTray();
    void hideToTray();
    void trayIconActivated(QSystemTrayIcon::ActivationReason reason);
    void exitApplication();
    void toggleFullScreen();
    // Slots para búsqueda y reemplazo
    void showFindDialog();
    void showReplaceDialog();
    void findNext();
    void findPrevious();
    void replaceNext();
    void replaceAll();
    void manageCustomEmojis();  // Gestionar emojis personalizados

    
    // Slots para manejo de pestañas
    void newTab();             // Crear nueva pestaña
    void openFileInNewTab();   // Abrir archivo en nueva pestaña
    void closeTab(int index);  // Cerrar pestaña específica
    void closeCurrentTab();    // Cerrar pestaña actual
    void tabChanged(int index); // Cambio de pestaña activa
    void saveCurrentFile();    // Guardar archivo actual
    void saveFileAs();         // Guardar como
    void documentModified();   // Documento modificado

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    void setDarkMode();
    void setLightMode();
    void setupMenuBar();
    void setupToolBar();       // Nueva función para configurar la toolbar
    QString processTablesManually(const QString &text);  // Procesar tablas manualmente
    QString processMarkdownWithTables(const QString &text);  // Procesar Markdown con tablas
    QString convertTableToHtml(const QString &tableText);   // Convertir tabla a HTML
    QString processMarkdownLine(const QString &line);       // Procesar línea de Markdown
    
    // Funciones auxiliares para pestañas
    TabDocument* createNewDocument();
    TabDocument* getCurrentDocument();
    void setupDocumentConnections(TabDocument* doc);
    void updateTabTitle(int index);
    bool saveDocument(TabDocument* doc);
    bool saveDocumentAs(TabDocument* doc);
    bool closeDocument(TabDocument* doc);

    // Variables para sistema de pestañas
    QTabWidget *tabWidget;     // Widget principal de pestañas
    QList<TabDocument*> documents; // Lista de documentos abiertos
    QToolBar *mainToolBar;     // Barra de herramientas principal
    MarkdownFormatter *formatter;
    TemplateManager *templateManager;
    bool darkMode;
    bool showLineNumbers;
    int untitledCounter;       // Contador para archivos sin título

    // Backward compatibility (se eliminarán)
    QTextEdit *editor;         // Apunta al editor actual
    QTextBrowser *browser;     // Apunta al browser actual
    QSplitter *splitter;       // Apunta al splitter actual

    // Miembros para sistema de bandeja y búsqueda
    bool isFullScreenMode;
    bool forceQuit = false;
    QSystemTrayIcon *trayIcon;
    QMenu *trayIconMenu;
    QAction *showAction;
    QAction *hideAction;
    QAction *exitAction;
    QDialog *findDialog;
    QDialog *replaceDialog;
    QLineEdit *findLineEdit;
    QLineEdit *replaceLineEdit;
    bool caseSensitive;
    bool wholeWords;
    QString lastSearchText;
    QStringList customEmojis;  // Lista de emojis personalizados

    // Miembros para sistema de instancia única
    QLocalServer *localServer;
    
    // Funciones para instancia única
    void setupSingleInstance();
    void handleNewConnection();
    void ingestInstanceData(QByteArray *buffer);
    void restoreFromTray();
    void applyPreviewMode(TabDocument *doc);

    // Funciones helper declaradas en .cpp
    QString getFileType(const QString &filePath);
    bool isMarkdownFile(const QString &filePath);
    bool shouldRenderPreview(const QString &filePath);
    void loadCustomEmojis();
    void saveCustomEmojis();
    QString getCustomEmojiPath();
};

#endif // MARKDOWN_EDITOR_H