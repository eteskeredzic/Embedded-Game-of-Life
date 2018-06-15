/*
* PROJEKTNI ZADATAK IZ UGRADBENIH SISTEMA - AKADEMSKA GODINA 2017./2018.
* KREATORI: Edvin Teskeredzic i Kenan Karahodzic;
* Bilo kakvo koristenje ovog koda bez znanja kreatora nije dozvoljeno!
*/

#include "mbed.h"
#include "rtos.h"
//#include <cstdio>
#define dp23 P0_0

// REGISTRI ZA DISPLAY
#define NO_OP_REG           0x00    // ???
#define DIGIT_0_REG         0x01    // prvi red
#define DIGIT_1_REG         0x02    // drugi red
#define DIGIT_2_REG         0x03    // treci red
#define DIGIT_3_REG         0x04    // cetvrti red
#define DIGIT_4_REG         0x05    // peti red
#define DIGIT_5_REG         0x06    // sesti red
#define DIGIT_6_REG         0x07    // sedmi red
#define DIGIT_7_REG         0x08    // osmi red
#define DECODE_MODE_REG     0x09    // ???
#define INTENSITY_MODE_REG  0x0A    // intenzitet kojim diode svijetle
#define SCAN_LIMIT_REG      0x0B    // koliko smije dioda goriti istovremeno (8)
#define SHUTDOWN_REG        0x0C    // ako je setovan, displaj je upaljen
#define DISPLAY_TEST_REG    0x0F    // odlucuje izmedju normal mode i test mode


// KOMUNIKACIJA
Serial pc(USBTX, USBRX);

// DISPLAY-i
DigitalOut cs1(PTD0);
DigitalOut cs2(PTC16);
DigitalOut cs3(PTD5);
DigitalOut cs4(PTC13);
SPI spi(PTD2, PTD3, PTD1); // MOSI, MISO, SCLK

// MATRICE ZA GAME OF LIFE
char trenutno[16][16] = { { 0 } };
char staro[16][16] = { { 0 } };
unsigned char red[8] = {0}; // ovo koristimo za ispis u jednu matricu

// TICKERI TIMER THREAD
Ticker tDisplej;/// OVO MOZDA NECE TREBATI OBZIROM DA DISPLAY IMA INTERNO REGISTRE KOJI CUVAJU STANJE
Timer tajmer;
Thread thread;

// MISC.
unsigned char pauza = 1; // oznacava da li se vrsi prelazak u novu generaciju - na pocetku je 1 jer sistem miruje (ceka se input korisnika)
float vrijeme_cekanja = 0.3; // oznacava koliko dugo cekamo na narednu generaciju
unsigned char smijem_crtati = 1; // oznacava da li funkcija za pisanje po matricama smije raditi
char a = 0; // za komsije

// ova funkcija vraca nasumican broj svaki put kada bude pozvana - pri tome ne koristeci rand funkciju, sto uklanja potrebu za cstdlib bibliotekom!
unsigned int SEED = time(NULL);
int dajNasumicanBroj(unsigned int *pSEED, int granica){

*pSEED = (*pSEED * 48271) % 2147483647;

return *pSEED % granica;
}

// pise na odgovarajuci display zadan kao parametar
void pisi(unsigned char registar, unsigned char vrijednost, unsigned char redni){

    switch(redni){
        case 1: // prvi display
            cs1 = 0;
            spi.write(registar);
            spi.write(vrijednost);
            cs1 = 1;
            break;
        case 2: // drugi display
            cs2 = 0;
            spi.write(registar);
            spi.write(vrijednost);
            cs2 = 1;
            break;
        case 3: // treci display
            cs3 = 0;
            spi.write(registar);
            spi.write(vrijednost);
            cs3 = 1;
            break;
        case 4: // cetvrti display
            cs4 = 0;
            spi.write(registar);
            spi.write(vrijednost);
            cs4 = 1;
            break;
        default:
            break;
    }
    wait(0.01);
}

// gasi matricu
void ocistiDisplej(int displej){

   for(int i = 1; i <= 8; ++i)
     pisi(i, 0, displej);
}

// cisti sve 4 matrica
void ocistiSve(){
    for(int i = 1; i<=4; ++i)
        ocistiDisplej(i);
    for(int i = 0; i<16;++i) for(int j = 0; j<16;++j) trenutno[i][j] = 0;
}

// postavljanje defaultnih vrijednosti za sve 4 matrice
void init(){
    for(int i = 1; i <= 4; ++i){
        pisi(DECODE_MODE_REG, 0x00, i);    // decode mode ne koristimo
        pisi(INTENSITY_MODE_REG, 0x0f, i); // stavi duty cycle na maksimalnu mogucu vrijednost (osvijetljenost)
        pisi(SCAN_LIMIT_REG,0x07, i);      // upisemo broj 7 jer zelimo da palimo sve do reda indeksa 7 (a to je osmi red)
        pisi(SHUTDOWN_REG,0x01, i);        // stavlja se u normal opmode, jer ne testiramo nista
        //pisi(0xff,0, i);                   // ovo po datasheet mora biti 0
        pisi(0x0F, 0x0F, i);  /// NOVO ENABLE DISPLAY TEST
        wait_ms(500);         /// NOVO 500 ms delay
        ocistiDisplej(i);                            //  ugasi sve diode
        pisi(0x0F, 0x00, i);  /// NOVO DISABLE DISPLAY TEST
        wait_ms(500);
    }
}

// spaja 8 charova u jedan
unsigned char spoji_red(unsigned char red[]){
    unsigned char rez = 0;
    int i = 0;
    for(; i < 8 ; ++i)
        rez |= red[i] << 7-i;
return rez;
}


// spaja 8 charova u jedan, ali naopako
unsigned char spoji_red_naopako(unsigned char red[]){
    unsigned char rez = 0;
    int i = 0;
    for(i = 0; i<8; ++i)
        rez |= red[i] << i;
return rez;
}

void osvjezi_displej(){
    unsigned char i = 0, j = 0, c = 0;
    for(; i < 8; ++i){ // gornja polovina

        for(j = 0; j < 8; ++j) red[j] = trenutno[i][j]; // prva matrica

        c = spoji_red(red);
        pisi(i+1,c,1);

        for(j = 8; j < 16; ++j) red[j-8] = trenutno[i][j]; // druga matrica

        c = spoji_red_naopako(red);
        pisi(8-i,c,2); // pisemo od zadnjeg reda prema prvom
    }
    int b = 0;
    for(; i < 16; ++i){ // donja polovina

        for(j = 0; j < 8; ++j) red[j] = trenutno[i][j]; // treca matrica

        c = spoji_red(red);
        pisi(i-7,c,3);
        for(j = 8; j < 16; ++j) red[j-8] = trenutno[i][j]; // cetvrta matrica

        c = spoji_red_naopako(red);
        pisi(i-b,c,4); // pisemo od zadnjeg reda prema prvom
        b+=2;
    }
}

// racuna broj komsija celije
int dajKomsije(short int x, short int y){
    int rez = 0;
    if (x != 0 && y != 0  && staro[x - 1][y - 1] == 1) // iznad-lijevo
        ++rez;
    if (x != 0 && staro[x - 1][y] == 1) // iznad
        ++rez;
    if (x != 0 && y != 15 && staro[x - 1][y + 1] == 1) // iznad-desno
        ++rez;
    if (y != 0  && staro[x][y - 1] == 1) // lijevo
        ++rez;
    if (y != 15 && staro[x][y + 1] == 1) // desno
        ++rez;
    if (x != 15 && y != 0 && staro[x + 1][y - 1] == 1) // ispod-lijevo
        ++rez;
    if (x != 15 && staro[x + 1][y] == 1) // ispod
        ++rez;
    if (x != 15 && y != 15 && staro[x + 1][y + 1] == 1) // ispod-desno
        ++rez;
    return rez;
}

// generise narednu generaciju celija na osnovu prethodne
void update_game(){

    for (int i = 0; i < 16; ++i)
        for (int j = 0; j < 16; ++j)
            staro[i][j] = trenutno[i][j]; // prepisuj


        for (int i = 0; i < 16; ++i) {
            for (int j = 0; j < 16; ++j) {
                    a = dajKomsije(i, j);
                    if (staro[i][j] == 1 && (a < 2 || a > 3))
                        trenutno[i][j] = 0; // pravila 1 i 3
                    else if (staro[i][j] == 0 && (a == 3))
                        trenutno[i][j] = 1; // pravilo 4
                    else if (staro[i][j] == 1 && (a == 2 || a == 3))
                        trenutno[i][j] = 1;
            }
        }
      osvjezi_displej();

        for (int i = 0; i < 16; ++i)
            for (int j = 0; j < 16; ++j)
                staro[i][j] = trenutno[i][j]; // prepisuj


}

// upravljanje igrom
void threadGameOfLife(){
    while(1){ // vrti beskonacno update
       wait(vrijeme_cekanja);
       if(pauza == 1) continue;
       update_game(); // radi update
    }
}


void zelena(){
 pc.printf("\033[0;32m");
}

void crvena(){
    pc.printf("\033[0;31m");
}

void defaultna(){
 pc.printf("\033[0m");
}

void credits(){
    zelena();
    pc.printf("\r\n===============================================================\r\n");
    pc.printf("\r\nPravili: Edvin Teskeredzic (17333) i Kenan Karahodzic (17853)\r\nCitav kod dostupan na: https://github.com/eteskeredzic/Embedded-Game-of-Life\r\n");
    pc.printf("\r\n================================================================\r\n");
    defaultna();
  /*  pauza = 1;
    trenutno[0][3] = trenutno[0][4]=trenutno[1][2]=trenutno[1][5]=trenutno[1][3]=trenutno[1][4]=trenutno[2][1]=trenutno[2][2]=trenutno[2][3]=trenutno[2][4]=
trenutno[2][5]=trenutno[2][6]=trenutno[3][0]=trenutno[3][1]=trenutno[3][3]=trenutno[3][4]=trenutno[3][6]=trenutno[3][7]=
trenutno[4][0]=trenutno[4][1]=trenutno[4][2]=trenutno[4][3]=trenutno[4][4]=trenutno[4][5]=trenutno[4][6]=trenutno[4][7]= trenutno[5][1] = trenutno[5][3] = trenutno[5][4]
=trenutno[5][6]=trenutno[6][0]=trenutno[6][7]=trenutno[7][1]=trenutno[7][6]=1;
osvjezi_displej();*/
}

void nasumicno(){
    int i  = 0, j = 0;
    for(i = 0; i<16;++i)
        for(j = 0; j<16; ++j)
            if(dajNasumicanBroj(&SEED, 10) < 5) trenutno[i][j] = 0;
            else trenutno[i][j] = 1;
    osvjezi_displej();
}

void ubrzaj(){
    vrijeme_cekanja-=0.1;
    if(vrijeme_cekanja <= 0.1) vrijeme_cekanja = 0.1;
}

void uspori(){
    vrijeme_cekanja+=0.1;
    if(vrijeme_cekanja >= 1.0) vrijeme_cekanja = 1;
}

int isNum(char c){
    int a = c-48;
    return (a >= 0 && a < 9) ? 1 : 0;
}

void mijenjajDiodu(){
    pc.printf("\r\nUnesite koordinate celije kojoj mijenjate stanje - U FORMATU 'X,Y;'\r\n");
    int xcoord = 0, ycoord = 0, i = 0;
    char c = '0';
    char s[10] = {'\0'};
    while(1)
    {
        c = pc.getc();
        pc.putc(c);
        if(c == ';' || i == 9) break;
        s[i++] = c;
    }
    if(isNum(s[0]) == 1 && s[1] == ',' && isNum(s[2]) == 1 && s[3] == 0) // slucaj B,B
    {
        xcoord = s[0]-48;
        ycoord = s[2]-48;
        if(xcoord < 0 || xcoord > 15 || ycoord < 0 || ycoord > 15)
        {
            crvena(); pc.printf("\r\nGreska!\r\n"); defaultna(); return;
        }
        trenutno[xcoord][ycoord] ^= 1;
        osvjezi_displej();
        return;
    }
    if(isNum(s[0]) == 1 && s[1] == ',' && isNum(s[2]) == 1 && isNum(s[3]) == 1 && s[4] == 0) // slucaj B,BB
    {
        xcoord = s[0]-48;
        ycoord = (s[2]-48) * 10 + (s[3]-48);
        if(xcoord < 0 || xcoord > 15 || ycoord < 0 || ycoord > 15)
        {
            crvena(); pc.printf("\r\nGreska!\r\n"); defaultna(); return;
        }
        trenutno[xcoord][ycoord] ^= 1;
        osvjezi_displej();
        return;
    }
    if(isNum(s[0]) == 1 && isNum(s[1]) == 1 && s[2] == ',' && isNum(s[3]) == 1 && s[4] == 0) // slucaj BB,B
    {
        xcoord = (s[0]-48)*10 + (s[1]-48);
        ycoord = s[3]-48;
        if(xcoord < 0 || xcoord > 15 || ycoord < 0 || ycoord > 15)
        {
            crvena(); pc.printf("\r\nGreska!\r\n"); defaultna(); return;
        }
        trenutno[xcoord][ycoord] ^= 1;
        osvjezi_displej();
        return;
    }
    if(isNum(s[0]) == 1 && isNum(s[1]) == 1 && s[2] == ',' && isNum(s[3]) == 1 && isNum(s[4]) == 1 && s[5] == 0) // slucaj BB,BB
    {
        xcoord = (s[0]-48)*10 + (s[1]-48);
        ycoord = (s[3]-48)*10 + (s[4]-48);
        if(xcoord < 0 || xcoord > 15 || ycoord < 0 || ycoord > 15)
        {
            crvena(); pc.printf("\r\nGreska!\r\n"); defaultna(); return;
        }
        trenutno[xcoord][ycoord] ^= 1;
        osvjezi_displej();
        return;
    }
    crvena();
    pc.printf("\r\nPogresan unos!\r\n");
    defaultna();
}

// ispisuje glavni meni i daje korisniku opcije
void meni(){

    pc.printf("\r\n|--------------------------CONWAY'S GAME OF LIFE--------------------------|\r\n");
    pc.printf("| Za mijenjanje stanja diode, pritisnite 1                                |\r\n"); //
    pc.printf("| Za povecanje brzine ispisa, pritisnite 2"); zelena(); pc.printf(" (trenutna brzina %.1f s)        ", vrijeme_cekanja); defaultna(); pc.printf("|\r\n"); // gotovo
    pc.printf("| Za smanjenje brzine ispisa, pritisnite 3"); zelena(); pc.printf(" (trenutna brzina %.1f s)        ", vrijeme_cekanja); defaultna(); pc.printf("|\r\n");// gotovo
    pc.printf("| Za pauziranje/ponovno pokretanje, pritisnite 4"); zelena(); pc.printf(" (trenutno pauzirano: %s) ", pauza == 1 ? "DA" : "NE"); defaultna(); pc.printf("|\r\n");// gotovo
    pc.printf("| Za ciscenje ploce, pritisnite 5                                         |\r\n");
    pc.printf("| Za nasumicnu pocetnu konfiguraciju, pritisnite 6                        |\r\n"); // gotovo
    pc.printf("| Za informacije o kreatorima, pritisnite 7                               |\r\n"); // gotovo
    pc.printf("|-------------------------------------------------------------------------|\r\n");


    while(1){
        pc.printf("Unesite redni broj komande: ");
        char c = pc.getc();
            if(c == '1'){
                if(pauza == 0){
                    crvena();
                    pc.printf("\r\nDozvoljeno samo dok je igra pauzirana!\r\n");
                    defaultna();
                    break;
                }
                mijenjajDiodu();
                break;
            }
            else if(c == '2'){
                ubrzaj();
                break;
            }
            else if(c == '3'){
                uspori();
                break;
            }
            else if(c == '4'){
                if(pauza == 1) pauza = 0; else pauza = 1;
                break;
            }
            else if(c == '5'){ ocistiSve(); break; }
            else if(c == '6'){
                if(pauza == 0){
                    crvena();
                    pc.printf("\r\nDozvoljeno samo dok je igra pauzirana!\r\n");
                    defaultna();
                    break;
                }
                nasumicno();
                break;
            }
            else if(c == '7'){
                credits();
                break;
            }
            else{
                    crvena();
                    pc.printf("\r\nNe postoji komanda! Pokusaj ponovo (i ovaj put unesi kako treba). . .\r\n");
                    defaultna();
                }
    }
}

int main()
{
    cs1 = 1;
    cs2 = 1;
    cs3 = 1;
    cs4 = 1;
    spi.format(8,0);
    spi.frequency(1000000);
    init();
    tajmer.start();
    thread.start(threadGameOfLife);
    while(1) meni();

}

