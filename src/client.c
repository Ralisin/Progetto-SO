#include <arpa/inet.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <ctype.h>
#include <time.h>
#include <signal.h>
#include <fcntl.h>

#include "helper.h"

#define DATE_FORMAT 11
#define RES_CODE_FORMAT 18
#define DAY 2
#define MONTH 2
#define YEAR 4
#define HOUR 2
#define MINUTE 2
#define ROW_LETTERS "ABCDEF"
#define RED_COLOR "\x1b[31m"
#define GREEN_COLOR "\x1b[32m"
#define YELLOW_COLOR "\x1b[33m"
#define DEFAULT_COLOR "\x1b[0m"

#define fflushOut(stdin) while(getchar() != '\n')

typedef struct seat {
    int column;
    char row;
} seat_t; 

// Global variables
sentdata_t data;
seat_t seat;
int sock_fd, fd;
int active_res = 0;
FILE *file;
bool today_date = false; // check for input date

/* Return Values
 * n : estSX < n < estDX
 * -1 on error
*/
int input(int estDX, int estSX) {
    char input[MAX_BUFF];
    int ret;

    ret = scanf("%[^\n]", input);
    if(ret == 0) return -1;
    fflushOut(stdin);

    for(int i = 0; i < strlen(input); i++) {
        if(input[i] < '0' || input[i] > '9') return -1;
    }

    ret = strtol(input, NULL, 10);
    if(ret < estDX || ret > estSX) return -1;

    return ret;
}

/*
* Setting sentdata_t type struct
*/
void setSentdata(sentdata_t *data, int getCode, char* getDate, char* getHour, char* getCc, char getLine, int getPlace, int getN_res, char* getCancel_res){
    if(getCode == 0)
        data->code = 0;
    else data->code = getCode;
    
    if(getDate == NULL) {
        for(int i=0; i<DATE_SIZE; i++)  
            data->date[i] = 0;
    }
    else {
        for(int i=0; i<DATE_SIZE; i++)
            data->date[i] = getDate[i];
    }

    if(getHour == NULL) {
        for(int i=0; i<HOUR_SIZE; i++)
            data->hour[i] = 0;
    }
    else {
        for(int i=0; i<HOUR_SIZE; i++) {
            data->hour[i] = getHour[i];
        } 
    }

    if(getCc == NULL) {
        for(int i=0; i<CC_SIZE; i++)
            data->cc[i] = 0;
    }
    else {
        for(int i=0; i<CC_SIZE; i++)
            data->cc[i] = getCc[i];
    }

    if(getLine == 0) 
        data->line = 0;
    else data->line = getLine;

    if(getPlace == 0)
        data->place = 0;
    else data->place = getPlace;

    if(getN_res == 0)
        data->n_res = 0;
    else data->n_res = getN_res;

    if(getCancel_res == NULL) {
        for(int i=0; i<RES_SIZE; i++) {
            data->cancel_res[i] = 0;
        }
    }
    else {
        for(int i=0; i<RES_SIZE; i++) {
            data->cancel_res[i] = getCancel_res[i];
        }
    }
}

/*
* CTRL+C handler that quit client and send 
* exit code to server
*/
void handler(int dummy) {
    printf(RED_COLOR "\n\nRecived signal CTRL+C\nExiting program...\n");

    setSentdata(&data, CLIENT_EXIT, NULL, NULL, NULL, 0, 0, 0, NULL);
    int ret = send(sock_fd, &data, sizeof(data), 0);
    if(ret == -1) {
        printf("Error sending data to server\n");
        exit(-1);
    }

    close(sock_fd);
    exit(-1);
}

/* Return Values:
 * 0 to 3 based on user's choice
 * -1 on error
*/
int make_home(void) {
    system("clear");
    printf("                            Welcome to OS CINEMA\n");
    printf("1- Film programmation         2- Book ticket(s)       3- Cancel booked ticket(s)\n");
    printf("\t\t\t\t0- Exit\n");
 
    int num;
    printf("What would you do?: ");
retry:
    num = input(0, 3);
    if(num == -1) {
        printf("Wrong input, please retry: ");
        goto retry;
    }

    return num;
}

/* Return Values:
 *  1 on success
 *  0 if input is today date 
 * -1 on error
*/
int check_date(int day, int month, int year) {
    time_t t = time(NULL);
    struct tm today = *localtime(&t);
    
    if(year >= (today.tm_year + 1900) && year < 2025) {
        if(month > 0 && month < 13 && month != (today.tm_mon + 1) && month > (today.tm_mon + 1)) {
            if((day > 0 && day < 32) && (month == 1 || month == 3 || month == 5 || month == 7 || month == 8 || month == 10 || month == 12))
                goto exit_ret;
            else if((day > 0 && day < 31) && (month == 4 || month == 6 || month == 9 || month == 11)) 
                goto exit_ret;
            else if((day > 0 && day < 29) && (month == 2)) 
                goto exit_ret;
            else if(day == 29 && month == 2 && (year%400 == 0 ||(year%4 == 0 && year%100 != 0))) 
                goto exit_ret;
            else return -1;
        } else goto check;
    }

check:
    if(year == (today.tm_year + 1900)) {
        if(month == (today.tm_mon + 1)) {
            if(day > today.tm_mday) goto exit_ret;
            else if(day == today.tm_mday) return 0;
            else return -1;
        } else return -1;
    } else return -1;

exit_ret:
    return 1;
}

/* Return Values:
 *  DATE in string format on success
 *  NULL on error
*/
char* get_date(int code) {
    system("clear");

    char date_slash[DATE_FORMAT] = {0};
    int ret;
    
    if(code == FILM_LIST) printf("                                     FILM LIST\n");
    if(code == FILM_BOOKING) printf("                                TICKET BOOKING\n");
    printf(DEFAULT_COLOR);
    printf("Insert a date (dd/mm/aaaa): ");
reinsert:
    ret = scanf("%10s", date_slash);
    if(ret == -1 && errno != EINTR){
        puts("Error on acquiring input from scanf!");
        return NULL;
    }
    fflushOut(stdin);
    if(ret == 0 || date_slash[0] == '/') {
        printf("Wrong input!, please reinsert a valid date: ");
        goto reinsert;
    }
    date_slash[DATE_FORMAT-1] = 0;

    char *day, *month, *year;
    day = strtok(date_slash, "/");
    month = strtok(NULL, "/");
    year = strtok(NULL, "/");
    if(year == NULL || month == NULL) {
        printf("Wrong input!, please reinsert a valid date: ");
        goto reinsert;
    }
    if(strlen(day) != 2 || strlen(month) != 2 || strlen(year) != 4){
        printf("Wrong input!, please reinsert a valid date: ");
        goto reinsert;
    }

    int d = strtol(day, NULL, 10);
    int m = strtol(month, NULL, 10);
    int a = strtol(year, NULL, 10);
    if(d == 0 || m == 0 || a == 0 && errno == EINVAL){
        puts("Error converting string to int\nExiting...\n");
        return NULL;
    }
    int checker = check_date(d, m, a);
    if(checker == -1){
        printf("Wrong input!, please reinsert a valid date: ");
        goto reinsert;
    }
    else if(checker == 0) today_date = true;
    
    char *insert_date = (char *)malloc(sizeof(char) * (DATE_SIZE));
    if(insert_date == NULL)
        return NULL;

    int pos = 0;
    for(int j=0; j<YEAR; j++) insert_date[pos++] = year[j];
    for(int k=0; k<MONTH; k++) insert_date[pos++] = month[k];
    for(int i=0; i<DAY; i++) insert_date[pos++] = day[i];
    insert_date[pos] = 0;

    return insert_date;
} 

/* Return Values:
 *  DATE in string format on success
 *  NULL on error
*/
int listing(void) {
    char *date_ret;
reinsert:  
    date_ret = get_date(FILM_LIST);
    if(date_ret == NULL){
        puts("Error getting date\nExiting...\n");
        return -1;
    }

    setSentdata(&data, FILM_LIST, date_ret, NULL, NULL, 0, 0, 0, 0);
    
    int check_ret;
    check_ret = send(sock_fd, &data, sizeof(sentdata_t), 0);
    if(check_ret == -1) {
        printf("Error sending data to server\nExiting program...\n");
        return -1;
    }

    char buff[MAX_BUFF];
    check_ret = recv(sock_fd, &data, sizeof(sentdata_t), 0);
    if(check_ret == -1) {
        printf("Error reading data from server\nExiting program...\n");
        return -1;
    }

    if(data.code == -1) {
        if(data.place == -1) {
            printf("\nError! Server disconnected\n");
            close(sock_fd);
            exit(-1);
        }
        printf("\nProgrammation for the insert date doesn't exist!\nRedirecting to the 'insert date' page...\n");
        sleep(1);
        goto reinsert;
    }

    check_ret = recv(sock_fd, buff, data.n_res, 0);
    if(check_ret == -1) {
        printf("Error reading data from server\nExiting program...\n");
        return -1;
    }

    char *film_name = buff + HOUR_SIZE+1, * temp_hour = buff;
    char hour[HOUR_SIZE+1];

    printf("\n");
    for(int i=0; i<data.place; i++){
        for(int j=0; j<HOUR_SIZE; j++) {
            if (j == 2) hour[j] = ':';
            else hour[j] = temp_hour[j];
        }
        hour[HOUR_SIZE] = 0;
        printf("FILM: %s\t\tHOUR: %s\n", film_name, hour);
        film_name += strlen(film_name)+7;
        temp_hour += strlen(temp_hour)+1;
    }
    printf("\n");

    return 1;
}   

/* Return Values:
 *  1 on success
 *  -1 on error
*/
int check_hour(int hour, int minute) {
    time_t t = time(NULL);
    struct tm today = *localtime(&t);

    if(hour >= 00 && hour < 24 && today_date == false) {
        if(minute >= 00 && minute <= 59) return 1;
    } 
    else if(today_date == true && hour > today.tm_hour) return 1;
    else if(today_date == true && hour <= today.tm_hour) return 0;

    return -1;
}

/* Return Values:
 *  HOUR in string format on success
 *  NULL on error
*/
char *get_hour(void) {
    char hour_format[MAX_BUFF] = {0};
    int ret;
reinsert1:
    ret = scanf("%s", hour_format);
    if(ret == -1 && errno != EINTR){
        puts("Error on acquiring from scanf!");
        return NULL;
    }
    fflushOut(stdin);
    if(ret == 0 || hour_format[0] == ':' || strlen(hour_format) < 5) {
        printf("Wrong input!, please reinsert a valid hour: ");
        goto reinsert1;
    }

    char *hour, *minute;
    hour = strtok(hour_format, ":");
    minute = strtok(NULL, ":");
    if(strlen(hour) != 2 || strlen(minute) != 2){
        printf("Wrong input!, please reinsert a valid hour: ");
        goto reinsert1;
    }

    int h = strtol(hour, NULL, 10);
    int m = strtol(minute, NULL, 10);
    if(errno == EINVAL){
        puts("Error convertion string to int\n");
        return NULL;
    }
    int checker = check_hour(h, m);
    if(checker == -1){
        printf("Wrong input!, please reinsert a valid hour: ");
        goto reinsert1;
    }
    else if(checker == 0) {
        printf("\nReservations for this film are already closed!\nPlease reinsert a valid hour: ");
        goto reinsert1;
    }
    
    char *insert_hour = (char *)malloc(sizeof(char) * (HOUR_SIZE));
    if(insert_hour == NULL)
        return NULL;

    int pos = 0;
    for(int j=0; j<HOUR; j++) insert_hour[pos++] = hour[j];
    for(int k=0; k<MINUTE; k++) insert_hour[pos++] = minute[k];
    insert_hour[pos] = 0;

    return insert_hour;
}

/*  
* Printing cinema room 
*/
void print_room(char **room_map, int rows, int columns) {
    printf("\n");
    printf("\t\t\t\t CINEMA ROOM\n");
    int x = 0, y = 0;

    // printing first line of numbers
    printf("\t\t\t         ");
    for(int i=0; i<columns; i++) {
        if(i<9) printf(" ");
        else printf("%d", (i+1)/10);
    }

    // printing second line of numbers
    printf("\n\t\t\t         ");
    for(int i=0; i<columns; i++) printf("%d", (i+1)%10);

    printf("\n");
    for(int i=0; i<rows; i++) {
        printf("\t\t\t      %c  ", 'A'+i);
        for(int j=0; j<columns; j++) {    
            if(room_map[i][j] == 'x') printf(RED_COLOR "%c", room_map[i][j]);
            else if(room_map[i][j] == 'o') printf(GREEN_COLOR "%c", room_map[i][j]);
        }
        printf("\n");
        printf(DEFAULT_COLOR);
        fflush(stdout);
    }
}

/* Return Values:
 *  1 on success
 *  0 on race condition
 *  -1 on error
*/
int reservation(char **room_map, char row, int column, int total_col, int num_tickets, char *date_booking, char *hour_booking) {
    int ret;
    if(num_tickets == 1) {
        if(room_map[row-'A'][column-1] == 'o') {

            printf("Biglietto: %c%d\n", row, column);
        }
        else return -1;
    }
    else {
        int booked = 0;
        int j = 0;  // index for list_of_seat struct
        seat_t *list_of_seat = malloc(sizeof(seat_t) * num_tickets);
        if(list_of_seat == NULL) {
            printf("Malloc error\n");
            return -1;
        } 
        for(int i=0; i<num_tickets; i++) list_of_seat[i].row = row;

        if(room_map[row-'A'][column-1] != 'o') return -1;
        else {
            list_of_seat[j++].column = column;
            booked++;
        }
        
        for(int i=column; i<total_col; i++) {
            if(room_map[row-'A'][i] == 'o') {
                list_of_seat[j++].column = i+1;
                booked++;
            }
            if(booked == num_tickets) break;
            if(room_map[row-'A'][i] == 'x') return -1;
        }
        
        if(booked != num_tickets) return -1;
        else {
            printf("Biglietti: ");
            for(int i=0; i<num_tickets; i++) {
                printf("%c%d ", list_of_seat[i].row, list_of_seat[i].column);
                fflush(stdout);
            }
            printf("\n");            
        }
    }
    setSentdata(&data, FILM_BOOKING, date_booking, hour_booking, "00", row, column, num_tickets, NULL);

    ret = send(sock_fd, &data, sizeof(data), 0);
    if(ret == -1) {
        printf("Error sending data to server\n");
        return -1;
    }

    ret  = recv(sock_fd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        printf("Error reading data from server\n");
        return -1;
    }
    if(ret == 0) {
        puts("Server disconnected 1");
        return -1;
    }

    // speciale case when there is a conflict beetwen users that select same place
    if(data.code == -1) {
        if(data.place == -1) {
            printf("\nError! Server disconnected\n");
            close(sock_fd);
            exit(-1);
        }
        return 0;
    }

    return 1;
}

/* Return Values:
 *  1 on success
 *  -1 on error
*/
int booking(void) {
    system("clear");

    char *date_ret, *hour_ret;
    int ret;

    // check input date
reinsert:
    date_ret = get_date(FILM_BOOKING);
    if(date_ret == NULL) {
        printf("Error getting date\n");
        return -1;
    }

    setSentdata(&data, FILM_LIST, date_ret, NULL, NULL, 0, 0, 0, NULL);
    
    ret = send(sock_fd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        printf("Error sending data to server\n");
        return -1;
    }

    ret = recv(sock_fd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        printf("Error reading data from server\n");
        return -1;
    }
    if(ret == 0) {
        puts("Server disconnected");
        return -1;
    }

    if(data.code == -1) {
        if(data.place == -1) {
            printf("\nError! Server disconnected\n");
            close(sock_fd);
            exit(-1);
        }
        printf("\nProgrammation for the insert date doesn't exist!\n");
        sleep(1);
        goto reinsert;
    }

    char buff[MAX_BUFF], hour[HOUR_SIZE+1];
    char *film_name = buff + HOUR_SIZE+1, *temp_hour = buff, **check_hour;

    ret = recv(sock_fd, buff, data.n_res, 0);
    if(ret == -1) {
        printf("Error reading data from server\nExiting program...\n");
        return -1;
    }
    if(ret == 0) {
        puts("Server disconnected");
        return -1;
    }

    if(data.code == -1) {
        if(data.place == -1) {
            printf("\nError! Server disconnected\n");
            close(sock_fd);
            exit(-1);
        }
        printf("Programmation for the insert date doesn't exist!\nRedirecting to the 'insert date' page...\n");
        goto reinsert;
    }

    // check hour input
    printf("\n");
    check_hour = malloc(sizeof(char *) * data.place);
    if(check_hour == NULL) {
        printf("Malloc error\n");
        return -1;
    }
    for(int i=0; i<data.place; i++) {
        check_hour[i] = malloc(sizeof(char) * HOUR_SIZE);
        if(check_hour == NULL) {
            printf("Malloc error\n");
            return -1;
        }
    }

    int pos;
    for(int i=0; i<data.place; i++) {
        pos = 0;
        for(int j=0; j<HOUR_SIZE; j++) {
            if (j == 2) hour[j] = ':';
            else hour[j] = temp_hour[j];
            if(j != 2) check_hour[i][pos++] = temp_hour[j];
        }
        hour[HOUR_SIZE] = 0;
        check_hour[i][pos] = 0;
        printf("FILM: %s\t\tHOUR: %s\n", film_name, hour);
        film_name += strlen(film_name)+7;
        temp_hour += strlen(temp_hour)+1;
    }
    printf("\n");

    printf("Insert film hour (hh:mm): ");
reinsert_hour:
    hour_ret = get_hour();
    if(hour_ret == NULL) {
        printf("Error getting hour\n");
        return -1;
    }
    today_date = false;

    bool correct_hour = false;
    for(int i=0; i<data.place; i++) {
        if(strcmp(hour_ret, check_hour[i]) == 0) correct_hour = true;
    }
    if(correct_hour == false) { 
        printf("Invalid input! In the insert hour there isn't any film, reinsert a valid hour: ");
        goto reinsert_hour;
    }    

    bool first_try = true;
retry1:    
    setSentdata(&data, FILM_ROOM, date_ret, hour_ret, "00", 0, 0, 0, NULL);

    ret = send(sock_fd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        printf("Error sending data to server\n");
        return -1;
    }

    ret = recv(sock_fd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        printf("Error reading data from server\n");
        return -1;
    }
    if(ret == 0) {
        puts("Server disconnected 4");
        return -1;
    }
    if(data.code == -1) {
        if(data.place == -1) {
            printf("\nError! Server disconnected\n");
            close(sock_fd);
            exit(-1);
        }
        printf("Programmation for the insert hour or date doesn't exist!\nRedirecting to the 'insert date' page...\n");
        goto reinsert;
    }

    int size = data.code, rows = data.place, columns = data.n_res;
    char *buffer;
    buffer = (char *)malloc(sizeof(char) * size);
    if(buffer == NULL) {
        printf("Malloc error\n");
        return -1;
    }

    ret = recv(sock_fd, buffer, size, 0);
    if(ret == -1) {
        printf("Error reading data from server\n");
        return -1;
    }
    if(ret == 0) {
        puts("Server disconnected 5");
        return -1;
    }

    // build room map
    char **room_map;
    room_map = (char **)malloc(sizeof(char *) * rows);
    if(room_map == NULL) {
        printf("Malloc error\n");
        return -1;
    }

    for(int i=0; i<rows; i++) {
        room_map[i] = (char *)malloc(columns * sizeof(char));   // cause this is the format: A oooooooxxooox
        if(room_map[i] == NULL) {
            printf("Malloc error\n");
            return -1;
        }
    }

    for(int i=0; i<rows; i++) {
        for(int j=0; j<columns; j++) {
            if(buffer+j == 0) break;
            room_map[i][j] = *(buffer+j);
        }
        buffer += columns+1;
    }
    print_room(room_map, rows, columns);

    // booking tickets
    printf("\n");
    int tickets;
    printf(RED_COLOR "NOTE: ");
    printf(DEFAULT_COLOR);
    puts("If you want book more than one ticket you have to insert the coordinates of the first ticket.\nThe other tickets will be automatically booked on the right of the same row.");

    if(first_try == true) {
        printf("\nHow many tickets do you want to book?: ");
        first_try = false;
retry3:    
    tickets= input(1, columns);
        if(tickets == -1) {
            printf("Wrong input, please retry: ");
            goto retry3;
        }
    }

    printf("\n");
retry4:
    printf("Insert number of column: ");
retry:
    seat.column = input(1, columns);
    if(seat.column == -1) {
        printf("Wrong input, please retry: ");
        goto retry;
    }
    
    printf("Insert letter of row: ");
    char *input = malloc(sizeof(char) * MAX_BUFF);
    if(input == NULL) {
        printf("Malloc error!\n");
        return -1;
    }
    char *temp = malloc(sizeof(char) * MAX_BUFF);
    if(temp == NULL) {
        printf("Malloc error!\n");
        return -1;
    }
retry2:
    ret = scanf("%[^\n]", input);
    if(ret == 0) return -1;
    fflushOut(stdin);

    if(strlen(input) == 1) {
        if(strstr(ROW_LETTERS, input) == NULL) {
            printf("Wrong input, please retry: ");
            goto retry2;
        }
        strcpy(temp, input);
    }
    else {
        printf("Wrong input, please retry: ");
        goto retry2;
    }
    seat.row = temp[0];

    ret = reservation(room_map, seat.row, seat.column, columns, tickets, date_ret, hour_ret);
    if(ret == -1) {
        puts("\nCan't book ticket(s)!");
        puts("Reinsert coordinates\n");
        goto retry4;
    }
    if(ret == 0) {
        system("clear");
        printf(RED_COLOR "Booking ticket error! Conflict with other users.\n");
        printf(RED_COLOR "Please reinsert coordinates of seat.\n");
        printf(DEFAULT_COLOR);
        goto retry1;
    }

    printf(YELLOW_COLOR "\nTicket booking successfully completed!\n");
    printf(DEFAULT_COLOR);
    printf("Reservation code: %s\n", data.cancel_res);

    ret = fprintf(file, "%s\n", data.cancel_res);
    if(ret == -1) {
        puts("Writing error on file");
        return -1;
    }
    fflush(file);
    
    active_res++;
    printf("\nYou have %d active reservation!\n", active_res);

    return 1;
}

/*
 * Delete line_to_del from file. Use tempFile, same file pointer as file, to get next lines of file without using fseek complex logic
 * 
 * Return values:
 *      0 if line is deleted
 *      -1 if occurred some error
*/
int deleteFileLine(FILE** file, int line_to_del, char* filename) {
    int ret, currLine = 0;
    char buff[MAX_BUFF] = {0};
    char tempFilename[PATH_MAX] = {0};

    if(file == NULL || line_to_del < 0) {
        printf("Error params deleteFileLine\n");
        return -1;
    }

    strcpy(tempFilename, "temp");
    
    fseek(*file, 0, SEEK_SET);

    FILE* temp;
    temp = fopen(tempFilename, "w+");
    if(temp == NULL) {
        perror("Error fopen");
        return -1;
    }

    fseek(temp, 0, SEEK_SET);

redo_scanf:
    while((ret = fscanf(*file, "%s", buff)) != EOF) {
        if(line_to_del != currLine) {
            ret = fprintf(temp, "%s\n", buff);
            if(ret == 0) {
                perror("Error writing on temp file");
                goto error_deleteFileline;
            }
        }
        currLine++;
    }
    fflush(temp);
    
    if(ret == EOF && errno == EINTR) goto redo_scanf;
    
    if(fclose(*file) == -1) {
        perror("Error closing file descriptor");
        goto error_deleteFileline;
    }

    if(remove(filename) == -1) {
        perror("Error removing file");
        goto error_deleteFileline;
    }

    if(rename(tempFilename, filename) == -1) {
        perror("Error rename tempFilename, action by admin required");
        return -1;
    }

    *file = fopen(filename, "r+");
    if(*file == NULL) {
        perror("Error fopen");
        return -1;
    }

    return 0;

error_deleteFileline:
    if(remove(tempFilename) == -1)
        perror("Error remove tempFile");
    
    return -1;
}

/* Return Values:
 *  1 on success
 *  0 in case of inexisting code
 *  -1 on error
*/
int cancel_reservation(char *code) {
    int ret;

    setSentdata(&data, FILM_CANCEL, NULL, NULL, NULL, 0, 0, 0, code);
    ret = send(sock_fd, &data, sizeof(data), 0);
    if(ret == -1) {
        printf("Error sending data to server\n");
        return -1;
    }

    ret = recv(sock_fd, &data, sizeof(data), 0);
    if(ret == -1) {
        printf("Error reading data from server\n");
        return -1;
    }
    if(ret == 0) {
        puts("Server disconnected");
        return -1;
    }

    if(data.code == -1) {
        if(data.place == -1) {
            printf("\nError! Server disconnected\n");
            close(sock_fd);
            exit(-1);
        }
        return 0;
    }
    else printf(YELLOW_COLOR "Reservation code successfully deleted\n");
    printf(DEFAULT_COLOR);

    active_res--;

    file = fopen("Reservations.txt", "r+");
    if(file == NULL) {
        printf("fopen error!\n");
        return -1;
    }

    int counter = 0;
    char buff[MAX_BUFF] = {0};
    while(fscanf(file, "%s", buff) != 0) {
        buff[RES_CODE_FORMAT] = 0;
        if(strcmp(code, buff) == 0) break;
        counter++;
    }

    printf("Counter: %d\n", counter);
    deleteFileLine(&file, counter, "Reservations.txt");

    return 1;
}

int main(int argc, char **argv) {
    if(argc != 2){
        printf("Error input arguments!\nSyntax: %s IP_address\n", argv[0]);
        return -1;
    }

    struct sockaddr_in client;
    
    if((sock_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("Socket creation error\n");
        return -1;
    }
  
    client.sin_family = AF_INET;
    client.sin_port = htons(PORT);
  
    if (inet_pton(AF_INET, argv[1], &client.sin_addr) <= 0) {
        printf("Invalid address\n");
        return -1;
    }

    int client_fd;
    if((client_fd = connect(sock_fd, (struct sockaddr*)&client, sizeof(client))) < 0) {
        printf("Connection Failed\n");
        return -1;
    }
    
    fd = open("Reservations.txt", O_CREAT | O_RDWR, 0666);
    if(fd == -1) {
        puts("Open/creation file error");
        return -1;
    }
    
    file = fdopen(fd, "r+");
    if(file == NULL) {
        puts("Open/creation file error");
        return -1;
    }

    // conteggio delle prenotazioni attive appena viene startato il programma
    char buff[MAX_BUFF];
    while(fgets(buff, sizeof(buff), file) != NULL) active_res++;
    
    signal(SIGINT, handler);

home_redirect:
    setSentdata(&data, 0, NULL, NULL, NULL, 0, 0, 0, NULL);

    int choose = make_home(), ret;
    char c, res_code[RES_SIZE] = {0};
    char *temp = malloc(sizeof(char) * RES_SIZE-1);

    switch(choose) {
        case FILM_LIST:
            do{
                ret = listing();
                if (ret == -1) {
                    printf("Error on retriving film list...\nExiting program...\n");
                    return -1;
                } 
reanswer:                    
                printf("Do you want to continue on this page? If you type 'n' you will redirect to the home page [y/n/q]: ");
                ret = scanf("%c", &c);
                if(ret == -1 && errno != EINTR){
                    puts("Error on acquiring from scanf!");
                    exit(-1);
                }
                fflushOut(stdin);

                if(c != 'y' && c != 'n' && c != 'q') {
                    printf("Wrong input, please type only character 'y', 'n' or 'q': ");
                    goto reanswer;
                }
            } while(c != 'n' && c != 'q');
            if(c == 'n') goto home_redirect;
            if(c == 'q') goto ret;
            break;

        case FILM_ROOM:
            ret = booking();
            if(ret == -1) {
                printf("Booking ticket(s) error\nExiting program...\n");
                return -1;
            }

            printf("\nDo you want to go on the home page? If you type 'n' the program will be arrested [y/n]: ");
reanswer1:
            ret = scanf("%c", &c);
            if(ret == -1 && errno != EINTR){
                puts("Error on acquiring from scanf!");
                return -1;
            }
            fflushOut(stdin);

            if(c != 'y' && c != 'n') {
                printf("Wrong input, please type only character 'y' or 'n': ");
                goto reanswer1;
            }
            if(c == 'y') goto home_redirect;
            if(c == 'n') goto ret;
            break;

        case FILM_CANCEL:
            system("clear");
            printf("                            DELETING RESERVATION CODE\n");

            printf("\nYour reservation codes:\n");
            char temp_file[MAX_BUFF];
            if(active_res != 0) {
                sprintf(temp_file, "cat %s", "Reservations.txt");
                system(temp_file);
                printf("\n");
            } else {
                printf("No active reservation! Returning to home page.\n");
                sleep(1);
                goto home_redirect;
            }
reinsert_resCode:
            printf("\nInsert reservation code: ");
            ret = scanf("%s", temp);
            if(ret == -1 && errno != EINTR){
                puts("Error on acquiring from scanf!");
                return -1;
            }
            fflushOut(stdin);

            if(strlen(temp) != RES_SIZE-1){
                printf("Input error! Reservation code must be 18 characters!\n");
                goto reinsert_resCode;
            }
            memcpy(res_code, temp, strlen(temp));

            ret = cancel_reservation(res_code);
            if(ret == -1) {
                printf("Cancel reservation error!\nExiting program...\n");
                return -1;
            }
            if(ret == 0) {
                printf("Inexisting reservation code!\n");
                goto reinsert_resCode;
            }
            printf("\nYou have %d active reservation!\n", active_res);

            printf("\nDo you want to go on the home page? If you type 'n' the program will be arrested [y/n]: ");
reanswer2:
            ret = scanf("%c", &c);
            if(ret == -1 && errno != EINTR){
                puts("Error on acquiring from scanf!");
                return -1;
            }
            fflushOut(stdin);

            if(c != 'y' && c != 'n') {
                printf("Wrong input, please type only character 'y' or 'n': ");
                goto reanswer2;
            }
            if(c == 'y') goto home_redirect;
            if(c == 'n') goto ret;

            break;

        case CLIENT_EXIT:
            puts("Exiting program..."); 
            setSentdata(&data, CLIENT_EXIT, NULL, NULL, NULL, 0, 0, 0, NULL);
            ret = send(sock_fd, &data, sizeof(sentdata_t), 0);
            if(ret == -1) {
                printf("Error sending data to server\nExiting program...\n");
                return -1;
            }
            close(sock_fd);
            break;

        default:
            break;
    }
ret:
    return 0;
}