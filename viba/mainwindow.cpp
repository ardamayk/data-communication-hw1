#include "mainwindow.h"
#include <QFileDialog>
#include <QMessageBox>
#include <fstream>
#include <iostream>
#include <QCoreApplication>

std::vector<std::vector<bool>> globalFrameData;
std::vector<bool> checksumFrame;


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
    /*std::cout << "Islenen parca: ";
    for(bool bit: bits){
        std::cout << bit;
    }*/
    const std::vector<bool> DLE = {0,0,0,1,0,0,0,0}; // 0x10
    const std::vector<bool> STX = {0,0,0,0,0,0,1,0}; // 0x02
    const std::vector<bool> ETX = {0,0,0,0,0,0,1,1}; // 0x03

    size_t i = 0, j;
    bool startFound = false, isDLE, isStuffedDLE, isSTX, isETX, reachedEnd = false;
    // first, we need to find where the data starts.
    //std::cout << "\nIlgili frame'in baslangic noktasi bulunuyor...\n";
    while((i+16) <= bits.size() && !startFound){
        isDLE = std::equal(bits.begin() + i, bits.begin() + i + 8, DLE.begin());
        isSTX = std::equal(bits.begin() + i + 8, bits.begin() + i + 16, STX.begin());
        startFound = (isDLE & isSTX);
        if(!startFound){
            i++;
        }
        else{
            i += 16;
        }
    }
    //std::cout << "Frame'in baslangic noktasi: " << i << "\n";
    //after this while loop, we know the start point of the data
    std::vector<bool> data;
    //std::cout<< "Datalar ayristiriliyor...\n" << "Frame boyutu: " << bits.size() << ", Islenen indisler: ";
    while((i+8) <= bits.size() && !reachedEnd){
        //std::cout << i << "  ";
        isDLE = true;
        for(j=0; j<8 && isDLE; j++) {
            if(bits[i+j] != DLE[j]){
                isDLE = false;
            }
        }
        if(isDLE && (i+16) <= bits.size()){
            //std::cout << "DLE bulundu! ";
            isETX = true;
            isStuffedDLE = true;
            for(j=0; j<8; j++){
                if(bits[i+j+8] != ETX[j]){
                    isETX = false;
                    //std::cout << "DLE'den sonra ETX gelmiyor... ";
                }
                if(bits[i+j+8] != DLE[j]){
                    isStuffedDLE = false;
                    //std::cout << "DLE'den sonra tekrar DLE gelmiyor... ";
                }
            }
            if(isETX){
                //std::cout << "DLE sonrasi ETX bulundu! ";
                reachedEnd = true;
            }
            else if(isStuffedDLE){
                for(bool bit: DLE){
                    data.push_back(bit);
                }
                i+=16;
            }
            else{
                data.push_back(bits[i]);
                i++;
            }
        }
        else{
            data.push_back(bits[i]);
            i++;
        }
    }
    //std::cout << "While dongusunden cikildi.\n";

    const uint16_t polynomial = 0x1021;
    uint16_t crc = 0xFFFF; //initial value for crc
    bool msb;
    for(bool bit: data){
        msb = (crc & 0x8000) != 0; // get the msb
        crc <<= 1;
        crc |= bit;
        if(msb) {
            crc ^= polynomial;
        }
    }
    //std::cout << "CRC vektoru hazirlaniyor...\n";
    std::vector<bool> crcVector;
    int k;
    for(k=15; k>=0; k--)
        crcVector.push_back((crc >> k) & 1);
    //std::cout << "CRC vektoru hazir. vektor donduruluyor...";
    return crcVector;
}


uint16_t compute_checksum(const std::vector<std::vector<bool>>& frames){
    uint32_t sum = 0;
    uint16_t crc;
    size_t i;
    for(const auto& frame: frames){
        crc = 0;
        for(i = frame.size() - 16; i < frame.size(); i++){
            crc = (crc << 1) | frame[i];
        }
        sum += crc;
    }
    //std::cout << "Computed checksum: " << sum << "\n";
    return static_cast<uint16_t>(sum);
}




void corrupt_frame_data(std::vector<bool>& frame){
    const std::vector<bool> DLE = {0,0,0,1,0,0,0,0};
    const std::vector<bool> STX = {0,0,0,0,0,0,1,0};
    const std::vector<bool> ETX = {0,0,0,0,0,0,1,1};

    size_t i = 0,j, corruptedIndex;
    bool foundStart = false, isDLE, reachedEnd = false;
    while(i+16<=frame.size() && !foundStart){
        while (i + 16 <= frame.size() && !foundStart) {
            if (std::equal(frame.begin() + i, frame.begin() + i + 8, DLE.begin()) &&
                std::equal(frame.begin() + i + 8, frame.begin() + i + 16, STX.begin())) {
                foundStart = true;
                i += 16;
            } else {
                ++i;
            }
        }
    }
    std::vector<size_t> dataIndices;

    while((i+8) <= (frame.size()-16) && !reachedEnd){
        isDLE = std::equal(frame.begin() + i, frame.begin() + i  + 8, DLE.begin());
        if(isDLE && i+16 <= frame.size()){
            std::vector<bool> nextByte(frame.begin() + i + 8, frame.begin() + i + 16);
            if(nextByte == ETX){
                reachedEnd = true;
            }
            else if(nextByte == DLE){
                for(j=0;j<8;j++){
                    dataIndices.push_back(i+8+j);
                }
                i += 16;
            }
            else{
                dataIndices.push_back(i);
                i++;
            }
        }
        else{
            dataIndices.push_back(i);
            i++;
        }
    }
    if(!dataIndices.empty()){
        corruptedIndex = rand() % dataIndices.size();
        frame[dataIndices[corruptedIndex]] = !frame[dataIndices[corruptedIndex]];
    }


}


std::vector<bool> create_checksum_frame(std::vector<std::vector<bool>>& frames){
    uint16_t checksum;
    uint16_t checksumComplement;
    int i;

    checksum = compute_checksum(frames);
    checksumComplement = ~checksum;
    checksumComplement++;

    std::vector<bool> frame;
    std::vector<bool> header = {1, 0, 1, 0}; // Example 4-bit header for transparency
    frame.insert(frame.end(), header.begin(), header.end());

    for(i= 15; i>= 0; i--)
        frame.push_back((checksumComplement >> i) & 1);

    return frame;
}

bool assure_crc(std::vector<bool>& frame){
    std::vector<bool> receivedCrc(frame.end() - 16, frame.end());
    std::vector<bool> calculatedCrc = compute_crc16(frame);
    return (calculatedCrc == receivedCrc);
}

std::vector<std::vector<bool>> parcala_ve_kaydet(const std::string& dosya_yolu) {
    std::ifstream dosya(dosya_yolu, std::ios::binary | std::ios::ate); // Dosyayı binary ve sondan aç
    if (!dosya.is_open()) { // Dosya açılamadıysa hata verip çık
        std::cerr << "Dosya acilamadi!\n";
        return {}; // Boş vektör döndür
    }

    std::streamsize boyut = dosya.tellg(); // Dosyanın toplam boyutunu al
    dosya.seekg(0, std::ios::beg); // Dosyanın başına dön

    std::vector<bool> bit_dizisi; // Tüm bitleri tutacak dizi
    bit_dizisi.reserve(boyut * 8); // Belleği önceden ayır (performans için)

    char byte; // Her okunan baytı geçici olarak tutacak değişken
    while (dosya.read(&byte, 1)) { // Dosyadan byte byte oku
        unsigned char b = static_cast<unsigned char>(byte); // Baytı işaretsiz yap (bit işlemleri için güvenli)
        for (int i = 7; i >= 0; --i) { // Bayt içindeki bitleri çöz (MSB'den LSB'ye)
            bool bit = (b >> i) & 1; // İlgili biti çıkar
            bit_dizisi.push_back(bit); // Bit dizisine ekle
        }
    }

    // DLE (0x10), STX (0x02), ETX (0x03) karakterlerinin bit dizileri
    const std::vector<bool> DLE = {0,0,0,1,0,0,0,0};
    const std::vector<bool> STX = {0,0,0,0,0,0,1,0};
    const std::vector<bool> ETX = {0,0,0,0,0,0,1,1};

    std::vector<std::vector<bool>> matris; // Frame'leri tutacak 2 boyutlu vektör

    size_t i = 0; // bit_dizisi üzerinde ilerlemek için indeks
    while (i < bit_dizisi.size()) { // Tüm bitler işlenene kadar döngü
        std::vector<bool> frame; // Yeni frame oluştur
        size_t remaining_bits = 100; // Bu frame'e eklenecek maksimum bit sayısı

        frame.insert(frame.end(), DLE.begin(), DLE.end()); // Frame başına DLE ekle
        frame.insert(frame.end(), STX.begin(), STX.end()); // Ardından STX ekle

        // 8 bitlik blokları işle
        while (remaining_bits >= 8 && i + 8 <= bit_dizisi.size()) {
            std::vector<bool> blok(bit_dizisi.begin() + i, bit_dizisi.begin() + i + 8); // 8 bitlik blok al
            i += 8; // i'yi 8 ileri al

            if (blok == DLE) { // Eğer blok DLE ile aynıysa
                frame.insert(frame.end(), DLE.begin(), DLE.end()); // Ekstra DLE ekle (stuffing)
                remaining_bits -= 8; // Toplam 100 bitten 8 bit azalt
            }

            frame.insert(frame.end(), blok.begin(), blok.end()); // Bloğu frame'e ekle
            remaining_bits -= 8; // Kalan veri kapasitesini azalt
        }

        // Geriye kalan 0-7 arası bit varsa, onları direkt ekle
        if (remaining_bits > 0 && i < bit_dizisi.size()) {
            size_t bit_to_add = std::min(remaining_bits, bit_dizisi.size() - i); // Eklenebilecek maksimum bit sayısı
            frame.insert(frame.end(), bit_dizisi.begin() + i, bit_dizisi.begin() + i + bit_to_add); // Kalan bitleri ekle
            i += bit_to_add; // İndeksi ilerlet
        }

        frame.insert(frame.end(), DLE.begin(), DLE.end()); // Frame sonuna DLE ekle
        frame.insert(frame.end(), ETX.begin(), ETX.end()); // Ardından ETX ekle
        //std::cout << "crc hesabina baslaniliyor...\n";
        std::vector<bool> crc = compute_crc16(frame);
        frame.insert(frame.end(), crc.begin(), crc.end());

        matris.push_back(frame); // Oluşturulan frame'i matrise ekle
    }

    return matris; // Tüm frame'leri içeren matrisi döndür
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
        globalFrameData = frameData;
        quint16 checksum = 0, checksumComplement;

        for (size_t i = 0; i < frameData.size(); ++i) {
            std::vector<bool> crcBits = compute_crc16(frameData[i]);
            uint16_t crcValue = vector_to_uint16(crcBits);
            QString crcHex = QString::number(crcValue, 16).toUpper().rightJustified(4, '0');
            listFrames->addItem("Frame " + QString::number(i + 1) + ": Hazır");
            listCRC->addItem("Frame " + QString::number(i + 1) + " CRC: 0x" + crcHex);
            frameResults.push_back("Hazır");
        }

        // Checksum hesapla ve 16-bit olarak göster
        checksumFrame = create_checksum_frame(frameData);
        for(bool bit: checksumFrame){
            checksumComplement = (checksumComplement << 1) | bit;
        }
        checksum = (~checksumComplement) + 1;
        QString checksumHex = QString::number(checksum, 16).toUpper().rightJustified(4, '0');
        QString checksumComplementHex = QString::number(checksumComplement, 16).toUpper().rightJustified(4, '0');
        lblChecksum->setText("Checksum: 0x" + checksumHex);
        txtReceiverLog->append("[Checksum] Hesaplandı: 0x" + checksumHex);
        txtReceiverLog->append("Bu degerin 2'ye tümleyeni frame içinde karşı tarafa gönderilecektir. Checksum'un 2'ye tümleyeni: 0x" + checksumComplementHex);
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
    if(currentFrameIndex == 0)
        std::cout << "\n\n---Veri iletisimi basladi...---\n\n";
    std::vector<std::vector<bool>> framesCopy = frameData;
    int totalFrames = framesCopy.size();
    bool ackSent;
    int last = currentFrameIndex;
    bool expectedAck = currentFrameIndex % 2;
    while(currentFrameIndex == last){
        last = currentFrameIndex;
        txtSenderLog->append("Frame " + QString::number(currentFrameIndex + 1) + " gonderiliyor. ");
        std::cout << "Gonderici: Frame " << currentFrameIndex + 1 << " gonderiliyor. ";
        if(simulate_frame_loss()){ // frame yolda kayboldu ise
            ackSent = !expectedAck;
            txtReceiverLog->append("Frame " + QString::number(currentFrameIndex + 1) + " yolda kayboldu. Gonderilecek ACK: " +  QString::number(!ackSent));
            std::cout << "Alici: Frame " << currentFrameIndex + 1 << " yolda kayboldu. Gonderilecek ACK: " << (!ackSent) << "\n";
        }
        else { // frame yolda kaybolmadı, başarılı bir şekilde iletildi
            txtReceiverLog->append("Alici: Frame " + QString::number(currentFrameIndex + 1) + " teslim alindi.");
            std::cout << "Alici: Frame " << currentFrameIndex + 1 << " teslim alindi.\n";
            std::vector<bool> receivedFrame = framesCopy[currentFrameIndex];
            if(simulate_frame_corrupt()) { // frame bozulduysa
                corrupt_frame_data(receivedFrame);
            }
            if(assure_crc(receivedFrame)){ // crc is the same
                ackSent = expectedAck;
                txtReceiverLog->append("Alici: Frame " + QString::number(currentFrameIndex + 1) + " dogru alindi. Gonderilecek ACK: " + QString::number(ackSent) );
                std::cout << "Alici: Frame " << currentFrameIndex + 1 << " dogru alindi. Gonderilecek ACK: " << (ackSent) << "\n";
            }
            else { // crc different
                ackSent = !expectedAck;
                txtReceiverLog->append("Alici: Frame " + QString::number(currentFrameIndex + 1) + " hatali geldi. Gonderilecek ACK: " + QString::number(!ackSent) );
                std::cout << "Alici: Frame " << currentFrameIndex + 1 << " hatali geldi. Gonderilecek ACK: " << (!ackSent) << "\n";
            }
        }

        if(simulate_ack_loss()){ //ack lost on the way
            txtSenderLog->append("ACK yolda kayboldu. Frame " + QString::number(currentFrameIndex + 1) + " tekrar gonderilecek.");
            std::cout << "ACK yolda kayboldu. Frame " << currentFrameIndex + 1 << " tekrar gonderilecek.\n";
        }
        else {
            std::cout << "ACK gondericiye iletildi. Iletilen ACK: " << ackSent << "\n";
            txtSenderLog->append("ACK alindi. alinan ACK: " + QString::number(ackSent) );
            if(expectedAck != ackSent) { // beklenen ack gelmedi, demek ki hata olmuş
                txtSenderLog->append("Frame " + QString::number(currentFrameIndex + 1) + " gonderilirken bir hata olusmus. Frame tekrar gonderiliyor...");
                std::cout << "Gonderici: Frame " << currentFrameIndex + 1 << " gonderilirken bir hata olusmus. Frame tekrar gonderiliyor...\n";
            }
            else { // beklenen ack geldi, demek ki bir sorun yok
                txtSenderLog->append("Frame " + QString::number(currentFrameIndex + 1) + " basarili bir sekilde gonderilmis. Siradaki frame'e geciliyor...");
                std::cout << "Gonderici: Frame " << currentFrameIndex + 1 << " basarili bir sekilde gonderilmis. Siradaki frame'e geciliyor...\n\n";
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
    txtSenderLog->append("Tum frameler basariyla gonderildi. Checksum frame'i gönderiliyor...");
    std::cout << "Tum frameler basariyla gonderildi. Checksum frame'i gönderiliyor...\n\n";


    bool checksumSent = false;

    while (!checksumSent) {
        std::cout << "Gonderici: Checksum gonderiliyor...\n";
        txtSenderLog->append("Checksum gonderiliyor...");
        std::vector<bool> checksumCopy = checksumFrame;



        std::vector<bool> receivedChecksum(checksumCopy.begin() + 4, checksumCopy.end());
        uint16_t receivedChecksumValue = 0;
        for (bool bit : receivedChecksum) {
            receivedChecksumValue = (receivedChecksumValue << 1) | bit;
        }

        uint16_t computedChecksum = compute_checksum(globalFrameData);
        uint32_t total = computedChecksum + receivedChecksumValue;

        if ((total & 0xFFFF) == 0x0000) {
            std::cout << "Alici: Gonderilen checksum dogru. Gonderim tamamlandı.\n";
            txtReceiverLog->append("Alici: Gonderilen checksum dogru. Gonderim tamamlandı.");
            txtReceiverLog->append("2'ye tümleyeni alınmış gönderilen checksum: 0x" +
                                  QString::number(receivedChecksumValue, 16).toUpper().rightJustified(4, '0') +
                                   " --- Hesaplanan checksum: 0x" +
                                  QString::number(computedChecksum, 16).toUpper().rightJustified(4, '0') + " --- Toplam: 0x" + QString::number(total & 0xFFFF, 16).toUpper().rightJustified(4, '0'));
            checksumSent = true;
        } else {
            std::cout << "Alici: Checksum hatali. Tekrar gonderim yapilacak...\n";
            std::cout << "Received checksum: " << receivedChecksumValue
                      << " --- computed checksum: " << computedChecksum
                      << " --- Total: " << total << "\n";

            txtReceiverLog->append("Checksum hatali. Tekrar gonderim yapilacak...");
            txtReceiverLog->append("Received checksum: 0x" +
                                   QString::number(receivedChecksumValue, 16).toUpper().rightJustified(4, '0') +
                                   " --- Computed checksum: 0x" +
                                   QString::number(computedChecksum, 16).toUpper().rightJustified(4, '0'));
        }
    }
}




