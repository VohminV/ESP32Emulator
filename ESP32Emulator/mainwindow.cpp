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

    // Worker
    m_workerObject = new WorkerObject();
    m_workerObject->moveToThread(&m_workerThread);
    m_workerThread.start();

    // Кнопки
    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::on_actionStart_triggered);
    connect(ui->btnStop, &QPushButton::clicked, this, &MainWindow::on_actionStop_triggered);
    connect(ui->btnStep, &QPushButton::clicked, this, &MainWindow::on_actionStep_triggered);

    // Сигналы WorkerObject
    connect(m_workerObject, &WorkerObject::logMessage, this, &MainWindow::appendLog, Qt::QueuedConnection);
    connect(m_workerObject, &WorkerObject::emulationStateChanged, this, &MainWindow::onEmulationStateChanged, Qt::QueuedConnection);

    connect(ui->editSearch, &QLineEdit::textChanged, this, &MainWindow::filterComponents);

    // Стартовое состояние кнопок
    ui->btnStart->setEnabled(true);
    ui->btnStop->setEnabled(false);
    ui->btnStep->setEnabled(true);
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
    QMetaObject::invokeMethod(m_workerObject, "onStartRequested", Qt::QueuedConnection);
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
