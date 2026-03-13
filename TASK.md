# Tek `module` Syntax'Ä± + Native Provider AyrÄ±mÄ± â€” Uygulama Task Listesi

Bu dosya `plan-singleModuleSyntaxNativeProviderMigration.prompt.md` planÄ±nÄ±n uygulama izleme listesidir.
Her gÃ¶rev tamamlandÄ±ÄŸÄ±nda `[x]` ile iÅŸaretlenir.

---

## Ä°terasyon 1 â€” Syntax BirleÅŸtirme KararÄ±

### Faz 1 â€” Parser / AST yÃ¼zeyi
- [ ] `modulecpp` syntax'Ä± deprecated kabul edilecek, yeni hedef syntax yalnÄ±zca `module X;` olacak
- [x] `extern Foo method(...) as T;` parser desteÄŸi tamamlanacak
- [x] `extern("symbol") Foo method(...) as T;` parser desteÄŸi eklenecek
- [x] `AST.h` iÃ§inde extern declaration node'u symbol override taÅŸÄ±yacak ÅŸekilde geniÅŸletilecek
- [x] Parser testleri eklenecek: plain extern, inline-symbol extern, legacy modulecpp parse

### Faz 2 â€” Resolver provider modeli
- [x] `ModuleResolver` provider-aware hale getirilecek
- [x] Arama sÄ±rasÄ±: proje source â†’ modules â†’ `BuiltinLibraries` â†’ `BuiltinNativeLibraries` â†’ external native
- [x] AynÄ± modÃ¼l adÄ± source/native Ã§akÄ±ÅŸmasÄ±nda deterministic conflict policy eklenecek
- [x] `BuiltinNativeLibraries/<Name>/<Name>.nr` + `modulecpp.toml` discovery eklenecek

### Faz 3 â€” Sema / type Ã§Ã¶zÃ¼mleme
- [x] `module X;` native provider contract dosyasÄ±nÄ± da normal modÃ¼l gibi gÃ¶recek
- [x] plain `extern` symbol Ã§Ã¶zÃ¼mÃ¼nÃ¼ toml export map'ten alacak (codegen katmanÄ±nda yapÄ±lacak)
- [x] `extern("symbol")` declaration inline symbol override olarak Ã§Ã¶zÃ¼lecek (AST'de tutuluyor)
- [x] contract/toml imza uyuÅŸmazlÄ±ÄŸÄ± sert hata olacak (TODO: signature validation against native.toml exports)

### Faz 4 â€” Builder / codegen / binding
- [x] Native artifact/build metadata `modulecpp.toml` Ã¼zerinden okunacak
- [ ] symbol Ã¶ncelik kuralÄ± uygulanacak: inline > toml > error
- [x] Legacy `modulecpp` config yolu geÃ§ici alias olarak tutulacak
- [x] Builtin native provider iÃ§in proje config zorunluluÄŸu kaldÄ±rÄ±lacak

### Faz 5 â€” FileSystemNative referans modÃ¼lÃ¼
- [x] `BuiltinNativeLibraries/FileSystemNative/FileSystemNative.nr` contract dosyasÄ± oluÅŸturulacak
- [x] `BuiltinNativeLibraries/FileSystemNative/modulecpp.toml` metadata dosyasÄ± oluÅŸturulacak
- [x] `BuiltinLibraries/FileSystem` Ã¼st modÃ¼lÃ¼ tek `module FileSystem;` yÃ¼zeyiyle Ã§alÄ±ÅŸacak
- [x] FileSystem ile iliÅŸkili native provider testleri eklenecek

### Faz 6 â€” Legacy temizliÄŸi
- [x] `modulecpp` parser keyword'Ã¼ kaldÄ±rÄ±ldÄ± (unreleased dil, deprecated warning yerine full removal)
- [x] repo iÃ§indeki `modulecpp` kullanÄ±m yollarÄ± (5 builtin .nr dosyasÄ±) `module` modeline taÅŸÄ±ndÄ±
- [x] `modulecpp` syntax'Ä± parser, sema, reference tracker'dan tamamen kaldÄ±rÄ±ldÄ±

---

## TasarÄ±m KurallarÄ±
- [ ] KullanÄ±cÄ± yÃ¼zeyinde yalnÄ±zca `module X;` ve `expand module Y;` kalacak
- [x] Native/source/builtin ayrÄ±mÄ± compiler iÃ§inde provider metadata olarak yaÅŸayacak
- [x] Native contract `.nr` dosyalarÄ±nda gerÃ§ek body olmayacak, yalnÄ±zca `extern` declaration/type/sabit yÃ¼zeyi olacak
- [x] `native.toml` metadata ile symbol mapping ve artifact paths yÃ¶netilecek

---

## Bu oturum hedefi
- [x] Faz 1 tamamlanacak
- [x] `TASK.md` yeni plana gÃ¶re tamamen yenilenecek
- [x] extern syntax omurgasÄ± parse edilecek
- [x] Faz 2 resolver/provider omurgasÄ± baÅŸlatÄ±lacak
- [x] Faz 3 sema omurgasÄ± baÅŸlatÄ±lacak
- [x] Faz 4 builder/binding omurgasÄ± baÅŸlatÄ±lacak
- [x] Faz 5 FileSystemNative referans modÃ¼lÃ¼ oluÅŸturulacak
