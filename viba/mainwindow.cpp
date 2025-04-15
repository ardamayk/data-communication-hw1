#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <fstream>
#include <iostream>
#include <QCoreApplication>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), simTimer(new QTimer(this)), currentFrameIndex(0), isPaused(false)
{
    centralWidget = new QWidget(this);
    mainLayout = new QVBoxLayout(centralWidget);

    btnSelectFile = new QPushButton("Veri Dosyası Seç", this);
    lblFileName = new QLabel("Dosya seçilmedi", this);

    listFrames = new QListWidget(this);
    listFrames->setMinimumHeight(100);
    listFrames->setSelectionMode(QAbstractItemView::NoSelection);

    listCRC = new QListWidget(this);
    listCRC->setMinimumHeight(100);
    listCRC->setSelectionMode(QAbstractItemView::NoSelection);

    lblChecksum = new QLabel("Checksum: -", this);

    btnStartSim = new QPushButton("Simülasyonu Başlat", this);
    btnPauseSim = new QPushButton("Simülasyonu Duraklat / Devam", this);

    progressBar = new QProgressBar(this);
    progressBar->setRange(0, 100);
    progressBar->setValue(0);

    txtLog = new QTextEdit(this);
    txtLog->setReadOnly(true);

    lblSender = new QLabel("Gönderici", this);
    lblReceiver = new QLabel("Alıcı", this);
    lblSender->setAlignment(Qt::AlignCenter);
    lblReceiver->setAlignment(Qt::AlignCenter);

    QHBoxLayout *simLayout = new QHBoxLayout();
    simLayout->addWidget(lblSender);
    simLayout->addWidget(new QLabel("➡", this));
    simLayout->addWidget(lblReceiver);

    QSplitter *splitter = new QSplitter(Qt::Horizontal);

    QVBoxLayout *frameLayout = new QVBoxLayout();
    QLabel *frameLabel = new QLabel("Frame Durumu", this);
    frameLayout->addWidget(frameLabel);
    frameLayout->addWidget(listFrames);
    QWidget *frameWidget = new QWidget();
    frameWidget->setLayout(frameLayout);

    QVBoxLayout *crcLayout = new QVBoxLayout();
    QLabel *crcLabel = new QLabel("CRC Kodları", this);
    crcLayout->addWidget(crcLabel);
    crcLayout->addWidget(listCRC);
    QWidget *crcWidget = new QWidget();
    crcWidget->setLayout(crcLayout);

    splitter->addWidget(frameWidget);
    splitter->addWidget(crcWidget);

    mainLayout->addWidget(btnSelectFile);
    mainLayout->addWidget(lblFileName);
    mainLayout->addLayout(simLayout);
    mainLayout->addWidget(splitter);
    mainLayout->addWidget(lblChecksum);
    mainLayout->addWidget(btnStartSim);
    mainLayout->addWidget(btnPauseSim);
    mainLayout->addWidget(progressBar);
    mainLayout->addWidget(txtLog);

    setCentralWidget(centralWidget);

    connect(btnSelectFile, &QPushButton::clicked, this, &MainWindow::on_btnSelectFile_clicked);
    connect(btnStartSim, &QPushButton::clicked, this, &MainWindow::on_btnStartSim_clicked);
    connect(btnPauseSim, &QPushButton::clicked, this, &MainWindow::on_btnPauseSim_clicked);
    connect(simTimer, &QTimer::timeout, this, &MainWindow::on_simulationStep);
}

MainWindow::~MainWindow() {}

std::vector<std::vector<bool>> MainWindow::parcala_ve_kaydet(const std::string& dosya_yolu) {
    std::ifstream dosya(dosya_yolu, std::ios::binary | std::ios::ate);
    if (!dosya.is_open()) return {};

    std::streamsize boyut = dosya.tellg();
    dosya.seekg(0, std::ios::beg);

    std::vector<bool> bit_dizisi;
    bit_dizisi.reserve(boyut * 8);

    char byte;
    while (dosya.read(&byte, 1)) {
        unsigned char b = static_cast<unsigned char>(byte);
        for (int i = 7; i >= 0; --i)
            bit_dizisi.push_back((b >> i) & 1);
    }

    std::vector<std::vector<bool>> matris;
    for (size_t i = 0; i < bit_dizisi.size(); i += 100) {
        std::vector<bool> parca(bit_dizisi.begin() + i, bit_dizisi.begin() + std::min(i + 100, bit_dizisi.size()));
        matris.push_back(parca);
    }

    return matris;
}

void MainWindow::on_btnSelectFile_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Veri Dosyası Seç", "", "Data Files (*.dat)");
    if (!fileName.isEmpty()) {
        lblFileName->setText(fileName);
        txtLog->append("[Dosya] Seçilen dosya: " + fileName);

        listFrames->clear();
        listCRC->clear();
        frameResults.clear();
        currentFrameIndex = 0;
        progressBar->setValue(0);

        auto parcalar = parcala_ve_kaydet(fileName.toStdString());
        quint16 checksum = 0;

        for (size_t i = 0; i < parcalar.size(); ++i) {
            QString crc = QString::number(0x1234 + static_cast<int>(i), 16).toUpper();
            listFrames->addItem("Frame " + QString::number(i + 1) + ": Hazır");
            listCRC->addItem("Frame " + QString::number(i + 1) + " CRC: 0x" + crc);
            checksum ^= (0x1234 + static_cast<int>(i));
            frameResults.push_back("Hazır");
        }

        lblChecksum->setText("Checksum: 0x" + QString::number(checksum, 16).toUpper());
        txtLog->append("[Checksum] Hesaplandı: 0x" + QString::number(checksum, 16).toUpper());
    }
}

void MainWindow::on_btnStartSim_clicked() {
    txtLog->append("[Simülasyon] Başlatıldı.");
    isPaused = false;
    simTimer->start(500);
}

void MainWindow::on_btnPauseSim_clicked() {
    isPaused = !isPaused;
    if (isPaused) {
        simTimer->stop();
        txtLog->append("[Simülasyon] Duraklatıldı.");
    } else {
        txtLog->append("[Simülasyon] Devam Ediyor...");
        simTimer->start(500);
    }
}

void MainWindow::on_simulationStep() {
    if (currentFrameIndex >= listFrames->count()) {
        simTimer->stop();
        txtLog->append("[Simülasyon] Tamamlandı.");
        return;
    }

    QString frameName = "Frame " + QString::number(currentFrameIndex + 1);
    int fate = QRandomGenerator::global()->bounded(100);
    QString result;

    if (fate < 10) {
        result = "Kayıp - Yeniden Gönderiliyor";
    } else if (fate < 30) {
        result = "Bozuldu - Yeniden Gönderiliyor";
    } else if (fate < 45) {
        result = "ACK Yok - Yeniden Gönderiliyor";
    } else {
        result = "Başarılı";
    }

    listFrames->item(currentFrameIndex)->setText(frameName + ": " + result);
    txtLog->append("[" + frameName + "] Durum: " + result);

    if (result == "Başarılı") {
        currentFrameIndex++;
        int progress = static_cast<int>((currentFrameIndex / static_cast<double>(listFrames->count())) * 100);
        progressBar->setValue(progress);
    }
}
