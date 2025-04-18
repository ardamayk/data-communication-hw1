// parcala_ve_kaydet: Bir dosyadan bitleri okuyup, DLE-STX ile başlayan ve DLE-ETX ile biten frame'lere ayırır
// Her frame 100 bitlik veri içerir, DLE ile aynı 8 bitlik bloklar varsa stuffing yapılır (başına DLE eklenir)
#include <fstream> // Dosya okuma işlemleri için gerekli kütüphane
#include <iostream> // Ekrana yazdırma işlemleri için gerekli kütüphane
#include <vector> // Dinamik dizi (bit dizisi ve frame matrisleri) için gerekli kütüphane
#include <windows.h> // Windows API (dosya seçme penceresi için)
#include <commdlg.h> // GetOpenFileName fonksiyonu için

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
    const std::vector<bool> DLE = {0,1,0,0,0,0,0,1};
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
    } else {
        std::cout << "Dosya secilemedi veya islem iptal edildi." << std::endl;
    }
    return 0;
}
