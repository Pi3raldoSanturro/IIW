#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/errno.h>
#include <fcntl.h>			
#include <sys/stat.h>
#include <math.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/errno.h>
#include <sys/wait.h>
#include <setjmp.h>			
#include <sys/socket.h>		
#include <netinet/in.h>		
#include <arpa/inet.h>		
#include <sys/types.h>
#include <netdb.h>
#include <dirent.h>			
#include <libgen.h>			




#define NPORT 1340			//numero di porta scelto arbitrariamente
#define N 7					//dimensione finestra di ricezione
#define TIMEOUT 500			//dopo quanto arriva SIGALARM


int sid;					//descrittore socket
int ind;					//Indice invio dei pacchetti
struct sockaddr_in saddr;	//struttura per gli indirizzi IPv4
char buff[1024];			//Buffer generico
int file_lenght,len;
int port_num=1341;			//Inizializzato al primo numero di porta disponibile
pid_t pid;
int child_socket;			//socket processo figlio
DIR *directory;				//descrittore directory
struct dirent *dir;			//struttura per la gestione dei file del server
int Ack,ack_counter;


struct sigaction sigalarm;	//Stutture per segnale SIGALARM e gestione
struct itimerval timer;		//del timeout


typedef struct strutt_messaggio{
	int lastbyte;
	char message_buffer[1024];
}message;


static jmp_buf alarm_return;

void alarm_handler(int signum){

	printf("Retransimssion of packets from %d\n", ack_counter);

	getitimer(ITIMER_REAL,&timer);

	longjmp(alarm_return,1);
}


void GBNsend(message *msg,int num_msg,int fd,int file_lenght){

	int window=0;
	int lastAck=0;
	int i,j;
	int aux;

	memset(&sigalarm,0,sizeof(sigalarm));
	sigalarm.sa_handler=&alarm_handler;

	//Gestisco le variabili della struttura itimerval
    timer.it_interval.tv_sec = 0;
    timer.it_interval.tv_usec = TIMEOUT;
    timer.it_value.tv_sec = 0;
    timer.it_value.tv_usec = TIMEOUT;

    /*	ora inserisco le informazioni del file nella sezione
    	message_buffer e il valore dell'ultimo byte in last_byte
    */

    for(i=0;i<num_msg;i++){

    	if((aux=read(fd,msg[i].message_buffer,1024))==-1){
    		perror("Error in reading from file\n");
    		exit(EXIT_FAILURE);
    	}
    	else{

    		if(i==0){

    			//Primo messaggio

    			msg[i].lastbyte=aux;

    		}
    		else{

    			//Messaggi dopo il primo

    			msg[i].lastbyte=msg[i-1].lastbyte+aux;
    		}
    	}
    }

    Ack=0;

    sigaction(SIGALRM,&sigalarm,NULL);

    while(1){
SEND:
		
		if(setjmp(alarm_return)==-1){

			//ritorno dell'handler del timer

			ind=ack_counter;
			window=0;

			setitimer(ITIMER_REAL,&timer,NULL);
			getitimer(ITIMER_REAL,&timer);

		}

		printf("Index:%d\n",ind);

		if(ind=num_msg){

			/*	caso in cui tutti i pacchetti siano stati inviati,
				andremo in WAIT se non tutti gli ack saranno arrivati
				e andremo in END se gli ack saranno arrivati
			*/

			if(Ack < file_lenght){

				goto WAIT;
			}
			else{

				goto END;
			}
		}

		else{

			/*	Caso in cui non tutti i pacchetti siano stati
				inviati
			*/

			if(window < N){

				printf("Sending packet %d\n",ind);

				if(sendto(child_socket,msg[ind].message_buffer,(strlen(msg[ind].message_buffer)+1),0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
					perror("Error in sending message to client\n");
					exit(EXIT_FAILURE);
				}
				else{

					memset(buff,0,sizeof(buff));
					sprintf(buff,"%d",msg[ind].lastbyte);

					if(sendto(child_socket,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
						perror("Error, sending of last byte failed\n");
						exit(EXIT_FAILURE);
					}
					else{

						window++;
						ind++;

						setitimer(ITIMER_REAL,&timer,NULL);
						getitimer(ITIMER_REAL,&timer);

						goto SEND;
					}

				}

			}

			else{
WAIT:			
				printf("Wait for Ack\n");
				memset(buff,0,sizeof(buff));

				//attesa dell'ack per decrementare la finestra di ricezione

				if(recvfrom(child_socket,buff,1024,0,(struct sockaddr*)&saddr,&len) ==-1){
					printf("Error, can't receive ack\n");
					exit(EXIT_FAILURE);
				}
				else{

					Ack=atoi(buff);

					printf("Ack %d arrived\n",Ack);
					printf("Last Ack received: %d\n",lastAck);

					if(Ack > lastAck){

						lastAck=Ack;

						getitimer(ITIMER_REAL,&timer);


						window--;
						ack_counter++;

						goto SEND;
					}
					else if(Ack == lastAck){

						printf("Ack %d just arrived!\n",Ack);

						getitimer(ITIMER_REAL,&timer);

					}
				}	

			}
		}

    }

END:

	// Controllo se tutti gli Ack siano arrivati

	for(j=0;j<num_msg;j++){
		if(Ack == (msg[j].lastbyte+1)){
			 lastAck = Ack;
			 ind = j;
		}
	}

	if(lastAck=(file_lenght+1)){

		printf("Last Ack arrived!\n");
		alarm(0);
	}
	else{

		printf("Ack of message %d arrived\n",ind);
		printf("Last Ack arrived is: %d\n",lastAck);

		alarm(0);

		goto SEND;
	}    


}




void GBNreceive(int fd){



	int window=0;
	int num_msg;
	int i;

	//Ricevo dal client il numero dei messaggi in attesa

	if(recvfrom(child_socket,buff,1024,0,(struct sockaddr*)&saddr,&len)==-1){
		perror("Error,can't receive the number of packets\n");
		exit(EXIT_FAILURE);
	}

	//salvo in num_msg ciò che ricevo dal client, cioè il numero dei messaggi

	num_msg=atoi(buff);

	message msg[num_msg];
	Ack=0;

	memset(buff,0,sizeof(buff));

	for(i=0;i<num_msg;i++){
		memset(msg[i].message_buffer,0,1024);
		msg[i].lastbyte=0;
	}

	//Ricezione messaggi dal client

	for(i=0;i<num_msg;i++){

		if(recvfrom(child_socket,msg[i].message_buffer,1024,0,(struct sockaddr*)&saddr,&len)==-1){
			perror("Error, can't receive the response message\n");
			exit(EXIT_FAILURE);
		}
		else{

			//Messaggio i-esimo nel file

			if(write(fd,msg[i].message_buffer,strlen(msg[i].message_buffer))==-1){
				perror("Error,writing failed\n");
				exit(EXIT_FAILURE);
			}

			memset(buff,0,sizeof(buff));

			//ricezione ultimo byte

			if(recvfrom(child_socket,buff,1024,0,(struct sockaddr*)&saddr,&len)==-1){
				perror("Error, can't receive the last byte\n");
				exit(EXIT_FAILURE);
			}

			else{

				msg[i].lastbyte=atoi(buff);

				memset(buff,0,sizeof(buff));

				//Assegno il valore dell'ack
				Ack=msg[i].lastbyte+1;

				sprintf(buff,"%d",Ack);

				if(sendto(child_socket,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
					perror("Error,can't send the Ack\n");
					exit(EXIT_FAILURE);
				}
				
				memset(buff,0,sizeof(buff));
			}
		}
	}

	close(fd);


	printf("\n");


}




void function_list(){

	memset(buff,0,sizeof(buff));


	/*	Ora la directory verrà aperta e verrà
		effettuata la lettura
	*/

	if((directory=opendir("server_udp"))==NULL){
		perror("Error,reading of files failed\n");
		exit(EXIT_FAILURE);
	}



	while((dir=readdir(directory)) != NULL){
		if(dir->d_type == DT_REG){

			//Inserisco l'i-esimo file nel buffer
			strcat(buff,dir->d_name);
			strcat(buff,"\n");

			if(strlen(buff) >= (1024-1)){

				//Caso del buffer pieno

				if(sendto(child_socket,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr)) ==-1){
					perror("Error, sending response failed\n");
					exit(EXIT_FAILURE);
				}
				memset(buff,0,sizeof(buff));

				
			}
		}
	}

	closedir(directory);

	if(strlen(buff)>0){

		if(sendto(child_socket,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr)) == -1){
			perror("Error, sending response failed\n");
			exit(EXIT_FAILURE);
		}
	}
	
}

void function_put(){
	int fd;

	if((fd=open(buff,0,O_CREAT|O_RDWR|O_TRUNC,0660))==-1){
		perror("Error in creating file\n");
		exit(EXIT_FAILURE);
	}

	memset(buff,0,sizeof(buff));

	GBNreceive(fd);

}

void function_get(){

	int fd;
	int control=0; 		//ci indica se il file è presente o no nel server
	int nmess;				
	char *filename;
	int i;

	//Alloco risorse per la variabile filename

	filename=(char*)malloc(124);
	if(filename==NULL){
		perror("Error, allocation failed\n");
		exit(EXIT_FAILURE);
	}

	//Apro la directory

	if((directory=opendir("server_udp")) == NULL){
		perror("Error in opening the directory\n");
		exit(EXIT_FAILURE);
	}

	while((dir=readdir(directory)) != NULL){
		if(strcmp(dir->d_name,buff)==0){

			control=1;	//Il file è nel server


			//Stringa che contiene il percorso del file richiesto dal client
			strcpy(filename,"server_udp/");
			strcat(filename,dir->d_name);

			if((fd=open(filename,O_RDONLY,0666)) ==-1){
				perror("Error in opening the file\n");
				exit(EXIT_FAILURE);
			}
			free(filename);


			/*	Calcolo la lughezza del file da inviare e inserisco
				nel buffer la lunghezza del file da inviare
			*/

			file_lenght=lseek(fd,0,SEEK_END);

			memset(buff,0,sizeof(buff));
			sprintf(buff,"%d",file_lenght);

			if(sendto(child_socket,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr)) == -1){
				perror("Error, sending failed\n");
				exit(-1);
			}

			lseek(fd,0,SEEK_SET);

			//Calcolo del numero dei messaggi da inviare

			nmess=round((file_lenght/(1024-1))+0.5)+1;

			memset(buff,0,sizeof(buff));
			sprintf(buff,"%d",nmess);

			if(sendto(child_socket,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
				printf("Error,sending of number of packets failed\n");
				exit(EXIT_FAILURE);
			}

			memset(buff,0,sizeof(buff));

			/*	Allocazione dei campi per la struttura messaggio, inizializzo
				prima nmess strutture messaggio e le pulisco
			*/

			message msg[nmess];

			for(i=0;i<nmess;i++){
				memset(msg[i].message_buffer,0,1024);
				msg[i].lastbyte=0;
			}

			GBNsend(msg,nmess,fd,file_lenght);

			close(fd);
			closedir(directory);


		}
	}

	if(control=0){

		// caso in cui il file non è nel server

		if(sendto(child_socket,NULL,0,0,(struct sockaddr*)&saddr,sizeof(saddr))==-1){
			perror("Error in sending response\n");
			exit(EXIT_FAILURE);
		}
	}

}




int main(int argc, char *argv[]){

	/*	Creo la socket di ascolto per il processo server,
		verrà usata solo per l'ascolto delle richieste.
		Inizializzo poi l'indirizzo IP ed il numero di porta
		necessario alla comunicazione.
	*/

	if((sid=socket(AF_INET,SOCK_DGRAM,0))==-1){
		perror("Error in creating the socket\n");
		exit(EXIT_FAILURE);
	}

	memset((void*)&saddr,0,sizeof(saddr));
	saddr.sin_family=AF_INET;
	saddr.sin_addr.s_addr=htonl(INADDR_ANY);
	saddr.sin_port=htons(NPORT);

	/*	Qui ho svuotato la struttura e poi ho assegnato un
		indirizzo IPv4 a sin_family ed il numero di porta a
		sin_port
	*/

	if(bind(sid,(struct sockaddr*)&saddr,sizeof(saddr)) == -1){
		perror("Error\n");
		exit(EXIT_FAILURE);
	}

	/*	La funzione bind() assegna l'indirizzo specificato da saddr
		alla socket riferita da sid. L'ultimo parametro è la grandezza 
		della struttura

	*/

	printf("Waiting for request...\n");

	while(1){

		memset(buff,0,sizeof(buff));
		len=sizeof(saddr);

		//Ricezione dei processi client

		if(recvfrom(sid,buff,1024,0,(struct sockaddr*)&saddr,&len) == -1){
			perror("Error, request failed\n");
			exit(EXIT_FAILURE);
		}

		memset(buff,0,sizeof(buff));

		//Inserisco il numero di porta nel buffer per inviarlo al client e poi invio

		sprintf(buff,"%d",port_num);

		if(sendto(sid,buff,1024,0,(struct sockaddr*)&saddr,sizeof(saddr)) == -1){
			perror("Error, sending reply failed\n");
			exit(EXIT_FAILURE);
		}

		pid=fork();
		if(pid==-1){
			perror("Error in creating child\n");
			exit(EXIT_FAILURE);
		}

		// Il processo figlio viene creato per comunicare con il client

		if(pid==0){

			/*	Siamo nel processo figlio, viene chiusa la socket del processo padre
				e viene creata la socket per il processo figlio
			*/

			if((child_socket=socket(AF_INET,SOCK_DGRAM,0))==-1){
				perror("Error in creating the child socket\n");
				exit(EXIT_FAILURE);
			}

			memset((void*)&saddr,0,sizeof(saddr));
			saddr.sin_family=AF_INET;
			saddr.sin_addr.s_addr=htonl(INADDR_ANY);
			saddr.sin_port=htons(port_num);


			if(bind(child_socket,(struct sockaddr*)&saddr,sizeof(saddr)) == -1){
				perror("Error in binding child socket\n");
				exit(EXIT_FAILURE);
			}

			close(sid);


			while(1){

				memset(buff,0,sizeof(buff));

				//Ricezione del comando da far eseguire al server

				if(recvfrom(child_socket,buff,1024,0,(struct sockaddr*)&saddr,&len)==-1){
					perror("Error in receiving request\n");
					exit(EXIT_FAILURE);
				}

				/*	Da qui comincia il test sulla variabile buffer per vedere
					che tipo di comando sia arrivato al server
				*/

				if(strcmp("List",buff)==0){
					printf("List of file command\n");
					function_list();
				}

				if(strcmp("Get",buff)==0){
					memset(buff,0,sizeof(buff));

					if(recvfrom(child_socket,buff,1024,0,(struct sockaddr*)&saddr,&len)==-1){
						perror("Error in receiving the name of the file\n");
						exit(-1);
					}

					printf("Get file command\n");
					function_get();
				}

				if(strcmp("Put",buff)==0){
					memset(buff,0,sizeof(buff));

					if(recvfrom(child_socket,buff,1024,0,(struct sockaddr*)&saddr,&len)==-1){
						perror("Error in request Put\n");
						exit(EXIT_FAILURE);
					}

					printf("Put file command\n");					
					function_put();
				}


			}			

			

		}

		else{

			//Siamo nel processo padre, incrementiamo il valore della variabile di porta

			if(port_num<65535){

				port_num++;

			}else{
				//Caso in cui port_num assume il valore massimo consentito
				port_num=1341;
			}


		}

		//

	}

	return 0;

}


