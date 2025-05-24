#pragma once

#include <QStandardItemModel>
#include <mupdf/fitz.h>

class OutlineModel : public QStandardItemModel {
public:
    OutlineModel(QObject *parent = nullptr) : QStandardItemModel(parent) {
        setHorizontalHeaderLabels({"Title", "Page"});
    }

    void loadFromOutline(fz_outline *root) {
        clear();
        setHorizontalHeaderLabels({"Title", "Page"});

        struct StackItem {
            fz_outline *node;
            int level;
        };

        StackItem stack[256];
        int sp = 0;
        fz_outline *current = root;
        int level = 0;

        while (current || sp > 0) {
            while (current) {
                QList<QStandardItem *> rowItems;
                rowItems << new QStandardItem(QString::fromUtf8(current->title ? current->title : "<no title>"));
                rowItems << new QStandardItem(QString::number(current->page.page + 1));

                // Indent by level
                rowItems[0]->setData(level, Qt::UserRole);

                appendRow(rowItems);

                if (current->down) {
                    if (current->next) {
                        stack[sp++] = {current->next, level};
                    }
                    current = current->down;
                    ++level;
                } else {
                    current = current->next;
                }
            }

            if (sp > 0) {
                StackItem s = stack[--sp];
                current = s.node;
                level = s.level;
            }
        }
    }
};

