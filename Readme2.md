    a file descriptor is simply integer associated with an open file, that file can be a network
connection, FIFO, pipe, terminal, real on-the-disk file, everthing in Unix is file , so when you want
to communicate with another program over the internet you're gonna do it throught a file descriptor 


IRC ( Internet Relay Chat) : est un protocole de communicaion
        I- SYSCALLS :
            1- Creation :
                => socket() : elle cree un point de terminaison pour la communication 
                        elle renvoie un file descriptor (entier)
                            type de socket
                        /           \
                        /              \
                    AF_INET(ipv4)      SOCK_STREAM(tcp)
                => setsockopt() : elle permet de configurer le socket , on l'utilise surtout pour activer SO_REUSEADDR , ce qui permet de ralencer ton serveur immediatement snas que le systeme 
                te dise "Port alreday in use".
            2- Mise en place :
                => bind() : lie ton socket a une adresse IP et a un port . 
                        **exemple :
                            comme tu donne une adresse fixe a ton standariste pour les clients sachent
                            ou appeler.
                => listen() : elle place le socket en mode d'ecoute , le serveur est pret a recevoir 
                    des appels , ms il ne les a pas encore decroches. elle definit aussi la taille 
                    de la "file d'attente" des clients qui essaie de se connecter.
                => fcntl() : utiliser pour mettre les sockets en mon O_NONBLOCK , il permet au 
                    program de ne pas s'arreter net s'il essaie de lire sur une socket ou il n'y a rien a lire.
            3- Gestion de Traffic :
                => poll() (ou select()) : il donne une liste de tous tes clients connectes et il evite de faire une boucle infinie.
            4- Communication : 
                => accept() : quand poll() te previent qu'un nouveau client appelle sur le socket principal 
                    elle cree un nouveau socket specifique pour ce client.
                => recv() : pour lire les octets envoyees par un client . mais pour irc il termine 
                    les msgs par "\r\n " si tu ne recoit pas ce dlimiteur le messag en'est pas fini.
                => send() : pour envoyer une reponse au client.
            5- Fermeture :
                => close() : pour fermer la connexion d'un client ou eteindre le serveur, cela 
                    libere le file descriptor pour le systeme.
      II- PORT :
            1- pour savoir si un port est libre ou non :
                lsof -i :N de port 
            
            
