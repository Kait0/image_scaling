#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <string.h>

//Definition der im Code verwendeten C-Funktionen
unsigned char * zoom(unsigned char *data, int org_height, int org_width, int factor); 
//Zoom1 ist eine optimierte Zoomfunktion
unsigned char* zoom1(unsigned char *data, int org_height, int org_width, int factor);                     
void window(unsigned char *data, int x, int y, int height, int width, unsigned char* newData, int orig_width);

//Assembler Funktionen einbinden
extern void _asm_window(char* data, int x, int y, int height, int width, char* newData, int orig_width);
extern void _asm_zoom(char *data, char *neu, int org_height, int org_width, int factor);


#pragma pack(push)  //Derzeitiges Alignment aus den Stack pushen um es zu sichern
#pragma pack(1)     //Alignment auf 1 setzten damit padding in den Structs vermieden wird

//Structuren in denen der Header der bmp Datei gespeichert werden kann
//uint16_t fuer 2Byte an Daten und uint32_t fuer 4Byte Bloecke
struct BITMAPFILEHEADER
{
	uint16_t bfType; //Bestimmt den Typ der Datei. BM bei bmp dateien
	uint32_t bfSize; //Groesse der Datei
	uint32_t bfReserved; //Programmspeziefisch, wird meistens 0 sein.
	uint32_t bfOffBits; //Offset an dem die eigentlichen Farbwerte beginnen
};

struct BITMAPINFOHEADER
{
	uint32_t biSize; //Groesse dieser struct
	uint32_t biwidth; //Breite des Bildes
	uint32_t biHeight; //Hoehe des Bildes, kann negativ sein, dann werden die Farbwerte von oben nach unten gespeichert
	uint16_t biPlanes; //Anzahl an Farbebenen
	uint16_t biBitCount; //Abzahl an Bits pro Pixel. In unserem Fall 24
	uint32_t biCompression; //Komprimierung der Datei, 0 falls nicht komprimiert
	uint32_t biSizeImage; //Groesse der Farbwerte mit Padding
	uint32_t biXPixelsPerMeter; //Druckeraufloesung horizontal
	uint32_t biYPixelsPerMeter; //Druckeraufloesung vertikal
	uint32_t biClrUsed; //Anzahl an Farben in der Farbscala 0 heisst alle
	uint32_t biClrImportant; //wichtige Farben 0 heisst alle
};
//Strucktur um die Pixelfarbwerte aus der bmp Datei zu speichern
//Da die Daten im little Endian Format gespeichert sind fangen wir mit blue anstatt red an
struct PIXEL
{
	unsigned char blue;
	unsigned char green;
	unsigned char red;
};
#pragma pack(pop) //Orginales Alignment wiederherstellen

int main(int argc, char **argv)
{
	//Startzeitpunnkt der Funktion speichern
	float startTime = (float)clock() / CLOCKS_PER_SEC;
	if (argc == 9)//Code nur ausfuehren wenn genuegend Parameter uebergeben wurden 
	{
		//Uebergebene Parameter einlesen
		char *filename = argv[1];
		char *newfilename = argv[2];
		int x = atoi(argv[3]);
		int y = atoi(argv[4]);
		int width_window = atoi(argv[5]);
		int height_window = atoi(argv[6]);
		int scaleFactor = atoi(argv[7]);
		int n = atoi(argv[8]);//Anzahl an durchlaeufen der zu benchmarkenden Funktionen
		
		//Variablen zum Speichern der Bitmap
		struct BITMAPFILEHEADER *bitmapFileHeader;
		struct BITMAPINFOHEADER *bitmapInfoHeader;
		struct PIXEL *bitmapImage;
		FILE *file;
		
		bitmapFileHeader = (struct BITMAPFILEHEADER*)malloc(sizeof(struct BITMAPFILEHEADER));
		bitmapInfoHeader = (struct BITMAPINFOHEADER*)malloc(sizeof(struct BITMAPINFOHEADER));
		
		//Datei oeffnen
		file = fopen(filename, "rb");
		//Falls die Datei nicht geoeffnet werden konnte Programm beenden
		if (file == NULL)
		{
			free(bitmapFileHeader);
			free(bitmapInfoHeader);
			printf("Datei konnte nicht geoeffnet werden!\n");
			return 1;
		}
		
		//Erster Teil der Headerdaten einlesen
		fread(bitmapFileHeader, sizeof(struct BITMAPFILEHEADER), 1, file);
		//Falls es sich nicht um eine Bitmap Datei handelt Programm beenden
 		if (bitmapFileHeader->bfType != 0x4D42)
		{
			printf("Diese Datei ist keine Bitmap!\n");
			free(bitmapFileHeader);
			free(bitmapInfoHeader);
			fclose(file);
			return 1;
		}
		
		//Zweiter Teil des Headers einlesen
		fread(bitmapInfoHeader, sizeof(struct BITMAPINFOHEADER), 1, file);
		
		//Uebergene Parameter auf Korrektheit ueberpruefen
		if(n < 1 || scaleFactor < 1)
		{
			printf("Die Anzahl der Durchlauefe und der Skalierungsfaktor muss mindestens 1 sein!\n");
			free(bitmapFileHeader);
			free(bitmapInfoHeader);
			fclose(file);
			return 1;
		}
		if(width_window + x > bitmapInfoHeader->biwidth || height_window + y > bitmapInfoHeader->biHeight)
		{
			printf("Der Ausschnitt des Bildes ist groesser als das Bild selbst, oder liegt ausserhalb von diesem!\n");
			free(bitmapFileHeader);
			free(bitmapInfoHeader);
			fclose(file);
			return 1;
		}
		
		//Anzahl an Padding Bytes am Ende jeder Zeile berechnen
		int padd = (bitmapInfoHeader->biwidth * 3) % 4;

		//Speicher auf dem Heap zum einlesen der Farbwerte allokieren
		bitmapImage = (struct PIXEL *)malloc(bitmapInfoHeader->biHeight * bitmapInfoHeader->biwidth * sizeof(struct PIXEL) + bitmapInfoHeader->biHeight * padd);
		//Zaehlvariablen fuer Schleifen
		int i, o;
		
		//Farbwerte in bitmapImage einlesen
		for (i = 0; i<bitmapInfoHeader->biHeight; i++)
		{
			for (o = 0; o<bitmapInfoHeader->biwidth; o++)
			{
				fread(&bitmapImage[i * bitmapInfoHeader->biwidth + o], sizeof(struct PIXEL),1 , file);
			}
			//Falls es padding gibt, entferne es
			if(padd != 0)
			{
				unsigned char *c = (unsigned char *)malloc(padd); //Puffer um die Padding-Bytes auszulesen
				fread(c, 1, padd, file);
				free(c); //Padding Bytes werden nicht weiter gebraucht
			}
		}
		
		//Speicher fuer die von den Funktionen berechneten neuen Werte anlegen
		unsigned char *bildWindow = (unsigned char*)malloc(3 * width_window * height_window);
		unsigned char *neuesArray = (unsigned char*)malloc(3 * width_window * height_window * scaleFactor * scaleFactor);
		
		//Variablen zum Benchmarken der Funktionen
		float time = 0.f, endTime = 0.f, startTimeFunctions = (float)clock() / CLOCKS_PER_SEC;
		
		//Alle Funktionen werden n-mal ausgefuehrt und die durchschnittliche Zeit wird ausgegeben
		for(i=0; i < n; i++)
		{
			//Bildausschnitt der vergoessert werden soll ausschneiden
			_asm_window(&bitmapImage[0].blue, x, y, height_window, width_window, bildWindow, bitmapInfoHeader->biwidth);
		}
		endTime = (float)clock() / CLOCKS_PER_SEC;
		time = endTime - startTimeFunctions;
		//Time durch die Anzahl an Ausfuehrungen Teilen. So bekommt man die durschnittliche Laufzeit der Funktion
		time /= n;
		printf("Durchschnittiche Dauer der assembler Windowfunktion:%f\n", time);
		time = 0; //Dauer zuruecksetzen
		
		//Nur bei mehreren Durchlaeufen die C-Funktionen Benchmarken
		if(n != 1)
		{
			startTimeFunctions = (float)clock() / CLOCKS_PER_SEC;
			for(i=0; i < n; i++)
			{
				//Bildausschnitt der vergoessert werden soll ausschneiden in der C Version diesmal
				window(&bitmapImage[0].blue, x, y, height_window, width_window, bildWindow, bitmapInfoHeader->biwidth);
			}
			endTime = (float)clock() / CLOCKS_PER_SEC;
			time = endTime - startTimeFunctions;
			//Time durch die Anzahl an Ausfuehrungen Teilen. So bekommt man die durschnittliche Laufzeit der Funktion
			time /= n;
			printf("Durchschnittiche Dauer der C-Windowfunktion:%f\n", time);
			time = 0; //Dauer zuruecksetzen
			
			startTimeFunctions = (float)clock() / CLOCKS_PER_SEC;
			for(i=0; i < n; i++)
			{	
				//Bildausschnitt mit der ersten C-Funktion vergoessern
				unsigned char* zo = zoom(bildWindow, height_window, width_window, scaleFactor);
				//Ergebniss wird geloescht, da die C-Funktion nur zum Benchmarkvergleich dient
				free(zo);
			}
			endTime = (float)clock() / CLOCKS_PER_SEC;
			time = endTime - startTimeFunctions;
			time /= n;
			printf("Durchschnittiche Dauer der C Zoomfunktion:%f\n", time);
			
			startTimeFunctions = (float)clock() / CLOCKS_PER_SEC;
			for(i=0; i < n; i++)
			{
				//Bildausschnitt mit der optimierten C-Zoomfunktion vergroessern
				unsigned char* zo = zoom1(bildWindow, height_window, width_window, scaleFactor);
				//Ergebniss wird geloescht, da die C-Funktion nur zum Benchmarkvergleich dient
				free(zo);
			}
			endTime = (float)clock() / CLOCKS_PER_SEC;
			time = endTime - startTimeFunctions;
			time /= n;		
			printf("Durchschnittiche Dauer der optimierten C Zoomfunktion:%f\n", time);
		}
		
		
		startTimeFunctions = (float)clock() / CLOCKS_PER_SEC;
		for(i=0; i < n; i++)
		{
			//Bildausschnitt vergoessern
			_asm_zoom(bildWindow, neuesArray, height_window ,width_window, scaleFactor);
		}
		endTime = (float)clock() / CLOCKS_PER_SEC;
		time = endTime - startTimeFunctions;
		time /= n;
		printf("Durchschnittiche Dauer der assembler Zoomfunktion:%f\n", time);
		time = 0;
		
		
		//Der ausschnitt vom Originalbild wird nicht  weiter benoetigt
		free(bildWindow);
		
		//Neue Hoehe und Breite berechnen
		bitmapInfoHeader->biwidth = width_window * scaleFactor;
		bitmapInfoHeader->biHeight = height_window * scaleFactor;

		//Neues Padding berechnen
		padd = (bitmapInfoHeader->biwidth * 3) % 4;
		
		unsigned char *ausgabe = NULL;
		//Padding wieder hinzufuegen
		//Codesegment wird nur ausgefuehrt falls Padding hinzugefuegt werden muss um die Average-Case-Laufzeit des Programms zu verbessern
		if(padd != 0)
		{
			//Speicher fuer das neue Array mit Padding alloziieren
			ausgabe = (unsigned char *)malloc(bitmapInfoHeader->biHeight * bitmapInfoHeader->biwidth * sizeof(struct PIXEL) + bitmapInfoHeader->biHeight * padd);
			int j;//Zaehlervariable
			for (i = 0; i < bitmapInfoHeader->biHeight; i++)
			{
				for (o = 0; o < bitmapInfoHeader->biwidth; o++)
				{
					//Daten vom vorherigen Array uebertragen
					//3mal fuer die drei verscheidenen Farbwerte
					ausgabe[i * bitmapInfoHeader->biwidth *3+ o*3 + i*padd] = neuesArray[i * bitmapInfoHeader->biwidth *3+ o*3];
					ausgabe[i * bitmapInfoHeader->biwidth *3+ o*3 + i*padd + 1] = neuesArray[i * bitmapInfoHeader->biwidth *3+ o*3 + 1];
					ausgabe[i * bitmapInfoHeader->biwidth *3+ o*3 + i*padd + 2] = neuesArray[i * bitmapInfoHeader->biwidth *3+ o*3 + 2];
				}

				//Bevor wir in die naechste Zeile springen, die Zeile mit Padding auffuellen
				for (j = 0; j < padd; j++)
				{
					ausgabe[i * bitmapInfoHeader->biwidth *3+ bitmapInfoHeader->biwidth * 3 + i * padd + j] = 0x0;
				}
			}
			//Daten werden nach dem kopieren nicht mehr benoetigt. So wird auch doppeltes Loeschen vermieden falls der else Fall eintritt
			free(neuesArray);
		}
		//Kein Padding damit ist Ausgabe = Neues Array
		else
		{
			ausgabe = neuesArray;
		}
		//Neue Datei oeffnen in der das neue Bild gespeichert wird
		FILE *newFile;
		newFile = fopen(newfilename, "w");
		
		//Groesse der Farbwerte neu berechnen
		bitmapInfoHeader->biSizeImage = bitmapInfoHeader->biwidth* bitmapInfoHeader->biHeight * 3 + bitmapInfoHeader->biHeight * padd;
		
		//Dateigroesse neu berechnen
		bitmapFileHeader->bfSize = bitmapFileHeader->bfOffBits + bitmapInfoHeader->biwidth* bitmapInfoHeader->biHeight * 3 + bitmapInfoHeader->biHeight * padd;
		
		//Daten in die neue Datei schreiben
		fwrite(bitmapFileHeader, sizeof(struct BITMAPFILEHEADER), 1, newFile);
		fwrite(bitmapInfoHeader, sizeof(struct BITMAPINFOHEADER), 1, newFile);
		fwrite(ausgabe, bitmapInfoHeader->biSizeImage, 1, newFile);

		//Alloziierten speicher wieder freigeben
		free(ausgabe);
		free(bitmapImage);
		free(bitmapFileHeader);
		free(bitmapInfoHeader);
		
		//Geoeffnete Dateien schliessen
		fclose(newFile);
		fclose(file);
		
		//Zeitpunkt zum Ende des Programms messen und Gesammtdauer ausgeben
		endTime = (float)clock() / CLOCKS_PER_SEC;
		time = endTime - startTime;
		printf("Dauer des Programms: %f", time);
	}
	else
	{
		printf("Dem Programm muessen 8 Parameter uebergeben werden:\n");
		printf("Dateiname des Bildes(muss eine Bmp Datei sein)\n");
		printf("Dateiname des Ergebnisses (wird erzeugt falls nicht vorhanden)\n");
		printf("X-Wert der Startposition des Bildausschnitts\n");
		printf("Y-Wert der Startposition des Bildausschnitts\n");
		printf("Breite des Bildausschnitts\n");
		printf("Hoehe des Bildausschnitts\n");
		printf("Skalierungsfaktor");
		printf("Anzahl an Durchlaeufen der zu benchmarkenden Funktionen. Die Ausgegebene Zeit ist die durchschnittliche Zeit bei n Durchlaeufen. Bei 1 werden nur die Assembler-Funktionen ausgefuehrt.");
	}

	printf("\n");

	return 0;
}
//C- Funktionen des Programms
//Vergroessert uebergebene Bilddaten um Faktor in beide Richtungen
unsigned char* zoom(unsigned char *data, int org_height, int org_width, int factor)
{
	//Hoehe und Breite des neuen Bildes berechnen
	int width = org_width * factor;
	int height = org_height * factor;
	//Speicher fuer das neue Bild alloziieren
	unsigned char *neu = (unsigned char*)malloc(width * height * sizeof(unsigned char) * 3);
	//Zaehlervariablen i und j fuer das neue Bild x und y fuer das alte
	int x, y, i, j;
	for (i = 0; i < height; i++)
	{
		for (j = 0; j < width; j++)
		{
			//Position des Pixels im alten Bild berechnen
			x = j / factor; //Abrundung ist gewollt
			y = i/ factor; //Abrundung ist gewollt
			//Daten vom alten Bild in das Scalierte kopieren
			neu[i*width*3 + j*3] = data[y*org_width*3 + x*3];
			neu[i*width*3 + j*3 + 1] = data[y*org_width*3 + x*3 + 1];
			neu[i*width*3 + j*3 + 2] = data[y*org_width*3 + x*3 + 2];
		}
	}
	//Pointer auf die neuen Bilddaten zurueckgeben
	return neu;
}

//Optimierte Version der Zoom-Funktion kommt ohne division aus
unsigned char* zoom1(unsigned char *data, int org_height, int org_width, int factor)
{
	//Hoehe und Breite des neuen Bildes berechnen
	int width = org_width * factor;
	int height = org_height * factor;
	//Speicher fuer das neue Bild alloziieren
	unsigned char *neu = (unsigned char*)malloc(width * height * sizeof(unsigned char) * 3);
	//Da wir im Verlauf der Funktion den Pointer veraendern werden sichern wir ihn hier
	unsigned char *safeNeu = neu;
	//Zaehlervariablen
	int i, j,o,p;
	for (i = 0; i < org_height; i++)
	{
		for (o = 0; o < factor; o++)
		{
			for (j = 0; j < org_width; j++)
			{
				for (p = 0; p < factor; p++)
				{
					//Die 3 Farbwerte an die neue Position kopieren
					*neu = *data;
					*(++neu) = *(++data);
					*(++neu) = *(++data);
					//Durch die insgesammte erhoehung von Neu um 3 gehen wir genau einen Pixel weiter
					neu += 1;
					//Data wieder auf den ersten Farbwert des Pixels setzen
					data -= 2;
				}
				//Zum naechsten Pixel im alten Array springen
				data += 3;
			}
			//Im alten Bild um eine Zeile zurueckspringen
			data -= org_width * 3;
		}
		//Im alten Bild um eine Zeile vorspringen
		data += org_width * 3;
	}
	//Den Pointer von neu wieder auf das erste Element im Array setzen
	neu = safeNeu;
	return neu;
}
//Erzeugt einen Ausschnitt von einem Bild
void window(unsigned char *data, int x, int y, int height, int width, unsigned char* newData, int orig_width)
{
	//Data auf die Stelle x y im Bild zeigen lassen
	data = data + 3*x + 3*orig_width*y;
	//Zaehlervariablen
	int h, w;
	//Ueber die uebergeben Flaeche im Bild iterieren
	for (h = 0; h<height; h++)
	{
		for (w = 0; w<width; w++)
		{
			//jeweils Farbwerte von Data in das neues Array kopieren.
			*newData = *data;
			data++;
			newData++;
			*newData = *data;
			data++;
			newData++;
			*newData = *data;
			data++;
			newData++;
		}
		//Data um eine Zeile verschieben
		data += 3*orig_width;
		//Data wider an den Anfang der Zeile vom gewuenschten Bildausschnitt setzen
		data -= 3*width;
	}
}
