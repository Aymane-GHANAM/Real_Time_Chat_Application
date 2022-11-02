#ifndef GROUP_H
#define GROUP_H

typedef struct
{
    unsigned int id;
    char *name;
    int priv;
    int nbMembers;
    char **members;
    char *password;
} GroupDisc;

typedef struct
{
    unsigned int id;
    char *name;
    char *member1;
    char *member2;
    char *password;
} SimpleDisc;

typedef struct Groups
{
    GroupDisc group;
    struct Groups *suivant;
} Groups;

typedef struct Mps
{
    SimpleDisc disc;
    struct Mps *suivant;
} Mps;

#endif