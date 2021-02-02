//program.c

typedef unsigned char Byte8;
typedef unsigned short Word16;

void outp(Word16 addr, Byte8 data) {
	_asm {
		push dx
		push ax
		mov dx, addr
		mov al, data
		out dx, al
		pop ax
		pop dx
	}
}

Byte8 inp(Word16 addr) {
	Byte8 result;
	_asm {
		push dx
		push ax
		mov dx, addr
		in al, dx
		mov result, al
		pop ax
		pop dx
	}
	return result;
}

//Kontroler prekida - 8259
#define ADR_8259_0			0x0000
#define ADR_8259_1			0x0010

#define CTRL_8259_ICW1		0x10
#define CTRL_8259_EDGE		0x00
#define CTRL_8259_SNGL		0x02
#define CTRL_8259_IC4		0x01

#define CTRL_8259_NOT_SFN	0x00
#define CTRL_8259_NOT_BUF	0x00
#define CTRL_8259_NOT_AEOI	0x00
#define CTRL_8259_8086		0x01

#define CTRL_8259_OCW2		0x00
#define CTRL_8259_SPECEOI	0x60

#define CTRL_8259_OCW3		0x08
#define CTRL_8259_NOT_SM	0x00
#define CTRL_8259_NOT_POLL	0x00
#define CTRL_8259_READREG	0x02
#define CTRL_8259_IS		0x01

/*
 * Inicijalizacija kontrolera prekida:
 * EDGE, SINGLE, Fully Nested Mode, bez BUF, bez AEOI
 * Ulazi u IVT 0x20 - 0x27 (u ovom simulatoru to nije bitno - videti objasnjenje jedine prekidne rutine u ovom fajlu)
 */
void init8259() {
	outp(ADR_8259_0, CTRL_8259_ICW1 | CTRL_8259_EDGE | CTRL_8259_SNGL | CTRL_8259_IC4); //ICW1
	outp(ADR_8259_1, 0x20); //ICW2
	outp(ADR_8259_1, CTRL_8259_NOT_SFN | CTRL_8259_NOT_BUF | CTRL_8259_NOT_AEOI | CTRL_8259_8086); //ICW4
	
	outp(ADR_8259_0, CTRL_8259_OCW3 | CTRL_8259_NOT_SM | CTRL_8259_NOT_POLL | CTRL_8259_READREG | CTRL_8259_IS); //OCW3
}

//Paralelni port - 8255
#define ADR_8255_0			0x0047
#define ADR_8255_1			0x0043
#define ADR_8255_2			0x00C7
#define ADR_8255_3			0x00C3

#define CTRL_8255_MOD_DEF	0x80
#define CTRL_8255_GA_MODE	0x00
#define CTRL_8255_PA_DIR	0x00
#define CTRL_8255_PCU_DIR	0x00
#define CTRL_8255_GB_MODE	0x00
#define CTRL_8255_PB_DIR	0x00
#define CTRL_8255_PCL_DIR	0x00

/*
 * Inicijalizacija paralelnog porta:
 * Svi portovi su izlazni i rade u modu 0
 */
void init8255() {
	outp(ADR_8255_3, CTRL_8255_MOD_DEF | CTRL_8255_GA_MODE | CTRL_8255_PA_DIR | CTRL_8255_PCU_DIR | CTRL_8255_GB_MODE | CTRL_8255_PB_DIR | CTRL_8255_PCL_DIR);
	
	/*
	 * Inicijalno postavi nulu na svaku cifru displeja
	 */
	outp(ADR_8255_0, 0x3F);
	outp(ADR_8255_1, 0x00);
}
// strana 6 u sheet-u
//Tajmer - 8254
#define ADR_8254_0			0x0058
#define ADR_8254_1			0x0078
#define ADR_8254_2			0x0048
#define ADR_8254_3			0x0068

#define CTRL_8254_LSB_MSB	0x30
#define CTRL_8254_LSB_ONLY	0x10
#define CTRL_8254_MODE2		0x04
#define CTRL_8254_BINARY	0x00

#define CTRL_8254_CNT0		0x00
#define CTRL_8254_CNT0_LSB	0x88
#define CTRL_8254_CNT0_MSB	0x13

#define CTRL_8254_CNT2		0x80
#define CTRL_8254_CNT2_LSB	0x05 /* za 5 ms

/*
 * Inicijalizacija tajmera:
 * Koriste se CNT0 i CNT2, oba u modu 2
 * CNT0 generise prekide na svakih 1s (takt 1kHz, pocetna vrednost 1000 = 0x03E8)
 * CNT2 generise ~70 prekida u sekundi (takt 1kHz, pocetna vrednost 14 = 0x0E)
 */
void init8254() {
	outp(ADR_8254_3, CTRL_8254_CNT0 | CTRL_8254_LSB_MSB  | CTRL_8254_MODE2 | CTRL_8254_BINARY); //ControlWord CNT0
	outp(ADR_8254_3, CTRL_8254_CNT2 | CTRL_8254_LSB_ONLY | CTRL_8254_MODE2 | CTRL_8254_BINARY); //ControlWord CNT2
	
	outp(ADR_8254_0, CTRL_8254_CNT0_LSB);
	outp(ADR_8254_0, CTRL_8254_CNT0_MSB); //InitialValue CNT0 (prekid tajmera na svakih ((CTRL_8254_CNT0_MSB << 8) | CTRL_8254_CNT0_VAL) * Tclk0 = 0x03E8 * 1ms = 1s)
	
	outp(ADR_8254_2, CTRL_8254_CNT2_LSB); //InitialValue CNT2 (prekid tajmera na svakih CTRL_8254_CNT2_LSB * Tclk2 = 0x0005 * 1ms = 5ms)
}

/* Funkcija koja prihvata vrednost cifre, a vraca bitmapu za ukljucivanje/iskljucivanje odgovarajucih segmenata.
 *
 *                              a
 *                           *******
 *                         *         *
 *                       f *         * b
 *                         *    g    *
 *                           *******
 *                         *         *
 *                       e *         * c
 *                         *    d    *
 *                           *******
 *
 *
 * Neophodno je utvrditi sa seme kako su segmenti povezani na pinove paralelnog porta.
 */

Byte8 segmenti[] = { 0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F ,0x40  };

Byte8 dekodujCifru(Byte8 cifra) {
	if(cifra < 11)
		return segmenti[cifra];
	else
		return 0x00;
}

Byte8 cifre[8] = { 0x00, 0x00, 0x00, 0x00,0x00, 0x00, 0x00, 0x00  };
 
int datumi[] = { 0,-2,-1,-2,5,-2,8,15,16,17,17,18,19,15,14,13,8,9,8,15,14,14,15,14,18,13,12,10,-2,-2,-1 };
int brojac =  0 ;
int zadnjaZabelezenaTemperatura = 0 ; 
int razlikaTemperatura = 0 ;
int maxTemperatura = 0 ; 
int minTemperatura = 0;
	
int button1Clicked = 0 ; 
int button2Clicked = 0 ; 
int button3Clicked = 0;
int button4Clicked = 0;

int ABS(int broj ){ 
	if(broj < 0 ){
		return -1 * broj; 
	}
	else { 
		return broj ;
	}
}

Byte8 ukljucenaCifra = 0;

/*
 * Funkcija koja se poziva iz prekidne rutine da bi se izvrsilo resetovanje vremena.
 */
void prekidRestart() {
	brojac =  0 ;
	maxTemperatura = 0 ; 
    minTemperatura = 0;
}

/*
 * Funkcija koja se poziva iz prekidne rutine da bi se izvrsilo azuriranje vremena.
 */
void prekidTajmer0() {	
		if(brojac < 31){
			brojac ++;
		}
		else {
			brojac = 0 ;
			maxTemperatura = 0 ; 
            minTemperatura = 0;
		}
		
		if(button1Clicked == 1 ) {
			prekidPrikazTemperature();
		} 
		if(button2Clicked== 1 ) {
			prekidPrikazDatuma();
		}
		if(button3Clicked== 1 ) {
			prekidPrikazMax_MinTemperatura();
		}
		if(button4Clicked== 1 ) {
			prekidPrikazRazlikeTemperature();
		}
		

}

/*
 * Funkcija koja se poziva iz prekidne rutine da bi se izvrsilo osvezavanje prikaza na displeju.
 * U globalnoj promenljivoj "ukljucenaCifra" se cuva koja je to cifra trenutno ukljucena.
 * Svaki put kada se udje u ovu funkciju, prelazi se na prikazivanje sledece cifre.
 * PORTA (svih 8 bita) sluzi za ukljucivanje/iskljucivanje pojedinacnih segmenata trenutno ukljucene cifre (1-ukljucen segment, 0-iskljucen segment - CC(CommonCathode))
 * PORTB (niza 4 bita) sluzi za ukljucivanje/iskljucivanje pojedinacnih cifara (0-ukljucena, 1-iskljucena - CC(CommonCathode))
 */
void prekidTajmer2() {
	Byte8 cifraZaPrikaz, sadrzajZaPrikaz;
	
	outp(ADR_8255_1, 0xFF);								//prvo iskljuci sve cifre da bismo izbegli cifre "duhove"
	
	ukljucenaCifra = (ukljucenaCifra + 1) % 8;			//premestanje na sledecu cifru
	cifraZaPrikaz = cifre[ukljucenaCifra];				//dohvati vrednost cifre koju treba prikazati
	sadrzajZaPrikaz = dekodujCifru(cifraZaPrikaz);		//odredi koje segmente treba ukljuciti za prikaz prethodno dohvacene cifre
	outp(ADR_8255_0, sadrzajZaPrikaz);					//postavi vrednost porta A u skladu sa vrednoscu cifre koja se prikazuje
	
	outp(ADR_8255_1, ~(0x01 << ukljucenaCifra));		//ukljuci cifru koja je na redu za prikaz postavljanjem vrednosti porta B

}

/*
 * Funkcija koja se poziva iz prekidne rutine da bi se izvrsilo prikaz temperature.
 */
void prekidPrikazTemperature() {
	if(datumi[brojac] < 0 ) {
			int pozDatum = ABS(datumi[brojac]) ; 
			cifre[0] = 0; 
			cifre[1] = 0; 
			cifre[2] = 0; 
			cifre[3] = 0; 
			cifre[4] = 0; 
			cifre[5] = 0; 
			cifre[6] = 10;
			cifre[7] =  pozDatum  % 10;
	} 
	else {
			cifre[0] = 0; 
			cifre[1] = 0; 
			cifre[2] = 0; 
			cifre[3] = 0; 
			cifre[4] = 0; 
			cifre[5] = 0; 
			cifre[6] = datumi[brojac] / 10;
			cifre[7] = datumi[brojac] % 10;
	}


}

/*
 * Funkcija koja se poziva iz prekidne rutine da bi se izvrsilo prikaz datuma.
 */
 
void prekidPrikazDatuma() {
	if(datumi[brojac] < 0 ) {
				int pozDatum = ABS(datumi[brojac]) ; 
				cifre[0] = brojac / 10; 
				cifre[1] = brojac % 10;
				cifre[2] = 1; 
				cifre[3] = 0; 
				cifre[4] = 2; 
				cifre[5] = 0; 
				cifre[6] = 10;
				cifre[7] = pozDatum % 10;
	} 
	else {
			cifre[0] = brojac / 10; 
			cifre[1] = brojac % 10;
			cifre[2] = 1; 
			cifre[3] = 0; 
			cifre[4] = 2; 
			cifre[5] = 0; 
			cifre[6] = datumi[brojac] / 10;
			cifre[7] = datumi[brojac] % 10;
	}

}

/*
 * Funkcija koja se poziva iz prekidne rutine da bi se izvrsilo prikaz maksimalne i minimalne temperature od 1 do trenutnog dana.
 */
 
void prekidPrikazMax_MinTemperatura() {

	if(datumi[brojac] > maxTemperatura) {
		maxTemperatura = datumi[brojac] ;
	}
	if(datumi[brojac] < minTemperatura) {
		minTemperatura = datumi[brojac] ;
	}
	if(minTemperatura < 0 ) {
		if(maxTemperatura < 0){
			cifre[0] = 0; 
			cifre[1] = 0;
			cifre[2] = 0; 
			cifre[3] = 10 ; 
			cifre[4] = ABS(minTemperatura) % 10 ; 
			cifre[5] = 10; 
			cifre[6] = 10;
			cifre[7] = ABS(maxTemperatura) % 10;
		}
		else {
			cifre[0] = 0; 
			cifre[1] = 0;
			cifre[2] = 0; 
			cifre[3] = 10 ; 
			cifre[4] = ABS(minTemperatura) % 10 ; 
			cifre[5] = 10; 
			cifre[6] = ABS(maxTemperatura) / 10 ;
			cifre[7] = ABS(maxTemperatura) % 10;
		}
	}


	else {
		cifre[0] = 0; 
		cifre[1] = 0;
		cifre[2] = 0; 
		cifre[3] = ABS(minTemperatura) / 10 ;
		cifre[4] = ABS(minTemperatura) % 10 ; 
		cifre[5] = 10; 
		cifre[6] = ABS(maxTemperatura) / 10 ;
		cifre[7] = ABS(maxTemperatura) % 10;
	}
	
	
}

/*
 * Funkcija koja se poziva iz prekidne rutine da bi se izvrsilo azuriranje razlike temperature trenutne i prve pre nje.
 */


void prekidPrikazRazlikeTemperature() {
	if(brojac == 0 ) {
		zadnjaZabelezenaTemperatura =0 ;
	} 
	else {
		zadnjaZabelezenaTemperatura = datumi[brojac-1] ;
	}
	
	if ( zadnjaZabelezenaTemperatura < datumi[brojac] ) {
		razlikaTemperatura = datumi[brojac] -  zadnjaZabelezenaTemperatura  ; 
	}
	else { 
		razlikaTemperatura = datumi[brojac] - zadnjaZabelezenaTemperatura     ; 
	}
	if( razlikaTemperatura < 0 ) { 
		if(ABS(razlikaTemperatura) / 10 == 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 0; 
				cifre[4] = 0 ; 
				cifre[5] = 0 ; 
				cifre[6] = 10;
				cifre[7] = ABS(razlikaTemperatura) % 10;
			}	
		if(ABS(razlikaTemperatura) / 10 != 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 0; 
				cifre[4] = 0; 
				cifre[5] = 10; 
				cifre[6] = ABS(razlikaTemperatura) / 10 ;
				cifre[7] = ABS(razlikaTemperatura) % 10 ;
			}
		if(ABS(razlikaTemperatura) / 100 != 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 0; 
				cifre[4] = 10; 
				cifre[5] = ABS(razlikaTemperatura) / 100; 
				cifre[6] = ABS(razlikaTemperatura) / 10 ;
				cifre[7] = ABS(razlikaTemperatura) % 10 ;
			}
		if(ABS(razlikaTemperatura) / 1000 != 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 10; 
				cifre[4] = ABS(razlikaTemperatura) / 1000 ; 
				cifre[5] = ABS(razlikaTemperatura) / 100; 
				cifre[6] = ABS(razlikaTemperatura) / 10 ;
				cifre[7] = ABS(razlikaTemperatura) % 10 ;
			}
		}
		
	else { 
			if(razlikaTemperatura / 10 == 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 0; 
				cifre[4] = 0 ; 
				cifre[5] = 0 ; 
				cifre[6] = 0;
				cifre[7] = razlikaTemperatura % 10;
			}	
		if(razlikaTemperatura / 10 != 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 0; 
				cifre[4] = 0; 
				cifre[5] = 0; 
				cifre[6] = razlikaTemperatura / 10 ;
				cifre[7] = razlikaTemperatura % 10 ;
			}
		if(razlikaTemperatura / 100 != 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 0; 
				cifre[4] = 0; 
				cifre[5] = razlikaTemperatura / 100; 
				cifre[6] = razlikaTemperatura / 10 ;
				cifre[7] = razlikaTemperatura % 10 ;
			}
		if(razlikaTemperatura / 1000 != 0 ) {
				cifre[0] = 0; 
				cifre[1] = 0;
				cifre[2] = 0; 
				cifre[3] = 0; 
				cifre[4] = razlikaTemperatura / 1000 ; 
				cifre[5] = razlikaTemperatura / 100; 
				cifre[6] = razlikaTemperatura / 10 ;
				cifre[7] = razlikaTemperatura % 10 ;
			}
		}

}
/*
 * Model mikroprocesora u simulatoru ne prihvata ispravno broj ulaza u IVT koji mu salje kontroler prekida,
 * zbog cega je neophodno pribeci sledecem resenju (prikazano resenje se cesto koristi u slucaju
 * mikrokontrolera koji imaju mali broj ulaza za prekide):
 *
 * Posto se ne moze odrediti koji ulaz ce procesor prihvatiti, neophodno je napisati jednu prekidnu rutinu
 * koja ce obraditi sve moguce izvore prekida, a zatim adresu te prekidne rutine upisati u sve ulaze u IVT.
 * Posto jedna prekidna rutina obradjuje vise mogucih zahteva za prekid, na pocetku je neophodno utvrditi
 * stvarni izvor prekida citanjem sadrzaja iz IS registra kontrolera prekida (odredjivanjem najnizeg bita
 * koji je u ovom registru postavljen na 1), a potom odraditi onaj deo posla koji odgovara prekidu koji se
 * obradjuje.
 */
void interrupt prekidnaRutina() {
	Byte8 procitaniISR, irLevel, maska = 0x01;
	
	procitaniISR = inp(ADR_8259_0);
	
	for(irLevel = 0; irLevel < 8; irLevel++) {									//traganje za setovanim bitom najmanje tezine (najprioritetnijim prekidom)
		if(procitaniISR & maska)
			break;																//kada se pronadje iskace se iz petlje, kako bi u promenljivoj irLevel ostao redni broj prekida
		maska = maska << 1;
	}
	
	switch(irLevel) {															//skace se na obradu bas tog prekida koji je upravo prihvacen
		case 1:
			prekidRestart();
			outp(ADR_8259_0, CTRL_8259_OCW2 | CTRL_8259_SPECEOI | irLevel);		//specificna EOI komanda kontroleru prekida
			break;
		case 2:
			prekidPrikazTemperature();
			button1Clicked = 1 ; 
			button2Clicked = 0 ; 
			button3Clicked = 0 ; 
			button4Clicked = 0 ; 
			outp(ADR_8259_0, CTRL_8259_OCW2 | CTRL_8259_SPECEOI | irLevel);		//specificna EOI komanda kontroleru prekida
			break;
		case 3:
			prekidPrikazDatuma();
			button1Clicked = 0 ; 
			button2Clicked = 1 ; 
			button3Clicked = 0 ; 
			button4Clicked = 0 ; 
			outp(ADR_8259_0, CTRL_8259_OCW2 | CTRL_8259_SPECEOI | irLevel);		//specificna EOI komanda kontroleru prekida
			break;
		case 4:
			prekidPrikazMax_MinTemperatura();
			button1Clicked = 0 ; 
			button2Clicked = 0 ; 
			button3Clicked = 1 ; 
			button4Clicked = 0 ; 
			outp(ADR_8259_0, CTRL_8259_OCW2 | CTRL_8259_SPECEOI | irLevel);		//specificna EOI komanda kontroleru prekida
			break;
		case 5:
			prekidPrikazRazlikeTemperature();
			button1Clicked = 0 ; 
			button2Clicked = 0 ; 
			button3Clicked = 0 ; 
			button4Clicked = 1 ; 
			outp(ADR_8259_0, CTRL_8259_OCW2 | CTRL_8259_SPECEOI | irLevel);		//specificna EOI komanda kontroleru prekida
			break;
		case 6:
			
			
			prekidTajmer0();
			outp(ADR_8259_0, CTRL_8259_OCW2 | CTRL_8259_SPECEOI | irLevel);		//specificna EOI komanda kontroleru prekida
			break;
		case 7:
			
			
			prekidTajmer2();
			outp(ADR_8259_0, CTRL_8259_OCW2 | CTRL_8259_SPECEOI | irLevel);		//specificna EOI komanda kontroleru prekida
			break;
	}
}

void glavnaFunkcija() {
	/*
	 * inicijalizacija periferija
	 */
	init8259();
	init8255();
	init8254();
	
	_asm sti;					//demaskirati maskirajuce prekide procesora
	outp(ADR_8259_1, 0x00);		//demaskirati 3 ulaza (ulazi 1, 2 i 3) kontrolera prekida koji se koriste za prekide od tastera i tajmera
	
	/*
	 * glavnaFunkcija() se vraca u programski kod napisan na asembleru ("startup.asm") nakon cega upada u beskonacnu petlju,
	 * u kojoj se ostaje do gasenja ili reseta sistema, uz povremeno izvrsavanje prekidnih rutina
	 */
}
