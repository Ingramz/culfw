/*  * ----------------------------------------------------------------------------------------------------------------------------------------------------* * This is my first trial to send to Kopp Free Control Units via CUL CCD Module *  * Remaining lines from former MORITZ Modul, can be deleted as soon I understand meaning :-) *    * CC1100_PKTCTRL0.LENGTH_CONFIG = 1 //Variable packet length mode. Packet length configured by the first byte after sync word                         *  *                 CRC_EN = 1                                                                                                                          * *                 PKT_FORMAT = 00 //Use FIFOs                                                                                                         * *                 WHITE_DATA = 0                                                                                                                      * * MDMCFG2.SYNC_MODE = 3: 30/32 sync word bits detected                                                                                                * *        .MANCHESTER_EN = 0                                                                                                                           * *        .MOD_FORMAT = 0: 2-FSK                                                                                                                       * *        .DEM_DCFILT_OFF = 0                                                                                                                          * *                                                                                                                                                     * * EVENT0 = 34667                                                                                                                                      * * t_Event0 = 750/26Mhz * EVENT0 * 2^(5*WOR_RES) = 1 second                                                                                            * *                                                                                                                                                     * * One message with 12 payload bytes takes (4 byte preamble + 4 byte sync + 12 byte payload) / 1kbit/s = 160 ms.                                       * *																																					   * * Date		   Who				Comment																												   * * ----------  -------------   	-----------------------------------------------------------------------------------------------------------------------* * 2014-08-08  Claus M.			Now transmitting one block is working with Kopp Free Control protocol, key Code from Input parameter (1 character) * 2014-08-01  Claus M.			first Version * *  * ----------------------------------------------------------------------------------------------------------------------------------------------------* */#include "board.h"#ifdef HAS_KOPP_FC#include <stdlib.h>				// for strtol#include <string.h>#include <avr/pgmspace.h>#include <avr/interrupt.h>#include <avr/io.h>#include "cc1100.h"#include "delay.h"#include "rf_receive.h"#include "display.h"#include "clock.h"#include "rf_send.h" //credit_10ms//#include "rf_moritz.h"#include "kopp-fc.h"#include "fncollection.h"void kopp_fc_sendraw(uint8_t* buf, int longPreamble);void kopp_fc_sendAck(uint8_t* enc);void kopp_fc_handleAutoAck(uint8_t* enc);#define MCSM1_Init  		0x00    // CCA_Mode: Always; Receive: Idle after finisch package reception; Transmitt: Transmitter goes idle after finisching package transmission#define MCSM1_TXOFF_Mask  	0xFC    // MCSM1 And Mask for Transmitt Off behavior#define MCSM1_TXOFF_Idle  	0x00    // Transmitt: Transmitter goes idle after finisching package transmission#define MCSM1_TXOFF_TX  	0x02    // Transmitt: Transmitter stays active and sends preamble again after finisching package transmissionuint8_t kopp_fc_on = 0;uint8_t blkctr;char ErrorMSG[] ="ok";// Kopp Free-Control Inititalisieren// ==================================const PROGMEM const uint8_t CC1100_Kopp_CFG[EE_CC1100_CFG_SIZE] = {//  CC1101 Register Initialisation (see CC1101 Page 70ff and 62ff)//  Data   		Adr  Reg.Name RESET STUDIO COMMENT// ======  		==== ======== ===== ====== =================================================================================================================================	0x07, 		// 00  IOCFG2   *29   *0B    GDO2_CFG=7: GDO2 Asserts when a packet has been received with CRC OK. De-asserts when the first byte is read from the RX FIFO	0x2E, 		// 01  IOCFG1    2E    2E    no change yet	0x46,		// 02  IOCFG0   *3F   *0C    GDO0_CFG=2: Associated to the TX FIFO: Asserts when the TX FIFO is filled at or above the TX FIFO threshold. 				//                   		   De-asserts when the TXFIFO is below the same threshold.   	0x07, 		// 03  FIFOTHR   07   *47	0xAA, 		// 04  SYNC1     D3    D3    Sync High Byte = AA (assumption: High Byte is first send sync byte)	0x54, 		// 05  SYNC0     91    91    Sync Low  Byte = 54 (AA 54 sollte als Sync funktinieren)	0x0F, 		// 06  PKTLEN   *FF    3D    Package length for Kopp 15 Bytes (incl. Cks, because handled as data because no standard CC1101 checksum)	0xE0, 		// 07  PKTCTRL1  04    04    Preamble quality is maximum(7), No Auto RX Fifo Flush, No Status Bytes will be send, No Address check	0x00, 		// 08  PKTCTRL0 *45    32    Data whitening off,  Rx and Tx Fifo, CRC disabled, Fixed package length	0x00, 		// 09  ADDR      00    00    Device Adress (Address filter not used)	0x00, 		// 0A  CHANNR    00    00    Channel Number (added to Base Frequency) is not used	0x06, 		// 0B  FSCTRL1  *0F    06    152,34375 kHz IF Frquency (##Claus: to be adjusted for Kopp, later if RX is used)	0x00, 		// 0C  FSCTRL0   00    00    Frequency Offset = 0	0x21, 		// 0D  FREQ2    *1E    21    FREQ[23..0] = f(carrier)/f(XOSC)*2^16  -> 868,3Mhz / 26Mhz * 2^16 = 2188650 dez = 21656A hex  (f(XOSC)=26 Mhz)	0x65, 		// 0E  FREQ1    *C4    65    s.o.	0x6A, 		// 0F  FREQ0    *EC    e8    s.o.	0x97, 		// 10  MDMCFG4  *8C    55    bWidth 162,5 kHz   (Kopp 50 Khz!, but does not work))	0x82, 		// 11  MDMCFG3  *22   *43    Drate: 4785,5 Baud   (Kopp: 4789 Baud, measured value !! may be increase by 1 is needed (83) because value should be 4800) )	0x16, 		// 12  MDMCFG2  *02   *B0    DC Blocking filter enabled, GFSK modulation (Kopp uses FSK, do not know whether 2-FSK, GFSK odr 4-FSK), 				//                           manchester en-decoding disabled, 16 sync bits to match+carrier-sense above threshold	0x63, 		// 13  MDMCFG1  *22    23    Error Correction disabled, min 16 preamble bytes, Channel spacing = 350 khz	0xb9, 		// 14  MDMCFG0  *F8    b9    Channel spacing 350kHz  (Copied from somfy, do not know if ok ) 	0x47, 		// 15  DEVIATN  *47    00    frequency deviation = 47,607 khz (default, do not know if right, for RFM12b I used 45khz)	0x07, 		// 16  MCSM2     07    07    	MCSM1_Init, // 17  MCSM1     30    30    see above	0x29, 		// 18  MCSM0    *04    18    Calibration after RX/TX -> IDLE, PO_Timeout=2 ##Claus 0x01: Oszillator always on for testing	0x36, 		// 19  FOCCFG   *36    14	0x6C, 		// 1A  BSCFG     6C    6C	0x07, 		// 1B  AGCCTRL2 *03   *03    42 dB instead of 33dB	0x40, 		// 1C  AGCCTRL1 *40   *40	0x91, 		// 1D  AGCCTRL0 *91   *92    	0x87, 		// 1E  WOREVT1   87    87	0x6B, 		// 1F  WOREVT0   6B    6B	0xF8, 		// 20  WORCTRL   F8    F8	0x56, 		// 21  FREND1    56    56	0x11, 		// 22  FREND0   *16    17   0x11 for no PA ramping (before 16, this was the reason why transmission didn't run)	0xE9, 		// 23  FSCAL3   *A9    E9   as calculated by Smart RF Studio	0x2A, 		// 24  FSCAL2   *0A    2A   as calculated by Smart RF Studio	0x00, 		// 25  FSCAL1    20    00   as calculated by Smart RF Studio	0x1F, 		// 26  FSCAL0    0D    1F   as calculated by Smart RF Studio	0x41, 		// 27  RCCTRL1   41    41	0x00, 		// 28  RCCTRL0   00    00};// static uint8_t autoAckAddr[3] = {0, 0, 0};// static uint8_t fakeWallThermostatAddr[3] = {0, 0, 0};// static uint32_t lastSendingTicks = 0;voidkopp_fc_init(void){  EIMSK &= ~_BV(CC1100_INT);                 	// disable INT - we'll poll...  SET_BIT( CC1100_CS_DDR, CC1100_CS_PIN );   	// CS as output// Toggle chip select signal (why?)  CC1100_DEASSERT;                            	// Chip Select InActiv  my_delay_us(30);  CC1100_ASSERT;								// Chip Select Activ  my_delay_us(30);  CC1100_DEASSERT;								// Chip Select InActiv  my_delay_us(45);  ccStrobe( CC1100_SRES );                   	// Send SRES command (Reset CC110x)  my_delay_us(100);// load configuration (CC1100_Kopp_CFG[EE_CC1100_CFG_SIZE])    CC1100_ASSERT;								// Chip Select Activ	 cc1100_sendbyte( 0 | CC1100_WRITE_BURST );	 for(uint8_t i = 0; i < EE_CC1100_CFG_SIZE; i++) {	 	cc1100_sendbyte(__LPM(CC1100_Kopp_CFG+i));	 } 	CC1100_DEASSERT;							// Chip Select InActiv  // setup PA table (-> Remove as soon as transmitting ok?)	uint8_t *pa = EE_CC1100_PA;	CC1100_ASSERT;	cc1100_sendbyte( CC1100_PATABLE | CC1100_WRITE_BURST);	for (uint8_t i = 0; i < 8; i++) {		cc1100_sendbyte(erb(pa++));	}	CC1100_DEASSERT;	// Set CC_ON	ccStrobe( CC1100_SCAL);						// Calibrate Synthesizer and turn it of. ##Claus brauchen wir das	my_delay_ms(1);	cc_on = 1;  //This is ccRx() but without enabling the interrupt//  uint8_t cnt = 0xff;  //Enable RX. Perform calibration first if coming from IDLE and MCSM0.FS_AUTOCAL=1.  //Why do it multiple times?//  while(cnt-- && (ccStrobe( CC1100_SRX ) & 0x70) != 1)//    my_delay_us(10);  kopp_fc_on = 1;								//##Claus may be not needed in future (Tx Only)}// kopp_fc_init  E N D// ======================================================================================================// Kopp Free-Control Maint task (at least my assumtion it is :-) )// -------------------------------------------------------------------------------------------------------voidkopp_fc_func(char *in){  uint8_t blkTXcode=0x00;   uint8_t inhex_dec[MAX_kopp_fc_MSG];					// in_decbin: decimal value of hex commandline  uint8_t hblen = fromhex(in+2, inhex_dec, strlen(in));// If parameter 2 = "t" then  "Transmitt Free Control Telegram"   strcpy(ErrorMSG,"ok"); if(in[1] == 't')   {  if ((hblen*2 == kopp_fc_Command_char) && (fromhex(in+2, inhex_dec, 2)==2))		//##Claus hier noch eingreifen, falls nicht nur Hex Char. übergeben werden   {         											// Transmitt Block      DS_P(PSTR("Transmitt\r\n"));														// Some status Information   DS_P(PSTR("commandlineparameter: "));   DS(in);   DS_P(PSTR("\r\nStringlength: "));   DU(strlen(in),0);   DS_P(PSTR("\r\nCharacter after parameter: "));   DU((int)in[strlen(in)],0);   DS_P(PSTR("\r\nAmount of Hex char found inside command line parameter: "));   DU(hblen,0);   DS_P(PSTR("\r\n"));     kopp_fc_init();										// Init CC11xx for Kopp Free Control protocol	//	int fd;//	fd = open(device, O_RDWR);//	if (fd < 0)//      pabort("can't open device");// Command Line Argument 1 (Hex Char) for "Block Transmitt Code" (uint8_t)// -----------------------------------------------------------------------                                                                             //// blkTXcode = (uint8_t) strtol(in[1],NULL,0);                                         // Int wird hier automatisch nach uint8_t convertiert (literatur)//blkTXcode = (uint8_t) in[2];                                                         // Int wird hier automatisch nach uint8_t convertiert (literatur)blkTXcode=inhex_dec[0];																	//Transmitt Code 	                                                                               ////#ifdef PrintOn  // Print Befehle müssen für CCD vermutlich angepasst werden                                                                        ////printf("Kommandozeilenargument 1: %d\n",blkTXcode);                                   ////#endif                                                                                //// Command Line Argument 2 (Unsingned Integer) to "Time till Key off [µsec)" (uint)// -------------------------------------------------------------------------------- // uint KeyOffTime;                                                                      //// KeyOffTime = (uint) strtol(argv[2],NULL,0);                                           // Int wird hier automatisch nach uint8_t convertiert (literatur)                                                                                      //// #ifdef PrintOn                                                                        //// printf("Kommandozeilenargument 2: %u\n",KeyOffTime);                                   //// #endif                                                                                //                                                                                      //// Test Timer Größe (Ergebnis: 32 Bit, zählt bis 4294967295, danach Überlauf auf 0)//   do {                                                                         // //   BlockStartTime=TIMER_ARM_COUNT;//   printf("Aktuelle Zeit:  %u\n",BlockStartTime);//   } while(TIMER_ARM_COUNT >= (BlockStartTime));                     // wait till Key Off time is gone////   printf("Aktuelle Zeit:  %u\n",TIMER_ARM_COUNT);//   printf("Aktuelle Zeit - Blockstartzeit:  %u\n",TIMER_ARM_COUNT-BlockStartTime);//do {//} while (1);// Botschaft für kurzen Tastendruck aufbauen//-----------------------------------------// Kurzer Tastendruck, kleiner Handsender, Taste 1// Texas RF Studio (sniffer) always adds a "0" after first byte received (also with Kopp transmitter, may be a config issue of RF Studio)uint8_t DefaultKoppMessage[15] =  {0x7, 0xc8, 0xf9, 0x6e, 0x30, 0xcc, 0xf, 0x1, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};//                                |lgth|TrmitterCode|Cnt |KeyNo|----unknown----|CkSum|----------6x00---------------|// Byte:                            0     1     2     3     4     5     6     7    8    9    10   11   12   13   14  // keep in mind, CC110x will send Preamble automatically !!!!// Langer Tastendruck, kleiner Handsender, Taste 1// uint8_t DefaultKoppMessage [35] = {0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0x54, 0x7, 0xc8, 0xf9, 0x6e, 0xb0, 0xcc, 0xf, 0x1, 0x80, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0};//                                    |--------------------------------------- Präambel 17 x AA ------------------------------------------|Header|lgth|TrmitterCode|Cnt|KeyNo|----unknown----|CkSum|----------6x00--------------|// Byte:                                0     1     2     3     4     5     6     7     8     9     10    11    12    13    14   15    16    17    18    19    20    21    22    23   24   25   26   27   28   29   30   31   32// send the whole data block                                                                                         // --------------------------void TransmittKoppBlk(uint8_t[], uint8_t);TransmittKoppBlk(DefaultKoppMessage, blkTXcode);// If TX Code was >=0x80 -> Long Key preasure, now send 2x Key off// ------------------------------------------------------------//##Claus will test one block send first before long key preasure//if (blkTXcode >= 0x80) {                                                        //// blkTXcode = 0xf7;                                                               // Key Off Code//   do {                                                                         // //   } while(((TIMER_ARM_COUNT-BlockStartTime)&0xffffffff) <= KeyOffTime);        // wait till Key Off time is gone  (0xfff.... to compensate timer overflow / negative values, not sure wheter needed)//   TransmittKoppBlk(DefaultKoppMessage, blkTXcode);                             // Send 1st Key Off Block //   do {                                                                         // //   } while(((TIMER_ARM_COUNT-BlockStartTime)&0xffffffff) <= 160275);            // wait for 160,275 ms    (as measured with receiver)//   TransmittKoppBlk(DefaultKoppMessage, blkTXcode);                             // Send 2nd Key Off Block    // }//   close(fd);//   return 0 					##Claus not needed for CCD, because not a subroutine here// Main Ende !!!!! (to be clarified whether code below needed//--------------------------------------------------------------------------------------------------------------------------------------//####Claus neuer Code von Moritz, ggf. noch anpassen //     uint8_t dec[MAX_kopp_fc_MSG];//     uint8_t hblen = fromhex(in+2, dec, MAX_kopp_fc_MSG-1);//     if ((hblen-1) != dec[0]) //	    {//       DS_P(PSTR("LENERR\r\n"));//       return;//      }//     kopp_fc_sendraw(dec, in[1] == 's');  //##Claus ??????   }    else 											// Sub Command <> "t"   {                          // Off    DS_P(PSTR("Kopp Transmitt Command not equal to "));    DU(kopp_fc_Command_char,0);    DS_P(PSTR(" Bytes or Keycode is no hex value\r\n"));    kopp_fc_on = 0;   }  }  else 											// Sub Command <> "t"  {                          // Off    DS_P(PSTR("Kopp SubCommand Unknown\r\n"));    kopp_fc_on = 0;  }}// Main Ganz Ende !!!!! (to be clarified whether code below needed//===========================================================================================================================================// Transmitt data block for Kopp Free Control// ------------------------------------------------------------------------------------------------------------------------------------------void TransmittKoppBlk(uint8_t sendmsg01[15], uint8_t blkTXcode_i){// Read Blockcounter from Config File Datei (RAMDISK) // --------------------------------------------------uint32_t BlockStartTime;uint8_t blkcks = 0x0;#ifdef PrintOnint i = 0;//printf("passiert was?\n");#endif//uint8_t koppconfig[10];                                                         // Wir gehen mal von 10 config Bytes aus//uint8_t blkctr=0x6e; ##Claus till I've located counter at RAMDISK or EEPROM we use a global RAM variable (defined above)                                                                                // Byte 0 wird der Blockzähler  // FILE *fp;                                                                                // laut Internet ist das Verzeichnis "/run/shm/" bei Raspberry Pi eine frei nutzbare RAMDISK//    if ((fp=fopen("/run/shm/koppconfig.dat","w")))                            // fp != NULL ?  (-> Kein Fehler) Datei für lesen und schreiben öffnen                                                                             //    fp=fopen("/run/shm/koppconfig.dat","r+");                                   //  kommentar ist falschfp != NULL ?  (-> Kein Fehler) Datei für lesen und schreiben öffnen//     if (fp == NULL) {//      fp=fopen("/run/shm/koppconfig.dat","w+");                                 // wenn Datei nicht da ist, neue Datei zum lesen/schreiben anlegen//     }                                                                                                                                                  //                                                                                // Wenn beim öffnen der Datei alles ok//      fseek(fp, 0, SEEK_SET);                                                   // Dateizeiger auf Dateianfang stellen                            //      fread(koppconfig, sizeof(uint8_t),sizeof(koppconfig),fp);                 // 10 Config Bytes lesen#ifdef PrintOn //     fprintf(stderr,"Anzahl gelesener Daten: %d\n", i);                        // //     fprintf(stderr,"BlockCounter alt (vom letzten Schreiben): %02x\n", blkctr);                          //     fprintf(stderr,"Wert: "); //     for(i = 0; i < sizeof(koppconfig); i++) //     fprintf(stderr," %02x",koppconfig[i]); //     fprintf(stderr,"\n");#endif//      blkctr=koppconfig[0];#ifdef PrintOn//      fprintf(stderr,"BlockCounter aus Datei: %02x\n", blkctr);                  //#endif//      koppconfig[0]=++blkctr;//      fseek(fp, 0, SEEK_SET);                                                    // Dateizeiger auf Dateianfang stellen                            //      fwrite(&koppconfig, sizeof(uint8_t),sizeof(koppconfig),fp);                // 10 Config Bytes schreiben#ifdef PrintOn  //     fprintf(stderr,"BlockCounter aus Datei: %02x\n", blkctr);                  //#endif       //     if (fclose(fp)==EOF) //     { //        fprintf(stderr,"\nFehler beim Schließen der Datei\n");                  // Fehler beim Dateischliessen //     }  // Here we will try to send the data via Fifo // -------------------------------------------//    int kbhit(void);//     uint8_t blkcks = 0x0;#ifdef PrintOn//     uint timestamp = 0;//     uint timestampold;//     timestampold = TIMER_ARM_COUNT;#endif          int count = 0;     int count2 = 1;//     int nIRQ  = 0;//     uint16_t status;//do {	                                                       // wird benötigt wenn Taste wieder abgefragt wird 								//     status = send_command16(fd, CMD_STATUS); 			           // status lesen  (nIRQ=high)#ifdef PrintOn//     printf("Status: %04x",status);//     printf(" Daten: PreambleAA");#endif //     send_command16(fd, cmd_config_TX_R_ON);			             // Tx Register enabled (el=1))//     send_command16(fd, cmd_power_Tx);				                 // Transmitter Einschalten (et=1, jetzt sollten 2x AA automatisch gesendet werden)#ifdef PrintOn//     printf(" Zeitstempel: %u\n",TIMER_ARM_COUNT);             // Ausgabel Zeitstempel#endif//     usleep(5000);						                               // msec Pause zwischen den Signalen    count2=1;													// each block / telegram will be written n times (n = 13 see below)   sendmsg01[3] = blkctr;                                   	// Write BlockCounter (was incremented) and Transmitt Code (=Transmitter Key) to Array   sendmsg01[4] = blkTXcode_i;                              	// -----------------------------------------------------------------------------------   																																//	CC1100_ASSERT;													// after each block is sent, transmitter to restart sending preamble (till last block)//	cc1100_sendbyte(CC1100_MCSM1);									// ===================================================================================//	cc1100_sendbyte((MCSM1_Init&MCSM1_TXOFF_Mask)|MCSM1_TXOFF_TX);	// (not used yet, may be used to optimize inter block behavior (send block without any pause)//	CC1100_DEASSERT;												//// Send Block via Transmitter Fifo// --------------------------------   do {#ifdef PrintOn  //       printf("Block Nr. %d\n",count2+1);#endif                          							//   BlockStartTime = TIMER_ARM_COUNT;                          // remember Start Time   ccTX();														// initialize CC110x TX Mode?   if(cc1100_readReg( CC1100_MARCSTATE ) != MARCSTATE_TX) 		// If Statemachinenot MARCSTATE_TX -> error   {     DC('T');    DC('X');    DC('_');    DC('I');    DC('N');    DC('I');    DC('T');    DC('_');	DC('E');    DC('R');    DC('R');    DC('_');    DH2(cc1100_readReg( CC1100_MARCSTATE ));    DNL();    kopp_fc_init();    return;   }   BlockStartTime = ticks;                           			// remember Start Time (1 tick=8msec, s.o.)   blkcks=0xaa;                                                 // Checksumme initialisieren   count=0;                                                    	//   CC1100_ASSERT;												// Chip Select Activ   cc1100_sendbyte(CC1100_WRITE_BURST | CC1100_TXFIFO);			// Burst Mode via Fifo                                                                																// Now send !  do {	                                                        // ===========         cc1100_sendbyte(sendmsg01[count]);					    // write date to fifo (fifo is big enough)//         if (count >= 0)                                     	// Die Checksumme wird über Byte[1..8] gerechnet (+1x 0xAA))//		 {          if (count <= 8)                                    	//		  {           blkcks=blkcks^sendmsg01[count];                    	//           if (count==7) sendmsg01[8]=blkcks;               	// Sobald bekannt Cks in den Buffer schreiben           }                                                    ////        }                                                     //																//  	   count++;                                                	// 	   } while(count <= 14);                                   //  CC1100_DEASSERT;												 // Chip Select InActiv  //Wait for sending to finish (CC1101 will go to RX state automatically  //after sending  uint8_t i;  for(i=0; i< 200;++i)   {    if( cc1100_readReg( CC1100_MARCSTATE ) == MARCSTATE_RX)      break; //now in RX, good    if( cc1100_readReg( CC1100_MARCSTATE ) != MARCSTATE_TX)      break; //neither in RX nor TX, probably some error    my_delay_ms(1);  }    count2++;  //  if (count2 == 13)                                    				// after last block is sent, transmitter to go idle (no more preambles)//   {    																// ====================================================================//    CC1100_ASSERT;													//(not used yet, may be used to optimize inter block behavior (send block without any pause)//	cc1100_sendbyte(CC1100_MCSM1);										////	cc1100_sendbyte((MCSM1_Init&MCSM1_TXOFF_Mask)|MCSM1_TXOFF_Idle);	////	CC1100_DEASSERT;													////   }                                                    				//	  } while(count2 <= 13);        	                                // send same message 13x     //error ?  	//  if(cc1100_readReg( CC1100_MARCSTATE ) != MARCSTATE_RX) //  { //    DC('T');//    DC('X');//    DC('E');//    DC('R');//    DC('R');//    DH2(cc1100_readReg( CC1100_MARCSTATE ));//    DNL();//    kopp_fc_init();//  }//     count2++;// } while(count2 < 13);                                         // send same message 13x                                                                //                                                             blkctr++;  													// increase Blockcounter // kein Return, da  void// return 0;}// Transmitt data block for Kopp Free Control - end - //===========================================================================================================================================#endif