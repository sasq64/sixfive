	* = 0x1000
label:
	ldx #9
	ldy #11
	lda #$45
	sta $c000
	lda 49152
	sta $c001,x
	lda $c000,y
	sta $c002,y
	lda #$c0
	sta 7
	lda #$04
	sta $06
	lda #$e2
	sta ($06),y
	rts


