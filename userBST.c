#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include "userBST.h"

user_node* newUserBSTNode(char* user, char* pass){
    user_node* newNode;
    newNode = malloc(sizeof(user_node));
    char* tempuser = malloc(strlen(user) + 1);
    char* tempVal = malloc(strlen(pass) + 1);
    strcpy(tempuser, user);
    strcpy(tempVal, pass);
    newNode->user = tempuser;
    newNode->pass = tempVal;
    newNode->tree = newBST();
    newNode->leftChild = NULL;
    newNode->rightChild = NULL;
    return newNode;
}

user_node* insertUserHelper(user_node* root, char* user, char* pass){
    if(root == NULL){
        return newUserBSTNode(user, pass);
    }
    else if(strcmp(user, root->user) == 0){     //If the pass already exists, just increase the count by one
        printf("user already exists\n");
    }
    else if(strcmp(user, root->user) < 0){
        root->leftChild = insertUserHelper(root->leftChild, user, pass);
    }
    else{
        root->rightChild = insertUserHelper(root->rightChild, user, pass);
    }
    return root;
}

void insertUser(userBST* tree, char* user, char* pass){
    pthread_mutex_lock(&tree->lock);
    tree->root = insertUserHelper(tree->root, user, pass);
    tree->totalCount++;
    pthread_mutex_unlock(&tree->lock);
}

userBST* newuserBST(){
    userBST* tree = malloc(sizeof(userBST));
    tree->root = NULL;
    tree->totalCount = 0;
    pthread_mutex_init(&tree->lock, NULL);
    return tree;
}

void printUserTreeHelper(user_node* root){
    if(root != NULL){
        printUserTreeHelper(root->leftChild);
        printf("user: %s\tpass: %s\n", root->user, root->pass);
        printUserTreeHelper(root->rightChild);
    }
}

void printUserTree(userBST* tree){
    printUserTreeHelper(tree->root);
}

user_node* findUserHelper(user_node* root, char* user){
    if(root == NULL || strcmp(user, root->user) == 0)
        return root;
    else if(strcmp(user, root->user) < 0)                   //If the search pass is less than the current node, continue search left
        findUserHelper(root->leftChild, user);
    else                                //If the search pass is greater than the current node, continue search right
        findUserHelper(root->rightChild, user);
}

user_node* findUser(userBST* tree, char* user){
    return findUserHelper(tree->root, user);
}

user_node* findUserMin(user_node* root){
    if(root == NULL)
        return NULL;
    else if(root->leftChild != NULL)
        return findUserMin(root->leftChild);
    return root;
}

user_node* deleteUserHelper(user_node* root, char* user, userBST* tree){
    if(root == NULL){
        printf("user not found\n");
        return NULL;
    }
    else if(strcmp(user, root->user) < 0)                             //If the search pass is less than the current node, continue search left
        root->leftChild = deleteUserHelper(root->leftChild, user, tree);
    else if(strcmp(user, root->user) > 0)                               //If the search pass is greater than the current node, continue search right
        root->rightChild = deleteUserHelper(root->rightChild, user, tree);
    else{           //If the item is found
        if(root->leftChild == NULL && root->rightChild == NULL){
            free(root->user);
            free(root->pass);
            free(root);
            tree->totalCount--;
            return NULL;
        }

        else if(root->leftChild == NULL || root->rightChild == NULL){
            user_node* newParent;
            if(root->leftChild == NULL){
                newParent = root->rightChild;
            }
            else{
                newParent = root->leftChild;
            }
            free(root->user);
            free(root->pass);
            free(root);
            tree->totalCount--;
            return newParent;
        }
        else{
            user_node* newParent;
            newParent = findUserMin(root->rightChild);
            free(root->user);
            free(root->pass);

            char* tempuser = malloc(strlen(newParent->user) + 1);
            char* tempVal = malloc(strlen(newParent->pass) + 1);
            strcpy(tempuser, newParent->user);
            strcpy(tempVal, newParent->pass);

            root->user = tempuser;
            root->pass = tempVal;
            root->rightChild = deleteUserHelper(root->rightChild, newParent->user, tree);
        }
    }
    return root;
}

void deleteUser(userBST* tree, char* user){
    pthread_mutex_lock(&tree->lock);
    tree->root = deleteUserHelper(tree->root, user, tree);
    pthread_mutex_unlock(&tree->lock);
}

void freeuserBSTHelper(user_node* root){
    if(root != NULL){
        freeBST(root->tree);
        freeuserBSTHelper(root->rightChild);
        freeuserBSTHelper(root->leftChild);
        free(root->user);
        free(root->pass);
        free(root);
    }
}

void freeuserBST(userBST* tree){
    freeuserBSTHelper(tree->root);
    pthread_mutex_destroy(&tree->lock);
    free(tree);
}