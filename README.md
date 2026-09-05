# VaultSecurizat
Descriere: O aplicație securizată de tip Vault care permite accesul la fișiere criptate de pe
disc doar după o dublă verificare: o parolă master criptată cu hashing puternic (SHA-256)
și recunoaștere facială locală prin camera web.

 Tehnologii folosite

| Componentă | Bibliotecă | Rol |
|---|---|---|
| Hashing parolă | **Crypto++** (`SHA256`) | Calculează amprenta criptografică a parolei, fără a o stoca vreodată în clar |
| Criptare/decriptare fișiere | **Crypto++** (`AES` + `GCM`) | Criptează/decriptează directorul cu vaultul, folosind AES-256 în modul GCM (criptare autentificată) |
| Derivare cheie din parolă | **Crypto++** (`PKCS5_PBKDF2_HMAC<SHA256>`) | Transformă parola utilizatorului într-o cheie AES validă, folosind un salt unic și 1000 de iterații |
| Detecție facială | **OpenCV** (`FaceDetectorYN`, modelul YuNet) | Localizează fața în cadrul livrat de cameră |
| Recunoaștere facială | **OpenCV** (`absdiff` + `mean`) | Compară fața curentă cu fețele înregistrate, calculând un scor de diferență |
| Interfață grafică | **Qt6 Widgets** | Ferestre, câmpuri de text, listă de fișiere, feed video live de la cameră |
| Parcurgere fișiere/directoare | **C++17** (`std::filesystem`) | Listează și procesează fișierele din directoarele vaultului |
| Mediu de compilare | **MSYS2 / MinGW-w64 (64-bit)** + **Code::Blocks** | Toolchain-ul folosit pentru compilare pe Windows |


 Cum funcționează (flux complet)

### La prima rulare (înregistrare)

1. Nu există nicio parolă salvată → utilizatorul introduce o parolă nouă
2. Parola este trecută prin **SHA-256**, iar hash-ul rezultat este salvat în `all.txt` (parola în sine nu este niciodată stocată)
3. Camera pornește și capturează **10 imagini** cu fața utilizatorului, care sunt salvate în `face_model.yml`
4. Se generează:
   - un **IV** (Initialization Vector) aleator, salvat în `iv.bin`
   - un **salt** aleator, salvat în `salt.bin`
5. Din parolă + salt, se derivă o **cheie AES-256**, folosind PBKDF2 (1000 de iterații)
6. Toate fișierele din directorul `vault_test/` sunt criptate cu **AES-GCM**, iar rezultatele sunt salvate în `vault_encrypted/`, cu extensia `.enc`

### La rulările următoare (autentificare)

1. Utilizatorul introduce parola → se recalculează hash-ul SHA-256 și se compară cu cel salvat în `all.txt`
2. Dacă parola este corectă, camera pornește și capturează fața curentă
3. Fața curentă este comparată cu cele **10 fețe** înregistrate anterior; se calculează un **scor mediu de diferență**
4. Dacă scorul este sub un prag de recunoaștere (`PRAG_RECUNOASTERE`), accesul este permis
5. Se derivă din nou cheia AES (aceeași parolă + același salt salvat = aceeași cheie), iar fișierele din `vault_encrypted/` sunt **decriptate** în `vault_decriptat/`
6. Interfața grafică afișează lista fișierelor decriptate, care pot fi deschise direct cu dublu-click (folosind aplicația implicită din Windows)
7. La apăsarea butonului **„Închide vault”**, fișierele decriptate sunt șterse de pe disc, iar utilizatorul este întors la ecranul de autentificare



## Structura fișierelor generate de aplicație

| Fișier / Folder | Conținut |
|---|---|
| `all.txt` | Hash-ul SHA-256 al parolei |
| `iv.bin` | Initialization Vector, folosit de AES-GCM |
| `salt.bin` | Salt-ul folosit la derivarea cheii din parolă |
| `face_model.yml` | Cele 10 capturi faciale înregistrate |
| `vault_test/` | Fișierele originale, necriptate, puse manual de utilizator |
| `vault_encrypted/` | Fișierele criptate (`.enc`), rezultate din `vault_test/` |
| `vault_decriptat/` | Fișierele decriptate, disponibile temporar după autentificare reușită |
| `face_detection_yunet_2026may.onnx` | Modelul pre-antrenat folosit pentru detecția feței |



## Instalare și compilare (Windows, MSYS2 + Code::Blocks)

### 1. Instalarea mediului

Instalează [MSYS2](https://www.msys2.org), apoi deschide terminalul **MSYS2 MINGW64** și rulează:

```bash
pacman -Syu
pacman -S mingw-w64-x86_64-gcc
pacman -S mingw-w64-x86_64-crypto++
pacman -S mingw-w64-x86_64-opencv
pacman -S mingw-w64-x86_64-qt6-base
```

> **Important:** toate pachetele trebuie instalate cu prefixul `mingw-w64-x86_64-` (mediul **MINGW64**), nu `mingw-w64-ucrt-x86_64-` (mediul UCRT64) — cele două medii nu sunt compatibile binar între ele.

### 2. Configurarea în Code::Blocks

- **Compiler**: `GNU GCC MSYS MinGW64 Compiler`, cu directorul de instalare `C:\msys64\mingw64`
- **Search Directories → Compiler**:
  - `C:\msys64\mingw64\include`
  - `C:\msys64\mingw64\include\opencv4`
  - `C:\msys64\mingw64\include\qt6`
  - `C:\msys64\mingw64\include\qt6\QtWidgets`
  - `C:\msys64\mingw64\include\qt6\QtGui`
  - `C:\msys64\mingw64\include\qt6\QtCore`
- **Search Directories → Linker**: `C:\msys64\mingw64\lib`
- **Linker Settings → Link libraries**: `cryptopp`, bibliotecile OpenCV necesare (`opencv_core`, `opencv_imgproc`, `opencv_videoio`, `opencv_objdetect`, `opencv_highgui`), bibliotecile Qt6 (`Qt6Widgets`, `Qt6Gui`, `Qt6Core`)
- **Compiler Flags**: activează standardul **C++17** (necesar pentru `std::filesystem` și pentru Qt6)

### 3. Compilare și rulare

Deschide proiectul în Code::Blocks și apasă **Build and run** (F9).



## Rularea aplicației ca program independent (fără Code::Blocks)

Pentru ca executabilul (`.exe`) să ruleze de sine stătător (dublu-click, fără Code::Blocks deschis), trebuie ca, în același folder cu el, să existe:

- toate fișierele `.dll` din `C:\msys64\mingw64\bin`
- un subfolder **`platforms/`**, care să conțină `qwindows.dll` (plugin-ul de platformă necesar pentru Qt6 pe Windows)
- fișierul modelului: `face_detection_yunet_2026may.onnx`



## Limitări cunoscute

- **Metoda de recunoaștere facială** este una simplă (diferență medie de pixeli între imagini în tonuri de gri, `absdiff` + `mean`), nu o rețea neuronală de recunoaștere facială propriu-zisă. Este sensibilă la condiții de iluminare și unghi diferite față de momentul înregistrării, iar pragul de recunoaștere a fost calibrat empiric.
- **Salt-ul și IV-ul** sunt generate o singură dată pentru întregul director, nu individual pentru fiecare fișier — o variantă mai riguroasă ar folosi un IV unic per fișier.
- Nu există un mecanism de **resetare a parolei** dacă aceasta este uitată (parola nu poate fi recuperată din hash, prin design).
- Bibliotecile externe (Crypto++, OpenCV, Qt6) trebuie distribuite împreună cu executabilul (fișiere `.dll`), deoarece sunt legate dinamic.



## Autori

Proiect realizat în cadrul temei **„Sistem de Autentificare Biometrică și Criptografică”**:
- **Partea de criptografie** (hashing SHA-256, criptare/decriptare AES-GCM, derivare cheie PBKDF2) și **interfața grafică Qt6**
- **Partea de recunoaștere facială** (detecție OpenCV/YuNet, înregistrare și comparare fețe)
