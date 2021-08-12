;#Funktionen fuer die C Datei verfuegbar machen
.globl _asm_window
.globl _asm_zoom

;#32-bit Code
.code 32

;# void window(char *data, int x, int y, int height, int width, char* newData, int orig_width)
_asm_window:

push {r4 - r10, lr} ;# Register sichern

;# *data -> r0, x -> r1, y -> r2, height -> r3
;# Uebergebene Parameter vom Stack laden
ldr r4, [sp, #32]
ldr r5, [sp, #36]
ldr r9, [sp, #40]

;# width -> r4, newData -> r5, orig_width -> r9

mov r6, #0 ;# height counter
mov r7, #0 ;# width counter

mov r10, #3 ;# setze r10 auf 3 fuer die Multiplikationen

mul r8, r9, r10 ;# r8 gibt Bytelaenge einer Zeile im Ausgangsbild an

mul r1, r1, r10 ;# 3 Byte pro Pixel
add r0, r0, r1 ;# verschiebe *data um x*3 Byte
mul r2, r2, r10 ;# 3 Byte pro Pixel
mul r2, r2, r4 ;# eine Zeile hat width Pixel
add r0, r0, r2 ;# verschiebe *data um y*width*3 Byte		

mov r10, #0 ;# r10 enthaelt die Farbwerte

loop_width:
	;# Daten vom alten Array ins neue speichern
	;# durch die verwendung von ldrh koennen wir 2 Byte (=2Farben) auf einmal einlesen und uns einen Schritt sparen
	ldrh r10, [r0, #0]
	strh r10, [r5, #0]
	add r0, r0, #2
	add r5, r5, #2
	ldrb r10, [r0, #0]
	strb r10, [r5, #0]
	add r5, r5, #1
	add r0, r0, #1

	add r6, #1 ;# erhoehe den Counter um 1
	cmp r6, r4
	bne loop_width ;# falls Ende der Schleife nicht erreicht: springe an den Anfang der Schleife

loop_height:		
	add r0, r0, r8 ;# addiere Laenge einer Zeile der Ausgangsdatei auf *data
	
	mov r6, r4
	mov r10, #3
	mul r6, r6, r10 ;# speichere Laenge einer ausgeschnittenen Zeile in r6
	sub r0, r0, r6 ;# ziehe Laenge einer ausgeschnittenen Zeile von r0 ab

	mov r6, #0 ;# setze width-Counter zurueck

	add r7, r7, #1	;# inkrementiere height-counter
	cmp r7, r3
	bne loop_width ;# falls Ende der Schleife nicht erreicht: springe an den Anfang der Schleife	

end:
	;#Gesicherte Register wiederherstellen und in die Vorherige Funktion zurueckspringen
	pop {r4 - r10, pc}
	
	
;#Fenster vergroesseren in Assembler
;#void _asm_zoom(unsigned char *data, unsigned char *neu, int org_height, int org_width, int factor)
_asm_zoom:
	;# Register auf dem Stack sichern
	push {r4 - r10, lr}
	;# Den Parameter der auf dem Stack liegt in ein Register schreiben
	ldr r4, [sp, #32] ;#r4 = factor
	;#r7-r10 sind die Zaehlervariablen
	mov r7, #0 ;#r7 = i
	oheight:
		mov r8, #0 ;#r8 = o
		factor1:
			mov r9, #0 ;#r9 = j
			owidth:
				mov r10, #0 ;#r10 = p
				factor2:
					;#Drei Farbwerte vom alten Bild ins neue kopieren
					;#Durch ldrh sparen wir uns wie in window einen Schritt
					ldrh r6, [r0, #0]
					strh r6, [r1, #0]
					add r0, r0, #2
					add r1, r1, #2
					ldrb r6, [r0, #0]
					strb r6, [r1, #0]
					;#r1 auf den Anfang des naechsten Pixels setzen
					add r1, r1, #1
					;#r0 wieder auf den Anfang dieses Pixels setzen
					sub r0, r0, #2
				
				;#Schleife factor2
				add r10, r10, #1
				cmp r4, r10
				bne factor2
				
				;#Zum naechsten Pixel im alten Array springen
				add r0, r0, #3
				
			;#Schleife owidth
			add r9, r9, #1
			cmp r3, r9
			bne owidth
			
			;#Im alten Bild um eine Zeile zurueckspringen
			mov r6, #3
			mul r6, r3, r6
			sub r0, r0, r6
		
		;#Schleife factor1
		add r8, r8, #1
		cmp r4, r8
		bne factor1
		
		;#Im alten Bild eine Zeile vorspringen
		mov r6, #3
		mul r6, r3, r6
		add r0, r0, r6
				
	;#Schleife oheight
	add r7, r7, #1
	cmp r2, r7
	bne oheight
	
end2:
	;#Gesicherte Register wiederherstellen und in die vorherige Funktion zurueckspringen
	pop {r4 - r10, pc}
