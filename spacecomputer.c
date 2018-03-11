//RAINBOW 1 RTTY program

//we are using RADIOPIN 4
#define RADIOPIN 4

#include <stdio.h>
#include <stdint.h>
#include "wiringPi.h"
#include <string.h>
#include <dirent.h>
#include <stdlib.h>
#include <time.h>

//extras (for sleeping)
#include <unistd.h>

FILE *f;

char datastring[1000];
char final[1100];
/*
$$CALLSIGN,count,time,latitude,longitude,altitude,v_speed,l_speed,bearing,satellites,outside temp,inside temp, outside pressure, outside humidity,pictures taken,status code*002A
$$RAINBOW1,123,12:11:11,52.22222,-1.111111,35000,3.5,10.1,273,8,-40.1,20, 500.4, 56.4%, 123,0*002A\n
100 characters
 */
// 17 variables
char *dollars = "$$"; //constant (works)
char *callsign = "RAINBOW1"; //constant (works)
char countstring[100] = "1234"; //increasing by 1 every transmission (works)
char *timenow = "12:11:15"; //time from GPS  (works from local time)
char *latitude = "52.2222222"; //LAT from GPS (hard-coded, doesn't work yet)
char *longitude = "-1.111111"; //LONG from GPS (hard-coded, doesn't work yet)
char *altitude = "35123"; //ALT from GPS (works (pressure altitude))
char *v_speed = "3.5"; //V_SPEED from GPS (does not work)
char *l_speed = "10.1"; //L_SPEED from GPS (does not work)
char *bearing = "273"; //bearing from GPS (does not work)
char *satellites = "20"; //satellites from GPS (does not work)
char *temp_o = "-42.5"; //temp from temp/pressure combo (works)
char *temp_i = "13.4"; //temp from internal Dallas temp (does not work)
char *pressure = "1192.12"; //pressure from temp/pressure combo (works)
char *humidity = "56.4"; //humidity from  the humidity sensor (does not work)
unsigned int pics_taken = 0; //how many pictures have we taken (works)
char *status_code = "0"; //status codes, 0 - default, all ok, 1 - ascending, 2 - descending, 3 - minor problem (restart), 4 - major problem(exceptions), 5 - catastrophic error(how can you even transmit) 
//status code does not work
unsigned int count = 0;



//CRC helper method

uint16_t crc_xmodem_update(uint16_t crc, uint8_t data) {
    int i;

    crc = crc ^ ((uint16_t) data << 8);
    for (i = 0; i < 8; i++) {
        if (crc & 0x8000)
            crc = (crc << 1)^ 0x1021;
        else
            crc <<= 1;
    }
    return crc;
}

uint16_t gps_CRC16_checksum(char *string) {
    size_t i;
    uint16_t crc;
    uint8_t c;

    crc = 0xFFFF;

    for (i = 2; i < strlen(string); i++) {
        c = string[i];
        crc = crc_xmodem_update(crc, c);
    }
    return crc;
}

int get_pics_taken(){
        char *picfolder = "/home/pi/PICS"; 
	int file_count = 0;
	DIR *dir;
	struct dirent *entry;
	if ((dir = opendir(picfolder)) == NULL){
		fprintf(stderr, "exception in get_pics_taken method: can't open %s\n", dir);
		return;
	}

	while ((entry = readdir(dir)) !=NULL){
		if (entry->d_type == DT_REG){
			file_count++;
		}
	}
	closedir(dir);

  return file_count;
}

char* getField(char *filepath, int position){

	char *result;
	char fullstring[1000];
	FILE *file;
	file = fopen(filepath,"r");
	if (file == NULL){
		printf("exception in opening filepath in getField");
		result = "X";
	}

	fgets(fullstring,sizeof fullstring,file);
	int i=0;
	char *currentbit;
	char *fullstring2 = (char*) fullstring;
	while ((currentbit = strsep(&fullstring2,"|")) != NULL){
		i=i+1;
		if (i==position){
		//found it
			result=currentbit;
		}
	}
	free(fullstring2);
	fclose(file);
	return result;
}

void readPressureTempAltitude(){

	char *path = "/home/pi/prestemp.txt";
	temp_o = getField(path,1);
	pressure = getField(path,2);
	altitude = getField(path,3);
	return;
}

void getTime(){
 //sets the time to HH:MM:SS
	time_t currenttime;
	time(&currenttime);
	char timebuffer[100];
	struct tm* tm_info;
	tm_info = localtime(&currenttime);
	strftime(timebuffer,sizeof timebuffer,"%H:%M:%S",tm_info);
	timenow = timebuffer; 

}

void mainloop() {
    /*
$$CALLSIGN,count,time,latitude,longitude,altitude,v_speed,l_speed,bearing,satellites,outside temp,inside temp, outside pressure, outside humidity,pictures taken,status code*002A\n
$$RAINBOW1,123,12:11:11,52.22222222,-1.11111111,35000,3.5,10.1,273,8,-40.1,20, 500.4, 56.4%, 123,0*002A\n
100 characters
     */
    //populate the data string 32 things including commas, from $$RAINBOW1,123,....,until status string 7
    count += 1;
    snprintf(countstring, sizeof countstring, "%d", count);

    //pics taken
    int pics_taken = get_pics_taken();	

    //pressure, temperature and altitude (from pressure)
   readPressureTempAltitude();

    //get time
    getTime();

    snprintf(datastring, sizeof datastring, "%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%s%d%s%s", dollars, callsign, ",", countstring, ",", timenow, ",", latitude, ",", longitude, ",", altitude, ",", v_speed, ",", l_speed, ",", bearing, ",", satellites, ",", temp_o, ",", temp_i, ",", pressure, ",", humidity, ",", pics_taken, ",", status_code);

    //add the CRC checksum based on the data string so far
    unsigned int CHECKSUM = gps_CRC16_checksum(datastring);
    char checksum_str[10];
    //print checksum %04X = in hexadecimal, 4 digits at least, including leading zeroes, this also adds the * and the \n newline in correct places (star - checksum - newline)
    sprintf(checksum_str, "*%04X\n", CHECKSUM);

    snprintf(final, sizeof final, "%s%s", datastring, checksum_str);
    //data string is ready
    //send the data string with RTTY
    rtty_txstring(final);
    printf(final);
    f = fopen("telemetry.csv", "ab+");
    fprintf(f, final);
    fclose(f);
    sleep(1);
}

void setup() {
    wiringPiSetupGpio();
    //setting radio pin 4 to output mode
    pinMode(RADIOPIN, OUTPUT);
}

void rtty_txbit(int bit) {
    if (bit) {
        //high
        digitalWrite(RADIOPIN, HIGH);
    } else {
        //low
        digitalWrite(RADIOPIN, LOW);
    }

    delayMicroseconds(20000);

}


void rtty_txbyte(char c) {
    int i;
    rtty_txbit(0); //start bit

    for (i = 0; i < 7; i++) {
        if (c & 1) rtty_txbit(1);
        else rtty_txbit(0);
        c = c >> 1;
    }
    rtty_txbit(1); //stop bit
    rtty_txbit(1); //stop bit
}


void rtty_txstring(char*string) {
    char c;
    c = *string++;

    while (c != '\0') {
        rtty_txbyte(c);
        c = *string++;
    }
}



//MAIN METHOD, this is what runs when you run this program

int main(void) {
    //do setup first
    setup();
    //then infinite loop

    while (1 == 1) {
        mainloop();
    }
}






