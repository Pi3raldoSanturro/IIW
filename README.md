# Trasferimento Affidabile di File su UDP con Protocollo Go-Back-N

## 1. Introduzione

Il progetto consiste nella realizzazione di un'applicazione **client-server in linguaggio C** per il trasferimento di file tramite rete, utilizzando l'API dei **Berkeley Socket**.

La comunicazione tra client e server avviene mediante socket di tipo:

```c
SOCK_DGRAM
```

quindi utilizzando **UDP** come protocollo di trasporto.

UDP è un protocollo semplice e leggero, ma non garantisce automaticamente:

- consegna dei pacchetti;
- ordinamento dei pacchetti;
- assenza di duplicati;
- controllo automatico degli errori tramite ritrasmissione.

Per questo motivo, il progetto implementa a livello applicativo un meccanismo di affidabilità ispirato al protocollo **Go-Back-N**, con ACK, timeout, finestra di trasmissione e ritrasmissione dei pacchetti non confermati.

L'obiettivo principale è mostrare come sia possibile costruire un trasferimento file affidabile anche utilizzando un protocollo non affidabile come UDP.

---

## 2. Obiettivi del progetto

Il software realizzato permette di:

- stabilire una comunicazione client-server senza autenticazione;
- visualizzare dal client la lista dei file disponibili sul server;
- scaricare file dal server tramite comando `GET`;
- caricare file sul server tramite comando `PUT`;
- chiudere la comunicazione tramite comando `CLOSE`;
- trasferire file in modo affidabile sopra UDP;
- simulare la perdita di pacchetti;
- gestire timeout statici e timeout adattivi;
- supportare più client concorrenti tramite processi figli generati con `fork()`.

---

## 3. Architettura generale

L'applicazione è composta da due programmi principali:

```text
client.c
server.c
```

Il server rimane in ascolto su una porta nota. Quando un client richiede una connessione, il server assegna una nuova porta dedicata alla comunicazione con quel client e crea un processo figlio tramite `fork()`.

In questo modo, il processo padre continua ad ascoltare nuove richieste di connessione, mentre il processo figlio si occupa della comunicazione con il client appena connesso. Questa scelta permette al server di gestire più client in modo concorrente.

---

## 4. Struttura delle directory

La directory `server_udp/` rappresenta lo spazio di archiviazione del server.

La directory `client_udp/` rappresenta lo spazio locale del client.

I file scaricati tramite `GET` vengono salvati nella directory del client, mentre i file caricati tramite `PUT` vengono salvati nella directory del server.

---

## 5. Protocollo applicativo

La comunicazione tra client e server è basata su un semplice protocollo applicativo costruito sopra UDP.

Il protocollo prevede due categorie principali di messaggi:

1. **Messaggi di comando**

   Sono inviati dal client al server per richiedere una determinata operazione.

   Esempi:

   ```text
   LIST
   GET
   PUT
   CLOSE
   ```

2. **Messaggi di risposta**

   Sono inviati dal server al client per comunicare l'esito dell'operazione richiesta o per trasferire informazioni, come la lista dei file disponibili o i dati di un file.

Durante il trasferimento dei file vengono inoltre utilizzati messaggi di controllo, come gli **ACK**, necessari per confermare la corretta ricezione dei pacchetti.

---

## 6. Comandi disponibili

### 6.1 Comando LIST

Il comando `LIST` permette al client di visualizzare l'elenco dei file disponibili sul server.

Il client invia al server una richiesta di tipo `LIST`. Il server accede alla propria directory di storage, legge i nomi dei file presenti e invia al client una risposta contenente la lista.

Flusso logico:

```text
Client -> Server: LIST
Server -> Client: lista dei file disponibili
```

Esempio:

```text
LIST
```

Il risultato viene mostrato direttamente sul terminale del client.

---

### 6.2 Comando GET

Il comando `GET` permette al client di scaricare un file dal server.

Il client invia al server:

1. il comando `GET`;
2. il nome del file richiesto;
3. la modalità di timeout da utilizzare.

Il server verifica se il file è presente nella propria directory. Se il file esiste, viene aperto, diviso in pacchetti e inviato al client tramite la funzione di invio affidabile basata su Go-Back-N.

Flusso logico:

```text
Client -> Server: GET
Client -> Server: nome_file
Server -> Client: dimensione file
Server -> Client: numero pacchetti
Server -> Client: pacchetti del file
Client -> Server: ACK cumulativi
```

Esempio:

```text
GET
file.txt
```

Il file ricevuto viene salvato nella directory locale del client.

---

### 6.3 Comando PUT

Il comando `PUT` permette al client di caricare un file sul server.

Il client invia al server:

1. il comando `PUT`;
2. il nome del file da caricare;
3. la modalità di timeout;
4. i pacchetti che compongono il file.

Il server riceve i pacchetti tramite la funzione di ricezione affidabile e ricostruisce il file nella propria directory di storage.

Flusso logico:

```text
Client -> Server: PUT
Client -> Server: nome_file
Client -> Server: dimensione file
Client -> Server: numero pacchetti
Client -> Server: pacchetti del file
Server -> Client: ACK cumulativi
```

Esempio:

```text
PUT
file.txt
```

---

### 6.4 Comando CLOSE

Il comando `CLOSE` permette di chiudere la comunicazione tra client e server.

Il client invia un messaggio di chiusura al server. Successivamente vengono chiuse le socket e rilasciate le risorse associate alla comunicazione.

Flusso logico:

```text
Client -> Server: CLOSE
chiusura socket client
chiusura socket server dedicata
```

---

## 7. Perché UDP richiede un meccanismo di affidabilità

UDP, a differenza di TCP, non offre garanzie sulla consegna dei dati.

Quando si invia un datagramma UDP, il mittente non sa automaticamente se:

- il pacchetto è arrivato;
- il pacchetto è stato perso;
- il pacchetto è arrivato duplicato;
- il pacchetto è arrivato fuori ordine;
- il destinatario è effettivamente pronto a ricevere.

Questa caratteristica rende UDP veloce e leggero, ma inadatto da solo a un trasferimento file affidabile.

Nel trasferimento di file, anche la perdita di un solo pacchetto può compromettere l'integrità del contenuto finale. Per questo motivo, il progetto implementa un protocollo di affidabilità a livello applicativo.

---

## 8. Protocollo Go-Back-N

Il protocollo **Go-Back-N** è una tecnica di trasmissione affidabile basata su finestra scorrevole.

L'idea principale è permettere al mittente di inviare più pacchetti consecutivi senza attendere l'ACK di ciascun pacchetto, migliorando così l'efficienza rispetto a un protocollo Stop-and-Wait.

Nel progetto, il file viene suddiviso in pacchetti. Ogni pacchetto viene associato a un numero progressivo, utilizzato per verificare l'ordine di ricezione e per inviare conferme cumulative.

---

## 9. Finestra di trasmissione

Nel Go-Back-N il mittente possiede una finestra di trasmissione di dimensione `N`.

La finestra indica il numero massimo di pacchetti che possono essere inviati senza aver ancora ricevuto conferma.

Esempio con finestra di dimensione 4:

```text
Pacchetti:   0   1   2   3   4   5   6   7
Finestra:   [0   1   2   3]
```

Il mittente può inviare i pacchetti 0, 1, 2 e 3. Quando riceve un ACK che conferma la ricezione del pacchetto 0, la finestra scorre in avanti:

```text
Pacchetti:   0   1   2   3   4   5   6   7
Finestra:       [1   2   3   4]
```

In questo modo può essere inviato anche il pacchetto 4.

---

## 10. ACK cumulativi

Il ricevente utilizza ACK cumulativi.

Un ACK cumulativo non conferma solo un singolo pacchetto, ma conferma tutti i dati ricevuti correttamente fino a un certo punto.

Ad esempio, se il ricevente invia:

```text
ACK 4096
```

significa che tutti i byte fino alla posizione precedente sono stati ricevuti correttamente e che il prossimo byte atteso è il byte 4096.

Questo meccanismo riduce il numero di messaggi di controllo necessari e consente al mittente di aggiornare la finestra di trasmissione.

---

## 11. Gestione dei pacchetti fuori ordine

Nel Go-Back-N classico, il ricevente accetta solo il pacchetto atteso.

Se riceve un pacchetto fuori ordine, lo scarta e invia nuovamente l'ACK relativo all'ultimo pacchetto ricevuto correttamente in sequenza.

Esempio:

```text
Pacchetto atteso: 3
Pacchetto ricevuto: 4
```

Il pacchetto 4 viene scartato, perché il pacchetto 3 non è ancora stato ricevuto. Il ricevente invia nuovamente l'ACK dell'ultimo dato corretto.

Questo comportamento permette al mittente di capire che uno o più pacchetti devono essere ritrasmessi.

---

## 12. Timeout e ritrasmissione

Per ogni trasmissione viene impostato un timeout.

Se il mittente non riceve un ACK entro il tempo previsto, considera perso uno o più pacchetti e procede con la ritrasmissione.

Nel Go-Back-N, quando si verifica una perdita, il mittente torna indietro al primo pacchetto non confermato e ritrasmette tutti i pacchetti successivi contenuti nella finestra.

Da qui deriva il nome:

```text
Go-Back-N
```

cioè “torna indietro di N pacchetti”.

---

## 13. Timeout statico

Nel timeout statico viene utilizzato un valore fisso per tutta la durata della comunicazione.

Questa soluzione è semplice da implementare, ma può essere poco efficiente:

- se il timeout è troppo breve, si rischiano ritrasmissioni inutili;
- se il timeout è troppo lungo, il sistema reagisce lentamente alla perdita di pacchetti.

Il timeout statico è quindi adatto a scenari semplici o a reti con ritardi prevedibili.

---

## 14. Timeout adattivo

Il timeout adattivo viene calcolato dinamicamente in base ai tempi di risposta osservati durante la comunicazione.

L'idea è stimare il **Round Trip Time**, cioè il tempo che intercorre tra l'invio di un pacchetto e la ricezione del relativo ACK.

A ogni ACK valido viene calcolato un valore di `SampleRTT`.

Successivamente viene aggiornata una media stimata:

```text
EstimatedRTT = (1 - α) * EstimatedRTT + α * SampleRTT
```

dove `α` è un coefficiente di peso.

Per tenere conto della variabilità dei ritardi, viene calcolato anche:

```text
DevRTT = (1 - β) * DevRTT + β * |SampleRTT - EstimatedRTT|
```

Il valore finale del timeout viene quindi stimato come:

```text
TimeoutInterval = EstimatedRTT + 4 * DevRTT
```

Questa tecnica permette al protocollo di adattarsi meglio alle condizioni della rete.

---

## 15. Ritrasmissione rapida

Oltre al timeout, il progetto prevede una logica di ritrasmissione rapida.

Se il mittente riceve più ACK duplicati, può dedurre che un pacchetto sia andato perso, anche prima dello scadere del timeout.

In particolare, dopo la ricezione di tre ACK duplicati, il mittente può ritrasmettere il pacchetto mancante senza attendere ulteriormente.

Questo meccanismo riduce i tempi di recupero in caso di perdita.

---

## 16. Simulazione della perdita di pacchetti

Per verificare il comportamento del protocollo in presenza di una rete non affidabile, il progetto permette di simulare la perdita dei pacchetti.

La perdita viene modellata tramite una probabilità configurabile nel codice.

Durante la ricezione, viene generato un valore casuale. Se tale valore rientra nella probabilità di perdita impostata, il pacchetto viene considerato perso e non viene inviato l'ACK corrispondente.

In questo modo è possibile osservare:

- scadenza dei timeout;
- ritrasmissione dei pacchetti;
- invio di ACK duplicati;
- funzionamento della ritrasmissione rapida;
- completamento del trasferimento nonostante le perdite simulate.

---

## 17. Funzione GBN_send

La funzione `GBN_send` gestisce l'invio affidabile dei pacchetti.

Il suo compito principale è:

1. leggere il file da inviare;
2. suddividerlo in pacchetti;
3. inviare più pacchetti entro la finestra di trasmissione;
4. attendere gli ACK dal destinatario;
5. aggiornare la finestra in base agli ACK ricevuti;
6. gestire timeout e ACK duplicati;
7. ritrasmettere i pacchetti non confermati.

Dal punto di vista logico, il comportamento può essere riassunto così:

```text
inizializza finestra
while esistono pacchetti non confermati:
    invia pacchetti finché la finestra non è piena
    attendi ACK
    if ACK valido:
        fai scorrere la finestra
    if timeout:
        ritrasmetti dal primo pacchetto non confermato
    if tre ACK duplicati:
        ritrasmetti rapidamente
```

Questa funzione viene utilizzata sia lato client sia lato server, a seconda dell'operazione richiesta.

Nel comando `GET`, il mittente è il server.

Nel comando `PUT`, il mittente è il client.

---

## 18. Funzione GBN_receive

La funzione `GBN_receive` gestisce la ricezione affidabile dei pacchetti.

Il suo compito principale è:

1. ricevere la dimensione del file;
2. ricevere il numero di pacchetti attesi;
3. creare il file di destinazione;
4. ricevere i pacchetti;
5. verificare che ogni pacchetto sia quello atteso;
6. scrivere i dati ricevuti nel file;
7. inviare ACK cumulativi;
8. gestire pacchetti persi o fuori ordine.

Dal punto di vista logico:

```text
ricevi informazioni sul file
crea file locale
while file non completato:
    ricevi pacchetto
    if pacchetto atteso:
        scrivi dati su file
        invia ACK aggiornato
    else:
        scarta pacchetto
        reinvia ultimo ACK valido
```

Nel comando `GET`, il ricevente è il client.

Nel comando `PUT`, il ricevente è il server.

---

## 19. Gestione della concorrenza

Il server è progettato per gestire più client.

La comunicazione iniziale avviene su una porta nota, usata come punto di ingresso.

Quando arriva una nuova richiesta, il server genera un processo figlio tramite:

```c
fork()
```

Il processo figlio gestisce la comunicazione con il client, mentre il processo padre torna in ascolto di nuove richieste.

Questo modello consente di separare la gestione dei diversi client e rende il server più flessibile.

---

## 20. Librerie utilizzate

Il progetto utilizza principalmente librerie standard del linguaggio C e dell'ambiente UNIX/Linux.

Tra le più importanti:

```c
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <time.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <signal.h>
```

Le librerie principali sono:

- `sys/socket.h`, per la gestione delle socket;
- `netinet/in.h`, per le strutture degli indirizzi Internet;
- `arpa/inet.h`, per la conversione degli indirizzi IP;
- `fcntl.h`, per la gestione dei file;
- `dirent.h`, per la lettura delle directory;
- `sys/time.h`, per la misura dei tempi e la gestione dei timeout;
- `signal.h`, per la gestione dei segnali di chiusura.

---

## 21. Compilazione

Il progetto può essere compilato con `gcc`.

```bash
gcc server.c -o server -lm
gcc client.c -o client -lm
```

L'opzione `-lm` collega la libreria matematica, utile nel caso in cui vengano utilizzate funzioni matematiche all'interno del codice.

---

## 22. Esecuzione

### Avvio del server

```bash
./server <indirizzo_ip_server>
```

Esempio in locale:

```bash
./server 127.0.0.1
```

### Avvio del client

```bash
./client <indirizzo_ip_server>
```

Esempio in locale:

```bash
./client 127.0.0.1
```

Una volta avviato, il client mostra un'interfaccia da terminale attraverso la quale l'utente può selezionare i comandi disponibili.

---

## 23. Esempio di utilizzo

### Visualizzazione dei file disponibili

```text
LIST
```

Il client mostra l'elenco dei file presenti nella directory del server.

### Download di un file

```text
GET
file.txt
```

Il client richiede il file `file.txt` al server e lo salva nella propria directory locale.

### Upload di un file

```text
PUT
file.txt
```

Il client invia il file `file.txt` al server, che lo salva nella propria directory.

### Chiusura della connessione

```text
CLOSE
```

La comunicazione viene terminata e le risorse vengono rilasciate.

---

## 24. Considerazioni finali

Il progetto mostra come sia possibile implementare un sistema di trasferimento file affidabile utilizzando UDP.

Poiché UDP non fornisce meccanismi di affidabilità, il progetto introduce a livello applicativo funzionalità tipiche dei protocolli di trasporto affidabili, tra cui:

- numerazione dei pacchetti;
- ACK cumulativi;
- finestra di trasmissione;
- timeout;
- ritrasmissione;
- ritrasmissione rapida;
- gestione dei pacchetti persi;
- gestione dei pacchetti fuori ordine.

L'applicazione permette quindi di comprendere in modo pratico il funzionamento dei protocolli affidabili e il ruolo svolto da meccanismi come Go-Back-N nella trasmissione dati su reti non affidabili.
