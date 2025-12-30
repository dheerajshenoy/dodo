#pragma once

#include <QStandardItemModel>
#include <mupdf/fitz.h>

class OutlineModel : public QStandardItemModel
{
public:
    OutlineModel(QObject *parent = nullptr) : QStandardItemModel(parent)
    {
        setHorizontalHeaderLabels({"Title", "Page"});
    }

    // Define a custom role for the hidden data
    enum OutlineRoles
    {
        TargetLocationRole = Qt::UserRole + 1
    };

    void loadFromOutline(fz_outline *root)
    {
        clear();
        setHorizontalHeaderLabels({"Title", "Page"});

        struct StackItem
        {
            fz_outline *node;
            QStandardItem *parent;
        };

        StackItem stack[256];
        int sp = 0;

        fz_outline *current       = root;
        QStandardItem *parentItem = invisibleRootItem();

        while (current || sp > 0)
        {
            while (current)
            {
                QStandardItem *titleItem = new QStandardItem(QString::fromUtf8(
                    current->title ? current->title : "<no title>"));
                QStandardItem *pageItem  = new QStandardItem(
                    QString::number(current->page.page + 1));

                titleItem->setData(QPointF{current->x, current->y},
                                   TargetLocationRole);

                // Add row under the correct parent
                parentItem->appendRow({titleItem, pageItem});

                if (current->down)
                {
                    if (current->next)
                    {
                        stack[sp++] = {current->next, parentItem};
                    }
                    parentItem = titleItem; // new parent for children
                    current    = current->down;
                }
                else
                {
                    current = current->next;
                }
            }

            if (sp > 0)
            {
                StackItem s = stack[--sp];
                current     = s.node;
                parentItem  = s.parent;
            }
        }
    }
};
