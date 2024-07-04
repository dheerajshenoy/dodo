#include "qtmetamacros.h"
#include <qt6/QtWidgets/QLineEdit>
#include <qt6/QtWidgets/QLabel>
#include <qt6/QtWidgets/QWidget>
#include <qt6/QtWidgets/QHBoxLayout>

enum Mode {
    VIEW_MODE,
    COMMAND_MODE,
    SEARCH_MODE,
};

class CommandBar : public QWidget {
    Q_OBJECT
public:
    CommandBar(QWidget *parent = nullptr);
    ~CommandBar();
    void Search();
    void ProcessInput();
    Mode GetMode() { return m_mode; }
    void SetMode(const Mode &m) { m_mode = m; }

signals:
    void searchMode(QString);
    void searchClear();

private:
    
    QLabel *m_prompt = new QLabel();
    QLineEdit *m_input = new QLineEdit();
    QHBoxLayout *m_layout = new QHBoxLayout();
    Mode m_mode = Mode::VIEW_MODE;
};
