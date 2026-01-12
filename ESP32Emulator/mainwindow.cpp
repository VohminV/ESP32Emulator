#include "mainwindow.h"
#include "./ui_mainwindow.h"
#include <QMetaObject>
#include <QStringList>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // --- Добавляем доки вручную (Qt6) ---
    addDockWidget(Qt::BottomDockWidgetArea, ui->dockLogger);
    addDockWidget(Qt::RightDockWidgetArea, ui->dockRegisters);

    // --- Поток для эмуляции ---
    m_workerObject.moveToThread(&m_workerThread);
    m_workerThread.start();

    // --- Подключаем кнопки ---
    connect(ui->btnStart, &QPushButton::clicked, this, &MainWindow::on_actionStart_triggered);
    connect(ui->btnStop, &QPushButton::clicked, this, &MainWindow::on_actionStop_triggered);
    connect(ui->btnStep, &QPushButton::clicked, this, &MainWindow::on_actionStep_triggered);

    // --- Сигналы WorkerObject ---
    connect(&m_workerObject, &WorkerObject::logMessage,
            this, &MainWindow::appendLog, Qt::QueuedConnection);
    connect(&m_workerObject, &WorkerObject::emulationStateChanged,
            this, &MainWindow::onEmulationStateChanged, Qt::QueuedConnection);

    // --- Изначальное состояние кнопок ---
    ui->btnStart->setEnabled(true);
    ui->btnStop->setEnabled(false);
    ui->btnStep->setEnabled(true);

    // --- Настройка дерева регистров ---
    ui->treeRegisters->setColumnCount(2);
    QStringList headers;
    headers << "Register" << "Value";
    ui->treeRegisters->setHeaderLabels(headers);
}

MainWindow::~MainWindow() {
    m_workerThread.quit();
    m_workerThread.wait();
    delete ui;
}

// ----- Слоты кнопок -----
void MainWindow::on_actionStart_triggered() {
    QMetaObject::invokeMethod(&m_workerObject, "onStartRequested", Qt::QueuedConnection);
}

void MainWindow::on_actionStop_triggered() {
    QMetaObject::invokeMethod(&m_workerObject, "onStopRequested", Qt::QueuedConnection);
}

void MainWindow::on_actionStep_triggered() {
    QMetaObject::invokeMethod(&m_workerObject, "onStepRequested", Qt::QueuedConnection);
}

// ----- Лог -----
void MainWindow::appendLog(const QString &message) {
    ui->textEditLog->append(message);
    ui->textEditLog->ensureCursorVisible();
}

// ----- Управление кнопками по состоянию эмуляции -----
void MainWindow::onEmulationStateChanged(bool running) {
    ui->btnStart->setEnabled(!running);
    ui->btnStop->setEnabled(running);
    ui->btnStep->setEnabled(!running);
}
