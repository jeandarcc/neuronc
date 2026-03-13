Builtin FileSystem kalan placeholder alanları ve dile eklenmesi gerekenler

Bu kütüphanede modulecpp ile kapatılabilen alanlar artık `FileSystemNative` üzerinden native-backed hale getirildi:
- File.Exists / Delete / Copy / Move
- File.ReadAllText / WriteAllText / AppendAllText
- Directory.Exists / Create / Delete
- Watch.Start / Poll / Stop

Aşağıdakiler ise hâlâ gerçek anlamda dil/compiler eksiği olduğu için placeholder kaldı veya zayıf durumda.

1. Güçlü string işleme
- Path.GetExtension, GetFileName, GetStem, GetParent, ChangeExtension gibi API'ler için:
  - string length
  - string indexing
  - string slicing
  - lastIndexOf / indexOf
  - replace
  gerekir.
- Şu an bu yüzden Path inspect API'leri placeholder bırakıldı.

2. Platform bilgisi ve separator politikası
- Tek bir Separator() yetmiyor.
- Compiler/runtime tarafında platform bilgisi veya canonical path policy gerekli.
- Windows drive harfi, UNC path, mixed separators için net model lazım.
- Modulecpp host çağrılarını yapabilir ama dil yüzeyinin platforma karşı nasıl davranacağı yine de tasarlanmalı.

3. Result / error taşıma modeli
- FileSystem API'lerinde bool dönmek zayıf.
- Result<T, E> veya en azından tuple dönüşü dili ciddi biçimde güçlendirir.
- Aksi halde hatayı kaybetme riski var.
- Modulecpp çağrıları çalışsa bile hata bilgisini taşıyacak tip sistemi eksik.

4. Array ve collection ergonomisi
- ListFiles / ListDirectories gibi API'ler için yalnızca Array<string> dönmek yetmez.
- iteration, append/push, count/length, map/filter gibi yüzeyler gerek.
- Bu yüzden enumeration API'leri placeholder kaldı.

5. Watch / event stream modeli
- Watch API'nin native tarafı modulecpp ile kapatılabilir, ama event tipi hâlâ zayıf.
- String dönmek geçici çözüm; gerçek model için struct/enum tabanlı event nesnesi lazım.
- Queue, iteration ve non-blocking poll davranışı da resmi olarak tanımlanmalı.

6. Static module object ergonomisi
- File.ReadAllText, Directory.Exists, Path.Join gibi kullanımın doğal kalması için module-object semantics daha da netleşmeli.
- Özellikle alt modül façade zinciri ile tek object yüzeyi arasındaki davranış resmi olarak tarif edilmeli.

7. Bytes / binary buffer modeli
- Array<int> geçici çözüm.
- Gerçek binary I/O için Byte, Span, Buffer veya Array<byte> benzeri türler gerekiyor.
- Bu yüzden ReadAllBytes / WriteAllBytes tarafı hâlâ placeholder.

8. Exceptions veya standard error contract
- Native-backed FileSystem çağrılarında permission denied, not found, locked, invalid path gibi durumlar olacak.
- Ya exception standardı ya da Result standardı gerekli.

9. Overload / variadic ergonomisi
- Path.Join için Join2/Join3 yerine variadic veya params benzeri mekanizma ciddi kalite artışı sağlar.

10. Metadata ve rich info nesneleri
- FileInfo / DirectoryInfo / PathInfo tipleri var ama native doldurma stratejisi ve field standardı net değil.
- Result/error modeliyle birlikte düşünülmeli.
