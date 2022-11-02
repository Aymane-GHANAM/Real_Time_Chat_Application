# Définition des variables du Makefile

# Compilateur
CC = gcc
# Flags de compilation
CFLAGS = -g
# Répertoires utiles
SERV = Serveur
CLI = Client

# Récupération des fichiers sources pour le serveur
SRCS_SERV = $(wildcard $(SERV)/*.c)
# Récupération des fichiers sources pour le client
SRCS_CLI = $(wildcard $(CLI)/*.c)

# Création des .o à partir des .c
OBJS_SERV = $(patsubst $(SERV)/%.c, $(SERV)/%.o, $(SRCS_SERV))
OBJS_CLI = $(patsubst $(CLI)/%.c, $(CLI)/%.o, $(SRCS_CLI))

# Noms des exécutables
SERV_EXE = server
CLI_EXE = client

SERV_BIN = $(SERV)/$(SERV_EXE)
CLI_BIN = $(CLI)/$(CLI_EXE)


# Création des exécutables
all: $(SERV_BIN)
all: $(CLI_BIN)
all: clean

# Edition des liens
$(SERV_BIN): $(OBJS_SERV)
	$(CC) $(CFLAGS) $(OBJS_SERV) -o $@
$(CLI_BIN): $(OBJS_CLI)
	$(CC) $(CFLAGS) $(OBJS_CLI) -o $@

# Assemblage
$(SERV)/%.o: $(SERV)/%.c
	$(CC) $(CFLAGS) -c $< -o $@
$(CLI)/%.o: $(CLI)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage
clean:
	rm $(SERV)/*.o
	rm $(CLI)/*.o
