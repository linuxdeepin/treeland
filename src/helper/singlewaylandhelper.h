#include <QObject>

class QProcess;

class SingleWaylandHelper : public QObject {
    Q_OBJECT
public:
    explicit SingleWaylandHelper(QObject *parent = nullptr);

    bool start(const QString& compositor, const QString& args);

private:
    QProcess *m_process;
};
