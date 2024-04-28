# TCP Concurrent Server

## Description:
Consider a concurrent TCP server capable of connecting a maximum of N clients.
Every T seconds (where T < 120), the server will receive a message from some of the clients, identifying a type of food (from 1 to 5) chosen randomly by the respective client.
The server will count the requests, and for the most preferred food type (having the highest number of requests), it will respond to the requesting clients with "Meal is served".
For the other clients (who have chosen a different type of food), it will respond with "Unavailable". A client that has been served lunch will display a message "Satisfied!" and terminate its execution.
A refused client will retry with another randomly chosen request after an also randomly chosen interval (up to 60 seconds).
After three refused requests, a client will display "Changing cafeteria! Starving here!" and terminate its execution.


## Compilation:

### Server:
```bash
g++ server.cpp -o server
```

### Client:
```bash
g++ client.cpp -o client
```

## Running the Server:
```bash
./server
```

## Running the Client:
```bash
./client 127.0.0.1 2908
```

## Collaborator
- Macsim Oana
