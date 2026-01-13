#pragma once
#include <QTreeWidget>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QDataStream>

class ProjectTreeWidget : public QTreeWidget
{
    Q_OBJECT
public:
    using QTreeWidget::QTreeWidget;

protected:
    void dragEnterEvent(QDragEnterEvent *event) override {
        if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
            event->acceptProposedAction();
        else
            QTreeWidget::dragEnterEvent(event);
    }

    void dragMoveEvent(QDragMoveEvent *event) override {
        if (event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist"))
            event->acceptProposedAction();
        else
            QTreeWidget::dragMoveEvent(event);
    }

    void dropEvent(QDropEvent *event) override {
        if (!event->mimeData()->hasFormat("application/x-qabstractitemmodeldatalist")) {
            QTreeWidget::dropEvent(event);
            return;
        }

        QByteArray encoded = event->mimeData()->data("application/x-qabstractitemmodeldatalist");
        QDataStream stream(&encoded, QIODevice::ReadOnly);

        while (!stream.atEnd()) {
            int row, col;
            QMap<int, QVariant> roleDataMap;
            stream >> row >> col >> roleDataMap;

            QString componentName = roleDataMap[Qt::DisplayRole].toString();

            // Находим родительскую семью из mimeData, если есть
            QTreeWidgetItem *sourceItem = nullptr;
            QTreeWidgetItem *parentFamily = nullptr;

            // Поиск родительской семьи
            QTreeWidgetItemIterator it(dynamic_cast<QTreeWidget*>(event->source()));
            while (*it) {
                if ((*it)->text(0) == componentName) {
                    sourceItem = *it;
                    parentFamily = (*it)->parent();
                    break;
                }
                ++it;
            }

            if (!sourceItem)
                continue;

            QString familyName = parentFamily ? parentFamily->text(0) : "Other";

            // Проверяем, есть ли уже такая семья в проекте
            QTreeWidgetItem *familyItem = nullptr;
            for (int i = 0; i < topLevelItemCount(); ++i) {
                if (topLevelItem(i)->text(0) == familyName) {
                    familyItem = topLevelItem(i);
                    break;
                }
            }

            // Если нет — создаем
            if (!familyItem) {
                familyItem = new QTreeWidgetItem(this);
                familyItem->setText(0, familyName);
            }

            // Добавляем компонент в эту семью
            QTreeWidgetItem *newItem = new QTreeWidgetItem(familyItem);
            newItem->setText(0, componentName);
        }

        event->acceptProposedAction();
    }
};
