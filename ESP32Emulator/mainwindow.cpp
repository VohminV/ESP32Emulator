#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMetaObject>
#include <QStringList>
#include <QDir>
#include <QFileInfo>
#include <QSvgRenderer>
#include <QPainter>
#include <QPixmap>
#include <QXmlStreamReader>
#include "CppHighlighter.h" 
#include <QStandardPaths>
QList<QFileInfo> MainWindow::findFzpRecursive(const QString &dirPath)
{
    QList<QFileInfo> result;
    QDir dir(dirPath);
    if (!dir.exists())
        return result;

    QFileInfoList entries = dir.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo &entry : entries) {
        if (entry.isDir()) {
            result.append(findFzpRecursive(entry.absoluteFilePath()));
        } else if (entry.suffix() == "fzp") {
            result.append(entry);
        }
    }
    return result;
}

QString MainWindow::findSvgFileRecursive(const QString &dirPath, const QString &fileName)
{
    QDir dir(dirPath);
    if (!dir.exists())
        return QString();

    QFileInfoList files = dir.entryInfoList(QStringList() << fileName,
                                            QDir::Files | QDir::NoSymLinks);
    if (!files.isEmpty())
        return files.first().absoluteFilePath();

    QFileInfoList subDirs = dir.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot);
    for (const QFileInfo &subDir : subDirs) {
        QString found = findSvgFileRecursive(subDir.absoluteFilePath(), fileName);
        if (!found.isEmpty())
            return found;
    }

    return QString();
}

void MainWindow::loadFritzingParts()
{
    QString basePartsPath = QCoreApplication::applicationDirPath() + "/fritzing-parts";
    QString svgBasePath   = basePartsPath + "/svg";

    QList<QFileInfo> fzpFiles = findFzpRecursive(basePartsPath);

    QMap<QString, QTreeWidgetItem*> familyMap;
    QMap<QString, QIcon> familyIcons;

    for (const QFileInfo &fileInfo : fzpFiles) {
        QFile fzpFile(fileInfo.absoluteFilePath());
        if (!fzpFile.open(QIODevice::ReadOnly | QIODevice::Text))
            continue;

        QXmlStreamReader xml(&fzpFile);
        QString title, family, iconRelativePath;

        while (!xml.atEnd()) {
            xml.readNext();
            if (xml.isStartElement()) {
                if (xml.name() == "title") title = xml.readElementText();
                else if (xml.name() == "property" && xml.attributes().value("name") == "family")
                    family = xml.readElementText();
                else if (xml.name() == "layers" && xml.attributes().hasAttribute("image"))
                    iconRelativePath = xml.attributes().value("image").toString();
            }
        }
        fzpFile.close();

        if (title.isEmpty()) title = fileInfo.baseName();
        if (family.isEmpty()) family = "Other";

        // Family branch
        QTreeWidgetItem *familyItem;
        if (!familyMap.contains(family)) {
            familyItem = new QTreeWidgetItem(ui->treeComponents);
            familyItem->setText(0, family);
            familyMap[family] = familyItem;
        } else {
            familyItem = familyMap[family];
        }

        // Component item
        QTreeWidgetItem *item = new QTreeWidgetItem(familyItem);
        item->setText(0, title);

        // Icon
        if (!iconRelativePath.isEmpty()) {
            QString svgFullPath = findSvgFileRecursive(svgBasePath,
                                                       QFileInfo(iconRelativePath).fileName());
            if (!svgFullPath.isEmpty()) {
                QPixmap pixmap(48, 48);
                pixmap.fill(Qt::transparent);

                QSvgRenderer renderer(svgFullPath);
                QPainter painter(&pixmap);
                renderer.render(&painter, QRectF(0, 0, pixmap.width(), pixmap.height()));

                QIcon icon(pixmap);
                item->setIcon(0, icon);

                if (!familyIcons.contains(family)) {
                    familyItem->setIcon(0, icon);
                    familyIcons[family] = icon;
                }
            }
        }
    }

    ui->treeComponents->expandAll();
}

void MainWindow::filterComponents(const QString &text)
{
    QString filter = text.trimmed();

    for (int i = 0; i < ui->treeComponents->topLevelItemCount(); ++i) {
        QTreeWidgetItem *familyItem = ui->treeComponents->topLevelItem(i);
        bool familyVisible = false;

        for (int j = 0; j < familyItem->childCount(); ++j) {
            QTreeWidgetItem *item = familyItem->child(j);
            bool match = filter.isEmpty() || item->text(0).contains(filter, Qt::CaseInsensitive);
            item->setHidden(!match);
            if (match) familyVisible = true;
        }

        familyItem->setHidden(!familyVisible);
        if (familyVisible && !filter.isEmpty())
            familyItem->setExpanded(true);
    }

    if (filter.isEmpty())
        ui->treeComponents->collapseAll();
}

void MainWindow::initCodeTab(const QString &projectPath)
{
    m_projectRoot = projectPath;

    // Создаём корневую папку, если не существует
    QDir dir(projectPath);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Модель дерева проекта
    m_projectModel = new QFileSystemModel(this);
    m_projectModel->setRootPath(projectPath);
    m_projectModel->setFilter(QDir::AllDirs | QDir::Files | QDir::NoDotAndDotDot);

    ui->treeProject->setModel(m_projectModel);
    ui->treeProject->setRootIndex(m_projectModel->index(projectPath));
    ui->treeProject->setColumnHidden(1, true); // скрываем Size
    ui->treeProject->setColumnHidden(2, true); // скрываем Type
    ui->treeProject->setColumnHidden(3, true); // скрываем Date

    // Сигналы
    connect(ui->treeProject, &QTreeView::doubleClicked, this, &MainWindow::onTreeProjectDoubleClicked);
}

void MainWindow::onTreeProjectDoubleClicked(const QModelIndex &index)
{
    QString filePath = m_projectModel->filePath(index);
    QFileInfo fi(filePath);

    if (fi.isDir()) return;

    // === Обработка прошивок (.bin / .elf) ===
    if (fi.suffix() == "bin" || fi.suffix() == "elf") {
        m_firmwarePath = filePath;
        appendLog("Прошивка выбрана: " + m_firmwarePath);
        return; // Не открываем в редакторе
    }

    // === Открытие текстовых файлов в редакторе ===
    // Проверяем, что файл не слишком большой и, скорее всего, текстовый
    if (fi.size() > 10 * 1024 * 1024) { // >10 МБ — считаем бинарным
        appendLog("Файл слишком большой для открытия в редакторе: " + fi.fileName());
        return;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        appendLog("Не удалось открыть файл: " + fi.fileName());
        return;
    }

    QByteArray content = file.readAll();
    file.close();

    // Быстрая проверка на бинарность (наличие нулевых байтов в начале)
    if (content.size() > 0 && memchr(content.constData(), '\0', qMin(1024, content.size())) != nullptr) {
        appendLog("Файл выглядит как бинарный: " + fi.fileName());
        return;
    }

    // Открываем в редакторе
    QTextEdit* editor = new QTextEdit();
    editor->setPlainText(QString::fromUtf8(content));

    // Подсветка синтаксиса
    new CppHighlighter(editor->document());

    ui->tabEditor->addTab(editor, fi.fileName());
    ui->tabEditor->setCurrentWidget(editor);

    m_openedFiles[editor] = filePath;

    // Автосохранение
    connect(editor, &QTextEdit::textChanged, [this, editor]() {
        QString path = m_openedFiles.value(editor);
        if (path.isEmpty()) return;
        QFile f(path);
        if (f.open(QIODevice::WriteOnly | QIODevice::Text)) {
            QTextStream out(&f);
            out << editor->toPlainText();
            f.close();
        }
    });
}

void MainWindow::createNewFile()
{
    bool ok;
    QString fileName = QInputDialog::getText(this, "New File", "File name:", QLineEdit::Normal, "", &ok);
    if (!ok || fileName.isEmpty()) return;

    QString path = m_projectRoot + "/" + fileName;
    QFile file(path);
    if (file.exists()) {
        QMessageBox::warning(this, "Warning", "File already exists");
        return;
    }

    if (file.open(QIODevice::WriteOnly)) {
        file.close();
        m_projectModel->setRootPath(m_projectModel->rootPath());
    }
}

void MainWindow::createNewFolder()
{
    bool ok;
    QString folderName = QInputDialog::getText(this, "New Folder", "Folder name:", QLineEdit::Normal, "", &ok);
    if (!ok || folderName.isEmpty()) return;

    QDir dir(m_projectRoot);
    if (!dir.mkdir(folderName)) {
        QMessageBox::warning(this, "Warning", "Cannot create folder");
    }

    m_projectModel->setRootPath(m_projectModel->rootPath());
}

void MainWindow::setupProjectStructure(const QString &projectPath)
{
    QDir projectDir(projectPath);
    
    // Создаём обязательные папки
    projectDir.mkdir("src");
    projectDir.mkdir("include");
    
    // Создаём main.cpp
    QFile mainFile(projectPath + "/src/main.cpp");
    if (mainFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&mainFile);
        out << "#include <stdio.h>\n"
            << "\n"
            << "void app_main() {\n"
            << "    printf(\"Hello from ESP32 Emulator!\\n\");\n"
            << "}\n";
        mainFile.close();
    }
}

void MainWindow::createNewProject()
{
    // Запрашиваем имя проекта
    bool ok;
    QString projectName = QInputDialog::getText(
        this, 
        "New Project", 
        "Project name:", 
        QLineEdit::Normal, 
        "MyESP32Project", 
        &ok
    );
    
    if (!ok || projectName.isEmpty()) return;

    // Базовая директория - Документы пользователя
    QString baseDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString projectPath = baseDir + "/" + projectName;

    // Проверка существования
    if (QDir(projectPath).exists()) {
        QMessageBox::warning(this, "Warning", "Project folder already exists!");
        return;
    }

    // Создаём папку проекта
    if (!QDir().mkpath(projectPath)) {
        QMessageBox::critical(this, "Error", "Failed to create project folder!");
        return;
    }

    // Создаём стандартную структуру ESP32 проекта
    setupProjectStructure(projectPath);
    
    // Обновляем дерево проекта
    initCodeTab(projectPath);
}

// Слот для popup меню
void MainWindow::onTreeProjectContextMenu(const QPoint &pos)
{
    QMenu menu(this);
    
    // ВСЕГДА показываем New Project (создаётся в Documents)
    QAction *createProject = menu.addAction("New Project");
    
    // New File/Folder только если есть активный проект
    QAction *createFile = nullptr;
    QAction *createFolder = nullptr;
    if (!m_projectRoot.isEmpty()) {
        menu.addSeparator();
        createFile = menu.addAction("New File");
        createFolder = menu.addAction("New Folder");
    }

    QAction *selectedAction = menu.exec(ui->treeProject->viewport()->mapToGlobal(pos));
    if (!selectedAction) return;

    // === Обработка New Project ===
    if (selectedAction == createProject) {
        createNewProject(); // ← Вызываем отдельный метод
        return;
    }

    // === Определяем родительскую папку для файлов/папок ===
    QString parentPath = m_projectRoot; // По умолчанию - корень проекта
    
    QModelIndex index = ui->treeProject->indexAt(pos);
    if (index.isValid()) {
        parentPath = m_projectModel->filePath(index);
        QFileInfo fi(parentPath);
        if (fi.isFile()) {
            parentPath = fi.absolutePath(); // Если кликнули на файл - берём его папку
        }
    }

    // === Обработка New File ===
    if (selectedAction == createFile) {
        bool ok;
        QString name = QInputDialog::getText(this, "Create File", "File Name:", 
                                           QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            QString fullPath = parentPath + "/" + name;
            if (QFile::exists(fullPath)) {
                QMessageBox::warning(this, "Warning", "File already exists!");
                return;
            }
            
            QFile file(fullPath);
            if (file.open(QIODevice::WriteOnly)) {
                file.close();
                m_projectModel->setRootPath(m_projectModel->rootPath());
            }
        }
    }
    // === Обработка New Folder ===
    else if (selectedAction == createFolder) {
        bool ok;
        QString name = QInputDialog::getText(this, "Create Folder", "Folder Name:", 
                                           QLineEdit::Normal, "", &ok);
        if (ok && !name.isEmpty()) {
            QDir dir(parentPath);
            if (!dir.mkdir(name)) {
                QMessageBox::warning(this, "Warning", "Cannot create folder!");
                return;
            }
            m_projectModel->setRootPath(m_projectModel->rootPath());
        }
    }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // ===== Настройка дерева компонентов =====
    ui->treeComponents->setDragEnabled(true);
    ui->treeComponents->setSelectionMode(QAbstractItemView::SingleSelection);

    // ProjectTreeWidget для проекта
    ui->treeProjectComponents->setAcceptDrops(true);
    ui->treeProjectComponents->setDragDropMode(QAbstractItemView::DropOnly);
    ui->treeProjectComponents->setDropIndicatorShown(true);

    // Заголовки
    ui->treeComponents->header()->setVisible(true);
    ui->treeProjectComponents->header()->setVisible(true);

    // Добавляем доки
    addDockWidget(Qt::BottomDockWidgetArea, ui->dockLogger);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockRegisters);

    // Настройка регистров
    ui->treeRegisters->setColumnCount(2);
    ui->treeRegisters->setHeaderLabels({"Register", "Value"});

    // Загружаем компоненты
    loadFritzingParts();
	// Разрешаем пользовательское контекстное меню
	ui->treeProject->setContextMenuPolicy(Qt::CustomContextMenu);
    // Worker
    m_workerObject = new WorkerObject();
    m_workerObject->moveToThread(&m_workerThread);
    m_workerThread.start();

    // Кнопки
    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::on_actionStart_triggered);
    connect(ui->btnStop, &QPushButton::clicked, this, &MainWindow::on_actionStop_triggered);
    connect(ui->btnStep, &QPushButton::clicked, this, &MainWindow::on_actionStep_triggered);
	connect(ui->treeProject, &QTreeView::customContextMenuRequested,
			this, &MainWindow::onTreeProjectContextMenu);
    // Сигналы WorkerObject
    connect(m_workerObject, &WorkerObject::logMessage, this, &MainWindow::appendLog, Qt::QueuedConnection);
    connect(m_workerObject, &WorkerObject::emulationStateChanged, this, &MainWindow::onEmulationStateChanged, Qt::QueuedConnection);

    connect(ui->editSearch, &QLineEdit::textChanged, this, &MainWindow::filterComponents);

    // Стартовое состояние кнопок
    ui->btnStart->setEnabled(true);
    ui->btnStop->setEnabled(false);
    ui->btnStep->setEnabled(true);

    // ===== Инициализация вкладки Code =====
    QString defaultProject = QDir::currentPath() + "/MyProject";
    initCodeTab(defaultProject);
}

MainWindow::~MainWindow()
{
    m_workerThread.quit();
    m_workerThread.wait();
    delete m_workerObject;
    delete ui;
}

void MainWindow::on_actionStart_triggered()
{
    if (m_firmwarePath.isEmpty()) {
        appendLog("❌ Сначала выберите .bin или .elf файл!");
        return;
    }
    // Передаём путь
    QMetaObject::invokeMethod(
        m_workerObject, "setFirmwarePath",
        Qt::QueuedConnection,
        Q_ARG(QString, m_firmwarePath)
    );
    // Запускаем
    QMetaObject::invokeMethod(
        m_workerObject, "onStartRequested",
        Qt::QueuedConnection
    );
}

void MainWindow::on_actionStop_triggered()
{
    QMetaObject::invokeMethod(m_workerObject, "onStopRequested", Qt::QueuedConnection);
}

void MainWindow::on_actionStep_triggered()
{
    QMetaObject::invokeMethod(m_workerObject, "onStepRequested", Qt::QueuedConnection);
}

void MainWindow::appendLog(const QString &message)
{
    ui->textEditLog->append(message);
    ui->textEditLog->ensureCursorVisible();
}

void MainWindow::onEmulationStateChanged(bool running)
{
    ui->btnStart->setEnabled(!running);
    ui->btnStop->setEnabled(running);
    ui->btnStep->setEnabled(!running);
}
