#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <stdarg.h>
#include <stdbool.h>

#include "server.h"
#include "client.h"
#include "group.h"

Groups *groupsList = NULL;
Mps *mpsList = NULL;
static unsigned int idDisc = 0;

static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if (err < 0)
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

static void app(void)
{
   init_disc();
   SOCKET sock = init_connection();
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];

   fd_set rdfs;

   while (1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for (i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      if (select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if (FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if (FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = {0};
         size_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if (csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         if (custom_read_client(csock, buffer) == -1)
         {
            /* disconnected */
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);

         Client c = {csock};
         strncpy(c.name, buffer, BUF_SIZE - 1);
         if (c.currentDiscussion == 0)
         {
            c.currentDiscussion = 1;
         }
         // Par défaut, le groupe de landing est sur l'id 1
         printf("%s just connected on channel %s\n", c.name, groupsList[c.currentDiscussion - 1].group.name);
         clients[actual] = c;
         actual++;

         get_history(c);
      }
      else
      {
         int i = 0;
         for (i = 0; i < actual; i++)
         {
            /* a client is talking */
            if (FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               int c = custom_read_client(clients[i].sock, buffer);
               /* client disconnected */
               if (c == 0)
               {
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  strncpy(buffer, client.name, BUF_SIZE - 1);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(clients, client, actual, buffer, 1);
                  save_message(client, buffer, 1);
                  print_message_server(client, buffer, 1);
               }
               else
               {
                  if (is_request(buffer) == 1)
                  {
                     extract_request(buffer);
                     printf("Command received : %s\n", buffer);
                     execute_command(client, clients, actual, buffer, i);
                  }
                  else
                  {
                     // TODO - Attribuer une discussion en cours à un client et envoyer au membres de la discussion en cours du client plutot qu'a tous les clients
                     save_message(client, buffer, 0);
                     print_message_server(client, buffer, 0);
                     send_message_to_specific_clients(clients, client, actual, buffer, 0);
                     // send_message_to_all_clients(clients, client, actual, buffer, 0);
                  }
                  memset(buffer, 0, BUF_SIZE);
               }

               break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void save_message(Client sender, const char *buffer, char from_server)
{
   char *extension = ".txt";
   char *filepath = "Discussions/";
   char *subpath = malloc(sizeof(char) * 6);
   subpath[0] = 0;

   snprintf(subpath, sizeof(char) * 6, "%d", sender.currentDiscussion);
   char *filename = malloc((strlen(filepath) + 1 + 6 + strlen(extension) + 1) * sizeof(char));
   filename[0] = 0;
   printf("Ouverture du fichier : %s \n", strcat(strcat(strcpy(filename, filepath), subpath), extension));
   FILE *ptr = fopen(strcat(strcat(strcpy(filename, filepath), subpath), extension), "a");

   if (ptr == NULL)
   {
      printf("Erreur lors de l'enregistrement du message\n");
      return;
   }

   char message[BUF_SIZE];
   message[0] = 0;

   if (from_server == 0)
   {
      strncpy(message, sender.name, BUF_SIZE - 1);
      strncat(message, " : ", sizeof message - strlen(message) - 1);
   }
   strncat(message, buffer, sizeof message - strlen(message) - 1);

   fprintf(ptr, "%s\n", message);
   fflush(ptr);
   fclose(ptr);

   free(subpath);
   free(filename);
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for (i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for (i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if (sender.sock != clients[i].sock)
      {
         if (from_server == 0)
         {
            strncpy(message, sender.name, BUF_SIZE - 1);
            strncat(message, " : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         custom_write_client(clients[i].sock, message);
         memset(message, 0, strlen(message));
      }
   }
}

static void send_message_to_specific_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for (i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if (sender.sock != clients[i].sock && clients[i].currentDiscussion == sender.currentDiscussion)
      {
         if (from_server == 0)
         {
            strncpy(message, sender.name, BUF_SIZE - 1);
            strncat(message, " : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         custom_write_client(clients[i].sock, message);
      }
   }
}

static void print_message_server(Client sender, const char *buffer, char from_server)
{
   char message[BUF_SIZE];
   message[0] = 0;

   if (from_server == 0)
   {
      strncpy(message, sender.name, BUF_SIZE - 1);
      strncat(message, " : ", sizeof message - strlen(message) - 1);
   }
   strncat(message, buffer, sizeof message - strlen(message) - 1);

   printf("%s \n", message);
}

static void get_history(Client receiver)
{
   char *extension = ".txt";
   char *filepath = "Discussions/";
   char *subpath = malloc(sizeof(char) * 6);
   subpath[0] = 0;

   snprintf(subpath, sizeof(char) * 6, "%d", receiver.currentDiscussion);
   char *filename = malloc((strlen(filepath) + 1 + 6 + strlen(extension) + 1) * sizeof(char));
   filename[0] = 0;
   printf("Ouverture du fichier : %s \n", strcat(strcat(strcpy(filename, filepath), subpath), extension));
   FILE *ptr = fopen(strcat(strcat(strcpy(filename, filepath), subpath), extension), "r");

   char message[BUF_SIZE];
   message[0] = 0;

   char *line = NULL;
   size_t len = 0;
   ssize_t read;

   if (ptr == NULL)
   {
      printf("Erreur lors de l'ouverture du fichier d'historique\n");
      return;
   }

   strcat(message, "\033[36mHistorique récent de la discussion :\033[0m\n");

   fseek(ptr, 0, SEEK_SET);
   long int pos = ftell(ptr);
   bool debut = false;
   
   fseek(ptr, -(BUF_SIZE-strlen("\033[36mHistorique récent de la discussion :\033[0m\n")-1-1), SEEK_END);

   if(ftell(ptr) != pos)
   {
      getline(&line, &len, ptr);
   }
   else
   {
      debut = true;
   }

   char * str = malloc(3*sizeof(char));
   fgets(str, 3, ptr);

   fflush(ptr);
   fclose(ptr);
   ptr = fopen(strcat(strcat(strcpy(filename, filepath), subpath), extension), "r");
   fseek(ptr, -(BUF_SIZE-strlen("\033[36mHistorique récent de la discussion :\033[0m\n")-1-1), SEEK_END);
   if(strstr(str,"\n") == 0 && !debut)
   {
      getline(&line, &len, ptr);
   }

   while ((read = getline(&line, &len, ptr)) != -1)
   {
      if (read != 0 && strstr(line, "disconnected") == NULL && line)
      {
         strncat(message, line, len);
      }
   }

   // printf("%s\n", message);
   if (message[0] != 0)
   {
      custom_write_client(receiver.sock, message);
   }

   fflush(ptr);
   fclose(ptr);
   free(subpath);
   free(filename);
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = {0};

   if (sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   int opt = 1;
   if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt)))
   {
      perror("setsockopt");
      exit(EXIT_FAILURE);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if (bind(sock, (SOCKADDR *)&sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if (listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int custom_read_client(SOCKET sock, char *buffer)
{
   int n = 0;
   uint32_t msgLength, tmp;
   memset(buffer, 0, BUF_SIZE);

   if ((n = recv(sock, &tmp, sizeof(uint32_t), 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   msgLength = ntohl(tmp);

   if ((n = recv(sock, buffer, msgLength, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disonnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void custom_write_client(SOCKET sock, const char *content)
{
   // printf("Buffer in Write_Client:\n%s\n", content);
   char *tmpMsg = calloc(strlen(content) + 1, sizeof(char));
   strcpy(tmpMsg, content);

   uint32_t length = htonl(strlen(tmpMsg) + 1);
   if (send(sock, &length, sizeof(uint32_t), 0) < 0)
   {
      perror("Erreur lors de l'envoi de la taille du message\n");
      free(tmpMsg);
      exit(errno);
   }
   if (send(sock, tmpMsg, strlen(tmpMsg) + 1, 0) < 0)
   {
      perror("Erreur lors de l'envoi du contenu du message\n");
      free(tmpMsg);
      exit(errno);
   }

   free(tmpMsg);
}

static int is_request(const char *buffer)
{
   char *strToken;
   char *tmpString = malloc((strlen(buffer) + 1) * sizeof(char));
   tmpString[0] = 0;

   strcpy(tmpString, buffer);

   const char *seps = "[]";
   const char *cmdCompare = "serv-request";

   strToken = strtok(tmpString, seps);
   if (strcmp(strToken, cmdCompare) == 0)
   {
      printf("Message is request\n");
      printf("%s\n", buffer);
      free(tmpString);
      return 1;
   }
   printf("Message is NOT request\n");
   printf("%s\n", buffer);
   free(tmpString);
   return 0;
}

static void extract_request(char *buffer)
{
   printf("%s\n", buffer);
   const char *seps = "[]";

   char *strToken = strtok(buffer, seps);
   char *cmd;
   if (strcmp(strToken, "serv-request") == 0)
   {
      cmd = strtok(NULL, seps);
   }
   strcpy(buffer, cmd);
}

static void execute_command(Client client, Client *clients, int actual, char *buffer, int numClient)
{
   char *strToken = malloc(sizeof(char) * BUF_SIZE);
   strToken[0] = 0;
   char *tmpString = malloc((strlen(buffer) + 1) * sizeof(char));
   tmpString[0] = 0;

   strcpy(tmpString, buffer);

   const char *seps = " ";
   strToken = strtok(tmpString, seps);
   printf("%s\n", strToken);

   if (strcmp(strToken, "co_list") == 0)
   {
      printf("Executing command co_list\n");
      get_connected_list(client, clients, actual);
   }
   else if (strcmp(strToken, "create_mp_disc") == 0)
   {
      char *member1 = client.name;

      strToken = strtok(NULL, seps);
      char *name = malloc((strlen(strToken) + 1) * sizeof(char));
      name[0] = 0;
      printf("name: %s\n", strToken);
      strcpy(name, strToken);

      strToken = strtok(NULL, seps);
      char *member2 = malloc((strlen(strToken) + 1) * sizeof(char));
      member2[0] = 0;
      printf("member2: %s\n", strToken);
      strcpy(member2, strToken);

      if((strToken = strtok(NULL, seps)) == NULL)
      {
         char * msg = "Invalid use of command, see README to know how to use each command";
         custom_write_client(client.sock, msg);
         free(name);
         free(member2);
         return;
      }
      char *password = malloc((strlen(strToken) + 1) * sizeof(char));
      password[0] = 0;
      printf("password: %s\n", strToken);
      strcpy(password, strToken);

      if (check_occurence_discussion_name(name))
      {
         char *duplicatTrouve = "Le nom de discussion est déjà utilisé. Veuiller réessayer. \n";
         custom_write_client(client.sock, duplicatTrouve);
      }
      else
      {
         create_mp_discussion(name, member1, member2, password);
         custom_write_client(client.sock, "\033[32mDiscussion crée avec succès\033[0m\n");
      }

      free(name);
      free(member2);
   }
   else if (strcmp(strToken, "create_group_disc") == 0)
   {
      int i = 0;
      int j;
      char **members;
      char *member1 = client.name;
      char *password;

      strToken = strtok(NULL, seps);
      char *priv = malloc((strlen(strToken) + 1) * sizeof(char));
      priv[0] = 0;
      strcpy(priv, strToken);
      int privInt = atoi(priv);
      printf("privInt: %d\n", privInt);

      strToken = strtok(NULL, seps);
      char *name = malloc((strlen(strToken) + 1) * sizeof(char));
      name[0] = 0;
      printf("name: %s\n", strToken);
      strcpy(name, strToken);

      if (privInt != 0)
      {
         strToken = strtok(NULL, seps);
         password = malloc((strlen(strToken) + 1) * sizeof(char));
         password[0] = 0;
         printf("password: %s\n", strToken);
         strcpy(password, strToken);

         members = malloc(100 * sizeof(char *));
         for (j = 0; j < 100; ++j)
         {
            members[j] = malloc(10 * sizeof(char));
            members[j][0] = 0;
         }

         strToken = strtok(NULL, seps);
         while (strToken != NULL)
         {
            strcpy(members[i], strToken);
            printf("member%d: %s\n", i, members[i]);
            strToken = strtok(NULL, seps);
            printf("strToken: %s\n", strToken);
            ++i;
         }

         // On ajoute le créateur de la discussion (pour avoir le bon nombre
         // de participants)
         ++i;
      }
      printf("nbMembres: %d\n", i);

      if (check_occurence_discussion_name(name))
      {
         char *duplicatTrouve = "Le nom de discussion est déjà utilisé. Veuiller réessayer. \n";
         custom_write_client(client.sock, duplicatTrouve);
      }
      else
      {
         create_group_discussion(privInt, name, member1, members, i, password);
         custom_write_client(client.sock, "\033[32mDiscussion crée avec succès\033[0m\n");
      }

      free(priv);
      free(name);
      if (privInt != 0)
      {
         for (j = 0; j < 100; ++j)
         {
            free(members[j]);
         }
         free(members);
      }
      free(password);
   }
   else if (strcmp(strToken, "disc_list") == 0)
   {
      get_list_discussions(client);
   }
   else if (strcmp(strToken, "change_disc") == 0)
   {
      strToken = strtok(NULL, seps);
      char *name = malloc((strlen(strToken) + 1) * sizeof(char));
      name[0] = 0;
      strcpy(name, strToken);
      int discussionIdInt = get_id_discussion(name);

      int discussionPrive = check_private_discussion(name);

      bool passwordCorrect = false;

      if (discussionPrive != 0)
      {
         if((strToken = strtok(NULL, seps)) == NULL)
         {
            char * msg = "Invalid use of command, see README to know how to use each command";
            custom_write_client(client.sock, msg);
            free(name);
            return;
         }
         char *password = malloc((strlen(strToken) + 1) * sizeof(char));
         password[0] = 0;
         strcpy(password, strToken);
         passwordCorrect = check_password(discussionIdInt, password);
      }

      if (discussionIdInt == -1)
      {
         char *discussionNonTrouve = "Il n'y a aucune discussion avec le nom spécifié. \n";
         custom_write_client(client.sock, discussionNonTrouve);
      }
      else if (discussionPrive != 0 && !passwordCorrect)
      {
         char *passwordIncorrectMsg = "Le mot de passe que vous avez entré est erroné. \n";
         custom_write_client(client.sock, passwordIncorrectMsg);
      }
      else
      {
         clients[numClient] = change_discussion(client, discussionIdInt);
      }
      free(name);
   }
   else if (strcmp(strToken, "get_id_disc") == 0)
   {
      strToken = strtok(NULL, seps);
      char *discussionName = malloc((strlen(strToken) + 1) * sizeof(char));
      discussionName[0] = 0;
      strcpy(discussionName, strToken);

      int idDiscussion = get_id_discussion(discussionName);

      if (idDiscussion == -1)
      {
         char *discussionNonTrouve = "Il n'y a aucune discussion avec le nom spécifié. \n";
         custom_write_client(client.sock, discussionNonTrouve);
      }
      else
      {
         char *messageDiscussionId = malloc(sizeof(char) * BUF_SIZE);
         messageDiscussionId[0] = 0;
         char *discussionIDString = malloc(sizeof(char) * BUF_SIZE);
         discussionIDString[0] = 0;
         strcat(messageDiscussionId, "La discussion ");
         strcat(messageDiscussionId, discussionName);
         strcat(messageDiscussionId, " a pour id: ");
         sprintf(discussionIDString, "%d", idDiscussion);
         strcat(messageDiscussionId, strcat(discussionIDString, "\n"));
         custom_write_client(client.sock, messageDiscussionId);
         free(messageDiscussionId);
         free(discussionIDString);
      }
      free(discussionName);
   }
   else
   {
      custom_write_client(client.sock, strcat(tmpString, " is not a valid command\n"));
   }
   free(tmpString);
}

static void get_connected_list(Client client, Client *clients, int actual)
{
   char *connected = malloc((sizeof(char *) * MAX_CLIENTS * 10) + (strlen("\033[33mListe des personnes connectées sur le serveur :\033[0m\n") + 1) * sizeof(char));
   connected[0] = 0;
   strcat(connected, "\033[33mListe des personnes connectées sur le serveur :\033[0m\n");
   int i;

   for (i = 0; i < actual; ++i)
   {
      strcat(connected, clients[i].name);
      strcat(connected, "\n");
   }

   custom_write_client(client.sock, connected);
   free(connected);
}

static void create_mp_discussion(char *name, char *member1, char *member2, char *password)
{
   ++idDisc;
   Mps *mps = malloc(sizeof(Mps));
   mps->disc.name = malloc((strlen(name) + 1) * sizeof(char));
   mps->disc.name[0] = 0;
   mps->disc.member1 = malloc((strlen(member1) + 1) * sizeof(char));
   mps->disc.member1[0] = 0;
   mps->disc.member2 = malloc((strlen(member2) + 1) * sizeof(char));
   mps->disc.member2[0] = 0;
   mps->disc.password = malloc((strlen(password) + 1) * sizeof(char));
   mps->disc.password[0] = 0;
   strcpy(mps->disc.password, password);

   mps->disc.id = idDisc;
   strcpy(mps->disc.name, name);
   strcpy(mps->disc.member1, member1);
   strcpy(mps->disc.member2, member2);
   mps->suivant = NULL;

   if (mpsList == NULL)
   {
      mpsList = mps;
   }
   else
   {
      Mps *parcours = mpsList;
      while (parcours->suivant != NULL)
      {
         parcours = parcours->suivant;
      }
      parcours->suivant = mps;
   }

   printf("Discussion : %s entre %s et %s ajoutée\n", name, member1, member2);
   char *extension = ".txt";
   char *filepath = "Discussions/";
   char *subpath = malloc(sizeof(char) * 6);
   subpath[0] = 0;

   snprintf(subpath, sizeof(char) * 6, "%d", mps->disc.id);
   char *filename = malloc((strlen(filepath) + 1 + 6 + 1 + strlen(extension)) * sizeof(char));
   filename[0] = 0;
   FILE *discFile = fopen(strcat(strcat(strcpy(filename, filepath), subpath), extension), "a");
   printf("Historique de %s créé avec succès\n", mps->disc.name);
   update_discId(idDisc);
   add_mp(mps->disc);

   free(subpath);
   free(filename);
}

static void create_group_discussion(int priv, char *name, char *member1, char **members, int nbMembers, char *password)
{
   ++idDisc;
   int i;
   Groups *group = malloc(sizeof(Groups));
   group->group.id = idDisc;

   group->group.name = malloc((strlen(name) + 1) * sizeof(char));
   group->group.name[0] = 0;
   strcpy(group->group.name, name);

   group->group.priv = priv;
   group->group.nbMembers = nbMembers;
   printf("nbMembres: %d\n", group->group.nbMembers);

   if (priv != 0)
   {
      group->group.members = malloc((group->group.nbMembers) * sizeof(char *));
      group->group.members[0] = malloc((strlen(member1) + 1) * sizeof(char));
      group->group.members[0][0] = 0;

      strcpy(group->group.members[0], member1);
      for (i = 0; i < group->group.nbMembers; ++i)
      {
         group->group.members[i + 1] = malloc((strlen(members[i]) + 1) * sizeof(char));
         group->group.members[i + 1][0] = 0;
         strcpy(group->group.members[i + 1], members[i]);
      }

      group->group.password = malloc((strlen(password) + 1) * sizeof(char));
      group->group.password[0] = 0;
      strcpy(group->group.password, password);
   }
   else
   {
      group->group.members = NULL;
   }

   group->suivant = NULL;

   if (groupsList == NULL)
   {
      // Insertion premier elem groupsList
      groupsList = group;
   }
   else
   {
      Groups *parcours = groupsList;
      // Insertion en fin de groupsList
      while (parcours->suivant != NULL)
      {
         parcours = parcours->suivant;
      }
      parcours->suivant = group;
   }

   printf("Discussion de groupe : %s ajoutée\n", name);
   char *extension = ".txt";
   char *filepath = "Discussions/";
   char *subpath = malloc(sizeof(char) * 6);
   subpath[0] = 0;

   snprintf(subpath, sizeof(char) * 6, "%d", group->group.id);
   char *filename = malloc((strlen(filepath) + strlen(subpath) + strlen(extension) + 3) * sizeof(char));
   filename[0] = 0;
   FILE *discFile = fopen(strcat(strcat(strcat(filename, filepath), subpath), extension), "a");
   printf("Historique de %s créé avec succès\n", name);
   update_discId(idDisc);
   add_group(group->group);

   free(subpath);
   free(filename);
}

static bool check_occurence_discussion_name(char *discussionName)
{
   bool discussionNameDuplicate = false;

   Mps *parcoursDiscSimple = mpsList;
   while (parcoursDiscSimple != NULL)
   {
      if (strcmp(parcoursDiscSimple->disc.name, discussionName) == 0)
      {
         discussionNameDuplicate = true;
         return discussionNameDuplicate;
      }
      parcoursDiscSimple = parcoursDiscSimple->suivant;
   }

   Groups *parcoursDiscGroupe = groupsList;
   while (parcoursDiscGroupe != NULL)
   {
      if (strcmp(parcoursDiscGroupe->group.name, discussionName) == 0)
      {
         discussionNameDuplicate = true;
         return discussionNameDuplicate;
      }
      parcoursDiscGroupe = parcoursDiscGroupe->suivant;
   }

   return discussionNameDuplicate;
}

static int get_id_discussion(char *discussionName)
{
   Mps *parcoursDiscSimple = mpsList;
   while (parcoursDiscSimple != NULL)
   {
      if (strcmp(parcoursDiscSimple->disc.name, discussionName) == 0)
      {
         return parcoursDiscSimple->disc.id;
      }
      parcoursDiscSimple = parcoursDiscSimple->suivant;
   }

   Groups *parcoursDiscGroupe = groupsList;
   while (parcoursDiscGroupe != NULL)
   {
      if (strcmp(parcoursDiscGroupe->group.name, discussionName) == 0)
      {
         return parcoursDiscGroupe->group.id;
      }
      parcoursDiscGroupe = parcoursDiscGroupe->suivant;
   }

   return -1;
}

static void get_list_discussions(Client client)
{
   char *discussions = malloc(sizeof(char) * BUF_SIZE);
   discussions[0] = 0;

   Groups *gList = groupsList;
   if (gList != NULL)
   {
      strcat(discussions, "\033[43mDiscussions de groupe\033[0m\n");
   }
   while (gList != NULL)
   {
      char *name = malloc(sizeof(char) * (strlen(gList->group.name) + 1));
      strcpy(name, gList->group.name);
      if (gList->group.priv != 0) // prive
      {
         strcat(strcat(discussions, "\033[31m"), strcat(name, "\033[0m\n"));
      }
      else // public
      {
         strcat(strcat(discussions, "\033[32m"), strcat(name, "\033[0m\n"));
      }
      free(name);
      gList = gList->suivant;
   }

   Mps *mList = mpsList;
   if (mList != NULL)
   {
      strcat(discussions, "\n\033[43mDiscussions privées\033[0m\n");
   }
   while (mList != NULL)
   {
      char *name = malloc(sizeof(char) * (strlen(mList->disc.name) + 1));
      strcpy(name, mList->disc.name);
      strcat(discussions, strcat(name, "\n"));
      free(name);
      mList = mList->suivant;
   }

   custom_write_client(client.sock, discussions);
   free(discussions);
}

static Client change_discussion(Client client, unsigned int discussionID)
{
   bool discussionTrouve = false;
   bool verificationMembre = false;

   char *messageChangementDiscussion = malloc(sizeof(char) * BUF_SIZE);
   messageChangementDiscussion[0] = 0;

   char *discussionIDString = malloc(sizeof(char) * BUF_SIZE);
   discussionIDString[0] = 0;

   if (mpsList != NULL)
   {
      Mps *parcoursDiscSimple = mpsList;
      while (!discussionTrouve && parcoursDiscSimple != NULL)
      {
         if (parcoursDiscSimple->disc.id == discussionID)
         {
            discussionTrouve = true;

            if (strcmp(parcoursDiscSimple->disc.member1, client.name) == 0 || strcmp(parcoursDiscSimple->disc.member2, client.name) == 0)
            {
               verificationMembre = true;
               client.currentDiscussion = discussionID;
               strcat(messageChangementDiscussion, "Vous êtes maintenant sur la discussion ");
               strcat(messageChangementDiscussion, parcoursDiscSimple->disc.name);
               strcat(messageChangementDiscussion, " qui a pour id: ");
               sprintf(discussionIDString, "%u", parcoursDiscSimple->disc.id);
               strcat(messageChangementDiscussion, strcat(discussionIDString, "\n"));
               custom_write_client(client.sock, messageChangementDiscussion);
               get_history(client);
            }

            break;
         }
         parcoursDiscSimple = parcoursDiscSimple->suivant;
      }
   }

   if (!discussionTrouve && groupsList != NULL)
   {
      Groups *parcoursDiscGroupe = groupsList;
      while (!discussionTrouve && parcoursDiscGroupe != NULL)
      {
         if (parcoursDiscGroupe->group.id == discussionID)
         {
            discussionTrouve = true;

            if (parcoursDiscGroupe->group.priv == 0)
            {
               verificationMembre = true;
            }
            else
            {
               for (int i = 0; i < parcoursDiscGroupe->group.nbMembers; i++)
               {
                  if (strcmp(parcoursDiscGroupe->group.members[i], client.name) == 0)
                  {
                     verificationMembre = true;
                     break;
                  }
               }
            }

            if (verificationMembre)
            {
               client.currentDiscussion = discussionID;
               strcat(messageChangementDiscussion, "Vous êtes maintenant sur la discussion ");
               strcat(messageChangementDiscussion, parcoursDiscGroupe->group.name);
               strcat(messageChangementDiscussion, " qui a pour id: ");
               sprintf(discussionIDString, "%u", parcoursDiscGroupe->group.id);
               strcat(messageChangementDiscussion, strcat(discussionIDString, "\n"));
               custom_write_client(client.sock, messageChangementDiscussion);
               get_history(client);
            }

            break;
         }
         parcoursDiscGroupe = parcoursDiscGroupe->suivant;
      }
   }

   if (!discussionTrouve)
   {
      char *discussionNonTrouvee = "La discussion avec l'id spécifiée n'existe pas !\n";
      custom_write_client(client.sock, discussionNonTrouvee);
   }
   else if (!verificationMembre)
   {
      char *clientNonMembre = "Vous n'avez pas le droit d'accéder à la discussion !\n";
      custom_write_client(client.sock, clientNonMembre);
   }

   free(messageChangementDiscussion);
   free(discussionIDString);

   return client;
}

static bool check_password(unsigned int discussionId, char *password)
{
   Mps *parcoursDiscSimple = mpsList;
   while (parcoursDiscSimple != NULL)
   {
      if (parcoursDiscSimple->disc.id == discussionId)
      {
         return (strcmp(parcoursDiscSimple->disc.password, password) == 0);
      }
      parcoursDiscSimple = parcoursDiscSimple->suivant;
   }

   Groups *parcoursDiscGroupe = groupsList;
   while (parcoursDiscGroupe != NULL)
   {
      if (parcoursDiscGroupe->group.id == discussionId)
      {
         return (strcmp(parcoursDiscGroupe->group.password, password) == 0);
      }
      parcoursDiscGroupe = parcoursDiscGroupe->suivant;
   }

   return false;
}

static bool check_private_discussion(char *discussionName)
{
   Mps *parcoursDiscSimple = mpsList;
   while (parcoursDiscSimple != NULL)
   {
      if (strcmp(parcoursDiscSimple->disc.name, discussionName) == 0)
      {
         return 1;
      }
      parcoursDiscSimple = parcoursDiscSimple->suivant;
   }

   Groups *parcoursDiscGroupe = groupsList;
   while (parcoursDiscGroupe != NULL)
   {
      if (strcmp(parcoursDiscGroupe->group.name, discussionName) == 0)
      {
         return parcoursDiscGroupe->group.priv;
      }
      parcoursDiscGroupe = parcoursDiscGroupe->suivant;
   }

   return 0;
}

static void init_disc()
{
   FILE *config = fopen("Serveur/app.cfg", "r");
   if (config == NULL)
   {
      printf("Erreur ou aucune discussion existante");
      update_discId(0);

      create_group_discussion(0, "Landing", NULL, NULL, 0, NULL);
      return;
   }

   char *line;
   size_t len = 0;

   while (!feof(config))
   {
      getline(&line, &len, config);

      // Gestion d'erreurs
      if (ferror(config))
      {
         printf("Erreur lors de la lecture du fichier de configuration des discussions\n");
         break;
      }

      if (line != NULL)
      {
         // Restoration de l'id général de discussion si existant
         if (strstr(line, "idDisc: ") != NULL)
         {
            char *seps = " ";
            char *idC = malloc(sizeof(char) * 6);
            idC[0] = 0;
            strtok(line, seps);
            idC = strtok(NULL, seps);
            idDisc = atoi(idC);
            printf("idDisc: %d\n", idDisc);
         }

         // Restoration d'une discussion
         if (strstr(line, "id: ") != NULL)
         {
            char *strToken = malloc(sizeof(char) * BUF_SIZE);
            strToken[0] = 0;

            // Lecture ID
            char *pass = " ";
            char *seps = "\n";
            int id;

            strtok(line, pass);
            strToken = strtok(NULL, seps);
            id = atoi(strToken);

            getline(&line, &len, config);

            if (strstr(line, "name: ") != NULL)
            {
               // Lecture NAME
               strtok(line, pass);
               strToken = strtok(NULL, seps);
               char *name = malloc(sizeof(char) * (strlen(strToken) + 1));
               name[0] = 0;
               strcpy(name, strToken);

               getline(&line, &len, config);

               // Restoration d'une discussion MP
               if (strstr(line, "type: simple") != NULL)
               {
                  getline(&line, &len, config);

                  if(strstr(line, "password: ") != NULL)
                  {
                     // Lecture PASSWORD
                     strtok(line, pass);
                     strToken = strtok(NULL, seps);
                     char *password = malloc(sizeof(char) * (strlen(strToken) + 1));
                     password[0] = 0;
                     strcpy(password, strToken);

                     getline(&line, &len, config);

                     // Lecture membre 1
                     if (strstr(line, "member1: ") != NULL)
                     {
                        strtok(line, pass);
                        strToken = strtok(NULL, seps);
                        char *member1 = malloc(sizeof(char) * (strlen(strToken) + 1));
                        member1[0] = 0;
                        strcpy(member1, strToken);

                        getline(&line, &len, config);

                        // Lecture membre 2
                        if (strstr(line, "member2: ") != NULL)
                        {
                           strtok(line, pass);
                           strToken = strtok(NULL, seps);
                           char *member2 = malloc(sizeof(char) * (strlen(strToken) + 1));
                           member2[0] = 0;
                           strcpy(member2, strToken);

                           // Affichage TEST
                           // TODO - appel à une fonction de restauration
                           printf("\033[43mRestauration d'une discussion MP\033[0m\n");
                           printf("id: %d\n", id);
                           printf("name: %s\n", name);
                           printf("member1: %s\n", member1);
                           printf("member2: %s\n", member2);

                           load_mp_disc(id, name, member1, member2, password);

                           // Libération des ressources
                           free(name);
                           free(password);
                           free(member1);
                           free(member2);
                        }
                     }
                  }
               }

               // Restoration d'une discussion de groupe
               if (strstr(line, "type: groupe") != NULL)
               {
                  getline(&line, &len, config);

                  // Lecture PRIV
                  int priv;

                  strtok(line, pass);
                  strToken = strtok(NULL, seps);
                  priv = atoi(strToken);

                  // Restoration d'un groupe publique
                  if (priv == 0)
                  {
                     // Affichage TEST
                     // TODO - appel à une fonction de restauration
                     printf("\033[43mRestauration d'une discussion de groupe publique\033[0m\n");
                     printf("id: %d\n", id);
                     printf("name: %s\n", name);
                     printf("priv: %d\n", priv);

                     load_group_disc(id, name, priv, 0, NULL, NULL);

                     // Libération des ressources
                     free(name);
                  }
                  // Restoration d'un groupe privé
                  else
                  {
                     getline(&line, &len, config);
                     
                     if(strstr(line, "password: ") != NULL)
                     {
                        // Lecture PASSWORD
                        strtok(line, pass);
                        strToken = strtok(NULL, seps);
                        char *password = malloc(sizeof(char) * (strlen(strToken) + 1));
                        password[0] = 0;
                        strcpy(password, strToken);

                        getline(&line, &len, config);

                        if (strstr(line, "nbMembers: ") != NULL)
                        {
                           // Lecture NBMEMBERS
                           int nb;

                           strtok(line, pass);
                           strToken = strtok(NULL, seps);
                           nb = atoi(strToken);

                           getline(&line, &len, config);
                           getline(&line, &len, config);

                           // Lecture MEMBERS
                           int k;
                           char **members = malloc(sizeof(char *) * nb);
                           for (k = 0; k < nb; ++k)
                           {
                              members[k] = malloc(sizeof(char) * 10);
                              strToken = strtok(line, seps);
                              strcpy(members[k], strToken);
                              if (k < nb - 1)
                              {
                                 getline(&line, &len, config);
                              }
                           }

                           // Affichage TEST
                           // TODO - appel à une fonction de restauration
                           printf("\033[43mRestauration d'une discussion de groupe privée\033[0m\n");
                           printf("id: %d\n", id);
                           printf("name: %s\n", name);
                           printf("priv: %d\n", priv);
                           printf("nbMembers: %d\n", nb);
                           printf("members:\n");
                           for (k = 0; k < nb; ++k)
                           {
                              printf("%s\n", members[k]);
                           }

                           load_group_disc(id, name, priv, nb, members, password);

                           // Libération des ressources
                           free(name);
                           free(password);
                           for (k = 0; k < nb; ++k)
                           {
                              free(members[k]);
                           }
                           free(members);
                        }
                     }
                  }
               }
            }
         }
      }

      line = NULL;
   }

   if (groupsList == NULL && mpsList == NULL)
   {
      idDisc = 0;
      update_discId(0);
      create_group_discussion(0, "Landing", NULL, NULL, 0, NULL);
   }
}

static void update_discId(int newValue)
{
   FILE *config = fopen("Serveur/app.cfg", "r+");
   if (config == NULL)
   {
      config = fopen("Serveur/app.cfg", "a");
   }
   fseek(config, 0, SEEK_SET);
   fprintf(config, "idDisc: %d\n", newValue);
   fflush(config);
   fclose(config);
}

static void add_mp(SimpleDisc disc)
{
   FILE *config = fopen("Serveur/app.cfg", "a");
   if (config == NULL)
   {
      printf("Erreur lors de la mise à jour du fichier de configuration\n");
   }

   fprintf(config, "\nid: %d\n", disc.id);
   fprintf(config, "name: %s\n", disc.name);
   fprintf(config, "type: simple\n");
   fprintf(config, "password: %s\n", disc.password);
   fprintf(config, "member1: %s\n", disc.member1);
   fprintf(config, "member2: %s\n", disc.member2);

   fflush(config);
   fclose(config);
}

static void add_group(GroupDisc disc)
{
   FILE *config = fopen("Serveur/app.cfg", "a");
   if (config == NULL)
   {
      printf("Erreur lors de la mise à jour du fichier de configuration\n");
   }

   fprintf(config, "\nid: %d\n", disc.id);
   fprintf(config, "name: %s\n", disc.name);
   fprintf(config, "type: groupe\n");
   fprintf(config, "priv: %d\n", disc.priv);
   if(disc.priv == 1)
   {
      fprintf(config, "password: %s\n", disc.password);
   }
   fprintf(config, "nbMembers: %d\n", disc.nbMembers);
   fprintf(config, "members:\n");

   /* Lors de la création d'une discussion publique, on ne souhaite pas
    * enregistrer de liste de membre. Cependant, d'après le code serveur,
    * le client ayant exécuté la commande sera quand même compté comme membre
    * de la discussion. C'est pourquoi la discussion doit avoir plus d'un
    * seul membre pour réellement posséder des participants à enregistrer.
    */
   if (disc.nbMembers > 1)
   {
      int i;
      for (i = 0; i < disc.nbMembers; ++i)
      {
         fprintf(config, "%s\n", disc.members[i]);
      }
   }

   fflush(config);
   fclose(config);
}

static void load_group_disc(unsigned int id, char *name, int priv, int nbMembers, char **members, char * password)
{
   int i;
   Groups *group = malloc(sizeof(Groups));
   group->group.id = id;

   group->group.name = malloc((strlen(name) + 1) * sizeof(char));
   group->group.name[0] = 0;
   strcpy(group->group.name, name);

   group->group.priv = priv;
   group->group.nbMembers = nbMembers;

   if (priv != 0)
   {
      group->group.members = malloc((group->group.nbMembers) * sizeof(char *));

      for (i = 0; i < group->group.nbMembers; ++i)
      {
         group->group.members[i] = malloc((strlen(members[i]) + 1) * sizeof(char));
         group->group.members[i][0] = 0;
         strcpy(group->group.members[i], members[i]);
      }

      group->group.password = malloc((strlen(password) + 1) * sizeof(char));
      group->group.password[0] = 0;
      strcpy(group->group.password, password);
   }
   else
   {
      group->group.members = NULL;
   }

   group->suivant = NULL;

   if (groupsList == NULL)
   {
      // Insertion premier elem groupsList
      groupsList = group;
   }
   else
   {
      // Insertion en fin de groupsList
      Groups *parcours = groupsList;
      while (parcours->suivant != NULL)
      {
         parcours = parcours->suivant;
      }
      parcours->suivant = group;
   }
}

static void load_mp_disc(unsigned int id, char *name, char *member1, char *member2, char * password)
{
   Mps *mps = malloc(sizeof(Mps));
   mps->disc.name = malloc((strlen(name) + 1) * sizeof(char));
   mps->disc.name[0] = 0;
   mps->disc.password = malloc((strlen(password) + 1) * sizeof(char));
   mps->disc.password[0] = 0;
   mps->disc.member1 = malloc((strlen(member1) + 1) * sizeof(char));
   mps->disc.member1[0] = 0;
   mps->disc.member2 = malloc((strlen(member2) + 1) * sizeof(char));
   mps->disc.member2[0] = 0;

   mps->disc.id = id;
   strcpy(mps->disc.name, name);
   strcpy(mps->disc.password, password);
   strcpy(mps->disc.member1, member1);
   strcpy(mps->disc.member2, member2);
   mps->suivant = NULL;

   if (mpsList == NULL)
   {
      // Insertion premier elem mpsList
      mpsList = mps;
   }
   else
   {
      // Insertion en fin de mpsList
      Mps *parcours = mpsList;
      while (parcours->suivant != NULL)
      {
         parcours = parcours->suivant;
      }
      parcours->suivant = mps;
   }
}

int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
