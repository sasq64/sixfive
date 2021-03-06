;THIS SUBROUTINE ARRANGES THE 8-BIT ELEMENTS OF A LIST IN ASCENDING
;ORDER.  THE STARTING ADDRESS OF THE LIST IS IN LOCATIONS $30 AND
;$31.  THE LENGTH OF THE LIST IS IN THE FIRST BYTE OF THE LIST.  LOCATION
;$32 IS USED TO HOLD AN EXCHANGE FLAG.

	LDA #LIST>>8
	STA $31
	LDA	#LIST&0xff
	STA	$30
	JSR	SORT8

	LDA LIST
	TAX
LOOP
	LDA LIST+1,X
	DEX
	BNE LOOP

	RTS
		
LIST
		DB 8
		DB 9,5,2,12,1,15,8,2

SORT8    LDY #$00      ;TURN EXCHANGE FLAG OFF (= 0)
         STY $32
         LDA ($30),Y   ;FETCH ELEMENT COUNT
         TAX           ; AND PUT IT INTO X
@req x=8
         INY           ;POINT TO FIRST ELEMENT IN LIST
         DEX           ;DECREMENT ELEMENT COUNT
NXTEL    LDA ($30),Y   ;FETCH ELEMENT
         INY
         CMP ($30),Y   ;IS IT LARGER THAN THE NEXT ELEMENT?
         BCC CHKEND
         BEQ CHKEND
                       ;YES. EXCHANGE ELEMENTS IN MEMORY
         PHA           ; BY SAVING LOW BYTE ON STACK.
         LDA ($30),Y   ; THEN GET HIGH BYTE AND
         DEY           ; STORE IT AT LOW ADDRESS
         STA ($30),Y
         PLA           ;PULL LOW BYTE FROM STACK
         INY           ; AND STORE IT AT HIGH ADDRESS
         STA ($30),Y
         LDA #$FF      ;TURN EXCHANGE FLAG ON (= -1)
         STA $32
CHKEND   DEX           ;END OF LIST?
         BNE NXTEL     ;NO. FETCH NEXT ELEMENT
         BIT $32       ;YES. EXCHANGE FLAG STILL OFF?
         BMI SORT8     ;NO. GO THROUGH LIST AGAIN
         RTS           ;YES. LIST IS NOW ORDERED

