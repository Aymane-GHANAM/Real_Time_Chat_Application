#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>

#include "client.h"

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(const char *address, const char *name)
{
   SOCKET sock = init_connection(address);
   char buffer[BUF_SIZE];

   fd_set rdfs;

   /* send our name */
   custom_write_server(sock, name);
   //write_server(sock, name);

   while(1)
   {
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the socket */
      FD_SET(sock, &rdfs);

      if(select(sock + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         fgets(buffer, BUF_SIZE - 1, stdin);
         {
            char *p = NULL;
            p = strstr(buffer, "\n");
            if(p != NULL)
            {
               *p = 0;
            }
            else
            {
               /* fclean */
               buffer[BUF_SIZE - 1] = 0;
            }
         }
         custom_write_server(sock, buffer);
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         int n = custom_read_server(sock, buffer);
         /* server down */
         if(n == 0)
         {
            printf("Server disconnected !\n");
            break;
         }
         puts(buffer);
      }
   }

   end_connection(sock);
}

static int init_connection(const char *address)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };
   struct hostent *hostinfo;

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   hostinfo = gethostbyname(address);
   if (hostinfo == NULL)
   {
      fprintf (stderr, "Unknown host %s.\n", address);
      exit(EXIT_FAILURE);
   }

   sin.sin_addr = *(IN_ADDR *) hostinfo->h_addr;
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(connect(sock,(SOCKADDR *) &sin, sizeof(SOCKADDR)) == SOCKET_ERROR)
   {
      perror("connect()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int custom_read_server(SOCKET sock, char *buffer)
{
   int n = 0;
   uint32_t msgLength, tmp;
   memset(buffer, 0, BUF_SIZE);

   if((n = recv(sock, &tmp, sizeof(uint32_t), 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   msgLength = ntohl(tmp);

   if((n = recv(sock, buffer, msgLength, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void custom_write_server(SOCKET sock, const char * content)
{
   char * tmpMsg = calloc((strlen(content) + 1), sizeof(char));
   strcpy(tmpMsg, content);

   uint32_t length = htonl(strlen(tmpMsg)+1);
   if(send(sock, &length, sizeof(uint32_t), 0) < 0)
   {
      perror("Erreur lors de l'envoi de la taille du message\n");
      free(tmpMsg);
      exit(errno);
   }
   if(send(sock, tmpMsg, strlen(tmpMsg)+1, 0) < 0)
   {
      perror("Erreur lors de l'envoi du contenu du message\n");
      free(tmpMsg);
      exit(errno);
   }

   free(tmpMsg);
}

int main(int argc, char **argv)
{
   if(argc != 3)
   {
      printf("Usage : %s [address] [pseudo]\n", argv[0]);
      return EXIT_FAILURE;
   }
   if(strlen(argv[2]) > 9)
   {
      printf("[pseudo] ne doit pas contenir plus de 9 caractères\n Longeur donnée: %lu\n", strlen(argv[2]));
      return EXIT_FAILURE;
   }

   init();

   app(argv[1], argv[2]);

   end();

   return EXIT_SUCCESS;
}
