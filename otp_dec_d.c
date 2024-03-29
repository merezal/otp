#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>

char* decrypt(char* ctxt, char* key){
    int p = 0,k = 0, len = strlen(ctxt);
    int c = 0;
    int o;
    char* deciphered = malloc((len+2)*sizeof(char));
    memset(deciphered, '\0', len+2);
    for(c = 0; c < len; c++){
        // Get letters removing ascii
        p = ctxt[c]-64;
        k = key[c]-64;

        // If space set to zero
        if(p<0){
            p = 0;
        }
        if(k<0){
            k = 0;
        }
        
        o=p-k;
        // Add 26 characters and 1 space
        if(o<0){
            o+=27;
        }
        // If zero then space ascii
        if(o==0){
            o=32;
        }
        // Else convert to ascii
        else{
            o+=64;
        }

        deciphered[c] = o;
        // If o=0 then space
        o=0;
    }
    // Append newline
    deciphered[len]='\n';
    deciphered[len+1]='\0';
    return deciphered;
}
// Returns opened socket filedescriptor
int openSocket(int portNumber){
    int listenSocketFD;
    struct sockaddr_in serverAddress;

	// Set up the address struct for this process (the server)
	memset((char *)&serverAddress, '\0', sizeof(serverAddress)); // Clear out the address struct
	serverAddress.sin_family = AF_INET; // Create a network-capable socket
	serverAddress.sin_port = htons(portNumber); // Store the port number
	serverAddress.sin_addr.s_addr = INADDR_ANY; // Any address is allowed for connection to this process

    // Open listen socket
    listenSocketFD = socket(AF_INET, SOCK_STREAM, 0); // Create the socket
    if (listenSocketFD < 0) perror("ERROR opening socket");
    // Enable the socket to begin listening
    if (bind(listenSocketFD, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0) // Connect socket to port
    {
        perror("ERROR on binding");
        exit(1);
    }
    listen(listenSocketFD, 5); // Flip the socket on - it can now receive up to 5 connections
    return listenSocketFD;
}
// Accepts a connection returning Connection FD, returns -1 if ERROR accepting connection
int acceptCon(int listenSocketFD){
    struct sockaddr_in clientAddress;
    socklen_t sizeOfClientInfo;
    int establishedConnectionFD;
	sizeOfClientInfo = sizeof(clientAddress); // Get the size of the address for the client that will connect
	establishedConnectionFD = accept(listenSocketFD, (struct sockaddr *)&clientAddress, &sizeOfClientInfo); // Accept
	if (establishedConnectionFD < 0){
        perror("ERROR on accept");
        return -1;
    }
    else{
        return establishedConnectionFD;
    }
}

// Verify Connected is otp_enc
// 1 if verified, 0 if error
int verifyConnection(int establishedConnectionFD){
	int charsRead = -5;
    char buffer[2];
    memset(buffer, '\0', 2);
    // Get encryption char
	charsRead = recv(establishedConnectionFD, buffer, 1, 0);
    if (charsRead <= 0) {
        perror("ERROR reading from socket\n");
    	fprintf(stderr,"SERVER: I received this from the client: \"%s\"\n", buffer);
        return 0;
    }
    
    // If not decryption char
    if(buffer[0]!='d'){
        fprintf(stderr, "REJECTED:\nNot otp_dec\n");
        buffer[0]='r';
        send(establishedConnectionFD, buffer, 1, 0);
        return 0;
    }
    // Else return 1
    else{
        buffer[0]='a';
        send(establishedConnectionFD, buffer, 1, 0);
        return 1;
    }

}

// Returns ciphertext back to client
void sendCipher(int establishedConnectionFD, char* cipher){
    int len = strlen(cipher);
    int charsOut = 0, sent = 0;
    do{
        // Send Plaintext
        charsOut = send(establishedConnectionFD, cipher, len, 0);
        sent+=charsOut;
    }while(sent!=len);
    return;
}
/*-------------------------------Handle--connection------------------------------*/
char* getSocketString(int socketFD){
    int charsRead = -5;
    char buffer[11];
    char* key = NULL;// Not necessarily the key, could be ptxt
    int strLength = 1; // Complete length of string + NULL term
    memset(buffer, '\0', 11);

    // Loop until no more characters received
    do{
        charsRead = recv(socketFD, buffer, 10, 0);
        if(charsRead>0){
            // Stop at newline
            charsRead = strcspn(buffer, "\n");
            strLength += charsRead;

            // Reallocate enough space
            key = realloc(key, strLength * sizeof(char));
            
            // If first time initialize
            if(strLength<=11){
                memset(key, '\0', strLength);
            }

            // Copy until newline
            strncat(key, buffer, charsRead);
            memset(buffer, '\0', 11);

            // If encountered newline, stop looping
            if(charsRead<10){
                break;
            }
        }
    }while(charsRead!=0);

    // Acknowledge string received
    charsRead = send(socketFD, "!", 1, 0);

    return key;

}
// Do valid connection, loop forever
void handleConnection(int lstnFD){
    int connection = -1;

    // Loop connections forever
    while(connection==-1){
        // Accept 
        // connection is the FD of accepted
        connection = acceptCon(lstnFD);
        if(connection!=-1){
            // If could not verify, loop new connection
            if(!verifyConnection(connection)){
                connection=-1;
            }
            else{
                char* key = NULL;
                char* ptxt = NULL;
                char* cipher = NULL;
                cipher = getSocketString(connection);
                key = getSocketString(connection);

                // Encrypt
                ptxt = decrypt(cipher, key);
            
                sendCipher(connection, ptxt);

                free(key);
                free(ptxt);
                free(cipher);
            }
        }

        // Close connection, loop new connection
        if(close(connection)==0){
            connection=-1;
        }
    }
    return;
}
int main(int argc, char* argv[]){
    if(argc<2){
        fprintf(stderr, "Syntax Error!\nUsage: %s port\n", argv[0]);
        return 1;
    }
    else{

        pid_t spawnpid[5];
        int i, port = atoi(argv[1]);
        // Open socket on port
        int sockFD = openSocket(port);

        // Fork child processes
        for(i=0;i<5;i++){
            spawnpid[i]=fork();
            // New process begin accepting connections
            if(spawnpid[i] == 0){
                handleConnection(sockFD);
                exit(0);
            }
            // Catch fork failing
            else if(spawnpid[i] == -1){
                fprintf(stderr, "Error starting process pool\n");
                exit(3);
            }
        }
        // Connect
        int j;
        pid_t done;
        for(j = 0; j < 5; j++){
            done = wait(&i);
            printf("%d DONE: %d\n", i, done);
        }

        close(sockFD); // Close the listening socket

    }
}