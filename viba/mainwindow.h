#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QLabel>
#include <QListWidget>
#include <QTextEdit>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QProgressBar>
#include <QGroupBox>
#include <QSplitter>
#include <QRandomGenerator>
#include <QTimer>
#include <vector>
#include <string>

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_btnSelectFile_clicked();
    void on_btnStartSim_clicked();
    void on_btnPauseSim_clicked();
    void on_simulationStep();

private:
    std::vector<std::vector<bool>> parcala_ve_kaydet(const std::string& dosya_yolu);
    void resetSimulation();

    QWidget *centralWidget;
    QVBoxLayout *mainLayout;

    QPushButton *btnSelectFile;
    QLabel *lblFileName;

    QListWidget *listFrames;
    QListWidget *listCRC;
    QLabel *lblChecksum;

    QPushButton *btnStartSim;
    QPushButton *btnPauseSim;
    QProgressBar *progressBar;
    QTextEdit *txtLog;

    QLabel *lblSender;
    QLabel *lblReceiver;

    QTimer *simTimer;
    int currentFrameIndex;
    std::vector<QString> frameResults;
    bool isPaused;
};

#endif // MAINWINDOW_H
