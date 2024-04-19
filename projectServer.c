#include<sys/socket.h>
#include<sys/types.h>
#include<sys/select.h>
#include<sys/time.h>
#include<sys/socket.h>
#include<netinet/ip.h>
#include<arpa/inet.h>
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<signal.h>
#include<string.h>
#include<pthread.h>
#include<fcntl.h>
#include<errno.h>
#include<pthread.h>
#include<map>
#include<vector>
#include<algorithm>
#include<string>

const int initialHealth = 100;
const int initialStats  = 100;
const int statLimit     = 65535;
const char* narrator    = "Server";
const int maxBots       = 1000;
int numClients          = 0;

typedef struct{ 
	uint16_t roomNumber;
	char name[32];
	char description[256];
} Room;
typedef struct{
	uint8_t type; 
	char name[32]; 
	uint8_t flags; uint16_t attack; uint16_t defense; uint16_t regen; int16_t health; uint16_t gold; uint16_t roomNumber; uint16_t descLength; char* description; int fd;
}character;
struct message{
	uint8_t type;
	uint16_t messageLength; char recipient[32]; char sender[32]; char* message;
};

bool sendVersion(int client_fd);
bool sendGame(int client_fd);
bool sendError(int client_fd, uint8_t errorCode, const char* errorMessage);
bool sendAccept(int client_fd, uint8_t actionType);
bool sendCharacter(int client_fd, character*ch);
bool sendRoom(int client_fd, Room room);
void sendConnections(int client_fd, uint16_t roomNumber);
bool sendMessage(int client_fd, const char* sender, const char* recipient, const char* message);

Room getRoomInfo(uint16_t roomNumber);
character* receiveCharacter(int client_fd);
void getMonsterInfo(int client_fd, uint16_t roomNumber, uint16_t monsterNumber);
struct message* receiveMessage(int client_fd);
uint16_t receiveChangeroom(int client_fd);

void handleFight(int client_fd, character* ch);
bool areMonstersInRoom(uint16_t roomNumber);
void addCharacterToRoom(character* ch, uint16_t roomNumber);
void sendCharactersInRoom(const char* clientName, uint16_t roomNumber);
void sendCharacterToAll(int client_fd, uint16_t roomNumber, character* sender);
void sendAllCharactersInRoom(uint16_t roomNumber);
void removeCharacterFromRoom(int client_fd, character* ch, uint16_t roomNumber);
void removeCharacter(int client_fd, character* ch);
void removeDeadCharacters();
int findClientFdByName(const char* name);
int16_t findMonsterGold(const std::string& monsterName, uint16_t roomNumber);

bool roomConnections[20][20] = {false};
bool areRoomsConnected(uint16_t room1, uint16_t room2) {
    return roomConnections[room1][room2] || roomConnections[room2][room1];
}

bool monsterAdded[20][10];
void initializeMonsterAdded() {
    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 10; j++) {
            monsterAdded[i][j] = false;
        }
    }
}

void* handleClient(void* arg);

std::map<uint16_t, std::vector<character*>> roomCharacters;

int main(int argc, char ** argv){
        signal(SIGPIPE, SIG_IGN);
        struct sockaddr_in sad;
        int server_socket;
        sad.sin_port = htons(5012); //5010 through 5014
        sad.sin_addr.s_addr = INADDR_ANY;
        sad.sin_family = AF_INET;

        int skt = socket(AF_INET, SOCK_STREAM, 0); // Step 1
        if(skt == -1){
                perror("socket");
                return 1;
        }
        if( bind(skt, (struct sockaddr*)(&sad), sizeof(struct sockaddr_in)) ){ // step 2
                perror("bind");
                return 1;
        }
        if( listen(skt, 5) ){ // step 3
                perror("listen");
                return 1;
        }

        pthread_t threads[maxBots];
        void initializeMonsterAdded();
        while(1){

                int client_fd;
                struct sockaddr_in client_address;

                socklen_t address_size = sizeof(struct sockaddr_in);
                client_fd = accept(skt, (struct sockaddr *)(&client_address), &address_size); // step 4
                printf("\nConnection made from address %s\n", inet_ntoa(client_address.sin_addr));
                printf("The numClients is: %d\n", numClients);
                if(numClients + 1 > maxBots){
                        removeDeadCharacters();
                }
                if (numClients > maxBots) {
                        printf("Maximum number of clients reached. Connection refused.\n");
                        close(client_fd);
                        continue;
                }
                pthread_create(&threads[numClients], NULL, handleClient, &client_fd);
                numClients++;
        }

        close(skt);
        return 0;
}
void* handleClient(void* arg){
	int client_fd = *((int*)arg);
        struct sockaddr_in client_address;
        uint8_t command;
        bool characterCreated = false;
        bool startedGame = false;
        uint16_t roomNumberChange;
	uint16_t oldNumber;
	char joinMessage[100];

        sendVersion(client_fd);
        sendGame(client_fd);
        character* ch = NULL;
	while(1){
		 printf("\n\nClient FD: %d\n\n", client_fd);
                 if(recv(client_fd, &command, 1, 0) != 1) {
                        // Error receiving command
                        perror("recv");
                        close(client_fd);
                        return NULL; 
                }
                 if(command==1){   //Message
			message *msg = receiveMessage(client_fd);
			if(!startedGame){
				sendError(client_fd, 5, "Start the game before sending a message!");
			}
			else{
                       		printf("Waiting for a message from %s to %s\n", msg->sender, msg->recipient);
			
				int charNumber = findClientFdByName(msg->recipient);
				if(charNumber < 0)
					sendError(client_fd, 0, "Could not find character!");
				else{
					sendMessage(charNumber, msg->sender, msg->recipient, msg->message);	
					sendAccept(client_fd, 1);
				}
			}
                }
                else if(command==2){ //Changeroom
                        roomNumberChange=receiveChangeroom(client_fd);
			if(startedGame){
				if((ch->flags >> 7) & 1){
                        		if(roomNumberChange<1 || roomNumberChange>20){
                                		sendError(client_fd, 1, "Bad room! Room does not exist.");	
                        		}
					else if (!areRoomsConnected(ch->roomNumber, roomNumberChange)) {
            					sendError(client_fd, 1, "Room not connected! You cannot move to that room.");
					}
                        		else{
                                		//sendAccept(client_fd, 2);
						removeCharacterFromRoom(client_fd, ch, ch->roomNumber);	
						oldNumber = ch->roomNumber;
						ch->roomNumber=roomNumberChange;
                                		Room roomInfo = getRoomInfo(roomNumberChange);
                                		sendRoom(client_fd, roomInfo);
						sendCharacter(client_fd, ch);

						sendConnections(client_fd, roomNumberChange);

						addCharacterToRoom(ch, ch->roomNumber);
                                        	sendCharactersInRoom(ch->name, ch->roomNumber);
						sendCharacterToAll(client_fd, oldNumber, ch);
						
						if((ch->flags >> 6) & 1 && (ch->flags >> 7) & 1){       //If join battle is set and not dead, auto battle
							while(areMonstersInRoom(ch->roomNumber)==true && (ch->flags >> 7) & 1)
								handleFight(client_fd, ch);
						}

                        		}
				}
				else{
                                	sendError(client_fd, 7, "You are dead!");
				}
			}
			else{
				sendError(client_fd, 5, "Too Early! Make a character and start first.");
			}
                }
                else if (command == 3) { // Fight
                	if(startedGame)
				handleFight(client_fd, ch);
			else
				sendError(client_fd, 5, "Too Early! Make a character and start first.");
		
		}
		else if(command==4){
			char monsterName[32];
            		recv(client_fd, monsterName, 32, 0);

			sendError(client_fd, 8, "Server does not support PVP fights!");
		}
		else if(command==5){
			char monsterName[32];
            		recv(client_fd, monsterName, 32, 0);
			if(startedGame){
				if((ch->flags >> 7) & 1){
					printf("Looting!\n");
            				// Receive the length of the monster name
            				//char monsterName[32];
            				//recv(client_fd, monsterName, 32, 0);
					printf("Trying to loot monster: %s\n", monsterName);

            				// Search for the monster in the room
           				uint16_t roomNumber = ch->roomNumber;
            				int16_t monsterGold = findMonsterGold(monsterName, roomNumber);
					if(monsterGold == 0){
						sendError(client_fd, 3, "No available gold to loot!");
					}
					else if (monsterGold > 0) {
                				// Apply the gold to the character
                				ch->gold += monsterGold;
                				// Inform the client about the looted gold
                				char joinMessage[256];
                				sprintf(joinMessage, "You looted %d gold from %s!\n", monsterGold, monsterName);
                				sendMessage(client_fd, narrator, ch->name, joinMessage);
						sendCharacter(client_fd, ch);
						sendAllCharactersInRoom(ch->roomNumber);
	    				}
					else if(monsterGold == -1){
						sendError(client_fd, 3, "Monster is still alive, can't loot!");
					}
					else if(monsterGold == -2){
						sendError(client_fd, 3, "Attempt to loot a nonexistent monster!");
					}
				}
				else{
					sendError(client_fd, 7, "You are dead!");
				}
			}
			else{
				sendError(client_fd, 5, "Too Early! Make a character and start first.");
			}
		}
                else if(command==6){              //Start Game
			if(!startedGame){
                        	if(!characterCreated){
                                	sendError(client_fd, 5, "Character not created. Please create a character before starting the game.");
                        	}
                        	else{
                                	printf("Starting game!!\n");
                                	startedGame=true;
					ch->flags |= (1 << 4); //set started bit
                                	sendAccept(client_fd, 6);
                                	sendCharacter(client_fd, ch);
                                	sprintf(joinMessage, "%s has joined the game!", ch->name);
                                	sendMessage(client_fd, narrator, ch->name, joinMessage);
                                	Room roomInfo = getRoomInfo(ch->roomNumber);
                                	sendRoom(client_fd, roomInfo);
 				
					addCharacterToRoom(ch, ch->roomNumber);
                                	sendCharactersInRoom(ch->name, ch->roomNumber);
				
					sendConnections(client_fd, ch->roomNumber);

                        	}
			}
			else{
				sendError(client_fd, 0, "Game already started!");
			}
                }
                else if(command==10){
			if(startedGame){
				character *temp = receiveCharacter(client_fd); //stores it so it doesnt error
				sendError(client_fd, 2, "You already made a player!");
			}
			else{
                        	ch=receiveCharacter(client_fd);
				int charNumber = findClientFdByName(ch->name);
				characterCreated = false;
				if(charNumber < 0){
                                	if(ch->attack + ch->defense + ch->regen > initialStats){
                                        	sendError(client_fd, 4, "Stat error! Automaticaly assigning stats.");
						ch->attack = initialStats/3;
						ch->defense = initialStats/3;
						ch->regen = initialStats/3;
                                	}

                                	ch->health=initialHealth; //Only do this for new characters
                                	ch->gold=0;
                               		ch->roomNumber=1;

					ch->flags |= (1 << 7);//set alive to 1
					ch->flags |= (1 << 3);//set ready to 1
					
					sendAccept(client_fd, 10);
					sendCharacter(client_fd, ch);
					characterCreated = true;		

				}
				else{
					sendError(client_fd, 2, "Player Name already exists!");
				}
			}

                }
                else if (command==12){   //Leave Game
                        printf("Leave game here\n");
			sendAccept(client_fd, 12);
			if(characterCreated){
				if((ch->flags >> 7) & 1){
					printf("Removing alive character!\n");
                       			removeCharacter(client_fd, ch);
				}
			}
                        close(client_fd);
                        return NULL;
                }
                else{
                        printf("Invalid command! User entered an invalid command: %u\n", command);
                        sendError(client_fd, 0, "User did not enter a valid command.");
			close(client_fd);
                }
	}
        close(client_fd);
        return NULL;
}//------------------------Sending to the Client----------------------------------

bool sendVersion(int client_fd){
	printf("Sending Version!!\n");
        unsigned char version_message[5];
	int bytes = 5;
        version_message[0] = 14; // Type
        version_message[1] = 2;  // LURK major revision
        version_message[2] = 3;  // LURK minor revision
        version_message[3] = 0;  // Size of extensions
        version_message[4] = 0;  //List of extensions

        return bytes = send(client_fd, version_message, sizeof(version_message), 0); //Send the VERSION, type 14
}
bool sendGame(int client_fd){
 	unsigned char message[1024];
    	message[0] = 11; // Type
    	unsigned short initialPoints = 100; // initial points
    	unsigned short statsLimit = statLimit; // stat limit
    	char description[] ="\n\n\t     []  ,----.___\n\t   __||_/___      '.\n\t  / O||    /|       )\n\t /   \"\"   / /   =._/\n\t/________/ /\n\t|________|/   This is a cool game!\n"; //game description
    	unsigned short descriptionLength = strlen(description);

    	memcpy(&message[1], &initialPoints, sizeof(unsigned short));
    	memcpy(&message[3], &statsLimit, sizeof(unsigned short));
    	memcpy(&message[5], &descriptionLength, sizeof(unsigned short));
    	memcpy(&message[7], description, descriptionLength);

    	ssize_t bytesSent = send(client_fd, message, 7 + descriptionLength, 0); // Send the GAME, type 11
	if (bytesSent == -1) {
        	perror("send");
        	return false;
    }
	    return true;
}	
bool sendError(int client_fd, uint8_t errorCode, const char* errorMessage){
	printf("Sending Error!!\n");
	uint16_t messageLength = strlen(errorMessage);
	
       	uint8_t buffer[4 + messageLength];
   	buffer[0] = 7; // Type
    	buffer[1] = errorCode;
    	buffer[2] = messageLength & 0xFF;        // Least significant byte
    	buffer[3] = (messageLength >> 8) & 0xFF; // Most significant byte

      	memcpy(buffer + 4, errorMessage, messageLength);
       	ssize_t bytesSent = send(client_fd, buffer, sizeof(buffer), 0);
	
 	if (bytesSent == -1) {
        	perror("send");
        	return false;
	}else if (bytesSent < sizeof(buffer)) {
       		printf("Incomplete message sent.\n");
        return false;
    }	 
    	return true;	
}
bool sendAccept(int client_fd, unsigned char actionType){
 	printf("Sending Accept!!\n");
	
        uint8_t buffer[2];
    	buffer[0] = 8; // Type
   	buffer[1] = actionType;

       	ssize_t bytesSent = send(client_fd, buffer, sizeof(buffer),0);
	if (bytesSent == -1) {
        	perror("send");
    		return false;
	}

       	return true;	
}
bool sendCharacter(int client_fd, character* ch){
	printf("Sending Character!!%s\n", ch->name);
	printf("Room Number character: %d\n", ch->roomNumber);
	
	int bytes = 48;
	size_t totalSize= bytes + ch->descLength;
	char* buffer = (char*)malloc(totalSize);

	if(buffer == NULL){
		printf("Failed to allocate memory for character buffer.\n");
		return false;
	}

	memcpy(buffer, ch, bytes);
	memcpy(buffer+bytes, ch->description, ch->descLength);
	ssize_t bytesSent = send(client_fd, buffer, totalSize, 0);
	bool success = (bytesSent == totalSize);
 
	free(buffer);
	return success;
}
bool sendRoom(int client_fd, Room room){
	printf("Sending Rooms!!\n");	
 
	int bytes = 37;
	size_t totalSize = bytes + strlen(room.description);
	char* buffer = (char*)malloc(totalSize);

	buffer[0] = 9; 
	memcpy(buffer + 1, &room.roomNumber, 2);
	memcpy(buffer + 3, room.name, 32);
	uint16_t descLength = strlen(room.description);
	memcpy(buffer + 35, &descLength, 2);
	memcpy(buffer + 37, room.description, descLength);

	ssize_t bytesSent = send(client_fd, buffer, totalSize, 0);	
	bool success = (bytesSent == totalSize);

	free(buffer);
	return success;
}
void sendConnections(int client_fd, uint16_t roomNumber) {
    printf("Sending Connections for Room %d\n", roomNumber);
	
    FILE *map_file = fopen("map.txt", "r");
    if (map_file == NULL) {
        perror("Failed to open map file");
        return;
    }
    char line[256];
    bool foundRoom = false;
    while (fgets(line, sizeof(line), map_file)) {
        uint16_t currentRoomNumber;
        if (sscanf(line, "ROOM %hu", &currentRoomNumber) == 1 && currentRoomNumber == roomNumber) {
            // Found the room with the matching room number
            foundRoom = true;
        } else if (foundRoom && strncmp(line, "CONNECT", 7) == 0) {
            uint16_t connectedRoom;
                    sscanf(line, "CONNECT %hu", &connectedRoom);
		    roomConnections[currentRoomNumber][connectedRoom]=true;
		   
                    Room connectedRoomInfo = getRoomInfo(connectedRoom);
                    char buffer[37 + strlen(connectedRoomInfo.description)];
                    buffer[0] = 13; // Type
                    memcpy(buffer + 1, &connectedRoom, 2); // Room number
                    memcpy(buffer + 3, connectedRoomInfo.name, 32); // Room name
                    uint16_t descLength = strlen(connectedRoomInfo.description);
                    memcpy(buffer + 35, &descLength, 2); // Room description length
                    memcpy(buffer + 37, connectedRoomInfo.description, descLength); // Room description
		  
                    ssize_t bytesSent = send(client_fd, buffer, sizeof(buffer), 0);
                    if (bytesSent != sizeof(buffer)) {
                        printf("Failed to send connection to room %d\n", connectedRoom);}
	} else if (foundRoom && strncmp(line, "MONSTER", 7) == 0) {
            	uint16_t monsterNumber;
            	sscanf(line, "MONSTER %hu", &monsterNumber);

            	getMonsterInfo(client_fd, roomNumber, monsterNumber);

        } else if (foundRoom && strncmp(line, "ROOM", 4) == 0) {
            break;
        }
    }
    fclose(map_file);
}
bool sendMessage(int client_fd, const char* sender, const char* recipient, const char* message) {
	printf("Sending Message!!\n");	
    	uint8_t buffer[68 + strlen(message)];
   	 buffer[0] = 1; // Type
    	uint16_t messageLength = strlen(message)+1; 
    	memcpy(buffer + 1, &messageLength, 2); // Message Length
    	strncpy((char*)(buffer + 3), recipient, 32); // Recipient Name
    	strncpy((char*)(buffer + 35), sender, 30); // Sender Name
    	buffer[65] = '\0'; // End of Sender Name
	if(strcmp(sender, "Server")==0){
    		buffer[66] = 1; // Narration marker
	}
	else{
		buffer[66] = 0;
	}
    	strcpy((char*)(buffer + 67), message);

    	size_t totalSize = 68 + strlen(message);
    	ssize_t bytesSent = send(client_fd, buffer, totalSize, 0);
    	return bytesSent == totalSize;
}
//----------------------Receiving from the Client---------------------------------
character* receiveCharacter(int client_fd) {
    printf("Receiving Character!!\n");

    char buffer[48 + 256+ sizeof(uint16_t)];
    ssize_t bytes_received;
    fd_set set;
    struct timeval timeout;
    // Initialize set to listen on client_fd
    FD_ZERO(&set);
    FD_SET(client_fd, &set);
    // Initialize timeout to 1 second
    timeout.tv_sec = 2;
    timeout.tv_usec = 0;
    // Loop until all data is received
    size_t total_bytes_received = 0;
    while (1) {
        // Wait for data to become available or timeout
        int ready = select(client_fd + 1, &set, NULL, NULL, &timeout);
        if (ready == -1) {
            perror("select");
            return NULL;
        } else if (ready == 0) {
            printf("Timeout: No more data received.\n");
            break; // No more data available, exit the loop
        }
        // Data is available, proceed with recv
        bytes_received = recv(client_fd, buffer + total_bytes_received, sizeof(buffer) - total_bytes_received, 0);
        if (bytes_received == -1) {
            perror("recv");
            return NULL;
        } else if (bytes_received == 0) {
            printf("Connection closed by peer.\n");
            return NULL;
        }
        total_bytes_received += bytes_received;
        // Check if we have received the entire character
        if (total_bytes_received >= 47 + sizeof(uint16_t)) { // Minimum size for character data
            uint16_t desc_length;
            memcpy(&desc_length, buffer + 45, sizeof(uint16_t));
            size_t expected_length = 47 + desc_length;
            if (total_bytes_received >= expected_length) {
                printf("Received entire character.\n");
                break; // Exit the loop, we have received all data for the character
            }
        }
        // Adjust timeout for the next select call
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
    }

    // Allocate memory for the character struct
    character *ch = (character*) malloc(48 + sizeof(size_t) + sizeof(int16_t));
    if (ch == NULL) {
        perror("malloc");
        return NULL;
    }
    ch->type = 10;
    ch->fd = client_fd;
    memcpy(ch->name, buffer, 32);
    ch->name[31] = '\0';
    ch->flags = buffer[32];
    memcpy(&ch->attack, buffer + 33, 2);
    memcpy(&ch->defense, buffer + 35, 2);
    memcpy(&ch->regen, buffer + 37, 2);
    memcpy(&ch->health, buffer + 39, 2);
    memcpy(&ch->gold, buffer + 41, 2);
    memcpy(&ch->roomNumber, buffer + 43, 2);
    memcpy(&ch->descLength, buffer + 45, sizeof(uint16_t));

    ch->description = (char*)malloc(ch->descLength + 1);
    if (ch->description == NULL) {
        perror("malloc");
        free(ch);
        return NULL;
    }
    memcpy(ch->description, buffer + 47, ch->descLength);
    ch->description[ch->descLength] = '\0';
    // Print character information
    printf("Character received:\n");
    printf("Name: %s\n", ch->name);
    printf("Flags: %02x\n", ch->flags);
    printf("Attack: %hu\n", ch->attack);
    printf("Defense: %hu\n", ch->defense);
    printf("Regen: %hu\n", ch->regen);
    printf("Health: %d\n", ch->health);
    printf("Gold: %hu\n", ch->gold);
    printf("Room Number: %hu\n", ch->roomNumber);
    printf("Description Length: %hu\n", ch->descLength);
    printf("Description: %s\n", ch->description);

   ch->flags &= ~(1<<5); //clear the monster and start bit since they can't start like that
   ch->flags &= ~(1<<4);
   ch->flags &= ~(1<<2);//clear the last 3 we dont use
   ch->flags &= ~(1<<1);
   ch->flags &= ~(1<<0);

   printf("Flags: Alive: %d, Join Battle: %d, Monster: %d, Started: %d, Ready: %d\n",
           (ch->flags >> 7) & 1,
           (ch->flags >> 6) & 1,
           (ch->flags >> 5) & 1,
           (ch->flags >> 4) & 1,
           (ch->flags >> 3) & 1);


    return ch;
}
struct message* receiveMessage(int client_fd) {
    printf("Receiving Message!!\n");

    struct message *msg = (struct message*) malloc(67+sizeof(size_t));
    if (msg == NULL) {
        perror("malloc");
        return NULL;
    }
    recv(client_fd, &msg->messageLength, 66, MSG_WAITALL); 
    msg->type = 1;
    msg->sender[31] = '\0';
    msg->recipient[31] = '\0';

    msg->message = (char*) malloc(msg->messageLength + 1);
      
    ssize_t message_received = recv(client_fd, msg->message, msg->messageLength, MSG_WAITALL);
    if (message_received != msg->messageLength) {
        perror("recv message");
        free(msg->message); // Free message memory
        free(msg); // Free message struct memory
        return NULL;
    }
    msg->message[msg->messageLength] = 0;
    return msg;
}




uint16_t receiveChangeroom(int client_fd){
	printf("Receiving Changeroom!!\n");
    	uint16_t room_number;
    	ssize_t bytes_received = recv(client_fd, &room_number, sizeof(uint16_t), 0);
    	if (bytes_received != sizeof(uint16_t)) {
        	perror("recv");
        	return 0;     }
    	printf("Received CHANGEROOM message. New room number: %d\n", room_number);
    	return room_number;	
}
//---------------------------Map Functions-------------------------i

Room getRoomInfo(uint16_t roomNumber) {
    printf("Retrieving Room Info for Room %d\n", roomNumber);

    Room room;
    room.roomNumber = roomNumber;

    FILE *map_file = fopen("map.txt", "r");
    if (map_file == NULL) {
        perror("Failed to open map file");
        snprintf(room.name, sizeof(room.name), "Default Room");
        snprintf(room.description, sizeof(room.description), "This is the default room!");
        return room;
    }
    char line[256];
    while (fgets(line, sizeof(line), map_file)) {
        uint16_t currentRoomNumber;
        if (sscanf(line, "ROOM %hu", &currentRoomNumber) == 1) {
         
            if (currentRoomNumber == roomNumber) {
                fgets(line, sizeof(line), map_file); // Read the next line (Room name)
                sscanf(line, "NAME %[^\n]", room.name);
               
                fgets(line, sizeof(line), map_file); // Read the next line (Room description)
                sscanf(line, "DESCRIPTION %[^\n]", room.description);
                
                fclose(map_file);
                return room;
            }
        }
    }

    fclose(map_file);
    snprintf(room.name, sizeof(room.name), "Default Room");
    snprintf(room.description, sizeof(room.description), "This is the default room!");
    return room;
}
void handleFight(int client_fd, character* ch){
	char joinMessage[100];
	printf("Starting Fight!\n");
        	if((ch->flags >> 7) & 1){
                	 int currentRoom = ch->roomNumber;
                         bool foundOpponent = false;
                         auto& characters = roomCharacters[currentRoom]; // Get reference to the vector of characters

                         for (auto it = characters.begin(); it != characters.end(); ++it) {
                         	character* opponent = *it; // Get the opponent character
				if (opponent != ch && opponent->flags & (1 << 5) && (opponent->flags >> 7) & 1) {
                                	foundOpponent = true;
                                        int playerDamage = ch->attack - opponent->defense;
                                        if (playerDamage < 0) {
                                        	playerDamage = 0; // Ensure non-negative damage
                                        }
                                        opponent->health -= playerDamage;
                                        sprintf(joinMessage, "You attacked %s for %d damage!\n", opponent->name, playerDamage);
                                        sendMessage(client_fd, narrator, ch->name, joinMessage);

                                        if (opponent->health <= 0) {
                                        	// Opponent defeated
                                                sprintf(joinMessage, "%s has been defeated!", opponent->name);
                                                sendMessage(client_fd, narrator, ch->name, joinMessage);
						opponent->flags &= ~(1<<7); //clear the alive bit
                                        }
                                }
			}
                        // Now let monsters attack the player
                        if (foundOpponent){
                        	for (auto it = characters.begin(); it != characters.end(); ++it) {
                                	character* opponent = *it;
                                        // Check if the opponent is a monster and not the player
                                        if (opponent != ch && opponent->flags & (1 << 5) && (opponent->flags >> 7) & 1) {
                                        	int monsterDamage = opponent->attack - ch->defense;//calculate damage
                                                if (monsterDamage < 0) {
                                                	monsterDamage = 0; // Ensure non-negative damage
                                                }
                                                ch->health -= monsterDamage; //apply  damage to player
                                                sprintf(joinMessage, "%s has attacked you for %d damage!\n", opponent->name, monsterDamage);
                                                sendMessage(client_fd, narrator, ch->name, joinMessage);

                                                // Check if the player is defeated
                                                if (ch->health <= 0) {
                                                	sprintf(joinMessage, "You have been defeated by %s!\n", opponent->name);
                                                        sendMessage(client_fd, narrator, ch->name, joinMessage);

                                                        ch->flags &= ~(1 << 7); // Clear the alive bit
                                                        //break; // Exit the loop since the player is defeated
                                                }
                                         }
				}
                                sendCharacter(client_fd, ch); // Send updated player information
                                sendAllCharactersInRoom(ch->roomNumber);
			}
                        else{
                        	sendError(client_fd, 7, "No available monsters to fight!");
                        }
		}
                else{
                	sendError(client_fd, 7, "You are dead!");
                }
}
bool areMonstersInRoom(uint16_t roomNumber) {
    const std::vector<character*>& characters = roomCharacters[roomNumber];
    for (character* ch : characters) {
        // Check if the character is a monster and is alive
        if ((ch->flags & (1 << 5)) && ((ch->flags >> 7) & 1)) {
            return true;
        }
    }
    return false;
}
void getMonsterInfo(int client_fd, uint16_t roomNumber, uint16_t monsterNumber) {
    printf("Searching for Monster %d\n", roomNumber);

    FILE *map_file = fopen("map.txt", "r");
    if (map_file == NULL) {
        perror("Failed to open map file");
        return;
    }
    char line[256];
    bool foundMonster = false;
    while (fgets(line, sizeof(line), map_file)) {
        int tempMonsterNumber;
        if (sscanf(line, "MONSTER %d", &tempMonsterNumber) == 1 && tempMonsterNumber == monsterNumber) {
            // Check if the monster has already been added to this room
            if (!monsterAdded[roomNumber][monsterNumber]) {
                // Mark the monster as added to this room
                monsterAdded[roomNumber][monsterNumber] = true;
                foundMonster = true;
                // Create a new character struct for this monster
                character* monsterCharacter = (character*)malloc(sizeof(character));
                if (monsterCharacter == NULL) {
                    fprintf(stderr, "Failed to allocate memory for monster\n");
                    break;
                }
                monsterCharacter->type = 10;
                monsterCharacter->flags = 0;
                monsterCharacter->roomNumber = roomNumber;
                monsterCharacter->descLength = 0;
                monsterCharacter->flags |= (1 << 7); // Set Alive flag to 1
                monsterCharacter->flags |= (1 << 5); // Set Monster flag 
                sscanf(line, "MONSTER %d", &(monsterCharacter->type));
                fgets(line, sizeof(line), map_file); // Read the next line (Monster name)

                int nameRead = sscanf(line, "NAME %255[^\n]", monsterCharacter->name);
                if (nameRead == 1) {
                    monsterCharacter->name[strcspn(monsterCharacter->name, "\n")] = '\0'; // Remove potential newline from name
                } else {
                    fprintf(stderr, "Error parsing monster name\n");
                    free(monsterCharacter);
                    break;
                }
                fgets(line, sizeof(line), map_file); // Read the next line (Monster description)
                char *descriptionStart = strstr(line, "DESCRIPTION");
                if (descriptionStart != NULL) {
                    descriptionStart += strlen("DESCRIPTION");
                    if (*descriptionStart == ' ') {
                        descriptionStart++; // Skip any leading space after "DESCRIPTION"
                    }
                    char *newline = strchr(descriptionStart, '\n');
                    if (newline != NULL) {
                        *newline = '\0';
                    }
                    // Set description and its length in the struct
                    monsterCharacter->descLength = strlen(descriptionStart);
                    monsterCharacter->description = strdup(descriptionStart); // Duplicate the description to avoid modifying original
                }
                fgets(line, sizeof(line), map_file); // Read the next line (Monster gold)
                sscanf(line, "GOLD %hu", &(monsterCharacter->gold));
                fgets(line, sizeof(line), map_file); // Read the next line (Monster attack)
                sscanf(line, "ATTACK %hu", &(monsterCharacter->attack));
                fgets(line, sizeof(line), map_file); // Read the next line (Monster defense)
                sscanf(line, "DEFENSE %hu", &(monsterCharacter->defense));
                fgets(line, sizeof(line), map_file); // Read the next line (Monster regen)
                sscanf(line, "REGEN %hu", &(monsterCharacter->regen));
                monsterCharacter->type = 10;
                monsterCharacter->health = initialHealth;

                addCharacterToRoom(monsterCharacter, roomNumber);
            }
        }
    }

    fclose(map_file);

    if (!foundMonster) {
        printf("Monster not found.\n");
    }
}

void addCharacterToRoom(character* ch, uint16_t roomNumber) {
    printf("SENDING THE CHARACTER IN ROOM 0\n");
    removeCharacterFromRoom(ch->fd, ch, ch->roomNumber);

    roomCharacters[roomNumber].push_back(ch);
    ch->roomNumber = roomNumber;

    // Notify existing occupants about the new character, excluding the new character itself
    for (character* occupant : roomCharacters[roomNumber]) {
        if (occupant != ch) {
            sendCharacter(occupant->fd, ch);
        }
    }
}

void sendCharactersInRoom(const char* clientName, uint16_t roomNumber){// sends all characters in room to a character
	printf("SENDING THE CHARACTER IN ROOM 1\n");
    if (roomCharacters.find(roomNumber) != roomCharacters.end()) {
        for (character* ch : roomCharacters[roomNumber]) {
            if (strcmp(ch->name, clientName) != 0) { // Exclude sending the character's own information back to themselves
                sendCharacter(findClientFdByName(clientName), ch);
            }        }
    }
}
void sendCharacterToAll(int client_fd, uint16_t roomNumber, character* sender) {// sends character to all characters in room
    printf("SENDING THE CHARACTER IN ROOM 2\n");
    if (roomCharacters.find(roomNumber) != roomCharacters.end()) {
        auto& characters = roomCharacters[roomNumber];
        for (character* receiver : characters) {
            if (receiver->fd != client_fd) { // Exclude sending the character's own information back to themselves
                sendCharacter(receiver->fd, sender);
            }
        }
    }
}
void sendAllCharactersInRoom(uint16_t roomNumber) { // sends all characters in room to all characters in room
    printf("SENDING ALL CHARACTERS IN ROOM 3\n");
    if (roomCharacters.find(roomNumber) != roomCharacters.end()) {
        auto& characters = roomCharacters[roomNumber]; // Get reference to the vector of characters
        for (character* sender : characters) {
            for (character* receiver : characters) {
                if (sender->fd != receiver->fd) { // Exclude sending character's own information back to themselves
                    sendCharacter(receiver->fd, sender);
                }
            }
        }
    }
}
void removeCharacterFromRoom(int client_fd, character* ch, uint16_t roomNumber){
    if (roomCharacters.find(ch->roomNumber) != roomCharacters.end()) {
        auto& characters = roomCharacters[ch->roomNumber];
        auto it = std::find(characters.begin(), characters.end(), ch);
        if (it != characters.end()) {
            characters.erase(it);
        }
    }
}

void removeCharacter(int client_fd, character* ch) {
    removeCharacterFromRoom(client_fd, ch, ch->roomNumber);

    for (auto& room : roomCharacters) {
        auto& characters = room.second;
        auto it = std::find(characters.begin(), characters.end(), ch);
        if (it != characters.end()) {
            characters.erase(it);
	    numClients--;
            break;
        }
    }
    // Clean up memory associated with the character
    delete ch;
}
void removeDeadCharacters() {
    for (auto& roomPair : roomCharacters) {
        for (auto it = roomPair.second.begin(); it != roomPair.second.end(); ) {
            character* ch = *it;
            if (!(ch->flags >> 7) & 1) { // Check if the 7th bit is set (character is dead)
                removeCharacter(ch->fd, ch); // Remove the dead character
                it = roomPair.second.erase(it); // Remove character from roomCharacters vector
            } else {
                ++it;
            }
        }
    }
}
int findClientFdByName(const char* name) {
    for (const auto& room : roomCharacters) {
        for (const auto& ch : room.second) {
            if (strcmp(ch->name, name) == 0) {
                return ch->fd;
            }
        }
    }
    return -1; // Character not found, return -1
}
int16_t findMonsterGold(const std::string& monsterName, uint16_t roomNumber) {
    std::vector<character*>& monsters = roomCharacters[roomNumber];
    for (character* monster : monsters) {
	if ((monster->flags & (1 << 5)) && !(monster->flags & (1 << 7)) && std::string(monster->name) == monsterName) { 
            uint16_t goldValue = monster->gold;
            monster->gold = 0; // Set gold value to 0 after retrieving
            printf("Found monster: %s\n", monsterName.c_str()); // Debugging: Print the monster name
            return goldValue;
        }
	else if((monster->flags & (1 << 5)) && (monster->flags & (1 << 7)) && std::string(monster->name) == monsterName) {
            printf("Monster is still alive: %s\n", monsterName.c_str()); // Debugging: Print that the monster is still alive
            return -1;
	}
    }
    printf("Monster not found: %s\n", monsterName.c_str()); // Debugging: Print that monster was not found
    return -2; // If monster not found, return 0
}


