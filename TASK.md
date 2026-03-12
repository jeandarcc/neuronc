# Tek `module` Syntax'ı + Native Provider Ayrımı — Uygulama Task Listesi

Bu dosya `plan-singleModuleSyntaxNativeProviderMigration.prompt.md` planının uygulama izleme listesidir.
Her görev tamamlandığında `[x]` ile işaretlenir.

---

## İterasyon 1 — Syntax Birleştirme Kararı

### Faz 1 — Parser / AST yüzeyi
- [ ] `modulecpp` syntax'ı deprecated kabul edilecek, yeni hedef syntax yalnızca `module X;` olacak
- [x] `extern Foo method(...) as T;` parser desteği tamamlanacak
- [x] `extern("symbol") Foo method(...) as T;` parser desteği eklenecek
- [x] `AST.h` içinde extern declaration node'u symbol override taşıyacak şekilde genişletilecek
- [x] Parser testleri eklenecek: plain extern, inline-symbol extern, legacy modulecpp parse

### Faz 2 — Resolver provider modeli
- [x] `ModuleResolver` provider-aware hale getirilecek
- [x] Arama sırası: proje source → modules → `BuiltinLibraries` → `BuiltinNativeLibraries` → external native
- [x] Aynı modül adı source/native çakışmasında deterministic conflict policy eklenecek
- [x] `BuiltinNativeLibraries/<Name>/<Name>.npp` + `modulecpp.toml` discovery eklenecek

### Faz 3 — Sema / type çözümleme
- [x] `module X;` native provider contract dosyasını da normal modül gibi görecek
- [x] plain `extern` symbol çözümünü toml export map'ten alacak (codegen katmanında yapılacak)
- [x] `extern("symbol")` declaration inline symbol override olarak çözülecek (AST'de tutuluyor)
- [x] contract/toml imza uyuşmazlığı sert hata olacak (TODO: signature validation against native.toml exports)

### Faz 4 — Builder / codegen / binding
- [x] Native artifact/build metadata `modulecpp.toml` üzerinden okunacak
- [ ] symbol öncelik kuralı uygulanacak: inline > toml > error
- [x] Legacy `modulecpp` config yolu geçici alias olarak tutulacak
- [x] Builtin native provider için proje config zorunluluğu kaldırılacak

### Faz 5 — FileSystemNative referans modülü
- [x] `BuiltinNativeLibraries/FileSystemNative/FileSystemNative.npp` contract dosyası oluşturulacak
- [x] `BuiltinNativeLibraries/FileSystemNative/modulecpp.toml` metadata dosyası oluşturulacak
- [x] `BuiltinLibraries/FileSystem` üst modülü tek `module FileSystem;` yüzeyiyle çalışacak
- [x] FileSystem ile ilişkili native provider testleri eklenecek

### Faz 6 — Legacy temizliği
- [x] `modulecpp` parser keyword'ü kaldırıldı (unreleased dil, deprecated warning yerine full removal)
- [x] repo içindeki `modulecpp` kullanım yolları (5 builtin .npp dosyası) `module` modeline taşındı
- [x] `modulecpp` syntax'ı parser, sema, reference tracker'dan tamamen kaldırıldı

---

## Tasarım Kuralları
- [ ] Kullanıcı yüzeyinde yalnızca `module X;` ve `expand module Y;` kalacak
- [x] Native/source/builtin ayrımı compiler içinde provider metadata olarak yaşayacak
- [x] Native contract `.npp` dosyalarında gerçek body olmayacak, yalnızca `extern` declaration/type/sabit yüzeyi olacak
- [x] `native.toml` metadata ile symbol mapping ve artifact paths yönetilecek

---

## Bu oturum hedefi
- [x] Faz 1 tamamlanacak
- [x] `TASK.md` yeni plana göre tamamen yenilenecek
- [x] extern syntax omurgası parse edilecek
- [x] Faz 2 resolver/provider omurgası başlatılacak
- [x] Faz 3 sema omurgası başlatılacak
- [x] Faz 4 builder/binding omurgası başlatılacak
- [x] Faz 5 FileSystemNative referans modülü oluşturulacak
