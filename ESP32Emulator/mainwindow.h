#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include "workerobject.h"

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_actionStart_triggered();
    void on_actionStop_triggered();
    void on_actionStep_triggered();

    void appendLog(const QString &message);
    void onEmulationStateChanged(bool running);

private:
    Ui::MainWindow *ui;

    QThread m_workerThread;
    WorkerObject m_workerObject;
};

#endif // MAINWINDOW_H
