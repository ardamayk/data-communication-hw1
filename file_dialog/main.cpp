// parcala_ve_kaydet: Bir dosyadan bitleri okuyup, DLE-STX ile başlayan ve DLE-ETX ile biten frame'lere ayırır
// Her frame 100 bitlik veri içerir, DLE ile aynı 8 bitlik bloklar varsa stuffing yapılır (başına DLE eklenir)
#include <fstream> // Dosya okuma işlemleri için gerekli kütüphane
#include <iostream> // Ekrana yazdırma işlemleri için gerekli kütüphane
#include <vector> // Dinamik dizi (bit dizisi ve frame matrisleri) için gerekli kütüphane
#include <windows.h> // Windows API (dosya seçme penceresi için)
#include <commdlg.h> // GetOpenFileName fonksiyonu için




// hata olasılıkları
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
        crcVector.push_back((crc >> i) & 1);
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
    std::vector<size_t> dataIndıces;

    while((i+8) <= (frame.size()-16) && !reachedEnd){
        isDLE = std::equal(frame.begin() + i, frame.begin() + i  + 8, DLE.begin());
        if(isDLE && i+16 <= frame.size()){
            std::vector<bool> nextByte(frame.begin() + i + 8, frame.begin() + i + 16);
            if(nextByte == ETX){
                reachedEnd = true;
            }
            else if(nextByte == DLE){
                for(j=0;j<8;j++){
                    dataIndıces.push_back(i+8+j);
                }
                i += 16;
            }
            else{
                dataIndıces.push_back(i);
                i++;
            }
        }
        else{
            dataIndıces.push_back(i);
            i++;
        }
    }
    if(!dataIndıces.empty()){
        corruptedIndex = rand() % dataIndıces.size();
        frame[dataIndıces[corruptedIndex]] = !frame[dataIndıces[corruptedIndex]];
    }


}

void transmission(const std::vector<std::vector<bool>>& frames, std::vector<bool>& checksumFrame){
    std::cout << "\n\n---Veri iletisimi basladi...---\n\n";
    std::vector<std::vector<bool>> framesCopy = frames;
    int totalFrames = framesCopy.size(), currentFrame = 0;
    bool expectedAck = true, ackSent;
    while(currentFrame < totalFrames){
        std::cout << "Gonderici: Frame " << currentFrame << " gonderiliyor. ";
        if(simulate_ack_loss()){ // frame yolda kayboldu ise
            ackSent = !expectedAck;
            std::cout << "Alici: Frame " << currentFrame << " yolda kayboldu. Gonderilecek ACK: " << (!ackSent) << "\n";
        }
        else { // frame yolda kaybolmadı, başarılı bir şekilde iletildi
            std::cout << "Alici: Frame " << currentFrame << " teslim alindi.\n";
            std::vector<bool> receivedFrame = framesCopy[currentFrame];
            if(simulate_frame_corrupt()) { // frame bozulduysa
                corrupt_frame_data(receivedFrame);
            }
            if(assure_crc(receivedFrame)){ // crc is the same
                ackSent = expectedAck;
                std::cout << "Alici: Frame " << currentFrame << " dogru alindi. Gonderilecek ACK: " << (ackSent) << "\n";
            }
            else { // crc different
                ackSent = !expectedAck;
                std::cout << "Alici: Frame " << currentFrame << " hatali geldi. Gonderilecek ACK: " << (!ackSent) << "\n";
                //receivedFrame[corruptedIndex] = !receivedFrame[corruptedIndex];
            }
        }

        if(simulate_ack_loss()){ //ack lost on the way
            std::cout << "ACK yolda kayboldu. Frame " << currentFrame << " tekrar gonderilecek.\n";
        }
        else {
            std::cout << "ACK gondericiye iletildi. Iletilen ACK: " << ackSent << "\n";
            if(expectedAck != ackSent) { // beklenen ack gelmedi, demek ki hata olmuş
                std::cout << "Gonderici: Frame " << currentFrame << " gonderilirken bir hata olusmus. Frame tekrar gonderiliyor...\n";
            }
            else { // beklenen ack geldi, demek ki bir sorun yok
                std::cout << "Gonderici: Frame " << currentFrame << " basarili bir sekilde gonderilmis. Siradaki frame'e geciliyor...\n\n";
                expectedAck = !expectedAck;
                currentFrame++;
            }
        }

    }

    std::cout << "Tum frameler basariyla gonderildi. Checksum frame'i gonderiliyor...\n\n";
    boolean checksumSent = false;
    while(!checksumSent){
        std::cout << "Gonderici: Checksum gonderiliyor...\n";
        std::vector<bool> checksumCopy = checksumFrame;
        /*if(simulate_checksum_error()){
            int checksumCorruptedIndex = rand() % checksumCopy.size();
            checksumCopy[checksumCorruptedIndex] = !checksumCopy[checksumCorruptedIndex];
        }*/
        std::vector<bool> receivedChecksum(checksumCopy.begin() + 4, checksumCopy.end());
        uint16_t receivedChecksumValue = 0;
        for(bool bit: receivedChecksum){
            receivedChecksumValue = (receivedChecksumValue << 1) | bit;
        }

        //std::cout << "Received: " << receivedChecksumValue << "\n";
        uint16_t computedChecksum = compute_checksum(framesCopy);
        uint32_t total = computedChecksum + receivedChecksumValue;
        //std::cout << "Total: " << total << "\n";
        if((total & 0xFFFF) == 0x0000){
            std::cout << "Alici: Gonderilen checksum dogru. Gonderim tamamlandi.\n";
            checksumSent = true;
        }
        else {
            std::cout << "Alici: Checksum hatali. Tekrar gonderim yapilacak...\n";
            std::cout << "Received checksum: " << receivedChecksumValue << " --- computed checksum: " << computedChecksum << "Total: " << total << "\n";
        }
    }


}





// parcala_ve_kaydet: Dosyayı okuyup bit dizisine dönüştürür, DLE-STX-ETX ile çevrelenmiş frame'ler üretir
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



// Ana fonksiyon: dosya seçme ve frame'leri ekrana yazdırma
int main() {
    OPENFILENAME ofn;
    char szFile[260] = { 0 };
    ZeroMemory(&ofn, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = NULL;
    ofn.lpstrFile = szFile;
    ofn.nMaxFile = sizeof(szFile);
    ofn.lpstrFilter = ".dat dosyasi seciniz (*.dat)\0*.dat\0tum dosyalar (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

    if (GetOpenFileName(&ofn) == TRUE) {
        std::cout << "Secilen dosya: " << ofn.lpstrFile << std::endl;
        auto sonuc = parcala_ve_kaydet(ofn.lpstrFile);
        int i = 1;
        for (const auto& parca : sonuc) {
            printf("Parca %5d : ", i++);
            for (bool bit : parca) {
                printf("%d", bit);
            }
            printf("\n");
        }

        std::vector<bool> checksumFrame = create_checksum_frame(sonuc); // create the checksum frame
        std::cout << "Checksum parcasi: ";
        for(bool bit: checksumFrame)
            std::cout << bit;
        std::cout << std::endl;

        transmission(sonuc, checksumFrame);

    } else {
        std::cout << "Dosya secilemedi veya islem iptal edildi." << std::endl;
    }




    return 0;
}
