#include <ncurses.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <string.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fstream>
#include <stdint.h>
#include <map>

#define MAX_SIZE 4096
#define MAX_PENDING 5

using namespace std;

#define WIDTH 43
#define HEIGHT 21
#define PADLX 1
#define PADRX WIDTH - 2

/***** Global Variables *****/

/* Game Variables */
int recv_refresh;    // Refresh rate
int recv_rounds;     // Game rounds
int ballX, ballY;    // Ball position
int dx, dy;          // Ball movement
int padLY, padRY;    // Paddle position
int scoreL, scoreR;  // Player scores
int played_rounds;   //Rounds played
WINDOW *win;         // ncurses window

/* Networking Variables */
bool isHost = false; 
int HOST_SOCKFD = -1; 
int CLIENT_SOCKFD = -1; 

/* Locks */
//not sure why you need these;
pthread_mutex_t ballX_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t ballY_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dx_lock    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t dy_lock    = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t padLY_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t padRY_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scoreL_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t scoreR_lock = PTHREAD_MUTEX_INITIALIZER;


/***** Helper Functions *****/
void printLog (string message) {
    const char* messageChar = message.c_str();
    FILE *f = fopen("log", "a");
    fprintf(f,"%s\n", messageChar);
    fclose(f);
}

bool send_update(string var_name, int val, string error) {

    printLog("var_name");
    printLog(var_name);
    printLog("val");
    printLog(to_string(val));

    const char* var_str = var_name.c_str();
    int sockfd = isHost ? HOST_SOCKFD : CLIENT_SOCKFD; 

    /* Create update string with "var_string val" format */ 
    char update_str[BUFSIZ];
    sprintf(update_str, "%s %d", var_str, val);

    /* Send update to appropriate socket */ 
	if (send(sockfd, update_str, strlen(update_str) + 1, 0) == -1) {
		printLog(error); 
		return false;
	}
	printLog("sent update string:"); 
    printLog(string(update_str));
	
    return true; 
}

int host_player(int port, int refresh,int rounds) {
    
    // Networking code
    struct sockaddr_in sock;
    // Clear memory location
    bzero((char*) &sock, sizeof(sock));
    // Specify IPv4
    sock.sin_family = AF_INET;
    // Use the default IP address of the server
    sock.sin_addr.s_addr = INADDR_ANY;
    // Convert from host to network byte order
    sock.sin_port = htons(port);

    /* Set up socket */
    int sockfd;
    if ((sockfd = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printLog("Socket failed.");
        return 1;
    }

    /* Set socket options */
    int opt = 1;
    if ((setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,(char*)&opt, sizeof(int))) < 0) {
        printLog("Setting socket options failed.");
        return 1;
    }

    /* Bind */ 
    if ((bind(sockfd, (struct sockaddr*) &sock, sizeof(sock))) < 0) {
        printLog("Bind failed.");
        return 1;
    }

    /* Listen */
    if ((listen(sockfd, MAX_PENDING)) < 0) {
        printLog("Listen failed.");
        return 1; 
    } 

    /* Wait for incoming connections */
    struct sockaddr_in client_sock;
    socklen_t len = sizeof(client_sock);
    pthread_t thread_id;
    
    cout << "Waiting for challengers on port " << port << endl;
    HOST_SOCKFD = accept(sockfd, (struct sockaddr*) &client_sock, &len);
    if (HOST_SOCKFD < 0) {
        printLog("Accept failed.");
        return 1;
    }

    /* Send refresh rate over to client */
    refresh = htonl(refresh);
    if (send(HOST_SOCKFD, &refresh, sizeof(refresh), 0) == -1) {
        printLog("Sending refresh rate to client failed.");
        return 1;
    }
       
    /* Send number of rounds to client */
    rounds = htonl(rounds);
    if (send(HOST_SOCKFD, &rounds, sizeof(rounds), 0) == -1) {
        printLog("Sending rounds to client failed.");
        return 1;
    }
    
    return 0;
}


int client_player(int port, char host[],int rounds) {
    
    /* Translate host name into peer's IP address */
    struct hostent* hostIP = gethostbyname(host);
    if (!hostIP) {
        printLog("Error: Unknown host.");
        return 1;
    }

    /* Create address data structure */
    struct sockaddr_in sock;
    // Clear memory location
    bzero((char*) &sock, sizeof(sock));
    // Specify IPv4
    sock.sin_family = AF_INET;
    // Copy host address into sockaddr
    bcopy(hostIP->h_addr, (char*) &sock.sin_addr, hostIP->h_length);
    // Convert from host to network byte order
    sock.sin_port = htons(port);

    /* Create the socket */
    if ((CLIENT_SOCKFD = socket(PF_INET, SOCK_STREAM, 0)) < 0) {
        printLog("Error creating socket.");
        return 1;
    } 

    /* Connect */
    if (connect(CLIENT_SOCKFD, (struct sockaddr*)&sock, sizeof(sock)) < 0) {
        printLog("Error connecting.");
        return 1;
    }

    /* Receive refresh rate from host */
    if (recv(CLIENT_SOCKFD, &recv_refresh, sizeof(recv_refresh), 0) == -1) {
        printLog("Error receiving refresh rate from host."); 
        return 1;
    }
    recv_refresh = ntohl(recv_refresh);

    cout <<"recv_refresh" << recv_refresh << endl;


    /* Receive rounds from host */
    if (recv(CLIENT_SOCKFD, &recv_rounds, sizeof(recv_rounds), 0) == -1) {
        printLog("Error receiving rounds from host."); 
        return 1;
    }
    recv_rounds = ntohl(recv_rounds);
    cout << "recv_rounds " << recv_rounds << endl;
    //printLog("rounds:");
    printLog(to_string(recv_rounds));

    cout << recv_rounds << endl;
    
    
    return 0;
}


/* Draw the current game state to the screen
 * ballX: X position of the ball
 * ballY: Y position of the ball
 * padLY: Y position of the left paddle
 * padRY: Y position of the right paddle
 * scoreL: Score of the left player
 * scoreR: Score of the right player
 */
void draw(int ballX, int ballY, int padLY, int padRY, int scoreL, int scoreR) {
    // Center line
    int y;
    for(y = 1; y < HEIGHT-1; y++) {
        mvwaddch(win, y, WIDTH / 2, ACS_VLINE);
    }
    // Score
    mvwprintw(win, 1, WIDTH / 2 - 3, "%2d", scoreL);
    mvwprintw(win, 1, WIDTH / 2 + 2, "%d", scoreR);
    // Ball
    mvwaddch(win, ballY, ballX, ACS_BLOCK);
    // Left paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padLY - 2 && y <= padLY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADLX, ch);
    }
    // Right paddle
    for(y = 1; y < HEIGHT - 1; y++) {
        int ch = (y >= padRY - 2 && y <= padRY + 2)? ACS_BLOCK : ' ';
        mvwaddch(win, y, PADRX, ch);
    }
    // Print the virtual window (win) to the screen
    wrefresh(win);
    // Finally erase ball for next time (allows ball to move before next refresh)
    mvwaddch(win, ballY, ballX, ' ');
}

/* Return ball and paddles to starting positions
 * Horizontal direction of the ball is randomized
 * Use locks to ensure multiple threads do not attempt to simultaneously edit a game
 * variable
 */
void reset() {
    pthread_mutex_lock(&ballX_lock);
    ballX = WIDTH / 2;
    pthread_mutex_unlock(&ballX_lock);
    
    pthread_mutex_lock(&ballY_lock);
    ballY = HEIGHT / 2;
    pthread_mutex_unlock(&ballY_lock);

    pthread_mutex_lock(&padRY_lock);
    padRY = HEIGHT / 2;
    pthread_mutex_unlock(&padRY_lock);
    
    pthread_mutex_lock(&padLY_lock);
    padLY = HEIGHT / 2;
    pthread_mutex_unlock(&padLY_lock);
    
    pthread_mutex_lock(&dx_lock);
    // dx is randomly either -1 or 1
    dx = (rand() % 2) * 2 - 1;
    pthread_mutex_unlock(&dx_lock);

    pthread_mutex_lock(&dy_lock);
    dy = 0;
    pthread_mutex_unlock(&dy_lock);

    // Draw to reset everything visually
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Display a message with a 3 second countdown
 * This method blocks for the duration of the countdown
 * message: The text to display during the countdown
 */
void countdown(const char *message) {
    int h = 4;
    int w = strlen(message) + 4;
    WINDOW *popup = newwin(h, w, (LINES - h) / 2, (COLS - w) / 2);
    box(popup, 0, 0);
    mvwprintw(popup, 1, 2, message);
    int countdown;
    for(countdown = 3; countdown > 0; countdown--) {
        mvwprintw(popup, 2, w / 2, "%d", countdown);
        wrefresh(popup);
        sleep(1);
    }
    wclear(popup);
    wrefresh(popup);
    delwin(popup);
    
    // Wipe out any input that accumulated during the delay
    pthread_mutex_lock(&ballY_lock);
    ballY = HEIGHT / 2;
    pthread_mutex_unlock(&ballY_lock);

    pthread_mutex_lock(&padRY_lock);
    padRY = HEIGHT / 2;
    pthread_mutex_unlock(&padRY_lock);
    
    pthread_mutex_lock(&padLY_lock);
    padLY = HEIGHT / 2;
    pthread_mutex_unlock(&padLY_lock);
}

/* Perform periodic game functions:
 * 1. Move the ball
 * 2. Detect collisions
 * 3. Detect scored points and react accordingly
 * 4. Draw updated game state to the screen
 */
void tock() {
    // Move the ball
    pthread_mutex_lock(&ballX_lock);
    ballX += dx;
    pthread_mutex_unlock(&ballX_lock);
    pthread_mutex_lock(&ballY_lock);
    ballY += dy;
    pthread_mutex_unlock(&ballY_lock);
    
    // Check for paddle collisions
    // padY is y value of closest paddle to ball
    int padY = (ballX < WIDTH / 2) ? padLY : padRY;
    // colX is x value of ball for a paddle collision
    int colX = (ballX < WIDTH / 2) ? PADLX + 1 : PADRX - 1;
    if(ballX == colX && abs(ballY - padY) <= 2) {
        // Collision detected!
        pthread_mutex_lock(&dx_lock);
        dx *= -1;
        pthread_mutex_unlock(&dx_lock);

        // Determine bounce angle
        pthread_mutex_lock(&dy_lock);
        if(ballY < padY) dy = -1;
        else if(ballY > padY) dy = 1;
        else dy = 0;
        pthread_mutex_unlock(&dy_lock);
    }

    // Check for top/bottom boundary collisions
    pthread_mutex_lock(&dy_lock);
    if(ballY == 1) dy = 1;
    else if(ballY == HEIGHT - 2) dy = -1;
    pthread_mutex_unlock(&dy_lock);

    // Score points
    if(ballX == 0) {
        pthread_mutex_lock(&scoreR_lock);
        scoreR = (scoreR + 1) % 100; //not sure why you do the % 100 thing
        pthread_mutex_unlock(&scoreR_lock);
        reset();
        countdown("SCORE -->");

        
        if(scoreR ==2){
            countdown("ROUND WIN -->");
            played_rounds +=1;
            if(played_rounds ==2 && (played_rounds!=recv_rounds)){
                //reset the score
                scoreR = 0;
                scoreL = 0;

            }
            if(played_rounds != recv_rounds){
               	send_update("exit", 0, "Error sending exit signal to client.");
                endwin();
                exit(0);
            }
        }

    } else if(ballX == WIDTH - 1) {
        pthread_mutex_lock(&scoreL_lock);
        scoreL = (scoreL + 1) % 100;
        pthread_mutex_unlock(&scoreL_lock);
        reset();
        countdown("<-- SCORE");
        if(scoreL ==2){
            countdown("<--ROUND WIN");
            played_rounds +=1;
            if(played_rounds!=recv_rounds){
                //reset the score
                scoreR = 0;
                scoreL = 0;

            }
            if(played_rounds == recv_rounds){
                send_update("exit", 0, "Error sending exit signal to client.");
                endwin();
                exit(0);
            }

            
        }
    }
    
    // Finally, redraw the current state
    draw(ballX, ballY, padLY, padRY, scoreL, scoreR);
}

/* Listen to keyboard input
 * Updates global pad positions
 */
void *listenInput(void *args) {
	// host: left, client: right
    while(1) {
        switch(getch()) {
			case KEY_UP: 
				if (isHost) {
                    pthread_mutex_lock(&padLY_lock);
	                padLY--; 
                    pthread_mutex_unlock(&padLY_lock);
                    printLog("padLY in listen:");
                    printLog(to_string(padRY));
				    // TODO: error checking on send_update???
                                    // Send updated paddle
			                                        //Send updated ball
                                        //int ballX, ballY;    // Ball position
                                        //int dx, dy;          // Ball movement
                                        
                                        send_update("padLY", padLY, "Error sending padLY to client.");
                                        send_update("ballX", ballX, "Error sending ballX to client.");
                                        send_update("ballY", ballY, "Error sending ballY to client.");
                                        send_update("dx", dx, "Error sending dx to client.");
                                        send_update("dy", dy, "Error sending dy to client.");







				} else {
                    pthread_mutex_lock(&padRY_lock);
					padRY--; 
                    pthread_mutex_unlock(&padRY_lock);
                    printLog("padRY in listen:");
                    printLog(to_string(padRY));
					send_update("padRY", padRY, "Error sending padRY to host.");
                                        send_update("ballX", ballX, "Error sending ballX to client.");
                                        send_update("ballY", ballY, "Error sending ballY to client.");
                                        send_update("dx", dx, "Error sending dx to client.");
                                        send_update("dy", dy, "Error sending dy to client.");


				}
            	break;
			case KEY_DOWN: 
				if (isHost) {
                    pthread_mutex_lock(&padLY_lock);
					padLY++; 
                    pthread_mutex_unlock(&padLY_lock);
                    printLog("padLY in listen:");
                    printLog(to_string(padRY));
					send_update("padLY", padLY, "Error sending padLY to client.");
                                        send_update("ballX", ballX, "Error sending ballX to client.");
                                        send_update("ballY", ballY, "Error sending ballY to client.");
                                        send_update("dx", dx, "Error sending dx to client.");
                                        send_update("dy", dy, "Error sending dy to client.");



				} else {
                    pthread_mutex_lock(&padRY_lock);
					padRY++; 
                    pthread_mutex_unlock(&padRY_lock);
                    printLog("padRY in listen:");
                    printLog(to_string(padRY));
					send_update("padRY", padRY, "Error sending padRY to host."); 
                                        send_update("ballX", ballX, "Error sending ballX to client.");
                                        send_update("ballY", ballY, "Error sending ballY to client.");
                                        send_update("dx", dx, "Error sending dx to client.");
                                        send_update("dy", dy, "Error sending dy to client.");

				}
	        	break;
            /*case 'w': padLY--;
             break;
            case 's': padLY++;
             break; */ 
            default: break;
       }
    }       
    return NULL;
}

void *listenUpdate(void *args) {

	int resp; 
	char update[MAX_SIZE]; 
	
	// Get sockfd	
	int sockfd = isHost ? HOST_SOCKFD : CLIENT_SOCKFD; 

	while (resp = recv(sockfd, &update, sizeof(update), 0)) {
		if (resp == -1) {
			printLog("Error receiving updated game state var from other player.");	
			return NULL; 
		}
               
                /*if(resp == "exit"){
                    exit(0);
                }*/
 
        /* Parse game update */
        char var_name[MAX_SIZE];
        char val[MAX_SIZE];

        sscanf(update, "%s %s", var_name, val);  
        printLog("update:");
        printLog(string(update));
        printLog(string(val));
        printLog(string(var_name));
        int val_int = stoi(val);

        if (strcmp(var_name, "ballX") == 0) {
            pthread_mutex_lock(&ballX_lock);
            ballX = val_int;
            pthread_mutex_unlock(&ballX_lock);
        }
        else if(strcmp(var_name,"exit")==0){
            endwin();
            exit(0);
        }
        else if (strcmp(var_name, "ballY") == 0) {
            pthread_mutex_lock(&ballY_lock);
            ballY = val_int;
            pthread_mutex_unlock(&ballY_lock);
        }
        else if (strcmp(var_name, "padLY") == 0) {
            pthread_mutex_lock(&padLY_lock);
            padLY = val_int;
            pthread_mutex_unlock(&padLY_lock);
        }
        else if (strcmp(var_name, "padRY") == 0) {
            pthread_mutex_lock(&padRY_lock);
            padRY = val_int;
            pthread_mutex_unlock(&padRY_lock);
        }
        else if (strcmp(var_name, "scoreL") == 0) {
            pthread_mutex_lock(&scoreL_lock);
            scoreL = val_int;
            pthread_mutex_unlock(&scoreL_lock);
        }
        else if (strcmp(var_name, "scoreR") == 0) {
            pthread_mutex_lock(&scoreR_lock);
            scoreR = val_int;
            pthread_mutex_lock(&scoreR_lock);
        }
        else { 
            printLog("Invalid game update received.");
        }
        
		/* Update game state */
		draw(ballX, ballY, padLY, padRY, scoreL, scoreR); 

	}

}

void initNcurses() {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    refresh();
    win = newwin(HEIGHT, WIDTH, (LINES - HEIGHT) / 2, (COLS - WIDTH) / 2);
    box(win, 0, 0);
    mvwaddch(win, 0, WIDTH / 2, ACS_TTEE);
    mvwaddch(win, HEIGHT-1, WIDTH / 2, ACS_BTEE);
}

int main(int argc, char *argv[]) {

	// Clear log file 
	FILE *f = fopen("log", "w");
    
    // Refresh is clock rate in microseconds
    // This corresponds to the movement speed of the ball
    int  refresh;
    char difficulty[10];
    if(argc != 3){  
        printf("Not enough arguments.");
        return 1;
    }

    int rounds; //FIXME: not doing anything
    
    int port = stoi(argv[2]);
    char* host = argv[1];
    if (strcmp(host, "--host") == 0) {
		isHost = true; 
        // Get difficulty level from user
        printf("Please select the difficulty level (easy, medium or hard): ");
        scanf("%s", &difficulty);
        printf("Please enter the maximum number of rounds to play: ");
        scanf("%d",&rounds);
        //printf("rounds: %d\n",rounds);
        printLog(to_string(rounds));
        //Send max number of rounds to client

        if(strcmp(difficulty, "easy") == 0) refresh = 80000;
        else if(strcmp(difficulty, "medium") == 0) refresh = 40000;
        else if(strcmp(difficulty, "hard") == 0) refresh = 20000;


        if (host_player(port, refresh,rounds) == 1) {
            printLog("Error encountered in host_player.");
			return 1; 
        }
    }
    else {
		if (client_player(port, host,rounds) == 1) {
            printLog("Error encountered in client_player.");
			return 1;
        }
        // Set refresh rate to value received from server
        refresh = recv_refresh;
        // Set rounds to val received from server
        rounds = recv_rounds;
        cout << "recv_rounds " << recv_rounds << endl;
        //printLog("rounds:");
        printLog(to_string(recv_rounds));
        cout << recv_rounds << endl;
    }
   
    // Set up ncurses environment
    initNcurses();

    // Set starting game state and display a countdown
    reset();
    countdown("Starting Game");
    
    // Listen to keyboard input in a background thread
    pthread_t pth;
    pthread_create(&pth, NULL, listenInput, NULL);

	// Listen for game state updates from other player
	pthread_create(&pth, NULL, listenUpdate, NULL);


    // Main game loop executes tock() method every REFRESH microseconds
    struct timeval tv;
    while(1) {
        gettimeofday(&tv,NULL);
        unsigned long before = 1000000 * tv.tv_sec + tv.tv_usec;
        tock(); // Update game state
        gettimeofday(&tv,NULL);
        unsigned long after = 1000000 * tv.tv_sec + tv.tv_usec;
        unsigned long toSleep = refresh - (after - before);
        // toSleep can sometimes be > refresh, e.g. countdown() is called during tock()
        // In that case it's MUCH bigger because of overflow!
        if(toSleep > refresh) toSleep = refresh;
        usleep(toSleep); // Sleep exactly as much as is necessary
    }
    
    // Clean up
    pthread_join(pth, NULL);
    endwin();
    return 0;
}




