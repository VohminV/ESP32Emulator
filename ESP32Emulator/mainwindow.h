#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QThread>
#include <QXmlStreamReader>
#include <QFile>
#include <QDir>
#include <QMap>
#include <QList>
#include <QFileInfo>
#include <QString>
#include <QPixmap>
#include <QSvgRenderer>
#include <QPainter>
#include <QFileSystemModel>
#include <QTextEdit>
#include <QInputDialog>
#include <QMessageBox>

#include "workerobject.h"
#include "projecttreewidget.h"

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

    // Загрузка компонентов Fritzing
    void loadFritzingParts();

    // Инициализация вкладки Code
    void initCodeTab(const QString &projectPath);

private slots:
    void on_actionStart_triggered();
    void on_actionStop_triggered();
    void on_actionStep_triggered();

    void filterComponents(const QString &text);
    void appendLog(const QString &message);
    void onEmulationStateChanged(bool running);

    // Слоты для Code tab
    void onTreeProjectDoubleClicked(const QModelIndex &index);
    void createNewFile();
	void onTreeProjectContextMenu(const QPoint &pos);
    void createNewFolder();
	void createNewProject();
	void setupProjectStructure(const QString &projectPath); 

private:
    Ui::MainWindow *ui;

    // Поток для WorkerObject
    QThread m_workerThread;
    WorkerObject *m_workerObject;

    // Модель для дерева проекта
    QFileSystemModel *m_projectModel;

    // Корневая папка проекта
    QString m_projectRoot;

    // Вспомогательные функции
    QList<QFileInfo> findFzpRecursive(const QString &dirPath);
    QString findSvgFileRecursive(const QString &baseDir, const QString &fileName);
	
	QMap<QTextEdit*, QString> m_openedFiles;
};

#endif // MAINWINDOW_H
