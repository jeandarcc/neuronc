# Builtin FileSystem Library

Bu kütüphane `builtin_libraries/FileSystem` altında modüler olarak düzenlenmiştir.

## Yapı
- `Core/`: separator ve mod sabitleri
- `Path/`: path yardımcıları
- `Info/`: metadata tipleri
- `File/`: dosya işlemleri
- `Directory/`: klasör işlemleri
- `Watch/`: file watch yüzeyi

## Durum
- `Core` ve bazı `Path` helper'ları mevcut dille yazılabildiği kadar gerçeklenmiştir.
- Gerçek filesystem erişimi isteyen API'ler şu anda placeholder durumundadır.
- Placeholder nedenleri ve dil/compiler tarafında gereken eksikler için `LANGUAGE_GAPS.md` dosyasına bakın.

