/* servTCPConcTh2.c - Exemplu de server TCP concurent care deserveste clientii
   prin crearea unui thread pentru fiecare client.
   Asteapta un numar de la clienti si intoarce clientilor numarul incrementat.
	Intoarce corect identificatorul din program al thread-ului.
  
   
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009
*/
//<<------
#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <errno.h>

#include <unistd.h>

#include <stdio.h>

#include <string.h>

#include <stdlib.h>

#include <signal.h>

#include <pthread.h>
//>>-------

#include <ctime>

#include <vector>

//<<-------

/* portul folosit */
#define PORT 2908

/* codul de eroare returnat de anumite apeluri */
extern int errno;

typedef struct thData {
    int idThread; //id-ul thread-ului tinut in evidenta de acest program
    int cl; //descriptorul intors de accept
}
thData;

//>>--------

int initSocket();
int waitForListeners();
static void* treatFoodRequest(void*); /* functia executata de fiecare thread ce realizeaza comunicarea cu clientii */
void addFoodOrder(void* , int);
void sendFoodResponseToClient(void * );
void readFoodRequest(void* arg, int& response);
void * handleConnections(void* arg);
void * handleTaskPhases(void* arg);
void readClientStatus(void* arg, int& responseSize, char*& responseText);

enum TaskPhase {
    eWaitingForFoodRequests,
    eProcessFoodRequests
}
currentPhase;

time_t lastFoodRequestTime;
float T = 60.f;
pthread_mutex_t currentPhaseLock;

//<<-------
struct sockaddr_in server; // structura folosita de server
struct sockaddr_in from;
int sd; //descriptorul de socket 
pthread_t th[100]; //Identificatorii thread-urilor care se vor crea
int threadId = 0; //identificator unic pentru fiecare thread in parte. Cand se creaza un thread, acest numar va creste
//>>-------

pthread_mutex_t clientsPreferencesLock;
std::vector < std::pair < int, int >> clientsPreferences; //key -> threadID, value -> ClientPreference
int currentClientPreferencesProcessed = 0;

int main() 
{
    initSocket();

    waitForListeners();

    //marcam ca asteptam comenzi de procesat de la clienti
    currentPhase = TaskPhase::eWaitingForFoodRequests;
    time(&lastFoodRequestTime);

    //initializam lacatele folosite pentru sincronizarea datelor
    if (pthread_mutex_init(&currentPhaseLock, NULL) != 0 || pthread_mutex_init(&clientsPreferencesLock, NULL) != 0)
        return 1;

    //cream 2 threaduri dintre care:
    //primul se ocupa de handle-uit conexiunile cu clientii
    //al doilea se ocupa strict de schimbarea fazelor (primire de comenzi / procesarea comenzilor)

    /*
    Folosim 2 threaduri din cauza ca la primirea comenzilor ne aflam intr-o faza blocanta din cauza la connect()
    unde nu putem sa determinam cand trece un anumit numar de secunde incat sa putem face si schimbarea fazelor
    */

    pthread_t thread1;
    pthread_t thread2;
    pthread_create(&thread1, NULL, &handleConnections, NULL);
    pthread_create(&thread2, NULL, &handleTaskPhases, NULL);

    pthread_join(thread1, NULL);
    pthread_join(thread2, NULL);

    exit(0);
}

int initSocket()
{
    //<<-------
    /* cream un socket */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("[server]Eroare la socket().\n");
        return errno;
    }

    /* utilizam optiunea SO_REUSEADDR */
    int on = 1;
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));

    /* pregatim structurile de date */
    bzero(&server, sizeof(server));
    bzero(&from, sizeof(from));

    /* umplem structura folosita de server */

    server.sin_family = AF_INET; /* stabilirea familiei de socket-uri */

    server.sin_addr.s_addr = htonl(INADDR_ANY); /* mentionam ca acceptam orice adresa */

    server.sin_port = htons(PORT); /* utilizam un port utilizator */

    /* atasam socketul */
    if (bind(sd, (struct sockaddr*) &server, sizeof(struct sockaddr)) == -1)
    {
        perror("[server]Eroare la bind().\n");
        return errno;
    }

    printf("Socketul a fost initializat cu succes\n");

    //>>-------
    return 0;
}

int waitForListeners()
{
    //<<-------
    /* punem serverul sa asculte daca vin clienti sa se conecteze */
    if (listen(sd, 10) == -1)
    {
        perror("[server]Eroare la listen().\n");
        return errno;
    }

    printf("Serverul asculta daca vin clienti sa se conecteze\n");
    
    //>>-------
    return 0;
}

void * handleConnections(void * arg)
{
    while (true)
    {
        pthread_mutex_lock(&currentPhaseLock);

        if (currentPhase == TaskPhase::eWaitingForFoodRequests)
        {
            //<<-------
            int client;
            thData * td; //parametru functia executata de thread     
            unsigned int length = sizeof(from);

            printf("[server]Asteptam la portul %d...\n", PORT);
            fflush(stdout);

            //>>-------

            pthread_mutex_unlock(&currentPhaseLock);

            //<<-------
            /* acceptam un client (stare blocanta pana la realizarea conexiunii) */
            if ((client = accept(sd, (struct sockaddr * ) &from, &length)) < 0)
            {
                perror("[server]Eroare la accept().\n");
                continue;
            }

            //pregatim structura pentru crearea unui thread
            td = (struct thData * ) malloc(sizeof(struct thData)); /* alocam memorie*/
            td -> idThread = threadId; //setam id-ul threadului
            td -> cl = client; // setam descriptorul intors de accept

            //cream un thread ce va face handling la mesajele client-server
            pthread_create(&th[threadId], NULL, &treatFoodRequest, td);
            
            //>>-------

            //incrementam threadId pentru urmatorul request
            threadId++;
        }
        else
        {
            pthread_mutex_unlock(&currentPhaseLock);
        }
    }

    pthread_exit(NULL);
}

void * handleTaskPhases(void * arg)
{
    while (true)
    {
        pthread_mutex_lock(&currentPhaseLock);
        time_t currentTime;
        time(&currentTime);

        if (currentPhase == TaskPhase::eWaitingForFoodRequests)
        {
            //suntem in faza de primire comenzi

            //verificam daca au trecut T secunde de la ultima procesare de comenzi
            if (difftime(currentTime, lastFoodRequestTime) >= T)
            {
                printf("Schimbare faza din eWaitingForFoodRequests in eProcessFoodRequests:\n");
                currentPhase = TaskPhase::eProcessFoodRequests;
                lastFoodRequestTime = currentTime;
            }
        }

        if (currentPhase == TaskPhase::eProcessFoodRequests)
        {
            //suntem in faza de procesare comenzi
            pthread_mutex_lock(&clientsPreferencesLock);

            //verificam daca s-au procesat toate requesturile deja primite
            if (currentClientPreferencesProcessed == clientsPreferences.size())
            {
                printf("S-au procesat toti clientii. Revenim in faza de primire comenzi!\n");

                //Am terminat de procesat toti clientii.
                //resetam lista de preferinte, numarul de clienti de procesat si schimbam faza in cea de primire de comenzi
                clientsPreferences.clear();
                currentClientPreferencesProcessed = 0;
                lastFoodRequestTime = currentTime;
                currentPhase = TaskPhase::eWaitingForFoodRequests;
            }

            pthread_mutex_unlock(&clientsPreferencesLock);
        }

        pthread_mutex_unlock(&currentPhaseLock);
        sleep(1);
    }

    pthread_exit(NULL);
}

static void * treatFoodRequest(void * arg)
{
    //<<-------
    struct thData tdL;
    tdL = * ((struct thData * ) arg);
    fflush(stdout);
    pthread_detach(pthread_self());

    //>>-------

    // daca un client se conecteaza cat suntem in faza de procesare a clientilor deja existenti,
    // asteptam pana aceasta faza se schimba in faza de primire a comenzilor
    pthread_mutex_lock(&currentPhaseLock);

    while (currentPhase == TaskPhase::eProcessFoodRequests)
    {
        pthread_mutex_unlock(&currentPhaseLock);
        sleep(1);
        pthread_mutex_lock(&currentPhaseLock);
    }

    pthread_mutex_unlock(&currentPhaseLock);

    printf("Incepem tratarea unei comenzi pe threadul %d \n", tdL.idThread);

    while (true)
    {
        int response = 0;
        readFoodRequest((struct thData * ) arg, response);

        if (response == -1)
            return (NULL);

        pthread_mutex_lock(&clientsPreferencesLock);

        //adaugam preferinta in lista de preferinte pentru a putea fi procesata cum se schimba faza curenta
        clientsPreferences.push_back(std::pair < int, int > (tdL.idThread, response));

        pthread_mutex_unlock(&clientsPreferencesLock);

        //acum asteptam pana serverul ajunge in faza de procesare a comenzilor

        pthread_mutex_lock(&currentPhaseLock);
        while (currentPhase != TaskPhase::eProcessFoodRequests)
        {
            pthread_mutex_unlock(&currentPhaseLock);
            //wait until server phase is set to process food requests
            sleep(1);
            pthread_mutex_lock(&currentPhaseLock);
        }

        pthread_mutex_unlock(&currentPhaseLock);

        //trimitem catre client un raspuns in functie de preferintele tuturor
        sendFoodResponseToClient((struct thData * ) arg);

        //acum ne asteptam de la client sa primim un raspuns structurat in 2 mesaje.
        //primul mesaj reprezinta dimensiunea celui de-al doilea
        //al doilea reprezinta un sir de caractere ce ne spune starea clientului respectiv

        int clientResponseStatusLength = 0;
        char * clientResponseStatus = nullptr;

        //citim dimensiunea mesajului
        if (read(tdL.cl, &clientResponseStatusLength, sizeof(int)) <= 0)
        {
            printf("[Thread %d]\n", tdL.idThread);
            perror("Eroare la read() dimensiune mesaj de dupa raspuns de la client.\n");
        }
        else
        {
            printf("[Thread %d] Am citit dimensiune raspuns de la client: %d\n", tdL.idThread, clientResponseStatusLength);
        }

        //alocam memorie
        clientResponseStatus = new char[clientResponseStatusLength];

        //citim sirul de caractere
        if (read(tdL.cl, clientResponseStatus, sizeof(char) * clientResponseStatusLength) <= 0)
        {
            printf("[Thread %d]\n", tdL.idThread);
            perror("Eroare la read() mesaj de dupa raspuns de la client.\n");
        }
        else
        {
            printf("[Thread %d] Am citit raspuns de la client: %s\n", tdL.idThread, clientResponseStatus);
        }

        bool endConnection = false;
        if (strcmp(clientResponseStatus, "Satul!") == 0 || strcmp(clientResponseStatus, "Schimb cantina! Aici mor de foame!") == 0)
        {
            //daca clientul ne spune ca este satul sau ca s-a saturat de asteptat, vrem sa oprim conexiunea cu acesta
            endConnection = true;
        }

        if (strcmp(clientResponseStatus, "Mai astept...") == 0)
        {
            //daca ne notifica ca acesta mai asteapta, nu inchidem conexiunea
            endConnection = false;
        }

        //dealocam memoria
        delete[] clientResponseStatus;

        if (endConnection)
        {
            printf("End connection\n");
            //incheiem conexiunea cu acest client
            close((intptr_t) arg);
            break; //si iesim din while(true), pentru a incheia executia threadului
        }
    }

    return (NULL);
};

void readFoodRequest(void * arg, int& responseValue)
{
    struct thData tdL;
    tdL = * ((struct thData * ) arg);

    printf("[thread %d] - Asteptam mesajul...\n", tdL.idThread);

    responseValue = -1;

    //citim preferinta clientului
    if (read(tdL.cl, &responseValue, sizeof(int)) <= 0)
    {
        printf("[Thread %d]\n", tdL.idThread);
        perror("Eroare la read() de la client.\n");
    }
    else
    {
        printf("[thread %d] - Comanda a fost receptionata... Preferinta clientului: %d\n", tdL.idThread, responseValue);
    }
}

void sendFoodResponseToClient(void * arg)
{
    //<<-------
    int nr, i = 0;
    struct thData tdL;
    tdL = * ((struct thData * ) arg);
    //>>-------

    pthread_mutex_lock(&clientsPreferencesLock);

    int currentClientIndex = -1;

    //determinam cea mai aleasa preferinta dintre clienti
    int foodPreferences[] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    int mostChoosedPreferenceNumber = -1;
    int mostChoosedPreferenceCount = 0;

    for (int i = 0; i < clientsPreferences.size(); i++)
    {
        //crestem frecventa preferintei clientsPreferences[i].second cu 1
        foodPreferences[clientsPreferences[i].second] += 1;

        //ne asiguram ca stocam maximul ei
        if (foodPreferences[clientsPreferences[i].second] > mostChoosedPreferenceCount)
        {
            mostChoosedPreferenceCount = foodPreferences[clientsPreferences[i].second];
            mostChoosedPreferenceNumber = clientsPreferences[i].second;
        }

        //determinam indexul threadului curent din array-ul de preferinte
        if (clientsPreferences[i].first == tdL.idThread)
        {
            currentClientIndex = i;
        }
    }

    //consideram ca am procesat acest client (variabila ajutatoare pentru threadul ce schimba fazele aplicatiei)
    currentClientPreferencesProcessed++;

    const char * messageToSend = nullptr;

    if (mostChoosedPreferenceNumber == clientsPreferences[currentClientIndex].second)
    {
        //daca preferinta clientului este una dintre cele mai dorite si de catre ceilalti clienti, trimitem clientului ca mancarea este servita
        messageToSend = "Masa e servita!";
    }
    else
    {
        //altfel il notificam ca nu este disponibila preferinta acestuia
        messageToSend = "Indisponibil";
    }

    int messageLength = strlen(messageToSend);

    //trimitem un raspuns clientului format din 2 mesaje
    //primul reprezinta dimensiunea celui de-al doilea mesaj.
    //al doilea reprezinta un sir de caractere de dimensiunea primului mesaj

    //trimitem dimensiunea celui de-al doilea mesaj
    if (write(tdL.cl, &messageLength, sizeof(int)) <= 0)
    {
        printf("[Thread %d] ", tdL.idThread);
        perror("[Thread]Eroare la write de dimensiune mesaj catre client.\n");
    }
    else
    {
        printf("[Thread %d]Dimensiunea a fost trasmisa cu succes.\n", tdL.idThread);
    }

    //trimitem un sir de caractere
    if (write(tdL.cl, messageToSend, sizeof(char) * messageLength) <= 0)
    {
        printf("[Thread %d] ", tdL.idThread);
        perror("[Thread]Eroare la write mesaj catre client.\n");
    }
    else
    {
        printf("[Thread %d]mesajul \'%s\' a fost trasmisa cu succes.\n", tdL.idThread, messageToSend);
    }

    pthread_mutex_unlock(&clientsPreferencesLock);
}