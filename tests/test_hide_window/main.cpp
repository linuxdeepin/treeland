#include <QApplication>
#include <QMainWindow>
#include <QPushButton>
#include <QTimer>

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr) : QMainWindow(parent) {
        QPushButton *button = new QPushButton("hide", this);
        setCentralWidget(button);
        connect(button, &QPushButton::clicked, this, &MainWindow::hideWindow);
    }

private slots:
    void hideWindow() {
        this->hide();
        QTimer::singleShot(3000, this, &MainWindow::showWindow);
    }

    void showWindow() {
        this->show();
    }
};

int main(int argc, char **argv) {
    QApplication app(argc, argv);

    MainWindow window;
    window.show();

    return app.exec();
}

#include "main.moc"
