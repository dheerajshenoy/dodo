#include <qt6/QtWidgets/QLabel>
#include <qt6/QtWidgets/QWidget>
#include <qt6/QtWidgets/QHBoxLayout>
#include <qt6/QtCore/QTimer>

class StatusBar : public QWidget {

public:

    StatusBar(QWidget *parent = nullptr);
    ~StatusBar();
    void Message(QString, int s = 1);
    void SetFileName(QString filename);
    void SetFilePageCount(int count);
    void SetCurrentPage(int);

private:

    QLabel  *m_msg_label = new QLabel(),
            *m_label_filename = new QLabel(),
            *m_label_filepagecount = new QLabel(),
            *m_label_cur_page_number = new QLabel();

    QHBoxLayout *m_layout = new QHBoxLayout();
};
