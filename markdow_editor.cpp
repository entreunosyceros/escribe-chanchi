#include "markdow_editor.h"
#include <QPalette>
#include <QFont>
#include <QTextCursor>
#include <QTextCharFormat>
#include <QPrinter>
#include <QSyntaxHighlighter>
#include <QRegularExpression>
#include <QFileDialog>
#include <QTextStream>
#include <QPrinter>
#include <QPainter>
#include <QFile>
#include <QMenu>
#include <QKeySequence>
#include <QDialog>
#include <QLabel>
#include <QPixmap>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QScrollArea>
#include <QApplication>
#include <QCoreApplication>
#include <QPushButton>
#include <QLineEdit>
#include <QCheckBox>
#include <QStatusBar>
#include <QDesktopServices>
#include <QUrl>
#include <cmark.h>
#include <QDataStream>
#include <QBuffer>
#include <QStatusBar>
#include <QSettings>

MarkdownEditor::MarkdownEditor(QWidget *parent) 
    : QMainWindow(parent), darkMode(false), showLineNumbers(false), untitledCounter(1),
      isFullScreenMode(false), formatter(nullptr), editor(nullptr), browser(nullptr), 
      splitter(nullptr), trayIcon(nullptr), trayIconMenu(nullptr), findDialog(nullptr),
      replaceDialog(nullptr), findLineEdit(nullptr), replaceLineEdit(nullptr),
      caseSensitive(false), wholeWords(false), localServer(nullptr) {
    
    // Crear el widget central
    QWidget *centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    // Crear el widget de pestañas
    tabWidget = new QTabWidget(this);
    tabWidget->setTabsClosable(true);
    tabWidget->setMovable(true);
    
    // Layout principal para el widget central
    QVBoxLayout* layout = new QVBoxLayout;
    layout->addWidget(tabWidget);
    centralWidget->setLayout(layout);

    setWindowTitle("EscribeChanchi");
    
    // Configurar icono de la ventana
    setWindowIcon(QIcon(":/img/logo_escribe_chanchi.png"));

    // Inicializar template manager
    templateManager = new TemplateManager(this);

    loadEditorPreferences();

    // Configurar la barra de menús
    setupMenuBar();
    
    // Configurar la barra de herramientas
    setupToolBar();
    
    // Configurar la bandeja del sistema
    setupSystemTray();
    
    // Cargar emojis personalizados
    loadCustomEmojis();

    // Conectar señales del tab widget
    connect(tabWidget, &QTabWidget::currentChanged, this, &MarkdownEditor::tabChanged);
    connect(tabWidget, &QTabWidget::tabCloseRequested, this, &MarkdownEditor::closeTab);

    // Crear la primera pestaana
    newTab();

    // Configuración inicial del modo claro
    setLightMode();
    
    // Configurar el tamaño y estado inicial de la ventana
    resize(1200, 800);  // Tamaño inicial razonable
    
    // Asegurar que la ventana no esté en modo pantalla completa al inicio
    isFullScreenMode = false;

    // El servidor se abre cuando la interfaz ya puede cargar documentos
    setupSingleInstance();
}

void MarkdownEditor::setupMenuBar() {
    // Menú Archivo
    QMenu* fileMenu = this->menuBar()->addMenu("Archivo");
    
    QAction* newTabAction = new QAction("Nueva pestaña", this);
    newTabAction->setShortcut(QKeySequence::New);
    connect(newTabAction, &QAction::triggered, this, &MarkdownEditor::newTab);
    fileMenu->addAction(newTabAction);
    
    QAction* openAction = new QAction("Abrir", this);
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &MarkdownEditor::openFileInNewTab);
    fileMenu->addAction(openAction);

    recentFilesMenu = fileMenu->addMenu("Archivos recientes");
    updateRecentFilesMenu();
    
    fileMenu->addSeparator();
    
    QAction* saveAction = new QAction("Guardar", this);
    saveAction->setShortcut(QKeySequence::Save);
    connect(saveAction, &QAction::triggered, this, &MarkdownEditor::saveCurrentFile);
    fileMenu->addAction(saveAction);
    
    QAction* saveAsAction = new QAction("Guardar como...", this);
    saveAsAction->setShortcut(QKeySequence::SaveAs);
    connect(saveAsAction, &QAction::triggered, this, &MarkdownEditor::saveFileAs);
    fileMenu->addAction(saveAsAction);
    
    fileMenu->addSeparator();
    
    QAction* closeTabAction = new QAction("Cerrar pestaña", this);
    closeTabAction->setShortcut(QKeySequence::Close);
    connect(closeTabAction, &QAction::triggered, this, &MarkdownEditor::closeCurrentTab);
    fileMenu->addAction(closeTabAction);
    
    fileMenu->addSeparator();
    
    QAction* exportHtmlAction = new QAction("Exportar a HTML", this);
    connect(exportHtmlAction, &QAction::triggered, this, &MarkdownEditor::exportToHTML);
    fileMenu->addAction(exportHtmlAction);
    
    QAction* exportPdfAction = new QAction("Exportar a PDF", this);
    connect(exportPdfAction, &QAction::triggered, this, &MarkdownEditor::exportToPDF);
    fileMenu->addAction(exportPdfAction);
    
    fileMenu->addSeparator();
    
    QAction* exitAction = new QAction("Salir", this);
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this, &MarkdownEditor::exitApplication);
    fileMenu->addAction(exitAction);

    // Menú Editar
    QMenu* editMenu = this->menuBar()->addMenu("Editar");
    
    QAction* findAction = new QAction("&Buscar...", this);
    findAction->setShortcut(QKeySequence::Find);
    connect(findAction, &QAction::triggered, this, &MarkdownEditor::showFindDialog);
    editMenu->addAction(findAction);
    
    QAction* replaceAction = new QAction("Buscar y &reemplazar...", this);
    replaceAction->setShortcut(QKeySequence::Replace);
    connect(replaceAction, &QAction::triggered, this, &MarkdownEditor::showReplaceDialog);
    editMenu->addAction(replaceAction);
    
    editMenu->addSeparator();
    
    QAction* findNextAction = new QAction("Buscar &siguiente", this);
    findNextAction->setShortcut(QKeySequence(Qt::Key_F3));
    connect(findNextAction, &QAction::triggered, this, &MarkdownEditor::findNext);
    editMenu->addAction(findNextAction);
    
    QAction* findPreviousAction = new QAction("Buscar &anterior", this);
    findPreviousAction->setShortcut(QKeySequence(Qt::SHIFT | Qt::Key_F3));
    connect(findPreviousAction, &QAction::triggered, this, &MarkdownEditor::findPrevious);
    editMenu->addAction(findPreviousAction);

    editMenu->addSeparator();

    QAction* goToLineAction = new QAction("Ir a la &línea...", this);
    goToLineAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_G));
    connect(goToLineAction, &QAction::triggered, this, &MarkdownEditor::goToLine);
    editMenu->addAction(goToLineAction);

    // Menú Formato
    QMenu* formatMenu = this->menuBar()->addMenu("Formato");
    
    // Formato básico de texto
    QAction* boldAction = new QAction("&Negrita", this);
    boldAction->setShortcut(QKeySequence::Bold);
    connect(boldAction, &QAction::triggered, this, &MarkdownEditor::setBold);
    formatMenu->addAction(boldAction);
    
    QAction* italicAction = new QAction("&Cursiva", this);
    italicAction->setShortcut(QKeySequence::Italic);
    connect(italicAction, &QAction::triggered, this, &MarkdownEditor::setItalic);
    formatMenu->addAction(italicAction);
    
    QAction* strikethroughAction = new QAction("&Tachado", this);
    strikethroughAction->setShortcut(QKeySequence("Ctrl+Shift+X"));
    connect(strikethroughAction, &QAction::triggered, this, &MarkdownEditor::setStrikethrough);
    formatMenu->addAction(strikethroughAction);
    
    formatMenu->addSeparator();
    
    // Código
    QAction* codeAction = new QAction("Código en &línea", this);
    codeAction->setShortcut(QKeySequence("Ctrl+`"));
    connect(codeAction, &QAction::triggered, this, &MarkdownEditor::setCode);
    formatMenu->addAction(codeAction);
    
    QAction* codeBlockAction = new QAction("&Bloque de código", this);
    codeBlockAction->setShortcut(QKeySequence("Ctrl+Shift+`"));
    connect(codeBlockAction, &QAction::triggered, this, &MarkdownEditor::setCodeBlock);
    formatMenu->addAction(codeBlockAction);
    
    formatMenu->addSeparator();
    
    // Encabezados
    QAction* headerAction = new QAction("&Encabezado", this);
    headerAction->setShortcut(QKeySequence("Ctrl+1"));
    connect(headerAction, &QAction::triggered, this, &MarkdownEditor::setHeader);
    formatMenu->addAction(headerAction);
    
    formatMenu->addSeparator();
    
    // Listas
    QAction* unorderedListAction = new QAction("Lista con &viñetas", this);
    unorderedListAction->setShortcut(QKeySequence("Ctrl+U"));
    connect(unorderedListAction, &QAction::triggered, this, &MarkdownEditor::setUnorderedList);
    formatMenu->addAction(unorderedListAction);
    
    QAction* orderedListAction = new QAction("Lista &numerada", this);
    orderedListAction->setShortcut(QKeySequence("Ctrl+Shift+O"));
    connect(orderedListAction, &QAction::triggered, this, &MarkdownEditor::setOrderedList);
    formatMenu->addAction(orderedListAction);
    
    formatMenu->addSeparator();
    
    // Enlaces e imágenes
    QAction* linkAction = new QAction("Insertar en&lace", this);
    linkAction->setShortcut(QKeySequence("Ctrl+L"));
    connect(linkAction, &QAction::triggered, this, &MarkdownEditor::setLink);
    formatMenu->addAction(linkAction);
    
    QAction* imageAction = new QAction("Insertar &imagen", this);
    imageAction->setShortcut(QKeySequence("Ctrl+Shift+I"));
    connect(imageAction, &QAction::triggered, this, &MarkdownEditor::setImage);
    formatMenu->addAction(imageAction);
    
    QAction* tableAction = new QAction("Insertar &tabla", this);
    tableAction->setShortcut(QKeySequence("Ctrl+T"));
    connect(tableAction, &QAction::triggered, this, &MarkdownEditor::insertTable);
    formatMenu->addAction(tableAction);
    
    formatMenu->addSeparator();
    
    // Otros elementos
    QAction* blockquoteAction = new QAction("&Cita", this);
    blockquoteAction->setShortcut(QKeySequence("Ctrl+Shift+Q"));
    connect(blockquoteAction, &QAction::triggered, this, &MarkdownEditor::setBlockquote);
    formatMenu->addAction(blockquoteAction);
    
    QAction* horizontalRuleAction = new QAction("Línea &horizontal", this);
    horizontalRuleAction->setShortcut(QKeySequence("Ctrl+Shift+H"));
    connect(horizontalRuleAction, &QAction::triggered, this, &MarkdownEditor::setHorizontalRule);
    formatMenu->addAction(horizontalRuleAction);
    
    formatMenu->addSeparator();
    
    // Insertar emoji
    QAction* emojiAction = new QAction("Insertar &emoji", this);
    emojiAction->setShortcut(QKeySequence("Ctrl+E"));
    connect(emojiAction, &QAction::triggered, this, &MarkdownEditor::insertEmoji);
    formatMenu->addAction(emojiAction);

    // Menú Plantillas
    QMenu* templatesMenu = this->menuBar()->addMenu("Plantillas");
    
    QAction* showTemplatesAction = new QAction("Usar plantilla...", this);
    showTemplatesAction->setShortcut(QKeySequence("Ctrl+Shift+T"));
    connect(showTemplatesAction, &QAction::triggered, this, &MarkdownEditor::showTemplates);
    templatesMenu->addAction(showTemplatesAction);
    
    QAction* createTemplateAction = new QAction("Crear plantilla...", this);
    createTemplateAction->setShortcut(QKeySequence("Ctrl+Shift+N"));
    connect(createTemplateAction, &QAction::triggered, this, &MarkdownEditor::createTemplate);
    templatesMenu->addAction(createTemplateAction);

    // Menú Ver
    QMenu* viewMenu = this->menuBar()->addMenu("Ver");
    
    QAction* toggleModeAction = new QAction("Alternar Modo Oscuro", this);
    toggleModeAction->setShortcut(QKeySequence("Ctrl+Shift+D"));
    connect(toggleModeAction, &QAction::triggered, this, &MarkdownEditor::toggleDarkMode);
    viewMenu->addAction(toggleModeAction);
    
    QAction* toggleLineNumbersAction = new QAction("Mostrar números de línea", this);
    toggleLineNumbersAction->setCheckable(true);
    toggleLineNumbersAction->setShortcut(QKeySequence("Ctrl+Shift+L"));
    connect(toggleLineNumbersAction, &QAction::triggered, this, &MarkdownEditor::toggleLineNumbers);
    viewMenu->addAction(toggleLineNumbersAction);
    
    QAction* toggleFullScreenMenuAction = new QAction("Pantalla completa", this);
    toggleFullScreenMenuAction->setCheckable(true);
    toggleFullScreenMenuAction->setShortcut(QKeySequence("F11"));
    connect(toggleFullScreenMenuAction, &QAction::triggered, this, &MarkdownEditor::toggleFullScreen);
    viewMenu->addAction(toggleFullScreenMenuAction);

    previewAction = new QAction("Vista previa", this);
    previewAction->setCheckable(true);
    previewAction->setChecked(previewEnabled);
    previewAction->setShortcut(QKeySequence("Ctrl+Shift+P"));
    connect(previewAction, &QAction::toggled, this, &MarkdownEditor::togglePreview);
    viewMenu->addAction(previewAction);

    viewMenu->addSeparator();

    QAction* increaseFontAction = new QAction("Aumentar tamaño de fuente", this);
    increaseFontAction->setShortcut(QKeySequence::ZoomIn);
    connect(increaseFontAction, &QAction::triggered, this, &MarkdownEditor::increaseFontSize);
    viewMenu->addAction(increaseFontAction);

    QAction* decreaseFontAction = new QAction("Reducir tamaño de fuente", this);
    decreaseFontAction->setShortcut(QKeySequence::ZoomOut);
    connect(decreaseFontAction, &QAction::triggered, this, &MarkdownEditor::decreaseFontSize);
    viewMenu->addAction(decreaseFontAction);

    QAction* resetFontAction = new QAction("Tamaño de fuente normal", this);
    resetFontAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_0));
    connect(resetFontAction, &QAction::triggered, this, &MarkdownEditor::resetFontSize);
    viewMenu->addAction(resetFontAction);

    // Menú Ayuda
    QMenu* helpMenu = this->menuBar()->addMenu("Ayuda");
    
    QAction* manualAction = new QAction("Manual", this);
    connect(manualAction, &QAction::triggered, this, &MarkdownEditor::showManual);
    helpMenu->addAction(manualAction);
    
    QAction* aboutAction = new QAction("Acerca de EscribeChanchi", this);
    connect(aboutAction, &QAction::triggered, this, &MarkdownEditor::showAbout);
    helpMenu->addAction(aboutAction);
}

void MarkdownEditor::setupToolBar() {
    // Crear la barra de herramientas principal
    mainToolBar = addToolBar("Formato");
    mainToolBar->setMovable(true);
    mainToolBar->setFloatable(true);
    mainToolBar->setIconSize(QSize(32, 32));
    mainToolBar->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    
    // Función auxiliar para crear iconos de texto mejorados
    auto createTextIcon = [](const QString &text, const QFont &font = QFont(), const QColor &textColor = Qt::black, const QColor &bgColor = QColor(245, 245, 245)) -> QIcon {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setRenderHint(QPainter::TextAntialiasing);
        
        // Fondo con gradiente
        QLinearGradient gradient(0, 0, 0, 32);
        gradient.setColorAt(0, bgColor.lighter(120));
        gradient.setColorAt(1, bgColor.darker(110));
        
        painter.setBrush(QBrush(gradient));
        painter.setPen(QPen(QColor(120, 120, 120), 1));
        painter.drawRoundedRect(2, 2, 28, 28, 6, 6);
        
        // Sombra de texto
        painter.setPen(QColor(0, 0, 0, 40));
        QFont shadowFont = font.family().isEmpty() ? QFont("Arial", 9, QFont::Bold) : font;
        painter.setFont(shadowFont);
        painter.drawText(pixmap.rect().adjusted(1, 1, 1, 1), Qt::AlignCenter, text);
        
        // Texto principal
        painter.setPen(textColor);
        painter.drawText(pixmap.rect(), Qt::AlignCenter, text);
        
        return QIcon(pixmap);
    };
    
    // Función para crear iconos gráficos simples
    auto createGraphicIcon = [](const QString &shape, const QColor &color = QColor(60, 60, 60)) -> QIcon {
        QPixmap pixmap(32, 32);
        pixmap.fill(Qt::transparent);
        
        QPainter painter(&pixmap);
        painter.setRenderHint(QPainter::Antialiasing);
        
        // Fondo
        QLinearGradient gradient(0, 0, 0, 32);
        gradient.setColorAt(0, QColor(250, 250, 250));
        gradient.setColorAt(1, QColor(235, 235, 235));
        painter.setBrush(QBrush(gradient));
        painter.setPen(QPen(QColor(180, 180, 180), 1));
        painter.drawRoundedRect(2, 2, 28, 28, 6, 6);
        
        // Dibujar forma
        painter.setPen(QPen(color, 2, Qt::SolidLine, Qt::RoundCap));
        painter.setBrush(QBrush(color));
        
        if (shape == "folder") {
            // Carpeta para "Abrir"
            painter.drawRect(8, 12, 16, 10);
            painter.drawRect(8, 10, 8, 2);
        } else if (shape == "save") {
            // Diskette para "Guardar"
            painter.drawRect(8, 8, 16, 16);
            painter.drawRect(11, 8, 10, 4);
            painter.setPen(QPen(Qt::white, 1));
            painter.drawRect(10, 12, 12, 8);
        } else if (shape == "link") {
            // Cadena para "Enlace"
            painter.drawEllipse(8, 10, 6, 6);
            painter.drawEllipse(18, 16, 6, 6);
            painter.drawLine(12, 14, 20, 18);
        } else if (shape == "template") {
            // Documento para "Plantilla"
            painter.drawRect(10, 6, 12, 18);
            painter.drawLine(18, 6, 22, 10);
            painter.drawLine(18, 6, 18, 10);
            painter.drawLine(18, 10, 22, 10);
        } else if (shape == "dark") {
            // Luna para "Modo oscuro"
            painter.setBrush(QBrush(color));
            painter.drawEllipse(10, 8, 12, 12);
            painter.setBrush(QBrush(QColor(250, 250, 250)));
            painter.drawEllipse(14, 8, 12, 12);
        } else if (shape == "search") {
            // Lupa para "Buscar"
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, 2));
            painter.drawEllipse(8, 8, 12, 12);
            painter.drawLine(18, 18, 24, 24);
        } else if (shape == "replace") {
            // Flechas circulares para "Reemplazar"
            painter.setBrush(Qt::NoBrush);
            painter.setPen(QPen(color, 2));
            painter.drawArc(8, 8, 16, 16, 0, 240 * 16);
            painter.drawLine(20, 10, 24, 10);
            painter.drawLine(22, 8, 24, 10);
            painter.drawLine(22, 12, 24, 10);
        } else if (shape == "prev") {
            // Flecha izquierda para "Anterior"
            painter.setBrush(QBrush(color));
            QPolygon triangle;
            triangle << QPoint(20, 12) << QPoint(12, 16) << QPoint(12, 20) << QPoint(20, 16) << QPoint(20, 20) << QPoint(12, 16) << QPoint(20, 12);
            painter.drawPolygon(triangle);
            // Simplificado: triángulo apuntando izquierda
            QPolygon leftArrow;
            leftArrow << QPoint(18, 10) << QPoint(10, 16) << QPoint(18, 22);
            painter.drawPolygon(leftArrow);
        } else if (shape == "next") {
            // Flecha derecha para "Siguiente"
            painter.setBrush(QBrush(color));
            QPolygon rightArrow;
            rightArrow << QPoint(14, 10) << QPoint(22, 16) << QPoint(14, 22);
            painter.drawPolygon(rightArrow);
        }
        
        return QIcon(pixmap);
    };
    
    // Sección de pestañas
    QAction* newTabAction = mainToolBar->addAction(createTextIcon("+", QFont("Arial", 16, QFont::Bold), QColor(50, 150, 50)), "Nueva");
    newTabAction->setToolTip("Nueva pestaña (Ctrl+N)");
    connect(newTabAction, &QAction::triggered, this, &MarkdownEditor::newTab);
    
    QAction* openTabAction = mainToolBar->addAction(createGraphicIcon("folder", QColor(80, 120, 200)), "Abrir");
    openTabAction->setToolTip("Abrir archivo (Ctrl+O)");
    connect(openTabAction, &QAction::triggered, this, &MarkdownEditor::openFileInNewTab);
    
    QAction* saveTabAction = mainToolBar->addAction(createGraphicIcon("save", QColor(100, 100, 200)), "Guardar");
    saveTabAction->setToolTip("Guardar archivo (Ctrl+S)");
    connect(saveTabAction, &QAction::triggered, this, &MarkdownEditor::saveCurrentFile);
    
    mainToolBar->addSeparator();
    
    // Sección de búsqueda
    QAction* findAction = mainToolBar->addAction(createGraphicIcon("search", QColor(200, 100, 50)), "Buscar");
    findAction->setToolTip("Buscar texto (Ctrl+F)");
    connect(findAction, &QAction::triggered, this, &MarkdownEditor::showFindDialog);
    
    QAction* replaceAction = mainToolBar->addAction(createGraphicIcon("replace", QColor(180, 120, 50)), "Reemplazar");
    replaceAction->setToolTip("Buscar y reemplazar (Ctrl+H)");
    connect(replaceAction, &QAction::triggered, this, &MarkdownEditor::showReplaceDialog);
    
    QAction* findPrevAction = mainToolBar->addAction(createGraphicIcon("prev", QColor(150, 100, 150)), "Anterior");
    findPrevAction->setToolTip("Buscar anterior (Shift+F3)");
    connect(findPrevAction, &QAction::triggered, this, &MarkdownEditor::findPrevious);
    
    QAction* findNextAction = mainToolBar->addAction(createGraphicIcon("next", QColor(100, 150, 100)), "Siguiente");
    findNextAction->setToolTip("Buscar siguiente (F3)");
    connect(findNextAction, &QAction::triggered, this, &MarkdownEditor::findNext);
    
    mainToolBar->addSeparator();
    
    // Sección de formato de texto
    QAction* boldAction = mainToolBar->addAction(createTextIcon("B", QFont("Arial", 10, QFont::Bold), QColor(60, 60, 60)), "Negrita");
    boldAction->setToolTip("Negrita (Ctrl+B)");
    connect(boldAction, &QAction::triggered, this, &MarkdownEditor::setBold);
    
    QFont italicFont("Arial", 10);
    italicFont.setItalic(true);
    QAction* italicAction = mainToolBar->addAction(createTextIcon("I", italicFont, QColor(80, 80, 80)), "Cursiva");
    italicAction->setToolTip("Cursiva (Ctrl+I)");
    connect(italicAction, &QAction::triggered, this, &MarkdownEditor::setItalic);
    
    QAction* codeAction = mainToolBar->addAction(createTextIcon("</\\>", QFont("Courier", 7), QColor(120, 80, 120)), "Código");
    codeAction->setToolTip("Código inline (Ctrl+`)");
    connect(codeAction, &QAction::triggered, this, &MarkdownEditor::setCode);
    
    QFont strikeFont("Arial", 10);
    strikeFont.setStrikeOut(true);
    QAction* strikeAction = mainToolBar->addAction(createTextIcon("S", strikeFont, QColor(150, 80, 80)), "Tachado");
    strikeAction->setToolTip("Tachado (Ctrl+Shift+X)");
    connect(strikeAction, &QAction::triggered, this, &MarkdownEditor::setStrikethrough);
    
    mainToolBar->addSeparator();
    
    // Sección de encabezados y estructura
    QAction* headerAction = mainToolBar->addAction(createTextIcon("H1", QFont("Arial", 8, QFont::Bold), QColor(50, 100, 200)), "Título");
    headerAction->setToolTip("Encabezado (Ctrl+1)");
    connect(headerAction, &QAction::triggered, this, &MarkdownEditor::setHeader);
    
    QAction* quoteAction = mainToolBar->addAction(createTextIcon("\"", QFont("Arial", 16, QFont::Bold), QColor(100, 150, 100)), "Cita");
    quoteAction->setToolTip("Cita (Ctrl+Shift+Q)");
    connect(quoteAction, &QAction::triggered, this, &MarkdownEditor::setBlockquote);
    
    QAction* codeBlockAction = mainToolBar->addAction(createTextIcon("{ }", QFont("Courier", 8), QColor(120, 80, 120)), "Bloque");
    codeBlockAction->setToolTip("Bloque de código (Ctrl+Shift+`)");
    connect(codeBlockAction, &QAction::triggered, this, &MarkdownEditor::setCodeBlock);
    
    mainToolBar->addSeparator();
    
    // Sección de listas
    QAction* unorderedListAction = mainToolBar->addAction(createTextIcon("•", QFont("Arial", 16)), "Lista");
    unorderedListAction->setToolTip("Lista sin ordenar (Ctrl+U)");
    connect(unorderedListAction, &QAction::triggered, this, &MarkdownEditor::setUnorderedList);
    
    QAction* orderedListAction = mainToolBar->addAction(createTextIcon("1.", QFont("Arial", 10, QFont::Bold)), "Números");
    orderedListAction->setToolTip("Lista numerada (Ctrl+Shift+O)");
    connect(orderedListAction, &QAction::triggered, this, &MarkdownEditor::setOrderedList);
    
    mainToolBar->addSeparator();
    
    // Sección de enlaces y medios
    QAction* linkAction = mainToolBar->addAction(createGraphicIcon("link", QColor(50, 120, 200)), "Enlace");
    linkAction->setToolTip("Insertar enlace (Ctrl+L)");
    connect(linkAction, &QAction::triggered, this, &MarkdownEditor::setLink);
    
    QAction* imageAction = mainToolBar->addAction(createTextIcon("IMG", QFont("Arial", 8, QFont::Bold), QColor(120, 80, 150)), "Imagen");
    imageAction->setToolTip("Insertar imagen (Ctrl+Shift+I)");
    connect(imageAction, &QAction::triggered, this, &MarkdownEditor::setImage);
    
    QAction* tableAction = mainToolBar->addAction(createTextIcon("TBL", QFont("Arial", 8, QFont::Bold), QColor(100, 100, 150)), "Tabla");
    tableAction->setToolTip("Insertar tabla (Ctrl+T)");
    connect(tableAction, &QAction::triggered, this, &MarkdownEditor::insertTable);
    
    QAction* hrAction = mainToolBar->addAction(createTextIcon("---", QFont("Arial", 8, QFont::Bold)), "Línea");
    hrAction->setToolTip("Línea horizontal (Ctrl+Shift+H)");
    connect(hrAction, &QAction::triggered, this, &MarkdownEditor::setHorizontalRule);
    
    QAction* emojiAction = mainToolBar->addAction(createTextIcon(":)", QFont("Arial", 12, QFont::Bold), QColor(200, 150, 50)), "Emoji");
    emojiAction->setToolTip("Insertar emoji (Ctrl+E)");
    connect(emojiAction, &QAction::triggered, this, &MarkdownEditor::insertEmoji);
    
    mainToolBar->addSeparator();
    
    // Sección de plantillas
    QAction* templatesAction = mainToolBar->addAction(createGraphicIcon("template", QColor(120, 150, 80)), "Plantilla");
    templatesAction->setToolTip("Usar plantilla (Ctrl+Shift+T)");
    connect(templatesAction, &QAction::triggered, this, &MarkdownEditor::showTemplates);
    
    QAction* newTemplateAction = mainToolBar->addAction(createTextIcon("NEW", QFont("Arial", 8, QFont::Bold), QColor(80, 150, 120)), "Nueva");
    newTemplateAction->setToolTip("Crear plantilla (Ctrl+Shift+N)");
    connect(newTemplateAction, &QAction::triggered, this, &MarkdownEditor::createTemplate);
    
    mainToolBar->addSeparator();
    
    // Sección de vista. La misma acción del menú, para no duplicar el atajo.
    if (previewAction) {
        previewAction->setIcon(createTextIcon("Md", QFont("Arial", 9, QFont::Bold), QColor(40, 90, 160)));
        previewAction->setToolTip("Mostrar u ocultar la vista previa (Ctrl+Shift+P)");
        mainToolBar->addAction(previewAction);
    }

    QAction* darkModeAction = mainToolBar->addAction(createGraphicIcon("dark", QColor(80, 80, 150)), "Oscuro");
    darkModeAction->setToolTip("Alternar modo oscuro (Ctrl+Shift+D)");
    connect(darkModeAction, &QAction::triggered, this, &MarkdownEditor::toggleDarkMode);
    
    QAction* lineNumbersAction = mainToolBar->addAction(createTextIcon("#", QFont("Arial", 12, QFont::Bold)), "Números");
    lineNumbersAction->setToolTip("Mostrar números de línea (Ctrl+Shift+L)");
    lineNumbersAction->setCheckable(true);
    connect(lineNumbersAction, &QAction::triggered, this, &MarkdownEditor::toggleLineNumbers);
    
    QAction* fullScreenAction = mainToolBar->addAction(createTextIcon("[ ]", QFont("Arial", 10, QFont::Bold)), "Pantalla");
    fullScreenAction->setToolTip("Alternar pantalla completa (F11)");
    fullScreenAction->setCheckable(true);
    connect(fullScreenAction, &QAction::triggered, this, &MarkdownEditor::toggleFullScreen);
}

void MarkdownEditor::setBold() {
    if (formatter) {
        formatter->setBold();
    }
}

void MarkdownEditor::setItalic() {
    if (formatter) {
        formatter->setItalic();
    }
}

void MarkdownEditor::setCode() {
    if (formatter) {
        formatter->setCode();
    }
}

void MarkdownEditor::setHeader() {
    if (formatter) {
        formatter->setHeader();
    }
}

void MarkdownEditor::setStrikethrough() {
    if (formatter) {
        formatter->setStrikethrough();
    }
}

void MarkdownEditor::setBlockquote() {
    if (formatter) {
        formatter->setBlockquote();
    }
}

void MarkdownEditor::setUnorderedList() {
    if (formatter) {
        formatter->setUnorderedList();
    }
}

void MarkdownEditor::setOrderedList() {
    if (formatter) {
        formatter->setOrderedList();
    }
}

void MarkdownEditor::setLink() {
    if (formatter) {
        formatter->setLink();
    }
}

void MarkdownEditor::setImage() {
    if (formatter) {
        formatter->setImage();
    }
}

void MarkdownEditor::setHorizontalRule() {
    if (formatter) {
        formatter->setHorizontalRule();
    }
}

void MarkdownEditor::setCodeBlock() {
    if (formatter) {
        formatter->setCodeBlock();
    }
}

void MarkdownEditor::insertEmoji() {
    // Cargar emojis personalizados
    loadCustomEmojis();
    
    // Crear un diálogo para seleccionar emojis
    QDialog* emojiDialog = new QDialog(this);
    emojiDialog->setWindowTitle("Insertar Emoji - Compatibles + Personalizados");
    emojiDialog->setFixedSize(600, 500);
    emojiDialog->setModal(true);
    
    // Configurar estilo del diálogo con fondo claro para mejor visibilidad de emojis
    emojiDialog->setStyleSheet(
        "QDialog {"
        "   background-color: #f5f5f5;"
        "   color: #333;"
        "}"
        "QLabel {"
        "   color: #333;"
        "   background-color: transparent;"
        "}"
        "QScrollArea {"
        "   background-color: #ffffff;"
        "   border: 1px solid #ddd;"
        "}"
        "QPushButton {"
        "   background-color: #ffffff;"
        "   color: #000000;"
        "   border: 1px solid #ccc;"
        "   border-radius: 5px;"
        "   font-size: 16px;"
        "}"
        "QPushButton:hover {"
        "   background-color: #e8f4fd;"
        "   border: 2px solid #007acc;"
        "}"
        "QPushButton:pressed {"
        "   background-color: #d0e8ff;"
        "}"
    );
    
    QVBoxLayout* mainLayout = new QVBoxLayout(emojiDialog);
    
    // Título
    QLabel* titleLabel = new QLabel("<h3>Emojis Básicos Compatibles:</h3>");
    titleLabel->setAlignment(Qt::AlignCenter);
    mainLayout->addWidget(titleLabel);
    
    // Crear un área de scroll para los emojis
    QScrollArea* scrollArea = new QScrollArea();
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    
    QWidget* emojiWidget = new QWidget();
    QGridLayout* emojiLayout = new QGridLayout(emojiWidget);
    
    // Lista de emojis actualizada según listado completo proporcionado
    QStringList emojis;
    emojis << "😄" << "😆" << "😊" << "😃" << "☺️" << "😏" << "😍" << "😘"
           << "😚" << "😳" << "😌" << "😁" << "😉" << "😜" << "😝" << "😀"
           << "😗" << "😙" << "😛" << "😴" << "😟" << "😦" << "😧" << "😮"
           << "😬" << "😕" << "😯" << "😑" << "😒" << "😅" << "😓" << "😥"
           << "😩" << "😔" << "😞" << "😖" << "😨" << "😰" << "😣" << "😢"
           << "😭" << "😂" << "😲" << "😱" << "😫" << "😠" << "😡" << "😤"
           << "😪" << "😋" << "😷" << "😎" << "😵" << "👿" << "😈" << "😐"
           << "😶" << "😇" << "👽" << "💛" << "💙" << "💜" << "❤️" << "💚"
           << "💔" << "💓" << "💗" << "💕" << "💞" << "💘" << "💖" << "✨"
           << "⭐" << "🌟" << "💫" << "💥" << "💢" << "❗" << "❓" << "❕"
           << "❔" << "💤" << "💨" << "💦" << "🎶" << "🎵" << "🔥" << "💩"
           << "👍" << "👎" << "👌" << "👊" << "✊" << "✌️" << "👋" << "✋"
           << "👐" << "☝️" << "👇" << "👈" << "👉" << "🙌" << "🙏" << "👆"
           << "👏" << "💪" << "🤘" << "🖕" << "🚶" << "🏃" << "👫" << "👪"
           << "👬" << "👭" << "💃" << "👯" << "🙆‍♀️" << "🙅" << "💁" << "🙋"
           << "👰‍♀️" << "🙇" << "💏" << "💑" << "💆" << "💇" << "💅" << "👦"
           << "👧" << "👩" << "👨" << "👶" << "👵" << "👴" << "👲" << "👳‍♂️"
           << "👷" << "👮" << "👼" << "👸" << "😺" << "😸" << "😻" << "😽"
           << "😼" << "🙀" << "😿" << "😹" << "😾" << "👹" << "👺" << "🙈"
           << "🙉" << "🙊" << "💂‍♂️" << "💀" << "🐾" << "👄" << "💋" << "💧"
           << "👂" << "👀" << "👃" << "👅" << "💌" << "👤" << "👥" << "💬"
           << "💭";
    
    // Agregar emojis personalizados
    emojis.append(customEmojis);
    
    // Crear botones para cada emoji
    int row = 0, col = 0;
    for (const QString& emoji : emojis) {
        QPushButton* emojiButton = new QPushButton(emoji);
        emojiButton->setFixedSize(40, 40);
        
        // Configurar fuente optimizada para emojis
        QFont emojiFont("Noto Color Emoji, Apple Color Emoji, Segoe UI Emoji, Arial", 16);
        emojiButton->setFont(emojiFont);
        
        emojiButton->setStyleSheet(
            "QPushButton {"
            "   border: 1px solid #ccc;"
            "   border-radius: 5px;"
            "   background-color: #ffffff;"
            "   color: #000000;"
            "   font-size: 16px;"
            "   font-family: 'Noto Color Emoji', 'Apple Color Emoji', 'Segoe UI Emoji', 'Arial';"
            "}"
            "QPushButton:hover {"
            "   background-color: #e8f4fd;"
            "   border: 2px solid #007acc;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #d0e8ff;"
            "}"
        );
        
        // Conectar el botón para insertar el emoji
        connect(emojiButton, &QPushButton::clicked, [this, emoji, emojiDialog]() {
            TabDocument* doc = getCurrentDocument();
            if (doc && doc->editor) {
                QTextCursor cursor = doc->editor->textCursor();
                cursor.insertText(emoji);
                doc->editor->setTextCursor(cursor);
                doc->isModified = true;
                updateTabTitle(documents.indexOf(doc));
            }
            emojiDialog->accept();
        });
        
        emojiLayout->addWidget(emojiButton, row, col);
        
        col++;
        if (col >= 10) {  // 10 columnas
            col = 0;
            row++;
        }
    }
    
    scrollArea->setWidget(emojiWidget);
    mainLayout->addWidget(scrollArea);
    
    // Botones de control
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* manageButton = new QPushButton("Gestionar Emojis");
    connect(manageButton, &QPushButton::clicked, [this, emojiDialog]() {
        emojiDialog->accept();
        manageCustomEmojis();
    });
    
    QPushButton* cancelButton = new QPushButton("Cancelar");
    connect(cancelButton, &QPushButton::clicked, emojiDialog, &QDialog::reject);
    
    buttonLayout->addWidget(manageButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(cancelButton);
    
    mainLayout->addLayout(buttonLayout);
    
    // Mostrar el diálogo
    emojiDialog->exec();
    delete emojiDialog;
}

void MarkdownEditor::renderMarkdown() {
    TabDocument* doc = getCurrentDocument();
    if (!doc || !doc->editor || !doc->browser) {
        return;
    }
    
    if (!previewEnabled || !shouldRenderPreview(doc->filePath)) {
        // Si no es un archivo de vista previa, limpiar el browser
        doc->browser->clear();
        return;
    }
    
    // Obtener el texto en Markdown desde el QTextEdit
    QString markdownText = doc->editor->toPlainText();
    
    // Si el texto está vacío, mostrar un mensaje de bienvenida
    if (markdownText.isEmpty()) {
        QString welcomeHtml = 
            "<html><head><meta charset='utf-8'></head><body>"
            "<h2 style='color: #555; text-align: center; margin-top: 50px;'>🎯 Escribe Chanchi</h2>"
            "<p style='color: #777; text-align: center;'>Editor de Markdown en tiempo real</p>"
            "<hr style='margin: 20px auto; width: 200px; border: 1px solid #ddd;'>"
            "<p style='color: #999; text-align: center; font-size: 14px;'>Empieza a escribir en el panel izquierdo...</p>"
            "<p style='color: #999; text-align: center; font-size: 12px;'>Soporte para <strong>negrita</strong>, <em>cursiva</em>, listas, enlaces y más</p>"
            "</body></html>";
        doc->browser->setHtml(welcomeHtml);
        return;
    }
    
    // Verificar si hay tablas en el texto
    QRegularExpression tableRegex("\\|.*\\|");
    if (markdownText.contains(tableRegex)) {
        // Si hay tablas, procesar todo como HTML personalizado con soporte de emojis
        QString htmlContent = processMarkdownWithTables(markdownText);
        
        // Agregar CSS mejorado para emojis
        QString htmlWithEmojiCSS = QString(
            "<!DOCTYPE html>"
            "<html><head>"
            "<meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<style>"
            "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; line-height: 1.6; padding: 20px; }"
            ".emoji { font-family: 'Apple Color Emoji', 'Segoe UI Emoji', 'Segoe UI Symbol', 'Noto Color Emoji'; font-size: 1.2em; }"
            "table { border-collapse: collapse; width: 100%%; margin: 10px 0; }"
            "th, td { border: 1px solid #ddd; padding: 8px; text-align: left; }"
            "th { background-color: #f2f2f2; font-weight: bold; }"
            "</style>"
            "</head><body>"
            "%1"
            "</body></html>"
        ).arg(htmlContent);
        
        // Envolver emojis en spans con clase específica
        QRegularExpression emojiRegex("([😀-🙏🌀-🗿🚀-🛿⚡-⚿✀-➿])");
        htmlWithEmojiCSS.replace(emojiRegex, "<span class='emoji'>\\1</span>");
        
        doc->browser->setHtml(htmlWithEmojiCSS);
    } else {
        // Si no hay tablas, usar cmark normal con mejoras para emojis
        QByteArray markdownByteArray = markdownText.toUtf8();
        int options = CMARK_OPT_HARDBREAKS;
        
        cmark_node *root = cmark_parse_document(markdownByteArray.constData(), markdownByteArray.size(), options);
        char *html = cmark_render_html(root, options);
        
        // Crear HTML mejorado con soporte completo de emojis
        QString htmlWithEmoji = QString(
            "<!DOCTYPE html>"
            "<html><head>"
            "<meta charset='UTF-8'>"
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<style>"
            "body { font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif; line-height: 1.6; padding: 20px; }"
            ".emoji { font-family: 'Apple Color Emoji', 'Segoe UI Emoji', 'Segoe UI Symbol', 'Noto Color Emoji'; font-size: 1.2em; }"
            "p, li, h1, h2, h3, h4, h5, h6 { font-family: inherit; }"
            "</style>"
            "</head><body>"
            "%1"
            "</body></html>"
        ).arg(QString::fromUtf8(html));
        
        // Envolver emojis en spans con clase específica
        QRegularExpression emojiRegex("([😀-🙏🌀-🗿🚀-🛿⚡-⚿✀-➿])");
        htmlWithEmoji.replace(emojiRegex, "<span class='emoji'>\\1</span>");
        
        doc->browser->setHtml(htmlWithEmoji);
        
        free(html);
        cmark_node_free(root);
    }
}


// Implementación de exportToHTML
void MarkdownEditor::exportToHTML() {
    QString fileName = QFileDialog::getSaveFileName(this, "Guardar como", "", "HTML files (*.html)");
    if (!fileName.isEmpty()) {
        QFile file(fileName);
        if (file.open(QIODevice::WriteOnly)) {
            QTextStream out(&file);
            out << browser->toHtml();  // Guardamos el contenido HTML del QTextBrowser
        }
    }
}

// Implementación de exportToPDF
void MarkdownEditor::exportToPDF() {
    QString fileName = QFileDialog::getSaveFileName(this, "Guardar como PDF", "", "PDF files (*.pdf)");
    if (!fileName.isEmpty()) {
        QPrinter printer(QPrinter::HighResolution);
        printer.setOutputFormat(QPrinter::PdfFormat);
        printer.setOutputFileName(fileName);
        
        QPainter painter;
        if (painter.begin(&printer)) {
            browser->document()->drawContents(&painter);
            painter.end();
        }
    }
}

void MarkdownEditor::saveToFile() {
    // Mostrar un cuadro de diálogo para elegir el archivo de destino
    QString fileName = QFileDialog::getSaveFileName(this, tr("Guardar archivo"), "", 
        tr("Archivos Markdown (*.md);;Archivos de texto (*.txt);;Listas M3U (*.m3u);;Archivos de código (*.cpp *.h *.js *.py);;Todos los archivos (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    // Abrir el archivo en modo escritura
    QFile file(fileName);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        // Escribir el contenido del editor al archivo
        out << editor->toPlainText();
        file.close();
    }
}

void MarkdownEditor::loadFromFile() {
    // Mostrar un cuadro de diálogo para elegir el archivo a cargar
    QString fileName = QFileDialog::getOpenFileName(this, tr("Abrir archivo"), "", 
        tr("Archivos Markdown (*.md);;Archivos de texto (*.txt);;Listas M3U (*.m3u);;Archivos de código (*.cpp *.h *.js *.py);;Todos los archivos (*)"));
    if (fileName.isEmpty()) {
        return;
    }

    // Abrir el archivo en modo lectura
    QFile file(fileName);
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream in(&file);
        // Cargar el contenido del archivo en el editor
        editor->setPlainText(in.readAll());
        file.close();
    }
}

void MarkdownEditor::showManual() {
    QDialog* manualDialog = new QDialog(this);
    manualDialog->setWindowTitle("Manual de Markdown");
    manualDialog->resize(720, 640);
    manualDialog->setMinimumSize(600, 480);
    
    QVBoxLayout* layout = new QVBoxLayout(manualDialog);
    
    QScrollArea* scrollArea = new QScrollArea(manualDialog);
    QLabel* manualLabel = new QLabel();
    
    const QString manualText = QStringLiteral(R"manual(
<h1>Manual de Markdown</h1>
<p>Escribe el artículo en el panel de la izquierda. A la derecha ves el mismo texto ya interpretado.
<b>Ctrl+Shift+P</b> muestra u oculta esa vista. Los botones de la barra y el menú Formato insertan estos signos por ti:
selecciona un trozo y pulsa el atajo, o déjalo vacío para colocar los signos y escribir dentro.</p>

<h2>Párrafos y saltos de línea</h2>
<p>Pulsa Intro para partir la línea. Deja una línea en blanco para empezar otro párrafo.</p>
<p>Escribes:</p>
<pre>Primera línea
sigue debajo.

Esto ya es otro párrafo.</pre>
<p>Se ve: la primera frase y la segunda van juntas, con un salto; la tercera queda en un párrafo aparte.</p>

<h2>Encabezados</h2>
<p>De uno a seis signos <code>#</code>, un espacio y el título. <b>Ctrl+1</b> inserta un encabezado de nivel 1 al principio de la línea.</p>
<pre># Título del artículo
## Un apartado
### Un subapartado
###### El más pequeño</pre>
<p>Se ve: títulos de mayor a menor tamaño, del nivel 1 al 6.</p>

<h2>Negrita, cursiva y tachado</h2>
<p><b>Ctrl+B</b> rodea el texto con dos asteriscos. <b>Ctrl+I</b> usa uno. Tres asteriscos juntan las dos cosas.
<b>Ctrl+Shift+X</b> lo tacha con dos virgulillas.</p>
<pre>**negrita**
*cursiva*
***negrita y cursiva***
~~tachado~~</pre>
<p>Se ve: <strong>negrita</strong>, <em>cursiva</em>, <strong><em>negrita y cursiva</em></strong> y <del>tachado</del>.</p>

<h2>Código en una línea</h2>
<p><b>Ctrl+`</b> rodea una palabra o una orden con comillas invertidas, para que no se interprete como formato.</p>
<pre>El atajo es `Ctrl+S`.</pre>
<p>Se ve: El atajo es <code>Ctrl+S</code>.</p>

<h2>Listas</h2>
<p><b>Ctrl+U</b> empieza una viñeta con <code>- </code>. <b>Ctrl+Shift+O</b> empieza una lista numerada con <code>1. </code>.
Para meter una lista dentro de otra, sangra la línea con dos espacios.</p>
<pre>- Fruta
  - Manzana
  - Pera
- Verdura

1. Abrir el archivo
2. Escribir
3. Guardar</pre>
<p>Se ve: una lista con dos elementos dentro del primero, y otra con los pasos 1, 2 y 3.</p>

<h2>Enlaces</h2>
<p><b>Ctrl+L</b> inserta el texto visible entre corchetes y la dirección entre paréntesis.</p>
<pre>[Página del proyecto](https://ejemplo.com)</pre>
<p>Se ve: un enlace con el texto «Página del proyecto» que apunta a esa dirección.</p>

<h2>Imágenes</h2>
<p><b>Ctrl+Shift+I</b> es igual que un enlace, con un signo de cierre de admiración delante.
El texto entre corchetes se muestra si la imagen no carga.</p>
<pre>![Logotipo](img/logo.png)</pre>
<p>Se ve: la imagen de esa ruta, o el texto «Logotipo» si no se encuentra.</p>

<h2>Bloque de código</h2>
<p><b>Ctrl+Shift+`</b> abre y cierra un bloque con tres comillas invertidas, cada una en su línea.
Si en la primera línea escribes el lenguaje, el bloque queda marcado como ese código.</p>
<pre>```cpp
int suma = 1 + 2;
```</pre>
<p>Se ve: esas dos líneas con letra de código, sin interpretar los signos que haya dentro.</p>

<h2>Citas</h2>
<p><b>Ctrl+Shift+Q</b> pone <code>&gt; </code> al principio de la línea. Repite el signo en cada línea de la cita.</p>
<pre>&gt; Una frase citada.
&gt; Sigue en la línea siguiente.</pre>
<p>Se ve: las dos frases en un bloque de cita.</p>

<h2>Línea horizontal</h2>
<p><b>Ctrl+Shift+H</b> inserta tres guiones en una línea sola. También valen tres asteriscos.</p>
<pre>---</pre>
<p>Se ve: una raya que separa dos partes del artículo.</p>

<h2>Tablas</h2>
<p>La primera fila son los títulos. La segunda solo tiene guiones, uno por columna, y marca esa fila como cabecera.
Las demás son los datos. Cada celda va entre barras verticales. <b>Ctrl+T</b> pregunta las filas y las columnas y escribe este esquema.</p>
<pre>| Nombre | Precio |
|--------|--------|
| Pan    | 1 €    |
| Leche  | 2 €    |</pre>
<p>Se ve:</p>
<table style="border-collapse: collapse;">
<tr><th style="border: 1px solid #999; padding: 4px 8px;">Nombre</th><th style="border: 1px solid #999; padding: 4px 8px;">Precio</th></tr>
<tr><td style="border: 1px solid #999; padding: 4px 8px;">Pan</td><td style="border: 1px solid #999; padding: 4px 8px;">1 €</td></tr>
<tr><td style="border: 1px solid #999; padding: 4px 8px;">Leche</td><td style="border: 1px solid #999; padding: 4px 8px;">2 €</td></tr>
</table>

<h2>Signos que deben verse tal cual</h2>
<p>Una barra invertida delante de <code>*</code>, de una comilla invertida o de <code>#</code> anula el formato.</p>
<pre>\*esto no es cursiva\*
\`esto no es código\`
\# esto no es un título</pre>
<p>Se ve: los signos *, ` y # como texto normal.</p>
)manual");
    
    manualLabel->setTextFormat(Qt::RichText);
    manualLabel->setText(manualText);
    manualLabel->setWordWrap(true);
    manualLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    manualLabel->setMargin(20);
    
    scrollArea->setWidget(manualLabel);
    scrollArea->setWidgetResizable(true);
    
    layout->addWidget(scrollArea);

    QHBoxLayout* buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch();
    QPushButton* closeButton = new QPushButton("Cerrar", manualDialog);
    connect(closeButton, &QPushButton::clicked, manualDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    layout->addLayout(buttonLayout);
    
    manualDialog->exec();
    delete manualDialog;
}

void MarkdownEditor::showAbout() {
    QDialog* aboutDialog = new QDialog(this);
    aboutDialog->setWindowTitle("Acerca de EscribeChanchi");
    aboutDialog->resize(620, 750);  // Aumentamos el tamaño para mostrar todo el contenido
    aboutDialog->setMinimumSize(600, 700);  // Tamaño mínimo para que sea redimensionable
    
    QVBoxLayout* mainLayout = new QVBoxLayout(aboutDialog);
    mainLayout->setContentsMargins(10, 10, 10, 10);  // Márgenes más pequeños
    
    // Crear un área de scroll
    QScrollArea* scrollArea = new QScrollArea(aboutDialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameStyle(QFrame::NoFrame);  // Sin marco para mejor apariencia
    
    // Widget contenedor para el contenido
    QWidget* contentWidget = new QWidget();
    QVBoxLayout* layout = new QVBoxLayout(contentWidget);
    layout->setAlignment(Qt::AlignTop);  // Alineación superior para mejor uso del espacio
    layout->setSpacing(10);  // Espaciado más compacto
    layout->setContentsMargins(15, 10, 15, 10);  // Márgenes ajustados
    
    // Título del programa con versión
    QLabel* titleLabel = new QLabel("<h1>EscribeChanchi v1.6</h1>");
    titleLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(titleLabel);
    
    // Logo - usar recurso integrado
    QLabel* logoLabel = new QLabel();
    QPixmap logo(":/img/logo.png");  // Usar el logo original para la ventana About
    if (!logo.isNull()) {
        logoLabel->setPixmap(logo.scaled(120, 120, Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else {
        logoLabel->setText("<p style='color: gray;'>[Logo no encontrado]</p>");
    }
    logoLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(logoLabel);
    
    // Descripción completa actualizada con todas las características
    QLabel* descLabel = new QLabel("<div style='text-align: left; line-height: 1.4;'>"
                                  "<p style='text-align: center; margin-bottom: 10px;'><strong>EscribeChanchi v1.6</strong> - Editor Markdown pamiscosas con lo que voy necesitando</p>"
                                  "<p style='text-align: center; margin-bottom: 15px; color: #666;'>Editor avanzado con sistema de pestañas, búsqueda/reemplazo y emojis optimizados</p>"
                                  
                                  "<p style='color: #2e7d32; font-weight: bold; margin-bottom: 8px;'>🆕 NUEVAS CARACTERÍSTICAS v1.6</p>"
                                  "<p style='margin: 4px 0;'>• Sistema completo de búsqueda y reemplazo avanzado</p>"
                                  "<p style='margin: 4px 0;'>• Barra de herramientas renovada con iconos descriptivos</p>"
                                  "<p style='margin: 4px 0;'>• 76 emojis optimizados universalmente compatibles</p>"
                                  "<p style='margin: 4px 0;'>• Scripts inteligentes de compilación y ejecución</p>"
                                  "<p style='margin: 4px 0;'>• Navegación circular en búsquedas (F3/Shift+F3)</p>"
                                  "<p style='margin: 4px 0;'>• Opciones avanzadas: Sensible a mayúsculas, palabras completas</p>"
                                  
                                  "<p style='color: #1976d2; font-weight: bold; margin: 15px 0 8px 0;'>⭐ CARACTERÍSTICAS PRINCIPALES</p>"
                                  "<p style='margin: 3px 0;'>• Sistema de pestañas para múltiples archivos simultáneos</p>"
                                  "<p style='margin: 3px 0;'>• Vista previa en tiempo real de Markdown con libcmark</p>"
                                  "<p style='margin: 3px 0;'>• Modo oscuro y modo claro (F11)</p>"
                                  "<p style='margin: 3px 0;'>• Exportación profesional a HTML y PDF</p>"
                                  "<p style='margin: 3px 0;'>• Plantillas predefinidas (blog, técnico, README)</p>"
                                  "<p style='margin: 3px 0;'>• Bandeja del sistema con minimizado inteligente</p>"
                                  "<p style='margin: 3px 0;'>• Atajos de teclado estándar completos</p>"
                                  "<p style='margin: 3px 0;'>• Soporte múltiples formatos (.md, .txt, .py, .js, .cpp, .json, etc.)</p>"
                                  "<p style='margin: 3px 0;'>• Modo pantalla completa (F10)</p>"
                                  "<p style='margin: 3px 0;'>• Números de línea opcionales</p>"
                                  
                                  "<p style='color: #7b1fa2; font-weight: bold; margin: 15px 0 8px 0;'>✍️ FORMATO DE TEXTO</p>"
                                  "<p style='margin: 3px 0;'>• Negrita (Ctrl+B), Cursiva (Ctrl+I), Tachado</p>"
                                  "<p style='margin: 3px 0;'>• Código en línea (`texto`) y bloques de código</p>"
                                  "<p style='margin: 3px 0;'>• Encabezados (H1-H6), Citas, Listas</p>"
                                  "<p style='margin: 3px 0;'>• Enlaces, Imágenes, Tablas, Líneas horizontales</p>"
                                  "<p style='margin: 3px 0;'>• Inserción de emojis optimizada (Ctrl+E)</p>"
                                  
                                  "<p style='color: #f57c00; font-weight: bold; margin: 15px 0 8px 0;'>📁 GESTIÓN DE ARCHIVOS</p>"
                                  "<p style='margin: 3px 0;'>• Abrir/Guardar múltiples formatos</p>"
                                  "<p style='margin: 3px 0;'>• Detección automática de cambios</p>"
                                  "<p style='margin: 3px 0;'>• Gestión inteligente de pestañas</p>"
                                  "<p style='margin: 3px 0;'>• Plantillas personalizables</p>"
                                  
                                  "<p style='color: #d32f2f; font-weight: bold; margin: 15px 0 8px 0;'>🎨 INTERFAZ AVANZADA</p>"
                                  "<p style='margin: 3px 0;'>• Barra de herramientas con iconos gráficos (sin dependencias Unicode)</p>"
                                  "<p style='margin: 3px 0;'>• Menús contextuales completos</p>"
                                  "<p style='margin: 3px 0;'>• Diálogos no modales para búsqueda</p>"
                                  "<p style='margin: 3px 0;'>• Integración con bandeja del sistema</p>"
                                  "<p style='margin: 3px 0;'>• Tooltips informativos</p>"
                                  
                                  "<p style='text-align: center; margin-top: 20px; color: #666; font-style: italic;'>Desarrollado en C++ con Qt5/Qt6 + libcmark</p>"
                                  "<p style='text-align: center; margin: 5px 0; color: #666; font-style: italic;'>Compatible con Linux (Ubuntu, Fedora, Arch)</p>"
                                  "</div>");
    descLabel->setAlignment(Qt::AlignTop);
    descLabel->setWordWrap(true);
    descLabel->setMargin(10);
    descLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    layout->addWidget(descLabel);
    
    // Configurar el scroll area
    scrollArea->setWidget(contentWidget);
    mainLayout->addWidget(scrollArea);
    
    // Layout horizontal para los botones
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    // Botón para abrir repositorio en GitHub
    QPushButton* githubButton = new QPushButton(">> Ver en GitHub");
    githubButton->setToolTip("Abrir repositorio en GitHub");
    connect(githubButton, &QPushButton::clicked, [this]() {
        QDesktopServices::openUrl(QUrl("https://github.com/sapoclay/escribe-chanchi"));
    });
    buttonLayout->addWidget(githubButton);
    
    // Agregar espacio entre botones
    buttonLayout->addStretch();
    
    // Botón de cerrar
    QPushButton* closeButton = new QPushButton("Cerrar");
    connect(closeButton, &QPushButton::clicked, aboutDialog, &QDialog::accept);
    buttonLayout->addWidget(closeButton);
    
    mainLayout->addLayout(buttonLayout);
    
    aboutDialog->exec();
}

void MarkdownEditor::insertTable() {
    if (formatter) {
        formatter->insertTable();
    }
}

void MarkdownEditor::toggleLineNumbers() {
    TabDocument* doc = getCurrentDocument();
    if (!doc || !doc->editor) {
        return;
    }
    
    showLineNumbers = !showLineNumbers;
    
    if (showLineNumbers) {
        // Crear un estilo más visible para los números de línea
        doc->editor->setStyleSheet(editorStyleSheet(darkMode));
        
        // Añadir números visualmente en el contenido
        QString content = doc->editor->toPlainText();
        if (!content.contains("│")) {  // Solo si no tiene ya números
            QStringList lines = content.split('\n');
            QString numberedContent;
            for (int i = 0; i < lines.size(); i++) {
                numberedContent += QString("%1 │ %2\n").arg(i + 1, 3).arg(lines[i]);
            }
            
            // Desconectar temporalmente para evitar recursión
            disconnect(doc->editor, &QTextEdit::textChanged, this, &MarkdownEditor::renderMarkdown);
            doc->editor->setPlainText(numberedContent);
            connect(doc->editor, &QTextEdit::textChanged, this, &MarkdownEditor::renderMarkdown);
        }
    } else {
        // Remover números de línea
        doc->editor->setStyleSheet(editorStyleSheet(darkMode));
        
        // Remover números del contenido
        QString content = doc->editor->toPlainText();
        if (content.contains("│")) {
            QStringList lines = content.split('\n');
            QString cleanContent;
            for (const QString &line : lines) {
                int separatorIndex = line.indexOf("│");
                if (separatorIndex > 0) {
                    cleanContent += line.mid(separatorIndex + 2) + '\n';
                } else {
                    cleanContent += line + '\n';
                }
            }
            
            // Desconectar temporalmente para evitar recursión
            disconnect(doc->editor, &QTextEdit::textChanged, this, &MarkdownEditor::renderMarkdown);
            doc->editor->setPlainText(cleanContent.trimmed());
            connect(doc->editor, &QTextEdit::textChanged, this, &MarkdownEditor::renderMarkdown);
        }
    }
}

void MarkdownEditor::updateLineNumbers() {
    if (!showLineNumbers) return;
    
    // Esta es una implementación básica usando tooltip
    // Para una implementación completa se necesitaría un widget personalizado
    QTextCursor cursor = editor->textCursor();
    int currentLine = cursor.blockNumber() + 1;
    int totalLines = editor->document()->blockCount();
    
    // Mostrar información de línea en la barra de estado (si la tuviéramos)
    // Por ahora, cambiaremos el color del borde para indicar actividad
    QString lineStyle = QString(
        "QTextEdit { "
        "   padding-left: 50px; "
        "   border-left: 3px solid %1; "
        "} "
    ).arg(darkMode ? "#555555" : "#cccccc");
    
    // Aplicar el estilo manteniendo el estilo existente
    QString currentStyle = editor->styleSheet();
    QRegularExpression borderRegex("border-left:\\s*[^;]+;");
    currentStyle.replace(borderRegex, QString("border-left: 3px solid %1;").arg(darkMode ? "#555555" : "#cccccc"));
    
    editor->setStyleSheet(currentStyle);
}

QString MarkdownEditor::processTablesManually(const QString &text) {
    QString result = text;
    QStringList lines = result.split('\n');
    QString processedText;
    
    for (int i = 0; i < lines.size(); i++) {
        QString line = lines[i].trimmed();
        
        // Detectar si es una línea de tabla (contiene |)
        if (line.contains('|') && !line.isEmpty()) {
            // Buscar el encabezado, separador y filas de datos
            QStringList tableLines;
            int startIndex = i;
            
            // Recopilar todas las líneas de la tabla
            while (i < lines.size() && lines[i].trimmed().contains('|') && !lines[i].trimmed().isEmpty()) {
                tableLines.append(lines[i].trimmed());
                i++;
            }
            i--; // Ajustar porque el bucle for incrementará i
            
            if (tableLines.size() >= 2) {
                // Convertir a HTML
                QString tableHtml = "<table border='1' style='border-collapse: collapse; margin: 10px 0;'>";
                
                // Primera fila como encabezado
                if (tableLines.size() > 0) {
                    QString headerRow = tableLines[0];
                    QStringList headers = headerRow.split('|');
                    tableHtml += "<tr style='background-color: #f0f0f0; font-weight: bold;'>";
                    for (const QString &header : headers) {
                        QString cleanHeader = header.trimmed();
                        if (!cleanHeader.isEmpty()) {
                            tableHtml += "<td style='padding: 8px; border: 1px solid #ddd;'>" + cleanHeader + "</td>";
                        }
                    }
                    tableHtml += "</tr>";
                }
                
                // Saltar la línea separadora (segunda línea con ---)
                for (int j = 2; j < tableLines.size(); j++) {
                    QString dataRow = tableLines[j];
                    QStringList cells = dataRow.split('|');
                    tableHtml += "<tr>";
                    for (const QString &cell : cells) {
                        QString cleanCell = cell.trimmed();
                        if (!cleanCell.isEmpty()) {
                            tableHtml += "<td style='padding: 8px; border: 1px solid #ddd;'>" + cleanCell + "</td>";
                        }
                    }
                    tableHtml += "</tr>";
                }
                
                tableHtml += "</table>";
                processedText += tableHtml + "\n";
            } else {
                // No es una tabla válida, añadir las líneas normalmente
                for (const QString &tableLine : tableLines) {
                    processedText += tableLine + "\n";
                }
            }
        } else {
            processedText += line + "\n";
        }
    }
    
    return processedText;
}

QString MarkdownEditor::processMarkdownWithTables(const QString &text) {
    QStringList lines = text.split('\n');
    QString htmlResult = "<html><body style='font-family: Arial, sans-serif; line-height: 1.6; margin: 20px;'>";
    
    bool inTable = false;
    QString currentTable;
    
    for (int i = 0; i < lines.size(); i++) {
        QString line = lines[i];
        
        // Detectar si es una línea de tabla
        if (line.contains('|') && !line.trimmed().isEmpty()) {
            if (!inTable) {
                inTable = true;
                currentTable = "";
            }
            currentTable += line + "\n";
        } else {
            // Si estábamos en una tabla, procesarla
            if (inTable) {
                htmlResult += convertTableToHtml(currentTable);
                inTable = false;
                currentTable = "";
            }
            
            // Procesar línea normal de Markdown
            htmlResult += processMarkdownLine(line);
        }
    }
    
    // Si terminamos en una tabla
    if (inTable) {
        htmlResult += convertTableToHtml(currentTable);
    }
    
    htmlResult += "</body></html>";
    return htmlResult;
}

QString MarkdownEditor::convertTableToHtml(const QString &tableText) {
    QStringList lines = tableText.split('\n', Qt::SkipEmptyParts);
    if (lines.size() < 2) return tableText; // No es una tabla válida
    
    QString html = "<table style='border-collapse: collapse; margin: 15px 0; width: 100%; border: 1px solid #ddd;'>";
    
    // Primera fila como encabezado
    QString headerLine = lines[0].trimmed();
    if (headerLine.startsWith('|')) headerLine = headerLine.mid(1);
    if (headerLine.endsWith('|')) headerLine.chop(1);
    
    QStringList headers = headerLine.split('|');
    html += "<tr style='background-color: #f5f5f5;'>";
    for (const QString &header : headers) {
        html += "<th style='border: 1px solid #ddd; padding: 12px; text-align: left; font-weight: bold;'>" 
                + header.trimmed() + "</th>";
    }
    html += "</tr>";
    
    // Saltar línea separadora y procesar filas de datos
    for (int i = 2; i < lines.size(); i++) {
        QString dataLine = lines[i].trimmed();
        if (dataLine.startsWith('|')) dataLine = dataLine.mid(1);
        if (dataLine.endsWith('|')) dataLine.chop(1);
        
        QStringList cells = dataLine.split('|');
        html += "<tr>";
        for (const QString &cell : cells) {
            html += "<td style='border: 1px solid #ddd; padding: 12px;'>" + cell.trimmed() + "</td>";
        }
        html += "</tr>";
    }
    
    html += "</table>";
    return html;
}

QString MarkdownEditor::processMarkdownLine(const QString &line) {
    QString result = line;
    
    // Encabezados
    if (result.startsWith("# ")) {
        return "<h1>" + result.mid(2) + "</h1>";
    } else if (result.startsWith("## ")) {
        return "<h2>" + result.mid(3) + "</h2>";
    } else if (result.startsWith("### ")) {
        return "<h3>" + result.mid(4) + "</h3>";
    }
    
    // Negrita **texto**
    QRegularExpression boldRegex("\\*\\*(.*?)\\*\\*");
    result.replace(boldRegex, "<strong>\\1</strong>");
    
    // Cursiva *texto*
    QRegularExpression italicRegex("\\*(.*?)\\*");
    result.replace(italicRegex, "<em>\\1</em>");
    
    // Código `texto`
    QRegularExpression codeRegex("`(.*?)`");
    result.replace(codeRegex, "<code style='background-color: #f4f4f4; padding: 2px 4px; border-radius: 3px;'>\\1</code>");
    
    // Tachado ~~texto~~
    QRegularExpression strikeRegex("~~(.*?)~~");
    result.replace(strikeRegex, "<del>\\1</del>");
    
    // Citas
    if (result.trimmed().startsWith("> ")) {
        return "<blockquote style='border-left: 4px solid #ddd; padding-left: 16px; margin: 10px 0; color: #666;'>" 
               + result.mid(result.indexOf("> ") + 2) + "</blockquote>";
    }
    
    // Listas
    if (result.trimmed().startsWith("- ")) {
        return "<ul><li>" + result.mid(result.indexOf("- ") + 2) + "</li></ul>";
    } else {
        QRegularExpression listRegex("^\\d+\\. ");
        if (listRegex.match(result.trimmed()).hasMatch()) {
            return "<ol><li>" + result.mid(result.indexOf(". ") + 2) + "</li></ol>";
        }
    }
    
    // Líneas horizontales
    if (result.trimmed() == "---" || result.trimmed() == "***") {
        return "<hr style='border: none; border-top: 2px solid #ddd; margin: 20px 0;'>";
    }
    
    // Párrafos normales
    if (!result.trimmed().isEmpty()) {
        return "<p>" + result + "</p>";
    }
    
    return "<br>";
}

void MarkdownEditor::showTemplates() {
    if (templateManager) {
        templateManager->showTemplateDialog(editor);
    }
}

void MarkdownEditor::createTemplate() {
    if (templateManager) {
        templateManager->showCreateTemplateDialog(editor);
    }
}

// Funciones para manejo de pestañas

TabDocument* MarkdownEditor::createNewDocument() {
    TabDocument* doc = new TabDocument();
    
       
    // Crear los widgets
    doc->editor = new QTextEdit();
    doc->browser = new QTextBrowser();
    doc->splitter = new QSplitter(Qt::Horizontal);
    
    // Agregar widgets al splitter
    doc->splitter->addWidget(doc->editor);
    doc->splitter->addWidget(doc->browser);
    
    // Configurar documento
    doc->fileName = QString("Sin título %1").arg(untitledCounter++);
    doc->isNewFile = true;
    doc->isModified = false;
    
    // Configurar conexiones
    setupDocumentConnections(doc);
    
    return doc;
}

TabDocument* MarkdownEditor::getCurrentDocument() {
    int currentIndex = tabWidget->currentIndex();
    if (currentIndex >= 0 && currentIndex < documents.size()) {
        return documents[currentIndex];
    }
    return nullptr;
}

void MarkdownEditor::setupDocumentConnections(TabDocument* doc) {
    // Conectar para renderizado automático
    connect(doc->editor, &QTextEdit::textChanged, [this, doc]() {
        // Verificar si este documento es el actual antes de renderizar
        TabDocument* currentDoc = getCurrentDocument();
        if (doc == currentDoc) {
            renderMarkdown();
        }
        if (!doc->isModified) {
            doc->isModified = true;
            updateTabTitle(documents.indexOf(doc));
        }
    });
    
    // Conectar para numeración de líneas
    connect(doc->editor, &QTextEdit::cursorPositionChanged, this, &MarkdownEditor::updateLineNumbers);
}

void MarkdownEditor::updateTabTitle(int index) {
    if (index >= 0 && index < documents.size()) {
        TabDocument* doc = documents[index];
        QString title = doc->fileName;
        if (doc->isModified) {
            title += " *";
        }
        tabWidget->setTabText(index, title);
    }
}

void MarkdownEditor::newTab() {
    TabDocument* doc = createNewDocument();
    documents.append(doc);
    
    int index = tabWidget->addTab(doc->splitter, doc->fileName);
    tabWidget->setCurrentIndex(index);
    
    // Actualizar punteros de compatibilidad
    editor = doc->editor;
    browser = doc->browser;
    splitter = doc->splitter;
    
    // Recrear formatter para el nuevo editor
    if (formatter) {
        delete formatter;
    }
    formatter = new MarkdownFormatter(editor, this);
    
    // Configurar estilos si está en modo oscuro
    if (darkMode) {
        setDarkMode();
    } else {
        setLightMode();
    }
    
    applyPreviewMode(doc);
    renderMarkdown();
}

void MarkdownEditor::applyPreviewMode(TabDocument *doc) {
    if (!doc || !doc->browser || !doc->splitter) {
        return;
    }

    const bool preview = previewEnabled && shouldRenderPreview(doc->filePath);
    doc->browser->setVisible(preview);
    doc->splitter->setStretchFactor(0, 1);
    doc->splitter->setStretchFactor(1, preview ? 1 : 0);

    const int total = qMax(doc->splitter->width(), 800);
    if (preview) {
        doc->splitter->setSizes({total / 2, total - total / 2});
    } else {
        doc->splitter->setSizes({total, 0});
    }
}

bool MarkdownEditor::openFilePath(const QString &fileName) {
    QFileInfo requested(fileName);
    if (!requested.exists() || !requested.isFile()) {
        QMessageBox::warning(this, "Error",
            QString("No se pudo abrir el archivo:\n%1").arg(fileName));
        return false;
    }

    QString absolute = requested.canonicalFilePath();
    if (absolute.isEmpty()) {
        absolute = requested.absoluteFilePath();
    }

    for (int i = 0; i < documents.size(); ++i) {
        if (documents.at(i)->filePath == absolute) {
            tabWidget->setCurrentIndex(i);
            restoreFromTray();
            addToRecentFiles(absolute);
            return true;
        }
    }

    QFile file(absolute);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
            QString("No se pudo abrir el archivo:\n%1").arg(absolute));
        return false;
    }

    QTextStream in(&file);
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    in.setEncoding(QStringConverter::Utf8);
#else
    in.setCodec("UTF-8");
#endif
    const QString content = in.readAll();
    file.close();

    TabDocument *doc = getCurrentDocument();
    const bool reuseBlank = doc
        && doc->isNewFile
        && !doc->isModified
        && doc->editor
        && doc->editor->toPlainText().isEmpty();

    if (!reuseBlank) {
        doc = createNewDocument();
        documents.append(doc);
        const int index = tabWidget->addTab(doc->splitter, doc->fileName);
        tabWidget->setCurrentIndex(index);
    }

    doc->editor->blockSignals(true);
    doc->editor->setPlainText(content);
    doc->editor->blockSignals(false);

    doc->fileName = requested.fileName();
    doc->filePath = absolute;
    doc->isNewFile = false;
    doc->isModified = false;

    const int index = documents.indexOf(doc);
    updateTabTitle(index);
    applyPreviewMode(doc);

    if (tabWidget->currentIndex() == index) {
        tabChanged(index);
    } else {
        tabWidget->setCurrentIndex(index);
    }

    restoreFromTray();
    addToRecentFiles(absolute);
    statusBar()->showMessage("Archivo cargado", 2000);
    return true;
}

void MarkdownEditor::openFileInNewTab() {
    const QString fileName = QFileDialog::getOpenFileName(this,
        "Abrir archivo", "",
        "Archivos de texto (*.md *.txt *.m3u *.cpp *.h *.js *.py *.html *.css *.xml *.json);;Archivos Markdown (*.md);;Todos los archivos (*)");

    if (!fileName.isEmpty()) {
        openFilePath(fileName);
    }
}

void MarkdownEditor::closeTab(int index) {
    if (index >= 0 && index < documents.size()) {
        TabDocument* doc = documents[index];
        
        if (closeDocument(doc)) {
            documents.removeAt(index);
            tabWidget->removeTab(index);
            delete doc;
            
            // Si no quedan pestañas, crear una nueva
            if (documents.isEmpty()) {
                newTab();
            }
        }
    }
}

void MarkdownEditor::closeCurrentTab() {
    int currentIndex = tabWidget->currentIndex();
    closeTab(currentIndex);
}

void MarkdownEditor::tabChanged(int index) {
    if (index >= 0 && index < documents.size()) {
        TabDocument* doc = documents[index];
        
        // Actualizar punteros de compatibilidad
        editor = doc->editor;
        browser = doc->browser;
        splitter = doc->splitter;
        
        // Recrear formatter para el editor actual
        if (formatter) {
            delete formatter;
        }
        formatter = new MarkdownFormatter(editor, this);
        
        // Actualizar título de ventana con tipo de archivo
        QString windowTitle = "EscribeChanchi";
        if (!doc->fileName.isEmpty()) {
            windowTitle += " - " + doc->fileName;
            QString fileType = getFileType(doc->filePath);
            if (fileType != "markdown" && fileType != "text") {
                windowTitle += QString(" (%1)").arg(fileType.toUpper());
            }
        }
        setWindowTitle(windowTitle);
        
        // Renderizar el contenido actual
        renderMarkdown();
        
        // Aplicar estilo actual
        if (darkMode) {
            setDarkMode();
        } else {
            setLightMode();
        }
    }
}

bool MarkdownEditor::closeDocument(TabDocument* doc) {
    if (doc->isModified) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Documento modificado",
            QString("El documento '%1' ha sido modificado.\n¿Desea guardar los cambios?")
                .arg(doc->fileName),
            QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel);
        
        if (reply == QMessageBox::Save) {
            return saveDocument(doc);
        } else if (reply == QMessageBox::Cancel) {
            return false;
        }
    }
    return true;
}

bool MarkdownEditor::saveDocument(TabDocument* doc) {
    if (doc->isNewFile || doc->filePath.isEmpty()) {
        return saveDocumentAs(doc);
    }
    
    QFile file(doc->filePath);
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&file);
        out << doc->editor->toPlainText();
        
        doc->isModified = false;
        updateTabTitle(documents.indexOf(doc));
        return true;
    }
    
    QMessageBox::warning(this, "Error", "No se pudo guardar el archivo.");
    return false;
}

bool MarkdownEditor::saveDocumentAs(TabDocument* doc) {
    QString fileName = QFileDialog::getSaveFileName(this,
        "Guardar archivo", doc->fileName, "Archivos Markdown (*.md);;Archivos de texto (*.txt);;Listas M3U (*.m3u);;Archivos de código (*.cpp *.h *.js *.py);;Todos los archivos (*)");
    
    if (!fileName.isEmpty()) {
        doc->filePath = fileName;
        QFileInfo fileInfo(fileName);
        doc->fileName = fileInfo.fileName();
        doc->isNewFile = false;
        
        if (saveDocument(doc)) {
            updateTabTitle(documents.indexOf(doc));
            return true;
        }
    }
    return false;
}

void MarkdownEditor::saveCurrentFile() {
    TabDocument* doc = getCurrentDocument();
    if (doc) {
        saveDocument(doc);
    }
}

void MarkdownEditor::saveFileAs() {
    TabDocument* doc = getCurrentDocument();
    if (doc) {
        saveDocumentAs(doc);
    }
}

void MarkdownEditor::documentModified() {
    TabDocument* doc = getCurrentDocument();
    if (doc && !doc->isModified) {
        doc->isModified = true;
        updateTabTitle(documents.indexOf(doc));
    }
}

// Funciones para la bandeja del sistema
void MarkdownEditor::setupSystemTray() {
    if (!QSystemTrayIcon::isSystemTrayAvailable()) {
        return;
    }
    
    createTrayIcon();
    
    // Mostrar el icono en la bandeja
    if (trayIcon) {
        trayIcon->show();
    }
}

void MarkdownEditor::createTrayIcon() {
    // Crear menú de la bandeja del sistema
    trayIconMenu = new QMenu(this);
    
    showAction = new QAction("&Mostrar", this);
    connect(showAction, &QAction::triggered, this, &MarkdownEditor::showFromTray);
    trayIconMenu->addAction(showAction);
    
    hideAction = new QAction("&Minimizar", this);
    connect(hideAction, &QAction::triggered, this, &MarkdownEditor::hideToTray);
    trayIconMenu->addAction(hideAction);
    
    trayIconMenu->addSeparator();
    
    QAction *newAction = new QAction("&Nuevo", this);
    connect(newAction, &QAction::triggered, this, &MarkdownEditor::newTab);
    trayIconMenu->addAction(newAction);
    
    QAction *openAction = new QAction("&Abrir", this);
    connect(openAction, &QAction::triggered, this, &MarkdownEditor::openFileInNewTab);
    trayIconMenu->addAction(openAction);
    
    trayIconMenu->addSeparator();
    
    exitAction = new QAction("&Salir", this);
    connect(exitAction, &QAction::triggered, this, &MarkdownEditor::exitApplication);
    trayIconMenu->addAction(exitAction);
    
    // Crear icono de la bandeja
    trayIcon = new QSystemTrayIcon(this);
    
    // Usar un icono por defecto si no se encuentra el archivo
    QIcon icon = QIcon(":/img/logo_escribe_chanchi.png");  // Usar el icono del programa
    if (icon.isNull()) {
        // Crear un icono simple con texto si no hay imagen
        QPixmap pixmap(32, 32);
        pixmap.fill(QColor(70, 130, 180)); // Color azul
        QPainter painter(&pixmap);
        painter.setPen(Qt::white);
        painter.setFont(QFont("Arial", 12, QFont::Bold));
        painter.drawText(pixmap.rect(), Qt::AlignCenter, "EC");
        icon = QIcon(pixmap);
    }
    
    trayIcon->setIcon(icon);
    trayIcon->setContextMenu(trayIconMenu);
    trayIcon->setToolTip("Escribe Chanchi - Editor de Markdown");
    
    // Conectar señales
    connect(trayIcon, &QSystemTrayIcon::activated, this, &MarkdownEditor::trayIconActivated);
}

void MarkdownEditor::showFromTray() {
    showNormal();
    raise();
    activateWindow();
}

void MarkdownEditor::hideToTray() {
    hide();
    
    // Mostrar mensaje la primera vez
    static bool firstTime = true;
    if (firstTime && trayIcon) {
        trayIcon->showMessage("Escribe Chanchi",
                             "La aplicación se minimizó a la bandeja del sistema.\n"
                             "Haz clic en el icono para restaurarla.",
                             QSystemTrayIcon::Information, 2000);
        firstTime = false;
    }
}

void MarkdownEditor::trayIconActivated(QSystemTrayIcon::ActivationReason reason) {
    switch (reason) {
    case QSystemTrayIcon::Trigger:
    case QSystemTrayIcon::DoubleClick:
        if (isVisible()) {
            hideToTray();
        } else {
            showFromTray();
        }
        break;
    default:
        break;
    }
}

void MarkdownEditor::exitApplication() {
    forceQuit = true;
    QApplication::quit();
}

void MarkdownEditor::closeEvent(QCloseEvent *event) {
    // Si hay bandeja del sistema disponible, minimizar en lugar de cerrar
    if (!forceQuit && trayIcon && trayIcon->isVisible()) {
        hideToTray();
        event->ignore();
        return;
    }
    
    // Si no hay bandeja del sistema, proceder con el cierre normal
    bool hasModified = false;
    for (TabDocument* doc : documents) {
        if (doc->isModified) {
            hasModified = true;
            break;
        }
    }
    
    if (hasModified) {
        QMessageBox::StandardButton reply = QMessageBox::question(this,
            "Cerrar EscribeChanchi",
            "Hay documentos con cambios sin guardar.\n¿Está seguro de que desea cerrar?",
            QMessageBox::Yes | QMessageBox::No);
        
        if (reply == QMessageBox::No) {
            event->ignore();
            return;
        }
    }
    
    // Limpiar memoria
    for (TabDocument* doc : documents) {
        delete doc;
    }
    documents.clear();
    
    event->accept();
}

void MarkdownEditor::toggleFullScreen() {
    if (isFullScreenMode) {
        // Salir de pantalla completa y volver al estado maximizado
        showNormal();
        showMaximized();
        isFullScreenMode = false;
        
        // Mostrar la barra de menús y herramientas cuando no está en pantalla completa
        menuBar()->show();
        if (mainToolBar) {
            mainToolBar->show();
        }
    } else {
        // Entrar en pantalla completa
        showFullScreen();
        isFullScreenMode = true;
        
        // Ocultar la barra de menús y herramientas en pantalla completa (opcional)
        // menuBar()->hide();
        // if (mainToolBar) {
        //     mainToolBar->hide();
        // }
    }
}

// Funciones para manejo de tipos de archivo
QString MarkdownEditor::getFileType(const QString& filePath) {
    QFileInfo fileInfo(filePath);
    QString extension = fileInfo.suffix().toLower();
    
    if (extension == "md" || extension == "markdown") {
        return "markdown";
    } else if (extension == "txt") {
        return "text";
    } else if (extension == "m3u") {
        return "playlist";
    } else if (extension == "cpp" || extension == "h" || extension == "hpp" || extension == "c") {
        return "cpp";
    } else if (extension == "js" || extension == "json") {
        return "javascript";
    } else if (extension == "py") {
        return "python";
    } else if (extension == "html" || extension == "htm") {
        return "html";
    } else if (extension == "css") {
        return "css";
    } else if (extension == "xml") {
        return "xml";
    }
    return "text"; // Por defecto, tratar como texto
}

bool MarkdownEditor::isMarkdownFile(const QString& filePath) {
    return getFileType(filePath) == "markdown";
}

bool MarkdownEditor::shouldRenderPreview(const QString& filePath) {
    // Si es un archivo nuevo (sin ruta) o vacío, tratarlo como markdown por defecto
    if (filePath.isEmpty() || filePath.contains("Sin título")) {
        return true;
    }
    
    QString type = getFileType(filePath);
    // Solo renderizar vista previa para archivos Markdown
    return type == "markdown";
}

void MarkdownEditor::toggleDarkMode() {
    darkMode = !darkMode;
    if (darkMode) {
        setDarkMode();
    } else {
        setLightMode();
    }
}

void MarkdownEditor::setDarkMode() {
    darkMode = true;
    
    // Aplicar tema oscuro a todas las pestañas abiertas
    for (TabDocument* doc : documents) {
        if (doc && doc->editor) {
            QPalette darkPalette;
            darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
            darkPalette.setColor(QPalette::Text, QColor(255, 255, 255));
            darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
            darkPalette.setColor(QPalette::WindowText, QColor(255, 255, 255));
            
            doc->editor->setPalette(darkPalette);
            doc->editor->setStyleSheet(editorStyleSheet(true));
        }
    }
    
    // Aplicar tema oscuro a la ventana principal
    QPalette darkPalette;
    darkPalette.setColor(QPalette::Window, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::WindowText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Base, QColor(30, 30, 30));
    darkPalette.setColor(QPalette::AlternateBase, QColor(66, 66, 66));
    darkPalette.setColor(QPalette::ToolTipBase, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::ToolTipText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Text, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::Button, QColor(53, 53, 53));
    darkPalette.setColor(QPalette::ButtonText, QColor(255, 255, 255));
    darkPalette.setColor(QPalette::BrightText, QColor(255, 0, 0));
    darkPalette.setColor(QPalette::Link, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::Highlight, QColor(42, 130, 218));
    darkPalette.setColor(QPalette::HighlightedText, QColor(0, 0, 0));
    
    setPalette(darkPalette);
    
    setStyleSheet(
        "QMainWindow {"
        "   background-color: #353535;"
        "}"
        "QTabWidget::pane {"
        "   border: 1px solid #555;"
        "   background-color: #2b2b2b;"
        "}"
        "QTabBar::tab {"
        "   background-color: #404040;"
        "   color: white;"
        "   padding: 8px 12px;"
        "   border: 1px solid #555;"
        "   border-bottom: none;"
        "}"
        "QTabBar::tab:selected {"
        "   background-color: #2b2b2b;"
        "   border-bottom: 1px solid #2b2b2b;"
        "}"
        "QTabBar::tab:hover {"
        "   background-color: #4a4a4a;"
        "}"
        "QMenuBar {"
        "   background-color: #2b2b2b;"
        "   color: white;"
        "   border-bottom: 1px solid #555;"
        "}"
        "QMenuBar::item {"
        "   background-color: transparent;"
        "   padding: 4px 8px;"
        "}"
        "QMenuBar::item:selected {"
        "   background-color: #404040;"
        "}"
        "QMenu {"
        "   background-color: #2b2b2b;"
        "   color: white;"
        "   border: 1px solid #555;"
        "}"
        "QMenu::item:selected {"
        "   background-color: #404040;"
        "}"
        "QToolBar {"
        "   background-color: #2b2b2b;"
        "   border: 1px solid #555;"
        "   color: white;"
        "}"
        "QToolButton {"
        "   background-color: transparent;"
        "   color: white;"
        "   border: none;"
        "   padding: 5px;"
        "}"
        "QToolButton:hover {"
        "   background-color: #404040;"
        "}"
        "QStatusBar {"
        "   background-color: #2b2b2b;"
        "   color: white;"
        "   border-top: 1px solid #555;"
        "}"
    );
}

void MarkdownEditor::setLightMode() {
    darkMode = false;
    
    // Aplicar tema claro a todas las pestañas abiertas
    for (TabDocument* doc : documents) {
        if (doc && doc->editor) {
            QPalette lightPalette;
            lightPalette.setColor(QPalette::Base, QColor(255, 255, 255));
            lightPalette.setColor(QPalette::Text, QColor(0, 0, 0));
            lightPalette.setColor(QPalette::Window, QColor(240, 240, 240));
            lightPalette.setColor(QPalette::WindowText, QColor(0, 0, 0));
            
            doc->editor->setPalette(lightPalette);
            doc->editor->setStyleSheet(editorStyleSheet(false));
        }
    }
    
    // Restaurar tema claro por defecto
    setPalette(QApplication::palette());
    setStyleSheet("");
}

QString MarkdownEditor::editorStyleSheet(bool dark) const {
    QString extra;
    if (showLineNumbers) {
        extra = QString("border-left: 40px solid %1; padding-left: 10px;")
                    .arg(dark ? "#3c3c3c" : "#f0f0f0");
    }

    return QString(
        "QTextEdit {"
        "   background-color: %1;"
        "   color: %2;"
        "   border: 1px solid %3;"
        "   font-family: 'Courier New', monospace;"
        "   font-size: %4pt;"
        "   %5"
        "}"
    ).arg(dark ? "#1e1e1e" : "#ffffff",
          dark ? "#ffffff" : "#000000",
          dark ? "#555" : "#ccc",
          QString::number(editorFontPointSize),
          extra);
}

void MarkdownEditor::togglePreview(bool enabled) {
    previewEnabled = enabled;
    QSettings settings;
    settings.setValue("previewEnabled", previewEnabled);
    for (TabDocument *doc : documents) {
        applyPreviewMode(doc);
    }
    renderMarkdown();
}

void MarkdownEditor::loadEditorPreferences() {
    QSettings settings;
    previewEnabled = settings.value("previewEnabled", true).toBool();
    editorFontPointSize = qBound(8, settings.value("editorFontPointSize", 12).toInt(), 32);
    recentFiles = settings.value("recentFiles").toStringList();

    QStringList existing;
    for (const QString &path : recentFiles) {
        if (QFileInfo::exists(path) && existing.size() < MaxRecentFiles) {
            existing.append(path);
        }
    }
    if (recentFiles != existing) {
        recentFiles = existing;
        saveRecentFiles();
    }
}

void MarkdownEditor::setEditorFontSize(int pointSize) {
    editorFontPointSize = qBound(8, pointSize, 32);
    if (darkMode) {
        setDarkMode();
    } else {
        setLightMode();
    }
    QSettings settings;
    settings.setValue("editorFontPointSize", editorFontPointSize);
}

void MarkdownEditor::increaseFontSize() {
    setEditorFontSize(editorFontPointSize + 1);
}

void MarkdownEditor::decreaseFontSize() {
    setEditorFontSize(editorFontPointSize - 1);
}

void MarkdownEditor::resetFontSize() {
    setEditorFontSize(12);
}

void MarkdownEditor::goToLine() {
    TabDocument *doc = getCurrentDocument();
    if (!doc || !doc->editor) {
        return;
    }

    const int total = qMax(1, doc->editor->document()->blockCount());
    const int current = doc->editor->textCursor().blockNumber() + 1;
    bool ok = false;
    const int line = QInputDialog::getInt(this, "Ir a la línea",
        "Número de línea:", current, 1, total, 1, &ok);
    if (!ok) {
        return;
    }

    QTextCursor cursor(doc->editor->document()->findBlockByNumber(line - 1));
    doc->editor->setTextCursor(cursor);
    doc->editor->ensureCursorVisible();
    doc->editor->setFocus();
}

void MarkdownEditor::addToRecentFiles(const QString &path) {
    if (path.isEmpty()) {
        return;
    }
    recentFiles.removeAll(path);
    recentFiles.prepend(path);
    while (recentFiles.size() > MaxRecentFiles) {
        recentFiles.removeLast();
    }
    saveRecentFiles();
    updateRecentFilesMenu();
}

void MarkdownEditor::saveRecentFiles() {
    QSettings settings;
    settings.setValue("recentFiles", recentFiles);
}

void MarkdownEditor::updateRecentFilesMenu() {
    if (!recentFilesMenu) {
        return;
    }
    recentFilesMenu->clear();
    if (recentFiles.isEmpty()) {
        QAction *emptyAction = recentFilesMenu->addAction("(vacío)");
        emptyAction->setEnabled(false);
        return;
    }

    for (const QString &path : recentFiles) {
        QAction *action = recentFilesMenu->addAction(QFileInfo(path).fileName());
        action->setToolTip(path);
        action->setData(path);
        connect(action, &QAction::triggered, this, &MarkdownEditor::openRecentFile);
    }

    recentFilesMenu->addSeparator();
    QAction *clearAction = recentFilesMenu->addAction("Limpiar lista");
    connect(clearAction, &QAction::triggered, this, [this]() {
        recentFiles.clear();
        saveRecentFiles();
        updateRecentFilesMenu();
    });
}

void MarkdownEditor::openRecentFile() {
    QAction *action = qobject_cast<QAction *>(sender());
    if (!action) {
        return;
    }

    const QString path = action->data().toString();
    if (!QFileInfo::exists(path)) {
        QMessageBox::warning(this, "Archivo no encontrado",
            QString("El archivo ya no existe:\n%1").arg(path));
        recentFiles.removeAll(path);
        saveRecentFiles();
        updateRecentFilesMenu();
        return;
    }

    openFilePath(path);
}

// ===== IMPLEMENTACIÓN DE FUNCIONES DE BÚSQUEDA Y REEMPLAZO =====

void MarkdownEditor::showFindDialog() {
    if (!findDialog) {
        findDialog = new QDialog(this);
        findDialog->setWindowTitle("Buscar");
        findDialog->setFixedSize(450, 200);
        findDialog->setModal(false); // No modal para permitir editar mientras se busca
        
        QVBoxLayout* mainLayout = new QVBoxLayout(findDialog);
        
        // Campo de búsqueda
        QHBoxLayout* searchLayout = new QHBoxLayout();
        searchLayout->addWidget(new QLabel("Buscar:"));
        findLineEdit = new QLineEdit();
        findLineEdit->setPlaceholderText("Ingresa el texto a buscar...");
        searchLayout->addWidget(findLineEdit);
        mainLayout->addLayout(searchLayout);
        
        // Opciones de búsqueda
        QHBoxLayout* optionsLayout = new QHBoxLayout();
        QCheckBox* caseSensitiveCheck = new QCheckBox("Sensible a mayúsculas");
        QCheckBox* wholeWordsCheck = new QCheckBox("Solo palabras completas");
        
        // Configurar estado inicial de los checkboxes
        caseSensitiveCheck->setChecked(caseSensitive);
        wholeWordsCheck->setChecked(wholeWords);
        
        // Asegurar que los checkboxes sean interactivos
        caseSensitiveCheck->setEnabled(true);
        wholeWordsCheck->setEnabled(true);
        
        connect(caseSensitiveCheck, &QCheckBox::toggled, [this](bool checked) {
            caseSensitive = checked;
        });
        connect(wholeWordsCheck, &QCheckBox::toggled, [this](bool checked) {
            wholeWords = checked;
        });
        
        optionsLayout->addWidget(caseSensitiveCheck);
        optionsLayout->addWidget(wholeWordsCheck);
        optionsLayout->addStretch();
        mainLayout->addLayout(optionsLayout);
        
        // Botones
        QHBoxLayout* buttonLayout = new QHBoxLayout();
        QPushButton* findNextBtn = new QPushButton("Buscar siguiente");
        QPushButton* findPrevBtn = new QPushButton("Buscar anterior");
        QPushButton* closeBtn = new QPushButton("Cerrar");
        
        findNextBtn->setDefault(true);
        
        connect(findNextBtn, &QPushButton::clicked, this, &MarkdownEditor::findNext);
        connect(findPrevBtn, &QPushButton::clicked, this, &MarkdownEditor::findPrevious);
        connect(closeBtn, &QPushButton::clicked, findDialog, &QDialog::close);
        
        // También buscar al presionar Enter
        connect(findLineEdit, &QLineEdit::returnPressed, this, &MarkdownEditor::findNext);
        
        buttonLayout->addWidget(findNextBtn);
        buttonLayout->addWidget(findPrevBtn);
        buttonLayout->addStretch();
        buttonLayout->addWidget(closeBtn);
        
        mainLayout->addLayout(buttonLayout);
        
        // Estilo del diálogo
        findDialog->setStyleSheet(
            "QDialog {"
            "   background-color: #f5f5f5;"
            "}"
            "QLabel {"
            "   color: #333333;"
            "   font-weight: bold;"
            "}"
            "QLineEdit {"
            "   padding: 5px;"
            "   border: 1px solid #ccc;"
            "   border-radius: 3px;"
            "   background-color: #ffffff;"
            "   color: #333333;"
            "   selection-background-color: #3399ff;"
            "   selection-color: white;"
            "}"
            "QCheckBox {"
            "   color: #333333;"
            "   spacing: 8px;"
            "   font-weight: bold;"
            "}"
            "QCheckBox::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "   border: 2px solid #ccc;"
            "   border-radius: 3px;"
            "   background-color: #ffffff;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border: 2px solid #007acc;"
            "   background-color: #f0f8ff;"
            "}"
            "QCheckBox::indicator:checked {"
            "   border: 2px solid #007acc;"
            "   background-color: #007acc;"
            "}"
            "QCheckBox::indicator:checked:hover {"
            "   background-color: #0066aa;"
            "   border: 2px solid #0066aa;"
            "}"
            "QCheckBox::indicator:disabled {"
            "   border: 2px solid #ddd;"
            "   background-color: #f5f5f5;"
            "}"
            "QPushButton {"
            "   padding: 5px 15px;"
            "   border: 1px solid #ccc;"
            "   border-radius: 3px;"
            "   background-color: #ffffff;"
            "   color: #333333;"
            "   font-weight: bold;"
            "   min-width: 100px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #e8f4fd;"
            "   border: 1px solid #007acc;"
            "   color: #333333;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #d0e8f7;"
            "   border: 1px solid #005a8c;"
            "   color: #333333;"
            "}"
            "QPushButton:default {"
            "   background-color: #007acc;"
            "   color: white;"
            "   border: 1px solid #005a8c;"
            "}"
            "QPushButton:default:hover {"
            "   background-color: #0066aa;"
            "   color: white;"
            "}"
        );
    }
    
    findDialog->show();
    findDialog->raise();
    findDialog->activateWindow();
    findLineEdit->setFocus();
    
    // Seleccionar texto actual si hay selección
    TabDocument* doc = getCurrentDocument();
    if (doc && doc->editor && doc->editor->textCursor().hasSelection()) {
        QString selectedText = doc->editor->textCursor().selectedText();
        if (!selectedText.isEmpty()) {
            findLineEdit->setText(selectedText);
            findLineEdit->selectAll();
        }
    }
}

void MarkdownEditor::showReplaceDialog() {
    if (!replaceDialog) {
        replaceDialog = new QDialog(this);
        replaceDialog->setWindowTitle("Buscar y reemplazar");
        replaceDialog->setFixedSize(500, 280);
        replaceDialog->setModal(false);
        
        QVBoxLayout* mainLayout = new QVBoxLayout(replaceDialog);
        
        // Campo de búsqueda
        QHBoxLayout* searchLayout = new QHBoxLayout();
        searchLayout->addWidget(new QLabel("Buscar:"));
        if (!findLineEdit) {
            findLineEdit = new QLineEdit();
        }
        // Crear una referencia local para el diálogo de reemplazo
        QLineEdit* findLineEditReplace = new QLineEdit();
        findLineEditReplace->setPlaceholderText("Texto a buscar...");
        searchLayout->addWidget(findLineEditReplace);
        mainLayout->addLayout(searchLayout);
        
        // Campo de reemplazo
        QHBoxLayout* replaceLayout = new QHBoxLayout();
        replaceLayout->addWidget(new QLabel("Reemplazar:"));
        replaceLineEdit = new QLineEdit();
        replaceLineEdit->setPlaceholderText("Texto de reemplazo...");
        replaceLayout->addWidget(replaceLineEdit);
        mainLayout->addLayout(replaceLayout);
        
        // Sincronizar los campos de búsqueda
        connect(findLineEditReplace, &QLineEdit::textChanged, [this](const QString& text) {
            lastSearchText = text;
            if (findLineEdit) {
                findLineEdit->setText(text);
            }
        });
        
        // Opciones de búsqueda
        QHBoxLayout* optionsLayout = new QHBoxLayout();
        QCheckBox* caseSensitiveCheck = new QCheckBox("Sensible a mayúsculas");
        QCheckBox* wholeWordsCheck = new QCheckBox("Solo palabras completas");
        
        // Configurar estado inicial de los checkboxes
        caseSensitiveCheck->setChecked(caseSensitive);
        wholeWordsCheck->setChecked(wholeWords);
        
        // Asegurar que los checkboxes sean interactivos
        caseSensitiveCheck->setEnabled(true);
        wholeWordsCheck->setEnabled(true);
        
        connect(caseSensitiveCheck, &QCheckBox::toggled, [this](bool checked) {
            caseSensitive = checked;
        });
        connect(wholeWordsCheck, &QCheckBox::toggled, [this](bool checked) {
            wholeWords = checked;
        });
        
        optionsLayout->addWidget(caseSensitiveCheck);
        optionsLayout->addWidget(wholeWordsCheck);
        optionsLayout->addStretch();
        mainLayout->addLayout(optionsLayout);
        
        // Botones
        QHBoxLayout* buttonLayout1 = new QHBoxLayout();
        QPushButton* findNextBtn = new QPushButton("Buscar siguiente");
        QPushButton* replaceBtn = new QPushButton("Reemplazar");
        QPushButton* replaceAllBtn = new QPushButton("Reemplazar todo");
        
        buttonLayout1->addWidget(findNextBtn);
        buttonLayout1->addWidget(replaceBtn);
        buttonLayout1->addWidget(replaceAllBtn);
        mainLayout->addLayout(buttonLayout1);
        
        QHBoxLayout* buttonLayout2 = new QHBoxLayout();
        QPushButton* closeBtn = new QPushButton("Cerrar");
        buttonLayout2->addStretch();
        buttonLayout2->addWidget(closeBtn);
        mainLayout->addLayout(buttonLayout2);
        
        // Conectar botones usando lambdas para capturar el campo local
        connect(findNextBtn, &QPushButton::clicked, [this, findLineEditReplace]() {
            lastSearchText = findLineEditReplace->text();
            findNext();
        });
        
        connect(replaceBtn, &QPushButton::clicked, [this, findLineEditReplace]() {
            lastSearchText = findLineEditReplace->text();
            replaceNext();
        });
        
        connect(replaceAllBtn, &QPushButton::clicked, [this, findLineEditReplace]() {
            lastSearchText = findLineEditReplace->text();
            replaceAll();
        });
        
        connect(closeBtn, &QPushButton::clicked, replaceDialog, &QDialog::close);
        
        // Enter en buscar = buscar siguiente
        connect(findLineEditReplace, &QLineEdit::returnPressed, [this, findLineEditReplace]() {
            lastSearchText = findLineEditReplace->text();
            findNext();
        });
        
        // Enter en reemplazar = reemplazar
        connect(replaceLineEdit, &QLineEdit::returnPressed, [this, findLineEditReplace]() {
            lastSearchText = findLineEditReplace->text();
            replaceNext();
        });
        
        // Aplicar el mismo estilo mejorado que el diálogo de búsqueda
        replaceDialog->setStyleSheet(
            "QDialog {"
            "   background-color: #f5f5f5;"
            "}"
            "QLabel {"
            "   color: #333333;"
            "   font-weight: bold;"
            "}"
            "QLineEdit {"
            "   padding: 5px;"
            "   border: 1px solid #ccc;"
            "   border-radius: 3px;"
            "   background-color: #ffffff;"
            "   color: #333333;"
            "   selection-background-color: #3399ff;"
            "   selection-color: white;"
            "}"
            "QCheckBox {"
            "   color: #333333;"
            "   spacing: 8px;"
            "   font-weight: bold;"
            "}"
            "QCheckBox::indicator {"
            "   width: 16px;"
            "   height: 16px;"
            "   border: 2px solid #ccc;"
            "   border-radius: 3px;"
            "   background-color: #ffffff;"
            "}"
            "QCheckBox::indicator:hover {"
            "   border: 2px solid #007acc;"
            "   background-color: #f0f8ff;"
            "}"
            "QCheckBox::indicator:checked {"
            "   border: 2px solid #007acc;"
            "   background-color: #007acc;"
            "}"
            "QCheckBox::indicator:checked:hover {"
            "   background-color: #0066aa;"
            "   border: 2px solid #0066aa;"
            "}"
            "QCheckBox::indicator:disabled {"
            "   border: 2px solid #ddd;"
            "   background-color: #f5f5f5;"
            "}"
            "QPushButton {"
            "   padding: 5px 15px;"
            "   border: 1px solid #ccc;"
            "   border-radius: 3px;"
            "   background-color: #ffffff;"
            "   color: #333333;"
            "   font-weight: bold;"
            "   min-width: 100px;"
            "}"
            "QPushButton:hover {"
            "   background-color: #e8f4fd;"
            "   border: 1px solid #007acc;"
            "   color: #333333;"
            "}"
            "QPushButton:pressed {"
            "   background-color: #d0e8f7;"
            "   border: 1px solid #005a8c;"
            "   color: #333333;"
            "}"
        );
    }
    
    replaceDialog->show();
    replaceDialog->raise();
    replaceDialog->activateWindow();
    
    // Seleccionar texto actual si hay selección
    TabDocument* doc = getCurrentDocument();
    if (doc && doc->editor && doc->editor->textCursor().hasSelection()) {
        QString selectedText = doc->editor->textCursor().selectedText();
        if (!selectedText.isEmpty()) {
            // Buscar el findLineEdit dentro del diálogo de reemplazo
            QLineEdit* findLineEditReplace = replaceDialog->findChild<QLineEdit*>();
            if (findLineEditReplace) {
                findLineEditReplace->setText(selectedText);
                findLineEditReplace->selectAll();
                lastSearchText = selectedText;
            }
        }
    }
}

void MarkdownEditor::findNext() {
    TabDocument* doc = getCurrentDocument();
    if (!doc || !doc->editor) {
        statusBar()->showMessage("No hay documento activo", 2000);
        return;
    }
    
    QString searchText;
    if (findLineEdit && !findLineEdit->text().isEmpty()) {
        searchText = findLineEdit->text();
    } else if (!lastSearchText.isEmpty()) {
        searchText = lastSearchText;
    } else {
        statusBar()->showMessage("No se encontró texto para buscar", 2000);
        return;
    }
    
    QTextDocument::FindFlags flags;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (wholeWords) flags |= QTextDocument::FindWholeWords;
    
    QTextCursor currentCursor = doc->editor->textCursor();
    
    if (doc->editor->find(searchText, flags)) {
        // Encontrado: resaltar el texto
        QTextCursor foundCursor = doc->editor->textCursor();
        doc->editor->setTextCursor(foundCursor);
        statusBar()->showMessage(QString("Encontrado: \"%1\"").arg(searchText), 2000);
    } else {
        // No encontrado desde la posición actual, intentar desde el inicio
        QTextCursor cursor = doc->editor->textCursor();
        cursor.movePosition(QTextCursor::Start);
        doc->editor->setTextCursor(cursor);
        
        if (doc->editor->find(searchText, flags)) {
            statusBar()->showMessage(QString("Búsqueda desde el inicio: \"%1\"").arg(searchText), 2000);
        } else {
            // Restaurar posición original si no se encuentra nada
            doc->editor->setTextCursor(currentCursor);
            statusBar()->showMessage(QString("No se encontró: \"%1\"").arg(searchText), 3000);
        }
    }
    
    lastSearchText = searchText;
}

void MarkdownEditor::findPrevious() {
    TabDocument* doc = getCurrentDocument();
    if (!doc || !doc->editor) {
        statusBar()->showMessage("No hay documento activo", 2000);
        return;
    }
    
    QString searchText;
    if (findLineEdit && !findLineEdit->text().isEmpty()) {
        searchText = findLineEdit->text();
    } else if (!lastSearchText.isEmpty()) {
        searchText = lastSearchText;
    } else {
        statusBar()->showMessage("No hay texto para buscar", 2000);
        return;
    }
    
    QTextDocument::FindFlags flags = QTextDocument::FindBackward;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (wholeWords) flags |= QTextDocument::FindWholeWords;
    
    QTextCursor currentCursor = doc->editor->textCursor();
    
    if (doc->editor->find(searchText, flags)) {
        // Encontrado hacia atrás
        statusBar()->showMessage(QString("Encontrado: \"%1\"").arg(searchText), 2000);
    } else {
        // No encontrado, ir al final y buscar hacia atrás
        QTextCursor cursor = doc->editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        doc->editor->setTextCursor(cursor);
        
        if (doc->editor->find(searchText, flags)) {
            statusBar()->showMessage(QString("Búsqueda desde el final: \"%1\"").arg(searchText), 2000);
        } else {
            // Restaurar posición original
            doc->editor->setTextCursor(currentCursor);
            statusBar()->showMessage(QString("No se encontró: \"%1\"").arg(searchText), 3000);
        }
    }
    
    lastSearchText = searchText;
}

void MarkdownEditor::replaceNext() {
    TabDocument* doc = getCurrentDocument();
    if (!doc || !doc->editor) {
        statusBar()->showMessage("No hay documento activo", 2000);
        return;
    }
    
    QString searchText = lastSearchText;
    if (searchText.isEmpty()) {
        statusBar()->showMessage("No hay texto para buscar", 2000);
        return;
    }
    
    QString replaceText = replaceLineEdit ? replaceLineEdit->text() : "";
    
    QTextCursor cursor = doc->editor->textCursor();
    
    // Si hay texto seleccionado y coincide con el texto de búsqueda, reemplazarlo
    if (cursor.hasSelection()) {
        QString selectedText = cursor.selectedText();
        
        // Verificar si el texto seleccionado coincide (considerar opciones)
        bool matches = false;
        if (caseSensitive) {
            matches = (selectedText == searchText);
        } else {
            matches = (selectedText.toLower() == searchText.toLower());
        }
        
        if (matches) {
            cursor.insertText(replaceText);
            doc->isModified = true;
            updateTabTitle(documents.indexOf(doc));
            statusBar()->showMessage(QString("Reemplazado: \"%1\" → \"%2\"").arg(searchText).arg(replaceText), 2000);
        }
    }
    
    // Buscar la siguiente ocurrencia
    findNext();
}

void MarkdownEditor::replaceAll() {
    TabDocument* doc = getCurrentDocument();
    if (!doc || !doc->editor) {
        statusBar()->showMessage("No hay documento activo", 2000);
        return;
    }
    
    QString searchText = lastSearchText;
    if (searchText.isEmpty()) {
        statusBar()->showMessage("No hay texto para buscar", 2000);
        return;
    }
    
    QString replaceText = replaceLineEdit ? replaceLineEdit->text() : "";
    
    // Confirmar reemplazo masivo
    int ret = QMessageBox::question(this, "Confirmar reemplazo",
        QString("¿Estás seguro de que deseas reemplazar todas las ocurrencias de \"%1\" con \"%2\"?")
        .arg(searchText).arg(replaceText),
        QMessageBox::Yes | QMessageBox::No,
        QMessageBox::No);
    
    if (ret != QMessageBox::Yes) {
        return;
    }
    
    // Realizar reemplazo masivo
    QTextDocument::FindFlags flags;
    if (caseSensitive) flags |= QTextDocument::FindCaseSensitively;
    if (wholeWords) flags |= QTextDocument::FindWholeWords;
    
    QTextCursor cursor = doc->editor->textCursor();
    cursor.movePosition(QTextCursor::Start);
    doc->editor->setTextCursor(cursor);
    
    int replaceCount = 0;
    
    while (doc->editor->find(searchText, flags)) {
        QTextCursor foundCursor = doc->editor->textCursor();
        foundCursor.insertText(replaceText);
        replaceCount++;
    }
    
    if (replaceCount > 0) {
        doc->isModified = true;
        updateTabTitle(documents.indexOf(doc));
    }
    
    QMessageBox::information(this, "Reemplazar Todo", 
        QString("Se reemplazaron %1 ocurrencias.").arg(replaceCount));
}

// Implementaciones para emojis personalizados
void MarkdownEditor::manageCustomEmojis() {
    QDialog* manageDialog = new QDialog(this);
    manageDialog->setWindowTitle("Gestionar Emojis Personalizados");
    manageDialog->setFixedSize(500, 400);
    manageDialog->setModal(true);
    
    QVBoxLayout* layout = new QVBoxLayout(manageDialog);
    
    QLabel* titleLabel = new QLabel("<h3>Emojis Personalizados</h3>");
    layout->addWidget(titleLabel);
    
    // Lista de emojis personalizados
    QListWidget* emojiList = new QListWidget();
    for (const QString& emoji : customEmojis) {
        emojiList->addItem(emoji);
    }
    layout->addWidget(emojiList);
    
    // Botones de gestión
    QHBoxLayout* buttonLayout = new QHBoxLayout();
    
    QPushButton* addButton = new QPushButton("Agregar");
    connect(addButton, &QPushButton::clicked, [this, emojiList]() {
        bool ok;
        QString newEmoji = QInputDialog::getText(this, "Agregar Emoji", 
            "Introduce el emoji o símbolo:", QLineEdit::Normal, "", &ok);
        if (ok && !newEmoji.isEmpty()) {
            customEmojis.append(newEmoji);
            emojiList->addItem(newEmoji);
            saveCustomEmojis();
        }
    });
    
    QPushButton* removeButton = new QPushButton("Eliminar");
    connect(removeButton, &QPushButton::clicked, [this, emojiList]() {
        int currentRow = emojiList->currentRow();
        if (currentRow >= 0) {
            customEmojis.removeAt(currentRow);
            delete emojiList->takeItem(currentRow);
            saveCustomEmojis();
        }
    });
    
    QPushButton* closeButton = new QPushButton("Cerrar");
    connect(closeButton, &QPushButton::clicked, manageDialog, &QDialog::accept);
    
    buttonLayout->addWidget(addButton);
    buttonLayout->addWidget(removeButton);
    buttonLayout->addStretch();
    buttonLayout->addWidget(closeButton);
    
    layout->addLayout(buttonLayout);
    
    manageDialog->exec();
    delete manageDialog;
}

void MarkdownEditor::loadCustomEmojis() {
    QFile file(getCustomEmojiPath());
    if (file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        
        customEmojis.clear();
        while (!stream.atEnd()) {
            QString line = stream.readLine().trimmed();
            if (!line.isEmpty()) {
                customEmojis.append(line);
            }
        }
        file.close();
    }
}

void MarkdownEditor::saveCustomEmojis() {
    QFile file(getCustomEmojiPath());
    if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream stream(&file);
        
        for (const QString& emoji : customEmojis) {
            stream << emoji << "\n";
        }
        file.close();
    }
}

QString MarkdownEditor::getCustomEmojiPath() {
    QString appDataPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir dir(appDataPath);
    if (!dir.exists()) {
        dir.mkpath(appDataPath);
    }
    return QDir(appDataPath).filePath("custom_emojis.txt");
}

// Implementaciones para sistema de instancia única
void MarkdownEditor::setupSingleInstance() {
    localServer = new QLocalServer(this);
    connect(localServer, &QLocalServer::newConnection, this, &MarkdownEditor::handleNewConnection);

    const QString serverName = QLatin1String(singleInstanceServerName());
    if (!localServer->listen(serverName)) {
        QLocalServer::removeServer(serverName);
        if (!localServer->listen(serverName)) {
            qDebug() << "No se pudo crear el servidor local:" << localServer->errorString();
        }
    }
}

void MarkdownEditor::handleNewConnection() {
    while (QLocalSocket *socket = localServer->nextPendingConnection()) {
        auto *buffer = new QByteArray;
        connect(socket, &QLocalSocket::readyRead, this, [this, socket, buffer]() {
            buffer->append(socket->readAll());
            ingestInstanceData(buffer);
        });
        connect(socket, &QLocalSocket::disconnected, this, [this, socket, buffer]() {
            buffer->append(socket->readAll());
            ingestInstanceData(buffer);
            delete buffer;
            socket->deleteLater();
        });
    }
}

void MarkdownEditor::ingestInstanceData(QByteArray *buffer) {
    if (!buffer || buffer->isEmpty()) {
        return;
    }

    int consumed = 0;
    {
        QBuffer device(buffer);
        device.open(QIODevice::ReadOnly);
        QDataStream in(&device);
        in.setVersion(QDataStream::Qt_5_15);

        while (!device.atEnd()) {
            in.startTransaction();
            QString command;
            QString argument;
            in >> command >> argument;
            if (!in.commitTransaction()) {
                break;
            }
            consumed = static_cast<int>(device.pos());
            if (command == QLatin1String("OPEN")) {
                openFilePath(argument);
            } else if (command == QLatin1String("SHOW")) {
                restoreFromTray();
            }
        }
    }

    if (consumed > 0) {
        buffer->remove(0, consumed);
    }
}

void MarkdownEditor::restoreFromTray() {
    if (isHidden()) {
        show();
    }
    if (isMinimized()) {
        showNormal();
    }
    raise();
    activateWindow();
}
