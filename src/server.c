#include <arpa/inet.h>
#include <dirent.h> 
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "helper.h"

#define _GNU_SOURCE

#define TIMEOUT 300
#define MAX_QUEUE 5

typedef struct _thdFileConn {
    int numThdConnected;
    int semR;
    int semW;
    char* filename[MAX_FILMNAME];
    char* date[DATE_SIZE];
    struct _thdFileConn* next;
} thdFileConn;

typedef struct _usingFile {
    pthread_mutex_t sem;
    char date[DATE_SIZE]; // Path of file
    char hour[HOUR_SIZE];
    char cc[CC_SIZE];
    struct _usingFile* next; // List with just one direction
} usingFile;

char *mainDir = NULL;
pthread_mutex_t buckupWrite;
FILE* logFile = NULL;
usingFile* listUsingFiles = NULL;
pthread_arg_t *listSockets = NULL;

/*
 * Handler to execute on SIGINT
*/
void handler(int dummy) {
    int ret, sockfd;
    sentdata_t data;
    pthread_arg_t *rmSock = listSockets;
    pthread_arg_t *rmSock1 = NULL;
    usingFile *rmFile;
    usingFile *rmFile1;

    printf("\nSending to every client closing code\n");

    // Free listSocket
    data.code = -1;
    data.place = -1; 
    while(rmSock != NULL) {
        sockfd = rmSock->new_sockfd;
        
        ret = send(sockfd, &data, sizeof(sentdata_t), 0);
        if(ret == -1) perror("Error send error value to client");

        rmSock1 = rmSock->next;
        free(rmSock);

        rmSock = rmSock1;
    }

    rmFile = listUsingFiles;
    while(rmFile != NULL) {
        pthread_mutex_destroy(&rmFile->sem);

        rmFile1 = rmFile->next;

        free(rmFile);

        rmFile = rmFile1;
    }

    exit(0);
}

/*
 * Set sentdata_t struct
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
        for(int i=0; i<HOUR_SIZE; i++)
            data->hour[i] = getHour[i];
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
 * Find the pointer the struct having the fileName. If it doesn't exit create a new item to append at the end
 * 
 * Return value:
 *      pointer to the struct having info of fileName
 *      NULL on error
*/
usingFile* findUsingFile(usingFile **list, char date[DATE_SIZE], char hour[HOUR_SIZE], char cc[CC_SIZE]) {
    usingFile* temp = *list;
    usingFile* tempPrev = NULL;
    struct sembuf op;

    if(temp == NULL) {
        *list = malloc(sizeof(usingFile));
        if(!*list) {
            perror("Error inizialising list of usingFiles");
            return NULL;
        }

        temp = *list;
    }

    while(temp != NULL && *list != NULL) {
        if(strcmp(date, temp->date) == 0 && strcmp(hour, temp->hour) == 0 && strcmp(cc, temp->cc) == 0)
            return temp;

        tempPrev = temp;
        temp = temp->next;
    }
    
    if(tempPrev != NULL) {
        tempPrev->next = malloc(sizeof(usingFile));
        if(!tempPrev->next) {
            perror("Error inizialising list of usingFiles");
            return NULL;
        }

        temp = tempPrev->next;
    }

    if(pthread_mutex_init(&(temp->sem), NULL)) {
        perror("Error init semNumThdOnSameFile");
        goto returnErrorMutex;
    }
    
    strcpy(temp->date, date);
    if(strlen(temp->date) != strlen(date)) {
        perror("Error writing in stuct date");
        goto returnError;
    }
    strcpy(temp->hour, hour);
    if(strlen(temp->hour) != strlen(hour)) {
        perror("Error writing in stuct hour");
        goto returnError;
    }
    strcpy(temp->cc, cc);
    if(strlen(temp->cc) != strlen(cc)) {
        perror("Error writing in stuct cc");
        goto returnError;
    }

    temp->next = NULL;

    return temp;

returnError:
    pthread_mutex_destroy(&(temp->sem));

returnErrorMutex:
    free(list);

    return NULL;
}

/*
 * Append at listSockets the new socket generated
*/
int appendListSocket(pthread_arg_t **list, pthread_arg_t *sock) {
    if(list == NULL)
        return -1;
    
    pthread_arg_t *temp = *list;
    if(temp == NULL) {
        sock->next = NULL;
        *list = sock;

        return 0;
    }

    while(temp->next != NULL)
        temp = temp->next;
    
    sock->next = NULL;
    temp->next = sock;

    return 0;
}

/*
 * Remove from listSockets a socket closed
*/
int remListSocket(pthread_arg_t **list, pthread_arg_t *toRem) {
    if(list == NULL) return -1;
    
    pthread_arg_t *temp = *list;
    if(temp == NULL) return -1;

    if(temp->new_sockfd == toRem->new_sockfd) {
        *list = temp->next;

        free(toRem);
        return 0;
    }
    
    while(temp != NULL && temp->next != NULL) {
        if(temp->next->new_sockfd == toRem->new_sockfd) {
            temp->next = temp->next->next;

            free(toRem);
            return 0;
        }

        temp = temp->next;
    }
}


/*
 * Navigate into path; if dir (direction) equal to 0 goback, else add fold to path
 * 
 * Return value:
 *      1 if correcly action done
 *      0 wrong parameters or error
*/
int navPath(bool dir, char path[PATH_MAX], char *fold) {
    // Check the correct passage of the parameters
    if(path == NULL) return 0;
    if(!dir && fold != NULL) return 0;
    
    // If dir == 0 delete goback in path
    int i = strlen(path)-1;
    if(!dir && fold == NULL) {
        for(; path[i] != '/'; i--) path[i] = 0;
        path[i] = 0;

        return 1;
    }

    // If dir == 1 enter in a folder; correct possible issues of parameters
    if(path[i] == '/' && fold[0] == '/') path[i] = 0;
    if(path[i] != '/' && fold[0] != '/') path[i+1] = '/';

    i = strlen(path);
    // Use memccpy cause using arrays and not strings ad variables, so haven't a terminator
    strcat(path, fold);
    if((i+strlen(fold)) != strlen(path)) {
        perror("Error strcat");
        return 0;
    }

    return 1;
}


/*
 * return date directory
 * 
 * Return value:
 *      1 path written
 *      0 or error
*/
int openStorageDateFold(char path[PATH_MAX], char* currDir, char date[DATE_SIZE]) {
    int ret;
    
    // Move through path to reach data folder to read files
    ret = strlen(path);
    strcat(path, currDir);
    if(ret + strlen(currDir) != strlen(path)) {
        perror("Error strcat");
        return 0;
    }

    ret = navPath(1, path, date);
    if(!ret) return 0;

    return 1;
}


/*
 * Send to client cinema programming
 * 
 * Return values:
 *      0 no issues
 *      -1 on error
*/
int sendProg(int sockfd, char* currDir, char date[DATE_SIZE]) {
    int ret, numFiles = 0, buffSize = 0;
    char path[PATH_MAX] = {0};
    char buff[MAX_BUFF] = {0};
    sentdata_t data;
    char* temp = buff;
    
    DIR *d;
    struct dirent *dir;

    if(!openStorageDateFold(path, currDir, date)) {
        printf("Error make path\n");
        goto sendProg_error1;
    }

    d = opendir(path);
    if(d == NULL) {
        perror("Error opening dir");
        goto sendProg_error1;
    }

    while((dir = readdir(d)) != NULL) {
        if(dir->d_type == DT_REG && strcmp(dir->d_name, ".") != 0 && strcmp(dir->d_name, "..") != 0) {
            strcpy(temp, dir->d_name);
            
            if(strlen(temp) != strlen(dir->d_name)) {
                perror("Error strcpy");
                goto sendProg_error;
            }

            buffSize += strlen(temp)+1;
            temp += strlen(temp)+1;
            numFiles++;
        }
    }

    data.place = numFiles;
    data.n_res = buffSize;
    data.code = FILM_LIST;
    ret = send(sockfd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        perror("Error send namefile");
        return -1;
    }

    data.code = FILM_LIST;
    ret = send(sockfd, buff, buffSize, 0);
    if(ret == -1) {
        perror("Error send namefile");
        return -1;
    }

    return 0;

sendProg_error:
    if(closedir(d) == -1)
        perror("Error closing dir");

sendProg_error1:
    // Send -1 on data->code cause there is an error with date sent to server
    data.code = -1;
    ret = send(sockfd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) perror("Error send error value to client");

    return -1;
}


/*
 * Send to client cinema seats order
 * 
 * Return values:
 *      0 no issues
 *      -1 on error
*/
int sendSeats(int sockfd, char* currDir, char date[DATE_SIZE], char hour[HOUR_SIZE], char cc[CC_SIZE]) {
    int ret, i, buffSize;
    char path[PATH_MAX] = {0};
    FILE* file;
    char *line, *temp;
    char buff[MAX_BUFF] = {0};
    char seats[MAX_FILMNAME] = {0};
    char filecode[HOUR_SIZE + CC_SIZE - 1] = {0}; // -1 cause two string terminators
    usingFile* strFile;
    sentdata_t data;
    struct sembuf op;
    op.sem_num = 0;
    op.sem_flg = 0;
    
    DIR *d;
    struct dirent *dir;

    ret = strlen(filecode);
    strcat(filecode, hour);
    strcat(filecode, cc);
    if((ret + strlen(hour) + strlen(cc)) != strlen(filecode)) {
        perror("Error memccpy");
        goto sendSeats_error;
    }

    if(!openStorageDateFold(path, currDir, date)) {
        printf("Error make path\n");
        goto sendSeats_error;
    }

    d = opendir(path);
    if(d == NULL) {
        perror("Error opening dir");
        goto sendSeats_error;
    }
    
    // Set path to correct file
    while((dir = readdir(d)) != NULL) {
        if(dir->d_type == DT_REG && strstr(dir->d_name, filecode) != NULL) {
            ret = navPath(1, path, dir->d_name);
            if(!ret) {
                printf("Error navPath");
                goto sendSeats_error;
            }
            break;
        }
    }

    file = fopen(path, "r+");
    if(file == NULL) {
        perror("Error fopen");

        goto sendSeatsCode_error;
    }

    strFile = findUsingFile(&listUsingFiles, date, hour, cc);

    // Lock semaphtore to read from a file
redo_lock_sem:
    ret = pthread_mutex_lock(&(strFile->sem));
    if(ret == -1 && errno == EINTR) goto redo_lock_sem;
    if(ret == -1) {
        perror("Error lock mutex");
        goto sendSeats_error;
    }


redo_fscafn_nlines:
    ret = fscanf(file, "%d", &data.place);
    if(ret == EOF && errno == EINTR) goto redo_fscafn_nlines;
    if(ret == EOF) {
        perror("Error fscanf 1");
        goto sendSeatsCode_error;
    }

redo_fscafn_nseats:
    ret = fscanf(file, "%d", &data.n_res);
    if(ret == EOF && errno == EINTR) goto redo_fscafn_nseats;
    if(ret == EOF) {
        perror("Error fscanf");
        goto sendSeatsCode_error;
    }

    temp = buff;
    buffSize = 0;
    for(i = 0; i < data.place; i++) {
redo_fscafn_lines:
        ret = fscanf(file, "%s", temp);
        if(ret == EOF && errno == EINTR) goto redo_fscafn_lines;
        if(ret == EOF) {
            perror("Error fscanf");
            goto sendSeatsCode_error;
        }

        buffSize += strlen(temp) + 1;
        temp += strlen(temp) + 1;
    }

redo_unlock_sem:
    ret = pthread_mutex_unlock(&(strFile->sem));
    if(ret == -1 && errno == EINTR) goto redo_unlock_sem;
    if(ret == -1) {
        perror("Error lock mutex");
        goto sendSeats_error;
    }

    data.code = buffSize;

    ret = send(sockfd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        perror("Error send namefile");
        goto sendSeats_error;
    }

    ret = send(sockfd, buff, buffSize, 0);
    if(ret == -1) {
        perror("Error send namefile");
        goto sendSeats_error;
    }
    
    ret = fclose(file);
    if(ret == EOF) {
        perror("Error closing file");
        return -1;
    }

    ret = closedir(d);
    if(ret == -1) {
        perror("Error closing dir");
        return -1;
    }

    return 0;

sendSeatsCode_error:
    ret = pthread_mutex_unlock(&(strFile->sem));
    if(ret == -1 && errno == EINTR) goto sendSeatsCode_error;
    if(ret == -1) {
        perror("Error lock mutex");
        goto sendSeats_error;
    }

sendSeats_error:
    fclose(file);
    closedir(d);

    return -1;
}


/*
 * Generate unique code and write it in data.cancel_res
 * 
 * Return values:
 *      0 no errors
 *      -1 if there are some errors
*/
int bookingCode(FILE* file, sentdata_t* data, int nLines, int nSeats, char date[DATE_SIZE], char hour[HOUR_SIZE], char cc[CC_SIZE], char line, int place, int n_res) {
    int index = 0, i, ret;
    char buff[MAX_BUFF];
    char *tokStr;
    char *codes[nLines * nSeats]; // Max number of prenotations
    char uniqueCode[UNIQUE_SIZE] = {0};

    while(fscanf(file, "%s", buff) != EOF) {
        codes[index] = malloc(sizeof(char) * UNIQUE_SIZE);
        if(!codes[index]) {
            perror("Error malloc");
            return -1;
        }

        tokStr = strtok(buff, "=");
        if(!tokStr) {
            printf("Error strtok");
            return -1;
        }

        strcpy(codes[index++], tokStr);
    }

    srand(time(NULL));

regenCode:
    // Generate code
    for(int i = 0; i < UNIQUE_SIZE-1; i++)
        sprintf(uniqueCode+i, "%d", rand()%10);

    // Check if code is unique
    for(int i = 0; i < index; i++)
        if(strcmp(uniqueCode, codes[i]) == 0) goto regenCode;

    strcat(data->cancel_res, date);
    strcat(data->cancel_res, hour);
    strcat(data->cancel_res, cc);
    strcat(data->cancel_res, uniqueCode);

    fseek(file, 0, SEEK_END);
    
    ret = fprintf(file, "%s=%c%d-%d\n", uniqueCode, line, place, n_res);
    if(ret == 0) {
        return -1;
    }
    fflush(file);

    printf("Written on file %s/%s%s...: %s=%c%d-%d\n", date, hour, cc, uniqueCode, line, place, n_res);
    printf("RESERVATION CODE: %s\n", data->cancel_res);

    // Free matrix codes
    for(i = 0; i < --index; i++)
        free(codes[i]);

    return 0;
}

/*
 * Send to client reservation code
 * 
 * Return values:
 *      0 no issues
 *      -1 on error
*/
int sendPrenCode(int sockfd, char* currDir, char date[DATE_SIZE], char hour[HOUR_SIZE], char cc[CC_SIZE], char line, int place, int n_res) {
    int ret, i, nLines, nSeats, currLine;
    char path[PATH_MAX] = {0};
    char buff[MAX_BUFF] = {0};
    char filecode[HOUR_SIZE + CC_SIZE - 1] = {0}; // -1 cause two string terminators
    sentdata_t data;
    FILE* file;

    usingFile* strFile;

    DIR *d;
    struct dirent *dir;

    ret = strlen(filecode);
    strcat(filecode, hour);
    strcat(filecode, cc);
    if((ret + strlen(hour) + strlen(cc)) != strlen(filecode)) {
        perror("Error strcat");
        
        goto sendErrorCode;
    }

    if(!openStorageDateFold(path, currDir, date)) {
        printf("Error make path\n");
        
        goto sendErrorCode;
    }

    d = opendir(path);
    if(d == NULL) {
        perror("Error opening dir");
        
        goto sendErrorCode;
    }

    // Set path to correct file
    while((dir = readdir(d)) != NULL) {
        if(dir->d_type == DT_REG && strstr(dir->d_name, filecode) != NULL) {
            ret = navPath(1, path, dir->d_name);
            if(!ret) {
                printf("Error navPath");
                goto sendErrorCode;
            }
            break;
        }
    }

    // Open file pointer in read/write option
    file = fopen(path, "r+");
    if(file == NULL) {
        perror("Error fopen");

        goto sendErrorCode;
    }

    strFile = findUsingFile(&listUsingFiles, date, hour, cc);

    // Lock semaphtore to write on a file
redo_lock_sem:
    ret = pthread_mutex_lock(&(strFile->sem));
    if(ret == -1 && errno == EINTR) goto redo_lock_sem;
    if(ret == -1) {
        perror("Error lock mutex");
        goto sendErrorCode;
    }

redo_fscafn_nlines:
    // Read number of lines in the cinema room
    ret = fscanf(file, "%d", &nLines);
    if(ret == EOF && errno == EINTR) goto redo_fscafn_nlines;
    if(ret == EOF) {
        perror("Error fscanf");
        goto sendErrorCode;
    }

redo_fscafn_nseats:
    // Read number of seats in one line of cinema room
    ret = fscanf(file, "%d", &nSeats);
    if(ret == EOF && errno == EINTR) goto redo_fscafn_nseats;
    if(ret == EOF) {
        perror("Error fscanf");
        goto sendErrorCode;
    }

    // Reset file pointer at the beginnign
    fseek(file, 0, SEEK_SET);
    
    // Set file pointer to right line through a counter of lines
    currLine = -2;
    while(currLine != line-'A') {
        fscanf(file, "%s", buff);
        currLine++;
    }
    // Move file pointer in the right line
    fseek(file, place, SEEK_CUR);
   
    // Check if the seat(s) is free
    for(i = 0; i < n_res; i++) {
        char t = fgetc(file);
        if(t == 'x') {
            printf("Placa alredy reserved\n");
            goto sendErrorCode;
        }
    }

    // Reset file pointer at the beginnign
    fseek(file, 0, SEEK_SET);
    // Skip first two lines, ret is a temp variable, not intrested i what is written in it
    fscanf(file, "%d", &ret);
    fscanf(file, "%d", &ret);
    // Set file pointer to right line through a counter of lines
    currLine = 0;
    while(currLine != line-'A') {
        fscanf(file, "%s", buff);
        currLine++;
    }
    // Set file pointer to right position
    fseek(file, place, SEEK_CUR);

    // Overwrite seats reserved
    for(i = 0; i < n_res; i++) {
        ret = fprintf(file, "x");
        if(!ret) {
            perror("Error fprintf writing x");
            goto sendErrorCode;
        }
    }
    fflush(file);

redo_unlock_sem:
    ret = pthread_mutex_unlock(&(strFile->sem));
    if(ret == -1 && errno == EINTR) goto redo_unlock_sem;
    if(ret == -1) {
        perror("Error unlock mutex");
        goto sendErrorCode;
    }

    // Write in data.cancel_res the code reservation
    if(bookingCode(file, &data, nLines, nSeats, date, hour, cc, line, place, n_res)) {
        printf("Error generation unique code\n");
        goto sendErrorCode;
    }

    // Send reservation code to the client, no error
    data.code = FILM_BOOKING;
    ret = send(sockfd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        perror("Error send namefile");
        return -1;
    }

    return 0;

sendErrorCode:
    printf("Error sendErrorCode\n");

redo_unlock_sem_error:
    ret = pthread_mutex_unlock(&(strFile->sem));
    if(ret == -1 && errno == EINTR) goto redo_unlock_sem_error;
    if(ret == -1) {
        perror("Error unlock mutex");
        goto sendErrorCode;
    }

    data.code = -1;
    ret = send(sockfd, &data, sizeof(sentdata_t), 0);
    if(ret == -1)
        perror("Error send error");

    return -1;
}


/*
 * Delete line_to_del from file. Use tempFile, same file pointer as file, to get next lines of file without using fseek complex logic
 * 
 * Return values:
 *      0 if line is deleted
 *      -1 if occurred some error
*/
int deleteFileLine(FILE** file, int line_to_del, char* filename) {
    int ret, currLine = 1;
    char buff[MAX_BUFF] = {0};
    char tempFilename[PATH_MAX] = {0};

    if(file == NULL || line_to_del < 0) {
        printf("Error params deleteFileLine\n");
        return -1;
    }

    strcpy(tempFilename, filename);
    navPath(0, tempFilename, NULL);
    navPath(1, tempFilename, "temp");

    // Set file pointer to the begin of the file
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

/*
 * Cancel a reservation and send to client the confirm
 * 
 * Return values:
 *      0 no isses
 *      -1 on error
*/
int delPrenCode(int sockfd, char* currDir, char cancel_res[RES_SIZE]) {
    int ret, i;
    char path[PATH_MAX] = {0};
    char buff[MAX_BUFF] = {0};
    char filecode[HOUR_SIZE + CC_SIZE - 1] = {0}; // -1 cause two string terminators
    char date[DATE_SIZE] = {0};
    char hour[HOUR_SIZE] = {0};
    char cc[CC_SIZE] = {0};
    char unique[UNIQUE_SIZE] = {0};
    bool foundPren = false;
    char line = 0;
    int nLines = 0;
    int nSeats = 0;
    int seat = 0;
    int nSeatsRes = 0;
    int lineToDel = 0;
    char* temp;
    
    sentdata_t data;
    FILE *file;

    usingFile* strFile = NULL;

    DIR *d;
    struct dirent *dir;


    memcpy(date, cancel_res, DATE_SIZE-1);
    memcpy(hour, cancel_res+(DATE_SIZE-1), HOUR_SIZE-1);
    memcpy(cc, cancel_res+(DATE_SIZE-1)+(HOUR_SIZE-1), CC_SIZE-1);
    memcpy(unique, cancel_res+(DATE_SIZE-1)+(HOUR_SIZE-1)+(CC_SIZE-1), UNIQUE_SIZE-1);

    if(strlen(date) != DATE_SIZE-1 || strlen(hour) != HOUR_SIZE-1 || strlen(cc) != CC_SIZE-1 || strlen(unique) != UNIQUE_SIZE-1)
        return -1;

    ret = strlen(filecode);
    strcat(filecode, hour);
    strcat(filecode, cc);
    if((ret + strlen(hour) + strlen(cc)) != strlen(filecode)) {
        perror("Error strcat");
        goto send_delErrorCode;
    }

    if(!openStorageDateFold(path, currDir, date)) {
        printf("Error make path\n");
        goto send_delErrorCode;
    }

    d = opendir(path);
    if(d == NULL) {
        perror("Error opening dir");
        goto send_delErrorCode;
    }

    // Set path to correct file
    while((dir = readdir(d)) != NULL) {
        if(dir->d_type == DT_REG && strstr(dir->d_name, filecode) != NULL) {
            ret = navPath(1, path, dir->d_name);
            if(!ret) {
                printf("Error navPath");
                goto send_delErrorCode;
            }
            break;
        }
    }

    // Open file pointer in read/write option
    file = fopen(path, "r+");
    if(file == NULL) {
        perror("Error fopen");
        goto send_delErrorCode;
    }

    strFile = findUsingFile(&listUsingFiles, date, hour, cc);

    strFile->sem;

redo_fscafn_nlines:
    // Read number of lines in the cinema room
    ret = fscanf(file, "%d", &nLines);
    if(ret == EOF && errno == EINTR) goto redo_fscafn_nlines;
    if(ret == EOF) {
        perror("Error fscanf");
        goto send_delErrorCode;
    }
redo_fscafn_nseats:
    // Not interested in number of seats
    ret = fscanf(file, "%d", &nSeats);
    if(ret == EOF && errno == EINTR) goto redo_fscafn_nseats;
    if(ret == EOF) {
        perror("Error fscanf");
        goto send_delErrorCode;
    }

    fseek(file, nLines*(sizeof(char)*(nSeats+1)), SEEK_CUR);

    lineToDel = 2+nLines;
    while(!foundPren) {
redo_fscafn_buff:
        // Not interested in number of seats
        ret = fscanf(file, "%s", buff);
        if(ret == EOF && errno == EINTR) goto redo_fscafn_buff;
        if(ret == EOF) {
            // End of file, no more to read
            break;
        }

        if(strstr(buff, unique)) {
            temp = strtok(buff, "=");
            if(!temp) {
                perror("strtok error");
                goto send_delErrorCode;
            }
            temp = strtok(NULL, "-");
            if(!temp) {
                perror("strtok error");
                goto send_delErrorCode;
            }
            ret = sscanf(temp, "%c", &line);
            if(ret == EOF) {
                printf("Error sscanf\n");
                goto send_delErrorCode;
            }
            ret = sscanf(temp+1, "%d", &seat);
            if(ret == EOF) {
                printf("Error sscanf\n");
                goto send_delErrorCode;
            }
            temp = strtok(NULL, "-");
            if(!temp) {
                perror("strtok error");
                goto send_delErrorCode;
            }
            ret = sscanf(temp, "%d", &nSeatsRes);
            if(ret == EOF) {
                printf("Error sscanf\n");
                goto send_delErrorCode;
            }

            foundPren = true;
        }

        lineToDel++;
    }

    if(!foundPren) goto send_delErrorCode;
 
    ret = deleteFileLine(&file, lineToDel, path);
    if(ret == -1) {
        printf("Error deleting line from file\n");
        goto send_delErrorCode;
    }

redo_fscanf_rewriteMatrix:
    ret = fscanf(file, "%s\n%s\n", buff, buff);
    if(ret == EOF && errno == EINTR) goto redo_fscanf_rewriteMatrix;
    
    fseek(file, (strlen(buff)+2) + (line-'A')*(sizeof(char)*(nSeats+1)) + sizeof(char)*seat, SEEK_SET);

    // Reset seats deleted
    for(i = 0; i < nSeatsRes; i++) {
        ret = fprintf(file, "o");
        if(ret == 0) {
            perror("Error fprintf");
            goto send_delErrorCode;
        }
    }
    fflush(file);

    data.code = FILM_CANCEL;
    ret = send(sockfd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        perror("Error send namefile");
        return -1;
    }

    return 0;

send_delErrorCode:
    data.code = -1;
    ret = send(sockfd, &data, sizeof(sentdata_t), 0);
    if(ret == -1) {
        perror("Error send namefile");
        return -1;
    }

    return -1;
}

/*
 * Print on logFile file every action
 * 
 * Return value:
 *      -1 on error
 *      0 no issues
*/
int writeOnBuckup(char* buff) {
    if(pthread_mutex_lock(&buckupWrite)) {
        perror("Error lock buckupWrite");
        return -1;
    }

    if(fprintf(logFile, "%s\n", buff) == 0) {
        perror("Error writing on file logFile");
        return -1;
    }
    fflush(logFile);

    if(pthread_mutex_unlock(&buckupWrite)) {
        perror("Error lock buckupWrite");
        return -1;
    }

    return 0;
}

/*
 * 
 * 
 * 
*/
void* thdFun(void* param) {
    int ret;
    char buff[MAX_BUFF];
    sentdata_t data;

    pthread_arg_t *thdArg = (pthread_arg_t *)param;
    int sockfd = thdArg->new_sockfd;
    struct sockaddr_in client_address = thdArg->client_address;

    if(appendListSocket(&listSockets, (pthread_arg_t *)param) == -1) {
        printf("Error appending to listSocket\n");
        goto endThd;
    }

    mainDir = getcwd(mainDir, PATH_MAX);
    if(mainDir == NULL) {
        perror("Error getcwd");
        exit(-1);
    }
    
    navPath(0, mainDir, NULL);
    navPath(1, mainDir, "/storage");

wait_client:
    printf("Waiting client request on socket %d\n", sockfd);

    ret = read(sockfd, &data, sizeof(sentdata_t));
    if(ret == 0) goto endThd;
    if(ret == -1 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        printf("Timeout read\n");
        goto endThd;
    } 
    if(ret == -1) {
        perror("Error read");
        goto endThd;
    }

    switch(data.code) {
        case FILM_LIST:
            printf("Client: %d. Code received: FILM_LIST\n", sockfd);
            ret = sendProg(sockfd, mainDir, data.date);
            if(ret) {
                sprintf(buff, "Error sending film list to client %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }
            else {
                sprintf(buff, "Cinema programming sent to socket %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }

            goto wait_client;
            break;
        case FILM_ROOM:
            printf("Client: %d. Code received: FILM_ROOM\n", sockfd);
            ret = sendSeats(sockfd, mainDir, data.date, data.hour, data.cc);
            if(ret) {
                sprintf(buff, "Error sending info to client %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }
            else {
                sprintf(buff, "Cinema seats sent to socket %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }

            goto wait_client;
        case FILM_BOOKING:
            printf("Client: %d. Code received: FILM_BOOKING\n", sockfd);
            ret = sendPrenCode(sockfd, mainDir, data.date, data.hour, data.cc, data.line, data.place, data.n_res);
            if(ret) {
                sprintf(buff, "Error reserve ticket on socket %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }
            else {
                sprintf(buff, "Ticket reserved, socket %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }

            goto wait_client;
        case FILM_CANCEL:
            printf("Client: %d. Code received: FILM_CANCEL\n", sockfd);
            ret = delPrenCode(sockfd, mainDir, data.cancel_res);
            if(ret) {
                sprintf(buff, "Error deleting reservation, socket %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }
            else {
                sprintf(buff, "Ticket deleted, socket %d\n", sockfd);
                printf("%s\n", buff);
                writeOnBuckup(buff);
            }

            goto wait_client;
        case CLIENT_EXIT:
            sprintf(buff, "Client: %d. Code received: CLIENT_EXIT\n", sockfd);
            printf("%s\n", buff);
            writeOnBuckup(buff);

            goto endThd;
    }

endThd:
    if(remListSocket(&listSockets, (pthread_arg_t*)param) == -1) 
        printf("Error removing socket from list\n");

    if(close(sockfd)) perror("Error closing socket");

    printf("Client disconnected from socket %d\n", sockfd);

    return NULL;
}

int main(int argc, char* argv[]) {
    int sockfd, opt, fd;

    signal(SIGINT, handler);

    fd = open("logFile.txt", O_CREAT | O_RDWR, 0660);
    if(fd == -1) {
        puts("Open/creation file error");
        return -1;
    }
    
    logFile = fdopen(fd, "r+");
    if(logFile == NULL) {
        perror("Error open logFile file");
        exit(-1);
    }

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if(sockfd == -1) {
        perror("Errore socket");
        exit(-1);
    }

    opt = 1;
    if(setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("Errore setsockopt");
        exit(-1);
    }

    struct sockaddr_in address;
    address.sin_family = AF_INET; // Specidica il protocollo su cui il socket è basato
    address.sin_addr.s_addr = INADDR_ANY; // Specifica l'indirizzo da assegnare al server al suo bind
    address.sin_port = htons(PORT); // Porta a cui connettersi, htons converte da uint_t a uint16_t
    socklen_t addrlen = (socklen_t)sizeof(address);
    if(bind(sockfd, (struct sockaddr*)&address, addrlen) < 0) {
        perror("Errore bind");
        exit(-1);
    }

    if(listen(sockfd, MAX_QUEUE)) {
        perror("Errore listen");
        exit(-1);
    }

    pthread_t tid;
    socklen_t clientAddrLen;
    struct timeval timeout;

    while(1) {
        pthread_arg_t* thdArg = (pthread_arg_t*)malloc(sizeof(pthread_arg_t));
        if(!thdArg) {
            perror("Malloc error thread aerguments");
            raise(SIGINT);
        }
        
        clientAddrLen = sizeof(thdArg->client_address);
        thdArg->new_sockfd = accept(sockfd, (struct sockaddr *)&thdArg->client_address, &clientAddrLen);

        // Set socket timeout
        timeout.tv_sec = TIMEOUT;
        timeout.tv_usec = 0;
        if(setsockopt(thdArg->new_sockfd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout)) < 0) {
            perror("Error set socket timeout");
            raise(SIGINT);
        }

        if(thdArg->new_sockfd == -1) {
            perror("Error accept from socket");
            raise(SIGINT);
        }

        if(pthread_create(&tid, NULL, thdFun, (void*)thdArg)) {
            perror("Error create thread");
            raise(SIGINT);
        }
    }
}


