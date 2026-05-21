#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/errno.h>
#include <sys/types.h>         
#include <sys/socket.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <math.h>
#include <sys/stat.h>
#include <fcntl.h>


// COMPILARE CON -lm PER IL FUNZIONAMENTO DELLA FUNZIONE round()


#define NPORT 1340				//numero di porta scelto arbitrariamente
#define N 7							//dimensione finestra di ricezione


// Variabili globali timeout
 #define code_client		1
#define server_client 	2
#define PERDITA			10
#define TIMEOUT			3000



int sid; 							//descrittore socket
struct sockaddr_in saddr; 		//struttura per l'indirizzo IPv4
char buff[1024];					//buffer ausiliario
int newport;						//variabile per catturare il numero di porta ricevuto dal server
DIR *directid;						//descrittore di directory
struct dirent* dir; 				//struttura per la gestione dei file 
int lenght;							//lunghezza del file
int nmess;							//numero di pacchetti da inviare



typedef struct message_struct {
	int last_byte;							//definisce l'ultimo byte letto dal messaggio
	char message_buff[1024];			//definisce il buffer in cui inserire il contenuto del file
} msg;


void GBNsend(msg *message,int nmess,int fd, int *lastBRead,int lenght){

	int i;
	int ack;
	int seqwin=0;
	int byte;

	while(1){

SEND:
			
			if(i==nmess){
				i++;
				goto WAIT;

			}else if(i > nmess){

					if(ack > lenght){
						goto WAIT;

					}else{

						goto END;
					}	

			}else{

				if(seqwin <= N){

					// Leggo il file ed inserisco i dati nel buffer

					if((byte=read(fd,message[i].message_buff,1024))==-1){
						perror("Error in reading from file\n");
						exit(EXIT_FAILURE);

					}else{

						// Inserisco nella struttura messaggio il valore dell'ultimo byte letto

						if(i==0){

							message[i].last_byte=byte;		//Invio del primo pacchetto

						}else{

							message[i].last_byte=message[i-1].last_byte + byte;		//Invio di un pacchetto che non sia il primo
						}

						lastBRead[i] = message[i].last_byte;

						// Invio

						if(sendto(sid,message[i].message_buff,(strlen(message[i].message_buff)+1),0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
							perror("Error in sending message\n");
							exit(EXIT_FAILURE);
						}
						else{

							memset(buff,0,sizeof(buff));
							sprintf(buff,"%d",message[i].last_byte);

							if(sendto(sid,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
								perror("Error in sending last byte to server\n");
								exit(EXIT_FAILURE);
							}
							else{
								seqwin++;
								i++;
								goto SEND;
							}
						}
					}
				}
				else{

WAIT:				

						memset(buff,0,sizeof(buff));

						if(recvfrom(sid,buff,1024,0,(struct sockaddr*)&saddr,&lenght)==-1){
							perror("Error in receiving from server\n");
							exit(EXIT_FAILURE);
						}
						else{
							ack=atoi(buff);
							seqwin--;
							goto SEND;
						}	
				}

			}	
	}

END:

	if(ack=(lenght+1)){
		memset(buff,0,sizeof(buff));
		printf("All ACKs received\n");
	}
	else{
		memset(buff,0,sizeof(buff));
		printf("Not all ACKs received");
	}

}




void GBNreceive(char *filename){

	time_t t;				//variabile temporale
	int seqwin;				//finestra
	int fd;
	int i;
	int control;
	int len;
	int lastb;				//Valore dell'ultimo byte
	int prob;				//Probabilità perdita
	int ack;					//Valore dell'ultimo Ack comulativo ricevuto

	memset(buff,0,sizeof(buff));

	//	Ricevo la lunghezza del file da ricevere

	if(recvfrom(sid,buff,1024,0,(struct sockaddr*)&saddr, &lenght)== -1){
		perror("Error in reception of file lenght\n");
		exit(EXIT_FAILURE);
	}

	len=atoi(buff);

	memset(buff,0,sizeof(buff));

	//	Ricevo il numero dei messaggi da ricevere

	if((control=recvfrom(sid,buff,1024,0,(struct sockaddr*)&saddr,&lenght))== -1){
		perror("Error,reception of packets failed\n");
		exit(EXIT_FAILURE);
	}
	else if(control = 0){
		printf("Requested file not in the server\n");
	}
	else{

		// Creo il file in lettura e scrittura

		if((fd=open(filename,O_CREAT|O_RDWR,0666)) == -1){
			perror("Error in opening file\n");
			free(filename);
			exit(EXIT_FAILURE);
		}
		free(filename);

		nmess=atoi(buff);		//salvo in nmess il numero dei msg da ricevere

		msg message[nmess];	//Inizializzo delle strutture da creare

		memset(buff,0,sizeof(buff));




		while(i<nmess){

			//Ricevo inizialmente il contenuto del messaggio i

			if(recvfrom(sid,message[i].message_buff,1024,0,(struct sockaddr*)&saddr, &lenght) == -1){
				perror("Error in receiving message\n");
				exit(EXIT_FAILURE);
			}

			memset(buff,0,sizeof(buff));

			if(recvfrom(sid,buff,1024,0,(struct sockaddr*)&saddr, &lenght) == -1){
				perror("Error in receiving message last byte\n");
				exit(EXIT_FAILURE);
			}

			message[i].last_byte=atoi(buff); 	//Salvo l'ultimo valore del byte nella struttura

			lastb=(1024*(i+1));						//Valore dell'ultimo byte associato al msg i-esimo

			if(lastb < lenght){

				printf("last byte:%d\n", lastb);	//Caso in cui si ricevano msg precedenti all'ultimo

				// Controllo che il messaggio ricevuto sia corretto

				if(lastb == message[i].last_byte){

						printf("message %d received\n", i);

						//Calcolo la probabilità di perdita del pacchetto

						prob=(rand()%100);
						printf("Prob of receiving message is %d\n", prob);

						if(prob < 50){

							//In questo caso considero il messaggio non arrivato

							printf("Message %d lost\n",i);

							goto LostAck;

						}

						printf("Message %d arrived\n",i);

						//Scrivo il messaggio i dentro il file

						if(write(fd,message[i].message_buff,strlen(message[i].message_buff)) == -1){
							perror("Error in writing on file\n");
							exit(EXIT_FAILURE);
						}

						//Assegno il valore dell'ultimo byte alla variabile ack e lo invio

						ack=message[i].last_byte +1;

						printf("Ack da inviare al server:%d\n",ack);

						memset(buff,0,sizeof(buff));
						sprintf(buff,"%d",ack);

						if(sendto(sid,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
							perror("Error in sending ack\n");
							exit(EXIT_FAILURE);
						}

						memset(buff,0,sizeof(buff));
						i++;
						seqwin++;

				}else{

					/*	Nel caso in cui il pacchetto arrivato con successo non sia in ordine,
						in questo caso si scarta il pacchetto e si rimanda al server l'ack 
						corrispondente
					*/

					printf("Message %d received\n",i);
					printf("Ack discarded\n");

					// Invio dell'ack relativo all'ultimo messaggio corrispondente

					memset(buff,0,sizeof(buff));
					sprintf(buff,"%d",ack);

					if(sendto(sid,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
							perror("Error in sending ack\n");
							exit(EXIT_FAILURE);
					}

					memset(buff,0,sizeof(buff));
					printf("Ack da inviare al server:%d\n",ack);

				}

			}else{

					printf("Last packet %d arrived\n",i);

					/* In questo caso è arrivato l'ultimo messaggio,
						calcolo la probabilità di perdita
					*/

					prob=(rand()%100);
					printf("Prob %d\n",prob);

					if(prob<50){
						printf("Message %d lost\n",i);
						exit(EXIT_FAILURE);

						goto LostAck;
					}

					// Inserisco il messaggio i all'interno del file, come prima

					if(write(fd,message[i].message_buff,strlen(message[i].message_buff)) == -1){
							perror("Error in writing on file\n");
							exit(EXIT_FAILURE);
					}

					printf("Message %d arrived\n",i);

					ack=message[i].last_byte+1;

					printf("Sending Ack...\n");

					memset(buff,0,sizeof(buff));
					sprintf(buff,"%d",ack);

					if(sendto(sid,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
							perror("Error in sending ack\n");
							exit(EXIT_FAILURE);
					}

					printf("Ack sended\n");

					memset(buff,0,sizeof(buff));
					i++;
					seqwin++;

					break;

			}

LostAck:
					continue;     //Non si invia nulla

		}

		close(fd);

	}


}






void functionList(char *buff){

	// Invio al server la richiesta

	if(sendto(sid, buff, 1024, 0, (struct sockaddr*)&saddr,sizeof(saddr)) == -1){
		perror("Error, sending of 'List' command failed\n");
		exit(EXIT_FAILURE);
	}

	// Ricevo la risposta

	if(recvfrom(sid, buff, 1024, 0, NULL, NULL) == -1){
		perror("Error, reception result of 'List' command failed\n");
		exit(EXIT_FAILURE);
	}



	while(strlen(buff)==(1024-1)){
		if(recvfrom(sid,buff,1024,0,NULL,NULL)==-1){
			perror("Error, reception of 'List' command failed\n");
			exit(EXIT_FAILURE);
		}
	}

	// I due NULL nei parametri della funzione recvfrom() indicano che non sappiamo chi invia la risposta

	printf("Files:\n%s", buff);
	return;
}


void functionGet(char *buff){

	char *filename;

	filename=(char*)malloc(512);


	// Invio richiesta

	if(sendto(sid, buff, 1024, 0, (struct sockaddr*)&saddr,sizeof(saddr)) == -1){
		perror("Error, sending of 'Get' command failed\n");
		exit(EXIT_FAILURE);
	}

	memset(buff, 0, sizeof(buff));

	printf("Insert the name of the requested file:");
	scanf("%[^\n]s",buff);
	getchar();

	filename="IIW/client_udp/";
	if(strcat(filename,buff) == NULL){
		perror("Error in strcat function\n");
		exit(EXIT_FAILURE);
	}

	printf("Controllo: %s\n",filename);

	//Invio la richiesta per il file da richiedere

	if(sendto(sid,buff,1024,0,(struct sockaddr*)&saddr, sizeof(saddr)) == -1){
		perror("Error in Get request\n");
		exit(EXIT_FAILURE);
	}

	GBNreceive(filename);

   return;


}

void functionPut(char *buff){

	char *auxname;
	int control=0;
	int fd;

	//Invio la richiesta per il comando Put

	if(sendto(sid,buff,1024,0,(struct sockaddr*)&saddr, sizeof(saddr)) == -1){
		perror("Error, sending of 'Put' command failed\n");
		exit(EXIT_FAILURE);
	}

	memset(buff, 0, sizeof(buff));

	printf("Insert the name of the file to be placed on the server\n");
	scanf("%[^\n]",buff);
	getchar();

   /* Utilizzo la funzione opendir per aprire la directory del client e 
   	controllo i file presenti in essa
   */
 
 	auxname="IIW/client_udp/";
   
   if((directid=opendir(auxname)) == NULL){
   	perror("Error in opening the client directory\n");
   	exit(EXIT_FAILURE);
   }

   /*	Adesso utilizzerò l'API readdir con la manipolazione della struttura dirent
		per leggere la directory creata
	*/


   while((dir=readdir(directid)) != NULL){						//leggo la directory, se arrivo alla fine dello stream della directory avrò NULL
   		if(dir->d_type == DT_REG){								//controllo sul tipo del File, in questo caso un file regular
   			if(strcmp(dir->d_name,buff) == 0){				//controllo se il file richiesto è nella directory

   				control=1;

   				memset(buff,0,sizeof(buff));

   				//creo il percorso di directory sul lato server
            	strcpy(buff, "IIW/server_udp/");
            	strcat(buff, dir->d_name);
                
               //invio il nome del file da caricare sul server
            	if(sendto(sid, buff, 1024, 0, (struct sockaddr *)&saddr, sizeof(saddr)) == -1){
            		perror("Error, can't send the file name\n");
            		exit(EXIT_FAILURE);

            	}
            	
            	
               memset(buff, 0, sizeof(buff));

               //creo il percorso sul lato client dove prelevare il file
            	strcpy(buff, "IIW/client_udp/");
            	strcat(buff, dir->d_name);

            	//apertura del file
            	
            	if((fd=open(buff,O_RDONLY,0666)) == -1){
            		perror("Error in opening the file\n");
            		exit(EXIT_FAILURE);
            	}

                /*	calcolo lunghezza del file da inviare tramite l'utilizzo dell'API lseek,
							riposizionando alla fine il file pointer all'inizio per evitare problemi successivi
					 */	

                lenght = lseek(fd, 0, SEEK_END);
           
                lseek(fd, 0, SEEK_SET);

                /* Ora calcolo il numero di pacchetti da inviare utilizzando la funzione round()
                	 che mi permette di arrotondare un numero decimale al suo intero più vicino,
						 siccome nella comunicazione dovrò sempre arrotondare per eccesso sommerò 0.5
						 al valore da passare alla funzione round

                */ 	 
                nmess=round((lenght/(1024-1))+0.5) + 1;

                //inserimento nel buffer del numero di pacchetti da inviare
                memset(buff, 0, sizeof(buff));
                sprintf(buff, "%d", nmess);

                //invio il numero di pacchetti 
                if(sendto(sid, buff, 1024, 0, (struct sockaddr *)&saddr, sizeof(saddr)) == -1){
            		perror("Error, can't send the file name\n");
            		exit(EXIT_FAILURE);

            	}
    
                //svuotamento del buffer
                memset(buff, 0, sizeof(buff));

                msg message[nmess];							//inizializzo la struttura 'message'
                int lastBRead[nmess];						//questo array contiene il valore dell'ultimo byte di ogni messaggio inviato

               
                //pulizia dei campi di ogni struttura
                for(int i = 0; i < nmess; i++) {
                    memset(message[i].message_buff, 0, 1024);
                    message[i].last_byte = 0;
                }
            	
            	 GBNsend(message,nmess,fd,lastBRead,lenght);
                
   			}
   		}
   }


    close(fd);
    closedir(directid);

    if(control == 0) {
        printf("file '%s' is not in the client\n", buff);
	}

	return;

}






int main(int argc, char *argv[]){

    int insert=0;


	if(argc != 2){
		perror("Insert an IP address\n");
		exit(EXIT_FAILURE);
	}

	/* Creazione della socket per la connessione con il server,
	   successivamente inizializzo l'indirizzo IP ed il numero
	   di porta necessario alla comunicazione, il numero di porta
	   è stato scelto in modo arbitrario da me
	*/

	if((sid=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("Error in creating the socket\n");
		exit(EXIT_FAILURE);
	}

	memset((void*)&saddr, 0, sizeof(saddr));       
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(NPORT);

    /* Qui ho semplicemente svuotato inizialmente la struttura,
       poi ho assegnato la tipologia di indirizzo IPv4 a sin_family
       ed il numero di porta a sin_port
    */


    if(inet_aton(argv[1], &(saddr.sin_addr)) == 0){
    	perror("Error in conversion of IP address\n");
    	exit(EXIT_FAILURE);
    }

    /* Ho utilizzato inet_aton per convertire in notazione binaria
       l'indirizzo IP passato come argomento
    */

    /* Ora comunico al server l'esistenza di un nuovo client,
       per farlo utilizzo l'API sendto inviando un messsaggio
       di lunghezza nulla
    */

    if(sendto(sid, NULL, 0, 0, (struct sockaddr*)&saddr, sizeof(saddr)) < 0){
    	perror("Error in comunication with server\n");
    	exit(EXIT_FAILURE);
    }

    /* Adesso ricevo il nuovo numero di porta dal server utilizzando
       l'API recvfrom che salva il messaggio ricevuto in buff1, recvfrom
       è bloccante, quindi finchè non è disponibile un nuovo messaggio sulla
       socket da parte del server il flusso di esecuzione rimarrà bloccato
    */

     int len = sizeof(saddr);
     
     if(recvfrom(sid, buff, 1024, 0, (struct sockaddr*)&saddr, &len) < 0){
     	perror("Error in taking message from server\n");
     	fflush(stdout);
     	close(sid);
     	exit(EXIT_FAILURE);
     }

     newport=atoi(buff);

     /* Ora chiudo la connessione precedente, e riapro la socket per 
     	comunicare con il processo figlio del server
     */

     close(sid); //chiusura

     //riapertura

    if((sid=socket(AF_INET, SOCK_DGRAM, 0)) == -1){
		perror("Error in creating the socket\n");
		exit(EXIT_FAILURE);
	}

	memset((void*)&saddr, 0, sizeof(saddr));       
	saddr.sin_family=AF_INET;
	saddr.sin_port=htons(NPORT);

    if(inet_aton(argv[1], &(saddr.sin_addr)) == 0){
    	perror("Error in conversion of IP address\n");
    	exit(EXIT_FAILURE);
    }

    //fine riapertura


    /* Utilizzo una switch per dare la possibilità all'utente
       di inserire da tastiera dei comandi da eseguire per
       comunicare con il server, ho pensato fosse una soluzione
       più elegante rispetto ad un'alternativa che mi facesse
       inserire direttamente la stringa di comando da tastiera
    */


    while(1){

    	memset(buff, 0, sizeof(buff));
    	printf("\n-1: List\n-2: Get\n-3: Put\n-0: Quit\n");
    	scanf("%d",&insert);
    	switch(insert){
    		case 1:
    			sprintf(buff,"%s","List");
    			functionList(buff);
    			break;
    		case 2:
    			sprintf(buff,"%s","Get");
    			functionGet(buff);
    			break;
    		case 3:
    			sprintf(buff,"%s","Put");
    			functionPut(buff);
    			break;
    		case 0:
    			exit(0);
    			break;
    		default:
    			printf("Insert only the specified numbers!\n");
    			break;
    	}
    }

    return 0;

}