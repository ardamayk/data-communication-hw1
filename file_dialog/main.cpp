// Eğer kod çalışmazsa lcomdlg32 kütüphanesini project ---> build options ---> linker settings ---> add ---> comdlg32 (kütüphane)kütüphanesini yazıp ekleyin


#include <windows.h>    // Windows API fonksiyonları için
#include <commdlg.h>    // GetOpenFileName gibi ortak diyalog kutuları için
#include <iostream>     // std::cout gibi C++ çıktı işlemleri için

int main()
{
    OPENFILENAME ofn;         // Dosya seçme kutusu için yapı (struct)
    char szFile[260] = { 0 }; // Seçilen dosyanın yolunu tutacak karakter dizisi (maksimum 260 karakter)

    // Yapıyı sıfırla
    ZeroMemory(&ofn, sizeof(ofn));

    ofn.lStructSize = sizeof(ofn);     // Yapının boyutu
    ofn.hwndOwner = NULL;              // Bu pencerenin sahibi yok (konsol uygulaması)
    ofn.lpstrFile = szFile;            // Seçilen dosyanın yolunu buraya yazacak
    ofn.nMaxFile = sizeof(szFile);     // Dosya yolunun maksimum uzunluğu
    ofn.lpstrFilter =                 // Filtre: sadece .dat veya tüm dosyaları göster
        ".dat dosyasi seciniz (*.dat)\0*.dat\0tum dosyalar (*.*)\0*.*\0";
    ofn.nFilterIndex = 1;              // İlk filtre varsayılan olarak seçili
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;  // Sadece var olan dosya ve klasörler seçilebilsin

    // Dosya seçme penceresini aç
    if (GetOpenFileName(&ofn) == TRUE)
    {
        // Kullanıcı bir dosya seçtiyse, seçilen dosya yolunu yazdır
        std::cout << "secilen dosya: " << ofn.lpstrFile << std::endl;
    }
    else
    {
        // Kullanıcı pencereyi kapattıysa veya iptal ettiyse
        std::cout << "Dosya secilemedi veya islem iptal edildi." << std::endl;
    }

    return 0;
}
