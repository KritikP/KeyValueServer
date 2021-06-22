typedef struct node{
    char* key;
    char* value;
    struct node* leftChild;
    struct node* rightChild;
} node;

typedef struct BST{
	pthread_mutex_t lock;
    int totalCount;
    struct node* root;
} BST;

node* newBSTNode(char* key, char* value);
node* insertHelper(node* root, char* key, char* value);
void insert(BST* tree, char* key, char* value);
BST* newBST();
void printTreeHelper(node* root);
void printTree(BST* tree);
node* findValueHelper(node* root, char* key);
node* findValue(BST* tree, char* key);
node* findMin(node* root);
node* deleteValueHelper(node* root, char* key, BST* tree);
void deleteValue(BST* tree, char* key);
void freeBSTHelper(node* root);
void freeBST(BST* tree);