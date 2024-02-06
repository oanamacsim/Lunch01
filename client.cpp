/* cliTCPIt.c - Exemplu de client TCP
   Trimite un numar la server; primeste de la server numarul incrementat.
         
   Autor: Lenuta Alboaie  <adria@infoiasi.ro> (c)2009
*/
//<<------
#include <sys/types.h>

#include <sys/socket.h>

#include <netinet/in.h>

#include <arpa/inet.h>

#include <errno.h>

#include <unistd.h>

#include <stdio.h>

#include <stdlib.h>

#include <netdb.h>

//>>------

#include <ctime>

#include <string.h>

/*[1-5]*/
#define FOOD_PREFERENCES_INTERVAL 5

//<<------
/* codul de eroare returnat de anumite apeluri */
extern int errno;

/* portul de conectare la server*/
int port;

struct sockaddr_in server;
int sd; // descriptorul de socket

//>>------

int main(int argc, char * argv[])
{   
    //initializare seed in functie de timpul curent
    srand(time(NULL));

    //<<------
    struct sockaddr_in server; // structura folosita pentru conectare 

    /* verificam nr de argumente primite de la linia de comanda? */
    if (argc != 3) 
    {
        printf("Sintaxa: %s <adresa_server> <port>\n", argv[0]);
        return -1;
    }

    /* Luam portul de la linia de comanda */
    port = atoi(argv[2]);

    /* cream un socket pentru comunicare */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
    {
        perror("Eroare la socket().\n");
        return errno;
    }

    /* umplem structura folosita pentru realizarea conexiunii cu serverul */
    /* familia socket-ului */
    server.sin_family = AF_INET;
    /* adresa IP a serverului */
    server.sin_addr.s_addr = inet_addr(argv[1]);
    /* portul de conectare */
    server.sin_port = htons(port);

    /* realizam conexiunea cu serverul (faza blocanta pana la realizarea conexiunii) */
    if (connect(sd, (struct sockaddr * ) & server, sizeof(struct sockaddr)) == -1)
    {
        perror("[client]Eroare la connect().\n");
        return errno;
    }

    //>>------

    int numOfTries = 1;
    while (true)
    {
        printf("Try number: %d\n", numOfTries);
        int randomFoodPreference = rand() % FOOD_PREFERENCES_INTERVAL + 1;

        printf("[client] preferinta alesa random este: %d\n", randomFoodPreference);

        /* trimitem preferinta aleasa random catre server */
        if (write(sd, & randomFoodPreference, sizeof(int)) <= 0)
        {
            perror("[client]Eroare la write() spre server.\n");
            return errno;
        }

        /* 
        citim raspunsul serverului (faza blocanta pana ce serverul ne trimite mesajul)

        raspunsul este format din 2 mesaje:
        primul mesaj reprezinta un int, ce ne da dimensiunea celui de-al doilea mesaj
        al doilea mesaj reprezinta un sir de caractere de caractere ce are dimensiunea setata in functie de primul mesaj
        */

        //citim dimensiunea mesajului
        int responseLength = 0;
        if (read(sd, & responseLength, sizeof(int)) < 0)
        {
            perror("[client]Eroare la read() de la server.\n");
            return errno;
        }

        printf("Am citit responseLength: %d\n", responseLength);

        //alocam memorie in functie de primul mesaj
        char * response = new char[responseLength + 1];

        //adaugam null la final
        response[responseLength] = '\0';

        //citim caracter cu caracter sirul de caractere 
        for (int i = 0; i < responseLength; i++)
        {
            if (read(sd, & response[i], sizeof(char)) < 0)
            {
                perror("[client]Eroare la read() caracter de la server.\n");
                delete[] response;
                return errno;
            }
        }

        printf("Am citit response: %s\n", response);

        //pregatim raspunsul de trimis inapoi in functie de ce am primit de la server
        //raspunsul pe care il trimitem e structurat in 2 mesaje.
        //primul reprezinta dimensiunea celui de-al doilea mesaj.
        //al doilea reprezinta un sir de caractere de dimensiunea primului mesaj

        char * messageToSendBack = nullptr;
        int messageToSendBackLength = 0;
        bool endConnection = false;

        if (strcmp(response, "Masa e servita!") == 0)
        {
            /* Am primit de la server ca masa este servita. Notificam serverul ca suntem satuli si marcam ca dorim sa incheiem conexiunea */
            printf("\n");
            messageToSendBackLength = strlen("Satul!");
            messageToSendBack = new char[messageToSendBackLength];
            strcpy(messageToSendBack, "Satul!");

            endConnection = true;
        } 
        else if (strcmp(response, "Indisponibil") == 0)
        {
            //Am primit de la server ca preferinta noastra nu este disponibila.

            if (numOfTries >= 3)
            {
                //daca nr de incercari a depasit 3, notificam cantina ca murim de foame si marcam ca dorim sa incheiem conexiunea
                messageToSendBackLength = strlen("Schimb cantina! Aici mor de foame!");
                messageToSendBack = new char[messageToSendBackLength];
                strcpy(messageToSendBack, "Schimb cantina! Aici mor de foame!");
                printf("Schimb cantina! Aici mor de foame!\n");
                endConnection = true;
            }
            else
            {
                //daca numarul de incercari nu a depasit 3, notificam cantina ca mai asteptam
                messageToSendBackLength = strlen("Mai astept...");
                messageToSendBack = new char[messageToSendBackLength];
                strcpy(messageToSendBack, "Mai astept...");
                printf("Mai astept...\n");

                //crestem nr de incercari la fiecare indisponibilitate data de cantina
                numOfTries++;
            }
        }

        //trimitem catre server dimensiunea celui de-al doilea mesaj
        if (write(sd, & messageToSendBackLength, sizeof(int)) <= 0)
        {
            perror("[client]Eroare la write() dimensiune raspuns spre server.\n");
            return errno;
        }
        else
        {
            printf("Am trimis dimensiune mesaj: %d\n", messageToSendBackLength);
        }

        //trimitem catre server sirul de caractere corespondent actiunii facute
        if (write(sd, messageToSendBack, sizeof(char) * messageToSendBackLength) <= 0)
        {
            perror("[client]Eroare la write() dimensiune raspuns spre server.\n");
            return errno;
        }
        else
        {
            printf("Am trimis mesajul: %s\n", messageToSendBack);
        }

        //dealocam sirurile alocate
        delete[] messageToSendBack;
        delete[] response;

        //daca actiunea curenta necesita inchiderea conexiunii, inchidem conexiunea
        if (endConnection)
        {
            close(sd);
            printf("End connection\n");
            break;
        }

        //daca nu inchidem conexiunea inseamna ca suntem in faza in care trebuie sa asteptam un nr de secunde.

        //luam un int <= 60 random ce reprezinta cat de mult urmeaza sa asteptam 
        int randomTimeToWait = rand() % 60 + 1;
        printf("Intru in faza de asteptare. Astept %d secunde...\n", randomTimeToWait);
        //asteptam randomTimeToWait
        sleep(randomTimeToWait);
    }
}