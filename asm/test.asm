	* = 0x1000
label:
	ldx #9
	ldy #11
	lda #$45
@req x=9,y=11,a=0x45
	sta $c000
@req 0xc000=0x45
	inc $c000
	lda 49152
	sec
	adc #3
@req a=0x4a
	sta $c001,x
	lda $c000,y
	sta $c002,y
	lda #$c0
	sta 7
	lda #$04
	sta $06
	lda #$4
	tay
	sta ($06),y
@req 0xc008=4
	rts


