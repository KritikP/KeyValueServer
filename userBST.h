#include "strBST.h"

typedef struct user_node{
    char* user;
    char* pass;
    BST* tree;
    struct user_node* leftChild;
    struct user_node* rightChild;
} user_node;

typedef struct userBST{
	pthread_mutex_t lock;
    int totalCount;
    struct user_node* root;
} userBST;

user_node* newUserBSTNode(char* user, char* pass);
user_node* insertUserHelper(user_node* root, char* user, char* pass);
void insertUser(userBST* tree, char* user, char* pass);
userBST* newuserBST();
void printUserTreeHelper(user_node* root);
void printUserTree(userBST* tree);
user_node* findUserHelper(user_node* root, char* user);
user_node* findUser(userBST* tree, char* user);
user_node* findUserMin(user_node* root);
user_node* deleteUserHelper(user_node* root, char* user, userBST* tree);
void deleteUser(userBST* tree, char* user);
void freeuserBSTHelper(user_node* root);
void freeuserBST(userBST* tree);