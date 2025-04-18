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

    txtSenderLog = new QTextEdit(this);
    txtSenderLog->setReadOnly(true);
    txtSenderLog->setPlaceholderText("Gönderici Logu");

    txtReceiverLog = new QTextEdit(this);
    txtReceiverLog->setReadOnly(true);
    txtReceiverLog->setPlaceholderText("Alıcı Logu");

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

    QHBoxLayout *logLayout = new QHBoxLayout();
    logLayout->addWidget(txtSenderLog);
    logLayout->addWidget(txtReceiverLog);

    mainLayout->addWidget(btnSelectFile);
    mainLayout->addWidget(lblFileName);
    mainLayout->addLayout(simLayout);
    mainLayout->addWidget(splitter);
    mainLayout->addWidget(lblChecksum);
    mainLayout->addWidget(btnStartSim);
    mainLayout->addWidget(btnPauseSim);
    mainLayout->addWidget(progressBar);
    mainLayout->addLayout(logLayout);

    setCentralWidget(centralWidget);

    connect(btnSelectFile, &QPushButton::clicked, this, &MainWindow::on_btnSelectFile_clicked);
    connect(btnStartSim, &QPushButton::clicked, this, &MainWindow::on_btnStartSim_clicked);
    connect(btnPauseSim, &QPushButton::clicked, this, &MainWindow::on_btnPauseSim_clicked);
    connect(simTimer, &QTimer::timeout, this, &MainWindow::on_simulationStep);
}

MainWindow::~MainWindow() {}

bool simulate_frame_loss()    { return rand() % 100 < 10; }
bool simulate_frame_corrupt() { return rand() % 100 < 20; }
bool simulate_ack_loss()      { return rand() % 100 < 15; }
bool simulate_checksum_error(){ return rand() % 100 < 5; }

std::vector<bool> compute_crc16(std::vector<bool>& bits){
    const uint16_t polynomial = 0x1021;
    uint16_t crc = 0xFFFF; //initial value for crc
    bool msb;
    int i;
    for(bool bit: bits){
        msb = (crc & 0x8000) != 0; // get the msb
        crc <<= 1;
        crc |= bit;
        if(msb) {
            crc ^= polynomial;
        }
    }
    std::vector<bool> crcVector;
    for(i=15; i>=0; i--)
        crcVector.push_back((crc >> i) & 1);
    return crcVector;
}


uint16_t compute_checksum(const std::vector<std::vector<bool>>& frames){
    uint32_t sum = 0;
    for(const auto& frame: frames){
        uint16_t word = 0;
        for(size_t i = frame.size() - 16; i < frame.size(); i++){
            word = (word << 1) | frame[i];
        }
        sum += word;
        if(sum > 0xFFFF){
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    return static_cast<uint16_t>(sum);
}




bool assure_crc(std::vector<bool>& frame){
    std::vector<bool> data(frame.begin(), frame.end() - 16);
    std::vector<bool> receivedCrc(frame.end() - 16, frame.end());
    std::vector<bool> calculatedCrc = compute_crc16(data);
    return (calculatedCrc == receivedCrc);
}

std::vector<std::vector<bool>> MainWindow::parcala_ve_kaydet(const std::string& dosya_yolu)  {
    // Belirtilen dosya yolunu kullanarak bir giriş dosyası akışı (ifstream) nesnesi oluşturur.
    // std::ios::binary: Dosyayı ikili modda açar, böylece baytlar olduğu gibi okunur.
    // std::ios::ate: Dosya açıldığında okuma/yazma işaretçisini dosyanın sonuna konumlandırır.
    std::ifstream dosya(dosya_yolu, std::ios::binary | std::ios::ate);

    // Dosyanın başarıyla açılıp açılmadığını kontrol eder.
    if (!dosya.is_open()) {
        // Eğer dosya açılamazsa, bir hata mesajı standart hata akışına (cerr) yazdırılır.
        std::cerr << "Dosya açılamadı!\n";
        // Boş bir vektör döndürülerek işlemin başarısız olduğu belirtilir.
        return {};
    }

    // Dosyanın sonundaki konumu alarak dosyanın toplam boyutunu (bayt cinsinden) belirler.
    std::streamsize boyut = dosya.tellg();
    // Okuma/yazma işaretçisini dosyanın başına geri konumlandırır, böylece dosyadan okuma işlemine baştan başlanabilir.
    dosya.seekg(0, std::ios::beg);

    // Dosyanın tüm bitlerini saklamak için bir boolean vektörü oluşturur.
    std::vector<bool> bit_dizisi;
    // Bit vektörünün boyutunu önceden ayırır. Her bayt 8 bit içerdiğinden, toplam bit sayısı dosya boyutunun 8 katıdır.
    bit_dizisi.reserve(boyut * 8);

    // Dosyadan tek tek baytları okumak için bir karakter değişkeni tanımlar.
    char byte;
    // Dosyanın sonuna ulaşılana kadar bayt bayt okuma işlemi gerçekleştirir.
    while (dosya.read(&byte, 1)) {
        // Okunan karakteri işaretsiz bir karaktere (unsigned char) dönüştürür.
        // Bu, bit kaydırma işlemlerinde taşma sorunlarını önler.
        unsigned char b = static_cast<unsigned char>(byte);
        // Okunan her bir baytın 8 bitini tek tek işlemek için bir döngü başlatır.
        // Bitler en yüksek anlamlı bitten (7) en düşük anlamlı bite (0) doğru işlenir.
        for (int i = 7; i >= 0; --i) {
            // Bitwise AND operatörü (&) ve sağa kaydırma operatörü (>>) kullanarak baytın i-inci bitini kontrol eder.
            // Eğer i-inci bit 1 ise, sonuç 1 (true), aksi takdirde 0 (false) olur.
            bool bit = (b >> i) & 1;
            // Elde edilen bit değerini bit dizisine ekler.
            bit_dizisi.push_back(bit);
        }
    }

    // Oluşturulan bit dizisini 100 bitlik parçalara ayırmak için bir vektör (matris) oluşturur.
    std::vector<std::vector<bool>> matris;
    // Bit dizisi üzerinde 100'lük adımlarla ilerleyen bir döngü başlatır.
    for (size_t i = 0; i < bit_dizisi.size(); i += 100) {
        // Her bir 100 bitlik parça için geçici bir boolean vektörü oluşturur.
        std::vector<bool> parca;
        // Mevcut parçanın başlangıç indeksinden itibaren 100 bit veya bit dizisinin sonuna kadar olan bitleri alır.
        for (size_t j = i; j < i + 100 && j < bit_dizisi.size(); ++j) {
            // Bit dizisindeki ilgili biti mevcut parçaya ekler.
            parca.push_back(bit_dizisi[j]);
        }
        std::vector<bool> crc = compute_crc16(parca);
        parca.insert(parca.end(), crc.begin(), crc.end());


        // Oluşturulan parçayı (vektörü) sonuç matrisine ekler.
        matris.push_back(parca);
    }

    // Oluşturulan parçaların bulunduğu matrisi döndürür.
    return matris;
}


uint16_t vector_to_uint16(const std::vector<bool>& vec) {
    uint16_t value = 0;
    for (size_t i = 0; i < vec.size(); ++i) {
        value = (value << 1) | vec[i]; // Bit kaydırarak integer'a çevir
    }
    return value;
}


void MainWindow::on_btnSelectFile_clicked() {
    QString fileName = QFileDialog::getOpenFileName(this, "Veri Dosyası Seç", "", "Data Files (*.dat)");
    if (!fileName.isEmpty()) {
        lblFileName->setText(fileName);
        txtSenderLog->append("[Dosya] Seçilen dosya: " + fileName);

        listFrames->clear();
        listCRC->clear();
        frameResults.clear();
        currentFrameIndex = 0;
        progressBar->setValue(0);
        txtSenderLog->clear();
        txtReceiverLog->clear();

        // Dosyayı parçala
        frameData = parcala_ve_kaydet(fileName.toStdString());
        quint16 checksum = 0;

        for (size_t i = 0; i < frameData.size(); ++i) {
            std::vector<bool> crcBits = compute_crc16(frameData[i]);
            uint16_t crcValue = vector_to_uint16(crcBits);
            QString crcHex = QString::number(crcValue, 16).toUpper().rightJustified(4, '0');
            listFrames->addItem("Frame " + QString::number(i + 1) + ": Hazır");
            listCRC->addItem("Frame " + QString::number(i + 1) + " CRC: 0x" + crcHex);
            frameResults.push_back("Hazır");
        }

        // Checksum hesapla ve 16-bit olarak göster
        checksum = compute_checksum(frameData);
        QString checksumHex = QString::number(checksum, 16).toUpper().rightJustified(4, '0');
        lblChecksum->setText("Checksum: 0x" + checksumHex);
        txtReceiverLog->append("[Checksum] Hesaplandı: 0x" + checksumHex);
    }
}


void MainWindow::on_btnStartSim_clicked() {
    txtSenderLog->append("[Simülasyon] Başlatıldı.");
    isPaused = false;
    simTimer->start(50);
}

void MainWindow::on_btnPauseSim_clicked() {
    isPaused = !isPaused;
    if (isPaused) {
        simTimer->stop();
        txtSenderLog->append("[Simülasyon] Duraklatıldı.");
    } else {
        txtSenderLog->append("[Simülasyon] Devam Ediyor...");
        simTimer->start(50);
    }
}


void MainWindow::on_simulationStep() {
    if (currentFrameIndex >= listFrames->count()){
        send_checksum();   \
    }

    if (currentFrameIndex >= listFrames->count()) {
        simTimer->stop();
        txtSenderLog->append("[Simülasyon] Tamamlandı.");
        txtReceiverLog->append("[Simülasyon] Tüm frameler alındı.");
        return;
    }
    std::cout << "\n\n---Veri iletisimi basladi...---\n\n";
    std::vector<std::vector<bool>> framesCopy = frameData;
    int totalFrames = framesCopy.size(), corruptedIndex;
    bool ackSent;
    int last = currentFrameIndex;
    bool expectedAck = currentFrameIndex % 2;
    while(currentFrameIndex == last){
        last = currentFrameIndex;
        txtSenderLog->append("Frame " + QString::number(currentFrameIndex) + " gonderiliyor. ");
        std::cout << "Gonderici: Frame " << currentFrameIndex << " gonderiliyor. ";
        if(simulate_ack_loss()){ // frame yolda kayboldu ise
            ackSent = !expectedAck;
            txtReceiverLog->append("Frame " + QString::number(currentFrameIndex) + " yolda kayboldu. Gonderilecek ACK: " +  QString::number(!ackSent));
            std::cout << "Alici: Frame " << currentFrameIndex << " yolda kayboldu. Gonderilecek ACK: " << (!ackSent) << "\n";
        }
        else { // frame yolda kaybolmadı, başarılı bir şekilde iletildi
            txtReceiverLog->append("Alici: Frame " + QString::number(currentFrameIndex) + " teslim alindi.");
            std::cout << "Alici: Frame " << currentFrameIndex << " teslim alindi.\n";
            std::vector<bool> receivedFrame = framesCopy[currentFrameIndex];
            if(simulate_frame_corrupt()) { // frame bozulduysa
                corruptedIndex = rand() % receivedFrame.size();
                receivedFrame[corruptedIndex] = !receivedFrame[corruptedIndex]; // bozulmasına karar verilen indexteki bit ters çevirilir

            }
            if(assure_crc(receivedFrame)){ // crc is the same
                ackSent = expectedAck;
                txtReceiverLog->append("Alici: Frame " + QString::number(currentFrameIndex) + " dogru alindi. Gonderilecek ACK: " + QString::number(ackSent) );
                std::cout << "Alici: Frame " << currentFrameIndex << " dogru alindi. Gonderilecek ACK: " << (ackSent) << "\n";
            }
            else { // crc different
                ackSent = !expectedAck;
                txtReceiverLog->append("Alici: Frame " + QString::number(currentFrameIndex) + " hatali geldi. Gonderilecek ACK: " + QString::number(!ackSent) );
                std::cout << "Alici: Frame " << currentFrameIndex << " hatali geldi. Gonderilecek ACK: " << (!ackSent) << "\n";
                //receivedFrame[corruptedIndex] = !receivedFrame[corruptedIndex];
            }
        }

        if(simulate_ack_loss()){ //ack lost on the way
            txtSenderLog->append("ACK yolda kayboldu. Frame " + QString::number(currentFrameIndex) + " tekrar gonderilecek.");
            std::cout << "ACK yolda kayboldu. Frame " << currentFrameIndex << " tekrar gonderilecek.\n";
        }
        else {
            std::cout << "ACK gondericiye iletildi. Iletilen ACK: " << ackSent << "\n";
            txtSenderLog->append("ACK alindi. alinan ACK: " + QString::number(ackSent) );
            if(expectedAck != ackSent) { // beklenen ack gelmedi, demek ki hata olmuş
                txtSenderLog->append("Frame " + QString::number(currentFrameIndex) + " gonderilirken bir hata olusmus. Frame tekrar gonderiliyor...");
                std::cout << "Gonderici: Frame " << currentFrameIndex << " gonderilirken bir hata olusmus. Frame tekrar gonderiliyor...\n";
            }
            else { // beklenen ack geldi, demek ki bir sorun yok
                txtSenderLog->append("Frame " + QString::number(currentFrameIndex) + " basarili bir sekilde gonderilmis. Siradaki frame'e geciliyor...");
                std::cout << "Gonderici: Frame " << currentFrameIndex << " basarili bir sekilde gonderilmis. Siradaki frame'e geciliyor...\n\n";
                listFrames->item(currentFrameIndex)->setText("Frame " + QString::number(currentFrameIndex + 1) + ": Gönderildi");
                expectedAck = !expectedAck;
                currentFrameIndex++;
                int progress = static_cast<int>((currentFrameIndex / static_cast<double>(listFrames->count())) * 100);
                progressBar->setValue(progress);
            }
        }

    }

}

void MainWindow::send_checksum(){
    std::cout << "Tum frameler basariyla gonderildi. Checksum frame'i gonderiliyor...\n\n";
    txtSenderLog->append("Tum frameler basariyla gonderildi. Checksum frame'i gonderiliyor...");

    std::cout << "Gonderici: Checksum gonderiliyor...\n";
    txtSenderLog->append("Checksum gonderiliyor..." );

    uint16_t sum = compute_checksum(frameData);
    uint16_t checksumValue = ~sum;  // Checksum: 1’s complement

    uint16_t receivedChecksum = checksumValue;

    // Simulate checksum error
    if (simulate_checksum_error()) {
        int bitToFlip = rand() % 16;
        receivedChecksum ^= (1 << bitToFlip);  // Bit tersleniyor
        std::cout << "Checksum bozuldu! Bit " << bitToFlip << " terslendi.\n";
    }

    uint16_t recomputedSum = compute_checksum(frameData);
    uint32_t total = receivedChecksum + recomputedSum;

    if ((total & 0xFFFF) == 0xFFFF) {
        std::cout << "Alici: Gonderilen checksum dogru. Gonderim tamamlandi.\n";
        txtReceiverLog->append("Gonderilen checksum dogru. Gonderim tamamlandi.");
    } else {
        std::cout << "Alici: Checksum hatali. Tum frameler tekrar gonderilecek!\n";
        txtReceiverLog->append("Checksum hatali. Tum frameler tekrar gonderilecek!");
        std::cout << "Received checksum: " << receivedChecksum
                  << " --- computed checksum: " << recomputedSum
                  << " --- Total: " << total << "\n";

        for (int i = 0; i < frameData.size(); ++i) {
            QString status = "Frame " + QString::number(i + 1) + ": tekrar gonderilecek";
            listFrames->item(i)->setText(status);
        }

        currentFrameIndex = 0;
    }



}




