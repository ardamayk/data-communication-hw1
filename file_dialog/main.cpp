// Eğer kod çalışmazsa lcomdlg32 kütüphanesini project ---> build options ---> linker settings ---> add ---> comdlg32 veya lcomdlg32 (kütüphane)kütüphanesini yazıp ekleyin

#include <windows.h>     // Windows API fonksiyonları için gerekli başlık dosyası
#include <commdlg.h>     // Ortak diyalog kutuları (örneğin, dosya açma) için gerekli başlık dosyası
#include <iostream>      // Standart giriş/çıkış işlemleri için gerekli başlık dosyası
#include <fstream>       // Dosya okuma/yazma işlemleri için gerekli başlık dosyası
#include <vector>        // Dinamik diziler (vector) için gerekli başlık dosyası
#include <cstdio>        // C tarzı giriş/çıkış fonksiyonları için gerekli başlık dosyası (printf gibi)


// Belirtilen dosya yolundaki dosyayı okur, içeriğini bitlere ayırır ve her biri en fazla 100 bitlik parçalar halinde bir vektör içinde saklar.
std::vector<std::vector<bool>> parcala_ve_kaydet(const std::string& dosya_yolu);
bool simulate_frame_loss();
bool simulate_frame_corrupt();
bool simulate_ack_loss();
bool simulate_checksum_error();
std::vector<bool> compute_crc16(std::vector<bool>& bits);
uint16_t compute_checksum(const std::vector<std::vector<bool>>& frames);
std::vector<bool> create_checksum_frame(std::vector<std::vector<bool>>& frames);

int main()
{
    OPENFILENAME ofn;         // Dosya açma diyalog kutusunun özelliklerini tutan yapı
    char szFile[260] = { 0 }; // Seçilen dosyanın tam yolunu saklamak için ayrılmış karakter dizisi (260 karakter maksimum)

    // OPENFILENAME yapısını sıfırlar, böylece varsayılan değerlerle başlar.
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);     // Yapının boyutunu belirtir.
    ofn.hwndOwner = NULL;              // Diyalog kutusunun sahibi olan pencere. Konsol uygulaması olduğu için NULL.
    ofn.lpstrFile = szFile;            // Seçilen dosyanın yolunun yazılacağı arabelleğin (szFile) adresini gösterir.
    ofn.nMaxFile = sizeof(szFile);     // lpstrFile arabelleğinin maksimum boyutunu belirtir.
    ofn.lpstrFilter =                 // Dosya türü filtrelerini tanımlar. Kullanıcıya gösterilecek seçenekler.
        ".dat dosyasi seciniz (*.dat)\0*.dat\0tum dosyalar (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;              // Varsayılan olarak seçili olan filtre indeksini belirtir (burada ".dat dosyasi seciniz").
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST; // Ayarlar: Sadece var olan dosya yolları ve dosyalar seçilebilir.

    // Ortak dosya açma diyalog kutusunu görüntüler.
    if (GetOpenFileName(&ofn) == TRUE)
    {
        // Kullanıcı bir dosya seçtiyse (GetOpenFileName TRUE döndürdüyse), seçilen dosyanın tam yolunu ekrana yazdırır.
        std::cout << "secilen dosya: " << ofn.lpstrFile << std::endl;
    }
    else
    {
        // Kullanıcı dosya seçme penceresini kapattıysa veya "İptal" düğmesine tıkladıysa.
        std::cout << "Dosya secilemedi veya islem iptal edildi." << std::endl;
    }

    // Seçilen dosyanın yolunu kullanarak dosyayı okur ve içeriğini 100 bitlik (veya daha az) parçalar halinde bir vektör içinde saklar.
    auto sonuc = parcala_ve_kaydet(ofn.lpstrFile);
    int i = 1; // Parça numarasını tutacak sayaç değişkeni.
    // Elde edilen parçalar üzerinde döngü.
    for (const auto& parca : sonuc) {
        printf("Parca %5d : ", i++); // "Parca" kelimesini ve parça numarasını (sağa dayalı 5 karakter genişliğinde) yazdırır.
        // Mevcut parçadaki her bir biti döngüyle yazdırır.
        for (bool bit : parca) {
            printf("%d", bit); // Biti (0 veya 1 olarak) yazdırır.
        }
        printf("\n"); // Bir sonraki parça için yeni bir satıra geçer.
    }
    std::vector<bool> checksumFrame = create_checksum_frame(sonuc); // create the checksum frame
    std::cout << "Checksum parcasi: ";
    for(bool bit: checksumFrame)
        std::cout << bit;
    std::cout << std::endl;

    return 0; // Programın başarıyla sonlandığını belirtir.
}





// Verilen dosyanın içeriğini bitlere ayırır ve her biri 100 bitlik parçalar halinde bir vektör içinde saklar.
std::vector<std::vector<bool>> parcala_ve_kaydet(const std::string& dosya_yolu) {
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
// hata olasılıkları
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
    uint16_t crc;
    size_t i;
    for(const auto& frame: frames){
        crc = 0;
        for(i = frame.size() - 16; i < frame.size(); i++){
            crc = (crc << 1) | frame[i];
        }
        sum += crc;
    }
    sum += 1;
    return static_cast<uint16_t>(sum);
}

std::vector<bool> create_checksum_frame(std::vector<std::vector<bool>>& frames){
    uint16_t checksum;
    uint16_t checksumComplement;
    int i;

    checksum = compute_checksum(frames);
    checksumComplement = ~checksum;

    std::vector<bool> frame;
    std::vector<bool> header = {1, 0, 1, 0}; // Example 4-bit header for transparency
    frame.insert(frame.end(), header.begin(), header.end());

    for(i= 15; i>= 0; i--)
        frame.push_back((checksumComplement >> i) & 1);

    return frame;
}









