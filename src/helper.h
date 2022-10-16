#define FILM_LIST       1
/* From Client to Server
sentdata.code = FILM_LIST;
sentdata.date = "20221031";
 -> From Server to Client
Server send in order:
sentdata.place = number of strings
senddata.n_res = sizeof(buff)
buff formar: ["hhmmccnome_film","hhmmccnome_film","hhmmccnome_film"]
*/
#define FILM_ROOM       2
/* From Client to Server
sentdata.code = FILM_ROOM;
sentdata.date = "20221031";
sentdata.hour = "1730";
sentdata.cc = "00";
 -> From Server to Client in order:
sentdata.place = num of lines
sentdata.n_res = num of columns
sentdata.code = buffSize
buff formar: ["ooooooooooooo","ooooooooooooo","ooooooooooooo"]
*/
#define FILM_CANCEL     3
/* From Client to Server
sentdata.code = FILM_CANCEL;
sentdata.cancel_res = "202210311730ccxxxx";
*/
#define FILM_BOOKING    4
/* From Client to Server
sentdata.code = FILM_BOOKING;
sentdata.date = "20221031";
sentdata.hour = "1730";
sentdata.cc = "00";
sentdata.line = 'b';
sentdata.place = 5;
sentdata.n_res = 4;
 -> From Server to Client in order:
sentdata.code = FILM_BOOKING or -1 on error
sentdata.cancel_res = "aaaammddhhmmccxxxx"
*/
#define CLIENT_EXIT    0
/* From Client to Server
sentdata.code = CLIENT_EXIT
*/
#define ERROR_CODE -1
/* From Client to Server
sentdata.code = ERROR_CODE
*/

#define PORT 8989

// Max number of reservation in one time
#define MAX_RES         10
#define MAX_BUFF        4096
#define MAX_FILMNAME    256

#define PATH_MAX        4096

#define HOUR_SIZE       5
#define DATE_SIZE       9
#define CC_SIZE         3
#define RES_SIZE        19

#define UNIQUE_SIZE     5

typedef struct _sentdata {
    int code;
    char date[DATE_SIZE]; // aaaammdd
    char hour[HOUR_SIZE]; // hhmm
    char cc[CC_SIZE]; // Film identifier
    char line; // Server: implementation dependent / Client: Line of seat(s)
    int place; // Server: implementation dependent / Client: Lowest integer of seat reserved
    int n_res; // Server: implementation dependent / Client: Number of reservations to do 
    char cancel_res[RES_SIZE]; // aaaammddhhmmccxxxx -> aaaa/mm/dd date, hh:mm film hour, xxxx unique code
} sentdata_t;

typedef struct _pthread_arg_t {
    int new_sockfd;
    struct sockaddr_in client_address;
    struct _pthread_arg_t *next;
} pthread_arg_t;
