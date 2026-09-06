#include <iostream>
#include <cryptopp/aes.h>
#include <cryptopp/gcm.h>
#include <cryptopp/osrng.h>
#include <cryptopp/hex.h>
#include <cryptopp/files.h>
#include <fstream>
#include <cryptopp/pwdbased.h>
#include <filesystem>
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/videoio.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/objdetect/face.hpp>
#include <vector>
#include <string>

#include <QApplication>
#include <QWidget>
#include <QStackedWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QListWidget>
#include <QTimer>
#include <QImage>
#include <QPixmap>
#include <QMessageBox>
#include <QDesktopServices>
#include <QUrl>
#include <QFileInfo>

using namespace std;
using namespace CryptoPP;
namespace fs = std::filesystem;


//  HASHING PAROLA


bool existaParola()
{
    ifstream verificare("all.txt");
    return (bool)verificare;
}

void salveazaParolaGUI(const string& parola2)
{
    string hashhex;
    SHA256 hash;
    StringSource(parola2, true,
        new HashFilter(hash,
            new HexEncoder(
                new StringSink(hashhex)
            )
        )
    );
    ofstream fisier("all.txt");
    fisier << hashhex;
    fisier.close();
}

bool verificaParola(const string& parola)
{
    ifstream fisier("all.txt");
    string hashhex2;
    fisier >> hashhex2;
    fisier.close();

    string hashhex;
    SHA256 hash;
    StringSource(parola, true,
        new HashFilter(hash,
            new HexEncoder(
                new StringSink(hashhex)
            )
        )
    );

    return hashhex == hashhex2;
}

//  CRIPTARE/DECRIPTARE DIRECTOR


void criptareDirector(const string& parola)
{
    CryptoPP::byte iv[AES::BLOCKSIZE];
    AutoSeededRandomPool rng;
    rng.GenerateBlock(iv, sizeof(iv));
    ofstream fisier("iv.bin", ios::binary);
    fisier.write((char*)iv, sizeof(iv));
    fisier.close();

    CryptoPP::byte cheie[AES::DEFAULT_KEYLENGTH];
    CryptoPP::byte salt[16];
    rng.GenerateBlock(salt, sizeof(salt));

    ofstream fisierSalt("salt.bin", ios::binary);
    fisierSalt.write((char*)salt, sizeof(salt));
    fisierSalt.close();

    PKCS5_PBKDF2_HMAC<SHA256> helper;
    helper.DeriveKey(cheie, sizeof(cheie), 0,
        (const CryptoPP::byte*)parola.data(), parola.size(),
        salt, sizeof(salt), 1000);

    fs::create_directories("vault_test");
    fs::create_directories("vault_encrypted");

    for (const auto& entry : fs::directory_iterator("vault_test"))
    {
        if (entry.is_regular_file())
        {
            string caleFisier = entry.path().string();
            string numeFisier = entry.path().filename().string();
            string caleDestinatie = "vault_encrypted/" + numeFisier + ".enc";

            GCM<AES>::Encryption aux;
            aux.SetKeyWithIV(cheie, sizeof(cheie), iv);

            FileSource(caleFisier.c_str(), true,
                new AuthenticatedEncryptionFilter(aux,
                    new FileSink(caleDestinatie.c_str())
                )
            );
        }
    }
}

void decriptareDirector(const string& parola)
{
    CryptoPP::byte iv[AES::BLOCKSIZE];
    ifstream citireIv("iv.bin", ios::binary);
    citireIv.read((char*)iv, sizeof(iv));
    citireIv.close();

    CryptoPP::byte salt[16];
    ifstream citireSalt("salt.bin", ios::binary);
    citireSalt.read((char*)salt, sizeof(salt));
    citireSalt.close();

    CryptoPP::byte cheie[AES::DEFAULT_KEYLENGTH];
    PKCS5_PBKDF2_HMAC<SHA256> helper;
    helper.DeriveKey(cheie, sizeof(cheie), 0,
        (const CryptoPP::byte*)parola.data(), parola.size(),
        salt, sizeof(salt), 1000);

    fs::create_directories("vault_decriptat");

    for (const auto& entry : fs::directory_iterator("vault_encrypted"))
    {
        if (entry.path().extension().string() == ".enc")
        {
            string caleFisier2 = entry.path().string();
            string numeOriginal = entry.path().stem().string();
            string caleDecriptat = "vault_decriptat/" + numeOriginal;

            GCM<AES>::Decryption aux2;
            aux2.SetKeyWithIV(cheie, sizeof(cheie), iv);

            FileSource(caleFisier2.c_str(), true,
                new AuthenticatedDecryptionFilter(aux2,
                    new FileSink(caleDecriptat.c_str())
                )
            );
        }
    }
}

//  RECUNOASTERE FACIALA


const std::string MODEL = "face_detection_yunet_2026may.onnx";
const std::string FISIER_FATA = "face_model.yml";
const int FACE_WIDTH = 200;
const int FACE_HEIGHT = 200;
const int NUMAR_CAPTURI = 10;
const double PRAG_RECUNOASTERE = 40.0;

cv::VideoCapture camera;
cv::Ptr<cv::FaceDetectorYN> detector;
std::vector<cv::Mat> feteInregistrate;
bool utilizatorInregistrat = false;

bool extrageFata(const cv::Mat& frame, cv::Mat& fata, cv::Rect* dreptunghiIesire = nullptr)
{
    detector->setInputSize(frame.size());

    cv::Mat faces;
    detector->detect(frame, faces);

    if (faces.empty())
        return false;

    float x = faces.at<float>(0, 0);
    float y = faces.at<float>(0, 1);
    float w = faces.at<float>(0, 2);
    float h = faces.at<float>(0, 3);

    cv::Rect dreptunghi(
        static_cast<int>(x),
        static_cast<int>(y),
        static_cast<int>(w),
        static_cast<int>(h)
    );
    dreptunghi &= cv::Rect(0, 0, frame.cols, frame.rows);

    if (dreptunghi.width <= 0 || dreptunghi.height <= 0)
        return false;

    if (dreptunghiIesire)
        *dreptunghiIesire = dreptunghi;

    fata = frame(dreptunghi).clone();
    cv::cvtColor(fata, fata, cv::COLOR_BGR2GRAY);
    cv::resize(fata, fata, cv::Size(FACE_WIDTH, FACE_HEIGHT));
    cv::equalizeHist(fata, fata);

    return true;
}

double comparaFete(const cv::Mat& fata1, const cv::Mat& fata2)
{
    cv::Mat diferenta;
    cv::absdiff(fata1, fata2, diferenta);
    cv::Scalar medie = cv::mean(diferenta);
    return medie[0];
}

bool salveazaFete()
{
    cv::FileStorage fisier(FISIER_FATA, cv::FileStorage::WRITE);
    if (!fisier.isOpened())
        return false;

    fisier << "fete" << "[";
    for (size_t i = 0; i < feteInregistrate.size(); i++)
        fisier << feteInregistrate[i];
    fisier << "]";
    fisier.release();
    return true;
}

bool incarcaFete()
{
    cv::FileStorage fisier(FISIER_FATA, cv::FileStorage::READ);
    if (!fisier.isOpened())
        return false;

    cv::FileNode lista = fisier["fete"];
    if (lista.empty())
    {
        fisier.release();
        return false;
    }

    for (cv::FileNodeIterator it = lista.begin(); it != lista.end(); ++it)
    {
        cv::Mat fata;
        (*it) >> fata;
        if (!fata.empty())
            feteInregistrate.push_back(fata);
    }

    fisier.release();
    return !feteInregistrate.empty();
}

bool initializeazaCameraSiDetector(QString& mesajEroare)
{
    camera.open(0);
    if (!camera.isOpened())
    {
        mesajEroare = "Camera nu poate fi deschisa!";
        return false;
    }

    detector = cv::FaceDetectorYN::create(
        MODEL, "", cv::Size(320, 320), 0.9f, 0.3f, 5000
    );

    if (detector.empty())
    {
        mesajEroare = "Modelul YuNet nu poate fi incarcat!\nVerifica fisierul: " + QString::fromStdString(MODEL);
        return false;
    }

    utilizatorInregistrat = incarcaFete();

    return true;
}

QImage matToQImage(const cv::Mat& mat)
{
    if (mat.empty())
        return QImage();

    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);

    return QImage(
        rgb.data, rgb.cols, rgb.rows,
        static_cast<int>(rgb.step),
        QImage::Format_RGB888
    ).copy();
}


// INTERFATA GRAFICA

enum class Mod { AUTENTIFICARE, INREGISTRARE_FATA, VERIFICARE_FATA, VAULT };

class FereastraVault : public QWidget
{
public:
    FereastraVault()
    {
        setWindowTitle("Vault Securizat");
        resize(640, 560);

        stack = new QStackedWidget(this);

        construiestePaginaAuth();
        construiestePaginaVault();

        QVBoxLayout* layoutPrincipal = new QVBoxLayout(this);
        layoutPrincipal->addWidget(stack);
        setLayout(layoutPrincipal);

        timer = new QTimer(this);
        connect(timer, &QTimer::timeout, this, &FereastraVault::actualizeazaCadru);
        timer->start(33);

        modCurent = Mod::AUTENTIFICARE;

        actualizeazaButonPrincipal();
    }

private:
    QStackedWidget* stack;
    QTimer* timer;

    QLabel* cameraLabel;
    QLabel* statusLabel;
    QLineEdit* parolaEdit;
    QPushButton* butonPrincipal;
    QListWidget* listaFisiere;
    QPushButton* butonDeschide;
    QPushButton* butonIesireVault;

    Mod modCurent;
    int capturiFacute = 0;
    string parolaCurenta;

    void construiestePaginaAuth()
    {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);

        cameraLabel = new QLabel();
        cameraLabel->setFixedSize(480, 360);
        cameraLabel->setStyleSheet("background-color: black;");
        cameraLabel->setAlignment(Qt::AlignCenter);

        statusLabel = new QLabel("Initializare...");
        statusLabel->setAlignment(Qt::AlignCenter);

        parolaEdit = new QLineEdit();
        parolaEdit->setEchoMode(QLineEdit::Password);
        parolaEdit->setPlaceholderText("Parola");

        butonPrincipal = new QPushButton("Continua");
        connect(butonPrincipal, &QPushButton::clicked, this, &FereastraVault::pePasulPrincipal);

        QHBoxLayout* layoutCamera = new QHBoxLayout();
        layoutCamera->addStretch();
        layoutCamera->addWidget(cameraLabel);
        layoutCamera->addStretch();

        layout->addLayout(layoutCamera);
        layout->addWidget(statusLabel);
        layout->addWidget(parolaEdit);
        layout->addWidget(butonPrincipal);

        stack->addWidget(pagina);
    }

    void construiestePaginaVault()
    {
        QWidget* pagina = new QWidget();
        QVBoxLayout* layout = new QVBoxLayout(pagina);

        QLabel* titlu = new QLabel("Fisiere din vault:");

        listaFisiere = new QListWidget();
        connect(listaFisiere, &QListWidget::itemDoubleClicked, this, &FereastraVault::deschideFisierSelectat);

        butonDeschide = new QPushButton("Deschide fisierul selectat");
        connect(butonDeschide, &QPushButton::clicked, this, [this]() {
            if (listaFisiere->currentItem())
                deschideFisierSelectat(listaFisiere->currentItem());
        });

        butonIesireVault = new QPushButton("Inchide vault-ul");
        connect(butonIesireVault, &QPushButton::clicked, this, &FereastraVault::inchideVault);

        layout->addWidget(titlu);
        layout->addWidget(listaFisiere);
        layout->addWidget(butonDeschide);
        layout->addWidget(butonIesireVault);

        stack->addWidget(pagina);
    }

    void pePasulPrincipal()
    {
        if (modCurent == Mod::AUTENTIFICARE)
        {
            string parola = parolaEdit->text().toStdString();

            if (parola.empty())
            {
                statusLabel->setText("Introdu o parola!");
                return;
            }

            if (!existaParola())
            {
                salveazaParolaGUI(parola);
                parolaCurenta = parola;

                feteInregistrate.clear();
                capturiFacute = 0;
                modCurent = Mod::INREGISTRARE_FATA;

                statusLabel->setText("Inregistrare fata: priveste spre camera (0/" + QString::number(NUMAR_CAPTURI) + ")");
                butonPrincipal->setEnabled(false);
                parolaEdit->setEnabled(false);
            }
            else
            {
                if (!verificaParola(parola))
                {
                    statusLabel->setText("Parola gresita!");
                    return;
                }

                if (!utilizatorInregistrat)
                {
                    statusLabel->setText("Nu exista utilizator inregistrat pentru verificare facială!");
                    return;
                }

                parolaCurenta = parola;
                modCurent = Mod::VERIFICARE_FATA;

                statusLabel->setText("Verificare: priveste spre camera...");
                butonPrincipal->setEnabled(false);
                parolaEdit->setEnabled(false);
            }
        }
    }

    void actualizeazaButonPrincipal()
    {
        if (!existaParola())
            butonPrincipal->setText("Creeaza parola noua");
        else
            butonPrincipal->setText("Autentificare");
    }

    void actualizeazaCadru()
    {
        if (modCurent == Mod::VAULT)
            return;

        cv::Mat frame;
        camera >> frame;
        if (frame.empty())
            return;

        cv::Mat fata;
        cv::Rect dreptunghi;
        bool gasita = extrageFata(frame, fata, &dreptunghi);

        if (gasita)
        {
            cv::rectangle(frame, dreptunghi, cv::Scalar(0, 255, 0), 2);
        }

        if (modCurent == Mod::INREGISTRARE_FATA)
        {
            if (gasita)
            {
                feteInregistrate.push_back(fata.clone());
                capturiFacute++;
                statusLabel->setText(
                    "Inregistrare fata: priveste spre camera (" +
                    QString::number(capturiFacute) + "/" + QString::number(NUMAR_CAPTURI) + ")"
                );

                if (capturiFacute >= NUMAR_CAPTURI)
                {
                    finalizeazaInregistrarea();
                }
            }
        }
        else if (modCurent == Mod::VERIFICARE_FATA)
        {
            if (gasita)
            {
                finalizeazaVerificarea(fata, frame);
            }
        }

        cameraLabel->setPixmap(
            QPixmap::fromImage(matToQImage(frame)).scaled(
                cameraLabel->size(), Qt::KeepAspectRatio
            )
        );
    }

    void finalizeazaInregistrarea()
    {
        if (feteInregistrate.size() >= 5 && salveazaFete())
        {
            utilizatorInregistrat = true;
            statusLabel->setText("Utilizator inregistrat! Se cripteaza vault-ul...");

            criptareDirector(parolaCurenta);

            statusLabel->setText("Vault initializat cu succes!");
        }
        else
        {
            statusLabel->setText("Nu s-au obtinut suficiente capturi. Incearca din nou.");
        }

        modCurent = Mod::AUTENTIFICARE;
        butonPrincipal->setEnabled(true);
        parolaEdit->setEnabled(true);
        parolaEdit->clear();
        actualizeazaButonPrincipal();
    }

    void finalizeazaVerificarea(const cv::Mat& fataCurenta, const cv::Mat& frameOriginal)
    {
        double suma = 0.0;
        for (size_t i = 0; i < feteInregistrate.size(); i++)
            suma += comparaFete(feteInregistrate[i], fataCurenta);

        double scorFinal = suma / feteInregistrate.size();

        if (scorFinal < PRAG_RECUNOASTERE)
        {
            statusLabel->setText("Acces permis! Se decripteaza vault-ul...");

            decriptareDirector(parolaCurenta);
            incarcaListaFisiere();

            modCurent = Mod::VAULT;
            stack->setCurrentIndex(1);
        }
        else
        {
            statusLabel->setText("Fata nu corespunde. Acces respins! (scor: " + QString::number(scorFinal, 'f', 1) + ")");
            modCurent = Mod::AUTENTIFICARE;
            butonPrincipal->setEnabled(true);
            parolaEdit->setEnabled(true);
            parolaEdit->clear();
        }
    }

    // PAGINA VAULT

    void incarcaListaFisiere()
    {
        listaFisiere->clear();

        if (!fs::exists("vault_decriptat"))
            return;

        for (const auto& entry : fs::directory_iterator("vault_decriptat"))
        {
            if (entry.is_regular_file())
            {
                QString nume = QString::fromStdString(entry.path().filename().string());
                QListWidgetItem* item = new QListWidgetItem(nume);
                item->setData(Qt::UserRole, QString::fromStdString(entry.path().string()));
                listaFisiere->addItem(item);
            }
        }

        if (listaFisiere->count() == 0)
        {
            listaFisiere->addItem("(niciun fisier in vault)");
        }
    }

    void deschideFisierSelectat(QListWidgetItem* item)
    {
        QString cale = item->data(Qt::UserRole).toString();
        if (cale.isEmpty())
            return;

        QDesktopServices::openUrl(QUrl::fromLocalFile(cale));
    }

 void inchideVault()
{
    if (fs::exists("vault_decriptat"))
    {
        for (const auto& entry : fs::directory_iterator("vault_decriptat"))
        {
            fs::remove(entry.path());
        }
    }
    modCurent = Mod::AUTENTIFICARE;
    stack->setCurrentIndex(0);
    statusLabel->setText("Vault inchis. Introdu parola pentru a reveni.");
    parolaEdit->clear();
    parolaEdit->setEnabled(true);
    butonPrincipal->setEnabled(true);
    actualizeazaButonPrincipal();
}

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    QString eroare;
    if (!initializeazaCameraSiDetector(eroare))
    {
        QMessageBox::critical(nullptr, "Eroare initializare", eroare);
        return 1;
    }

    FereastraVault fereastra;
    fereastra.show();

    int rezultat = app.exec();

    camera.release();

    return rezultat;
}
