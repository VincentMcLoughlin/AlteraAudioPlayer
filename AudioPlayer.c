
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

#include <system.h>
#include <sys/alt_alarm.h>
#include <io.h>

#include "fatfs.h"
#include "diskio.h"

#include "ff.h"
#include "monitor.h"
#include "uart.h"

#include "alt_types.h"

#include <altera_up_avalon_audio.h>
#include <altera_up_avalon_audio_and_video_config.h>

int previousFlag = 0;
int nextFlag = 0;
int playFlag = 0;
int stopFlag = 0;
int pauseFlag = 0;
int ffwd = 1;
int rwd = 0;

void put_rc(FRESULT rc) {
	const char *str =
			"OK\0" "DISK_ERR\0" "INT_ERR\0" "NOT_READY\0" "NO_FILE\0" "NO_PATH\0"
					"INVALID_NAME\0" "DENIED\0" "EXIST\0" "INVALID_OBJECT\0" "WRITE_PROTECTED\0"
					"INVALID_DRIVE\0" "NOT_ENABLED\0" "NO_FILE_SYSTEM\0" "MKFS_ABORTED\0" "TIMEOUT\0"
					"LOCKED\0" "NOT_ENOUGH_CORE\0" "TOO_MANY_OPEN_FILES\0";
	FRESULT i;

	for (i = 0; i != rc && *str; i++) {
		while (*str++)
			;
	}
	xprintf("rc=%u FR_%s\n", (uint32_t) rc, str);
}

int isWav( char *fileName){

	if(fileName == NULL){
		return 0;
	}
	int i = 0;
	while( fileName[i] != '.'){
		i++;
 	}

	if((fileName[i+1] == 'w' && fileName[i+2] == 'a' && fileName[i+3] == 'v') || fileName[i+1] == 'W' && fileName[i+2] == 'A' && fileName[i+3] == 'V'){
		return 1;
	}
	else
		return 0;

}int diskInitialize(){ // di 0
	(uint16_t) disk_initialize((uint8_t ) 0); // p1-->0
	return 1;
}int forceInitialize(FATFS * tmpFatAdr ){ // fi 0
	//f_mount((uint8_t) 0, tmpFatAdr );
	put_rc(f_mount((uint8_t) 0, tmpFatAdr )); // p1-->0, FATFS * tmpF&Fatfs[0]--> * tmpFatAdr
	return 1;
}int fileList(){ // fl
	DIR Dir;
	FILINFO Finfo;
	FRESULT res = f_opendir(&Dir, " ");
	for (;;){
		res = f_readdir(&Dir, &Finfo);
		if ((res != FR_OK) || !Finfo.fname[0])
			break;


		printf("%9lu  %s",Finfo.fsize,&(Finfo.fname[0]));
	}
	return 1;
}int songIndex(char fileName[][20], unsigned long int fileSize[20]){

	DIR Dir;
	FILINFO Finfo;
	FRESULT res = f_opendir(&Dir, "");
	int i = 0;
	for (;;) {
		res = f_readdir(&Dir, &Finfo);
		if ((res != FR_OK) || !Finfo.fname[0])
			break;

		if(isWav(Finfo.fname)){// //This is where the problem currently is, names and numbers not properly copying
			strcpy(fileName[i],Finfo.fname);// Finfo.fname--> "test"

			fileSize[i] = Finfo.fsize;
			i++;
		}
	}
	return i;
}FIL fileOpen(char *fileName){ //
	FIL File1; /* File objects */

	DIR Dir;
	FRESULT res = f_opendir(&Dir, " ");

	put_rc(f_open(&File1, fileName, 1));//p1-->uint8 1, ptr--> fileName
	return File1;
}
int filePlay(uint32_t numBytes, FIL FileToPlay, alt_up_audio_dev * audio_devTemp){
	uint32_t ofs = FileToPlay.fptr;
	uint32_t p1 = numBytes;	//p1 is the number of bytes to read from the file
	uint16_t soundBuffer = 2048;//256
	uint8_t Buff[8192] __attribute__ ((aligned(64))); /* Working buffer */
	uint32_t cnt= sizeof(Buff);
	FRESULT res;
	int debounce = 1;
	uint16_t l_buf1, l_buf2, r_buf1, r_buf2, index, l_buf, r_buf;
	int fifospacel,x;
	//int ffwd = 1;
	int buffersToPlay = 2;
	int revOffset =soundBuffer*buffersToPlay*2;
	//int rwd = 0;
	for (x=0;x++; x<100);

	while (p1 > 0 && playFlag ==1) {

		if (!pauseFlag){//!pauseFlag
			if(((0x0f)&(~IORD(BUTTON_PIO_BASE, 0))) != 0b00001000)
				rwd =0;
			if(((0x0f)&(~IORD(BUTTON_PIO_BASE, 0))) == 0b00000001)
				ffwd = 2;
			else
				ffwd = 1;
			if(!rwd){
				if ((uint32_t) p1 >= soundBuffer) { //General case, generally reading in bytes and bytes of P1
					cnt = soundBuffer;
					p1 -= soundBuffer;
				} else { //End case
					cnt = p1;
					p1 = 0;
				}
				res = f_read(&FileToPlay, Buff, cnt, &cnt); //have cnt and pointer to count because sometimes cnt will not be 16

				index = 0;
				while ( (index <soundBuffer) || (index < p1 && p1<soundBuffer)){
					fifospacel = alt_up_audio_write_fifo_space(audio_devTemp, ALT_UP_AUDIO_LEFT);
					if(fifospacel > 0){
						l_buf = ((uint16_t)Buff[index+1]<<8) | (uint16_t)Buff[index];
						r_buf = ((uint16_t)Buff[index+3]<<8) | (uint16_t)Buff[index+2];
						alt_up_audio_write_fifo(audio_devTemp, &(l_buf), 1,	ALT_UP_AUDIO_LEFT);
						alt_up_audio_write_fifo(audio_devTemp, &(r_buf), 1,	ALT_UP_AUDIO_RIGHT);
						index+=(4*ffwd);
						}
					}
			//ofs += soundBuffer;
			}
			else{// this is rewind code
				if ((numBytes -p1)>revOffset){
					p1 +=revOffset;
				}
				f_lseek(&FileToPlay, (numBytes - p1));
				for (x=0;x<buffersToPlay;x++){
					if ((uint32_t) p1 >= soundBuffer) { //General case, generally reading in bytes and bytes of P1
						cnt = soundBuffer;
						p1 -= soundBuffer;
					} else { //End case
						cnt = p1;
						p1 = 0;
					}
					res = f_read(&FileToPlay, Buff, cnt, &cnt); //have cnt and pointer to count because sometimes cnt will not be 16

					index = 0;
					while ( (index <soundBuffer) || (index < p1 && p1<soundBuffer)){
						fifospacel = alt_up_audio_write_fifo_space(audio_devTemp, ALT_UP_AUDIO_LEFT);
						if(fifospacel > 0){
							l_buf = ((uint16_t)Buff[index+1]<<8) | (uint16_t)Buff[index];
							r_buf = ((uint16_t)Buff[index+3]<<8) | (uint16_t)Buff[index+2];
							alt_up_audio_write_fifo(audio_devTemp, &(l_buf), 1,	ALT_UP_AUDIO_LEFT);
							alt_up_audio_write_fifo(audio_devTemp, &(r_buf), 1,	ALT_UP_AUDIO_RIGHT);
							index+=(4*ffwd);
						}
					}
				}
			}
		}
		else{ //paused

			while(pauseFlag == 1){
				if(stopFlag == 1 || previousFlag == 1 || nextFlag == 1){
					pauseFlag = 0;
					playFlag = 0;
					stopFlag = 0;

					return 0;
				}
			}
		}

	}
	return 1;

}static void button_ISR ( void* context, alt_u32 id){
	int debounce = 0;
	int debounceLength = 1200;
	int buttonsRead1 = IORD(BUTTON_PIO_BASE, 0);// 0x0f&(~IORD(BUTTON_PIO_BASE, 0)) ==0b00000001
	for(debounce = 0; debounce < debounceLength; debounce++);
	int buttonsRead2 = IORD(BUTTON_PIO_BASE, 0);
	if(buttonsRead1 != buttonsRead2)
		return;

	int buttonPressed = 0x0f&(~buttonsRead2);
	switch (buttonPressed){
		case 0b00001000 :
			//rewind
			if(playFlag == 0 || pauseFlag == 1){
				nextFlag = 0; //Added to make the transitions on the LCD a little more consistent
				previousFlag =1;
			}
			else if(playFlag == 1 && pauseFlag == 0){
				rwd = 1;
			}
				break;
		case 0b00000100 :
			//statements
			//stop
			stopFlag = 1;
			playFlag =0;
			break;
		case 0b00000010 :
			//play/pause
			if( playFlag ==0)// if this is initial play button press
				playFlag = 1;
			else if(pauseFlag ==0)//currently in playing state
				pauseFlag =1;
			else //currently in paused state
				pauseFlag =0;
			break;

		case 0b00000001 :
			//fast forward
			if(playFlag == 0){
				previousFlag =0;
				nextFlag =1;
			}
			else if(playFlag == 1 && pauseFlag == 0){
				int ffwd = 2;
			}
			else if(playFlag == 1 && pauseFlag == 1){
				stopFlag = 1;
				playFlag == 0;
				nextFlag = 1;
			}
			break;
	}
	IOWR(BUTTON_PIO_BASE, 3, 0x0);	//clear interrupt

}


int main(void)
{
	//alt_up_audio_dev * audio_dev;
	/* used for audio record/playback */

	alt_irq_register(BUTTON_PIO_IRQ, (void*)0, button_ISR);
	IOWR(BUTTON_PIO_BASE, 2, 0xF);
	char fileName[20][20];
	unsigned long int fileSize[20];
	int numFiles = 0;
	int i = 0;
	int currentSong = 5;
	alt_up_audio_dev * audio_dev;
	audio_dev = alt_up_audio_open_dev ("/dev/Audio");

	if ( audio_dev == NULL)
		alt_printf ("Error: could not open audio device \n");
	else
		alt_printf ("Opened audio device \n");
	FIL fileToPlay;
	FATFS Fatfs[_VOLUMES];
	diskInitialize();
	FATFS *tmpFatfs = &Fatfs[0];
	forceInitialize(tmpFatfs);
	uint32_t fsize= 626798;
	filePlay(fsize, fileToPlay,audio_dev);
	numFiles = songIndex(fileName, fileSize);
	for( i =0;i<numFiles;i++)
		printf("%s, %i\n",fileName[i], fileSize[i]);
	FILE *lcd;
	lcd = fopen("/dev/lcd_display", "w");
	fprintf(lcd, "%c%s", 27, "[2J");
	fprintf(lcd, "%lu. %s\n", currentSong,fileName[currentSong]);
	while(1){// running loop checks for button presses to change songs
		if(playFlag == 1){
			fileToPlay = fileOpen(fileName[currentSong]);
			filePlay(fileSize[currentSong],fileToPlay, audio_dev);
			playFlag = 0;
			stopFlag = 0;
		}else if(nextFlag == 1 ){//&& playFlag ==0
			if(currentSong == numFiles-1)
				currentSong = 0;
			else
				currentSong++;
			nextFlag = 0;
			fprintf(lcd, "%c%s", 27, "[2J");
			fprintf(lcd, "%lu. %s\n", currentSong,fileName[currentSong]);
		}else if(previousFlag == 1 ){//&& playFlag ==0
			if(currentSong == 0)
				currentSong = numFiles-1;
			else
				currentSong--;
			previousFlag = 0;
			fprintf(lcd, "%c%s", 27, "[2J");
			fprintf(lcd, "%lu. %s\n", currentSong,fileName[currentSong]);
		}

	}
	fclose( lcd );
}
