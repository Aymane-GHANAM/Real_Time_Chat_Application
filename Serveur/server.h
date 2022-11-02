#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>

#elif defined(linux)

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h>  /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

#define CRLF "\r\n"
#define PORT 1977
#define MAX_CLIENTS 100

#define BUF_SIZE 1024

#include "client.h"
#include "group.h"

static void init(void);
static void end(void);
static void app(void);
static void save_message(Client sender, const char *buffer, char from_server);
static void print_message_server(Client sender, const char *buffer, char from_server);
static void get_history(Client receiver);
static int init_connection(void);
static void end_connection(int sock);
static int custom_read_client(SOCKET sock, char *buffer);
static void custom_write_client(SOCKET sock, const char *content);
static void send_message_to_all_clients(Client *clients, Client client, int actual, const char *buffer, char from_server);
static void send_message_to_specific_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server);
static void remove_client(Client *clients, int to_remove, int *actual);
static void clear_clients(Client *clients, int actual);
static int is_request(const char *buffer);
static void extract_request(char *buffer);
static void execute_command(Client client, Client *clients, int actual, char *buffer, int numClient);
static void get_connected_list(Client client, Client *clients, int actual);
static void create_mp_discussion(char *name, char *member1, char *member2, char *password);
static void create_group_discussion(int priv, char *name, char *member1, char **members, int nbMembers, char *password);
static bool check_occurence_discussion_name(char *discussionName);
static int get_id_discussion(char *discussionName);
static void get_list_discussions(Client client);
static Client change_discussion(Client client, unsigned int discussionId);
static bool check_password(unsigned int discussionId, char *password);
static bool check_private_discussion(char *discussionName);

static void init_disc();

static void update_discId(int newValue);
static void add_mp(SimpleDisc disc);
static void add_group(GroupDisc disc);
static void load_group_disc(unsigned int id, char *name, int priv, int nbMembers, char **members, char * password);
static void load_mp_disc(unsigned int id, char *name, char *member1, char *member2, char * password);

#endif /* guard */
