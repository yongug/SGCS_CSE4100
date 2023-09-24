#include "csapp.h"


#define NTHREADS 4
#define SBUFSIZE 16


//data structure
typedef struct _item             // Contain Information of stock
{
    int ID;                      
    int left_stock;             
    int price;
    int readcnt;
    sem_t w;
    sem_t mutex;                   
    struct _item *left, *right;  // item 's child left, right pointer
}item;

//Buffer of clients
typedef struct                  
{
    int *buf;
    int n;
    int front;
    int rear;
    sem_t mutex;
    sem_t slots;
    sem_t items;
}sbuf_t;

typedef struct cmdInput           // info of command
{
    char cmd[6];                 // Command
    long ID;
    long num;
}cmdInput;


/* --------- Function Prototyes ---------- */

/*   Data Structure management   */
item* find_ID(int ID);
int read_data();
void writeData(item *ptr, char *str);
int insert(int ID, int left_stock, int price);
void free_mem(item *ptr);

/*   Installed SIGINT handler   */
void sig_int_handler(int sig);

/* Thread functions */
void* thread(void *vargp);
void sbuf_init(sbuf_t *fp, int n);
void sbuf_deinit(sbuf_t *sp);
void sbuf_insert(sbuf_t *sp, int item);
int sbuf_remove(sbuf_t *sp);

sbuf_t sbuf; /* Shared buffer of connected descriptors */
//global variables

item* root = NULL;
FILE *fp;                        // File pointer for data I/O
/* --------------------------------------- */
/* --------------------------------------- */
/* --------------------------------------- */

/* $begin echoserverimain */
int main(int argc, char **argv) 
{
    int listenfd, connfd;
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;  /* Enough space for any address */
    char client_hostname[MAXLINE], client_port[MAXLINE];
    pthread_t tid;

    if (argc != 2) {
       fprintf(stderr, "usage: %s <port>\n", argv[0]);
       exit(0);
    }
    
    // Read data from stock.txt
    if(read_data() == -1)
    {
        unix_error("Reading data failed");
    }
        

    Signal(SIGINT, sig_int_handler);

    // Initialize shared buffer
    // create worker threads
    
    sbuf_init(&sbuf, SBUFSIZE);
    for (int i = 0; i < NTHREADS; i++)
        Pthread_create(&tid, NULL, thread, NULL);

    listenfd = Open_listenfd(argv[1]);

    while (1) 
    {
        clientlen = sizeof(struct sockaddr_storage);
        connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);
        Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
        printf("\033[0;32mConnected\033[0m to (%s, %s)\n", client_hostname, client_port);
        sbuf_insert(&sbuf, connfd);  
    }
    
}
/* $end echoserverimain */

// Worker thread function
void *thread(void *vargp)
{   
    int n;
    char buf[MAXLINE];
    rio_t rio;
    cmdInput argv;
    item* ptr;

    Pthread_detach(pthread_self());
    while (1)
    {
        // Get connfd
        int connfd = sbuf_remove(&sbuf);
        Rio_readinitb(&rio, connfd);
        while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) // Loops until it meets EOF 
        {

            argv.cmd[0] = '\0';
            argv.ID = 0;
            argv.num = 0;
            sscanf(buf,"%s%ld%ld",argv.cmd, &(argv.ID),&(argv.num));

            printf("Server received %d bytes\n", n);

            if(!strcmp("show",argv.cmd))
            {
                char templine[MAXLINE];
                templine[0] = '\0';
                writeData(root,templine);
                Rio_writen(connfd,templine,MAXLINE);
            }
            else if(!strcmp("buy",argv.cmd))
            {
                ptr = find_ID(argv.ID);
                if(!ptr)
                    unix_error("Stock is not in list");
                
                P(&ptr->w);
                P(&ptr->mutex);

                if(ptr -> left_stock < argv.num)
                    Rio_writen(connfd,"Not enough left stock\n", MAXLINE);
                else{
                    ptr -> left_stock = ptr->left_stock - argv.num;
                    Rio_writen(connfd, "[buy] \033[0;32msuccess\033[0m\n", MAXLINE);
                }
                V(&ptr->mutex);
                V(&ptr->w);
                
            }
            else if(!strcmp("exit",argv.cmd))
            {
                break;
            }
            else
            {
                ptr = find_ID(argv.ID);
                if (!ptr)
                    unix_error("Stock not in list");

                P(&ptr->w);
                P(&ptr->mutex);

                ptr -> left_stock = ptr ->left_stock + argv.num;
                Rio_writen(connfd, "[sell] \033[0;32msuccess\033[0m\n", MAXLINE);

                V(&ptr->mutex);
                V(&ptr->w);
            }


        }
        Close(connfd); // Client disconnects to server
    }
}

//여기서부터 다시
void sbuf_init(sbuf_t *sp, int n)
{
    sp -> buf = Calloc(n, sizeof(int));
    sp -> n = n;
    sp -> front = sp -> rear = 0;
    Sem_init(&sp -> mutex, 0, 1);
    Sem_init(&sp -> slots, 0, n);
    Sem_init(&sp -> items, 0, 0);
}
void subf_deinit(sbuf_t *sp)
{
    Free(sp -> buf);
}
void sbuf_insert(sbuf_t *sp, int item)
{
    P(&sp -> slots);
    P(&sp -> mutex);
    sp -> buf[(++sp -> rear) % (sp -> n)] = item;
    V(&sp -> mutex);
    V(&sp -> items);
}
int sbuf_remove(sbuf_t *sp)
{
    int item;
    P(&sp -> items);
    P(&sp -> mutex);
    item = sp -> buf[(++sp -> front) % (sp -> n)];
    V(&sp -> mutex);
    V(&sp -> slots);
    return item;
}

item* find_ID(int ID)
{
    item *ptr = root;

    //find_ ID using ptr left, right operation    
    for(;ptr;)
    {
        if (ptr -> ID == ID)
            return ptr;
        else if (ptr -> ID > ID)
            ptr = ptr -> left;
        else if(ptr -> ID < ID)
            ptr = ptr -> right;
    }

    return NULL;
}

int read_data()
{
    int ID, left_stock, price;
    int k;
    int scan;
    fp = fopen("stock.txt", "r");

    if (fp == NULL)
    {
        return -1; 
    }
    while(!feof(fp))
    {
        //For No parsing, use sscanf with buf 
        scan = fscanf(fp,"%d %d %d\n",&ID, &left_stock, &price);
        scan++;
        //printf("total scan number : %d", scan);
        
        k= insert(ID, left_stock, price);
        if(k==-1)
        {
            unix_error("create node error\n");
        }
    }
         

    //FP CLOSE
    fclose(fp);
    return 0;
}

// Create BST
int insert(int ID, int left_stock, int price)
{
    item *stock, *ptr;

    //new stock create
    stock = (item*)malloc(sizeof(item));
    stock -> left = NULL;
    stock -> right = NULL;
    stock -> ID = ID;
    stock -> left_stock = left_stock;
    stock -> price = price;
    stock -> readcnt = 0;
    
    Sem_init(&stock ->mutex,0,1);
    Sem_init(&stock ->w,0,1);

    

    // root == NULL => init value
    if (root == NULL)
    {
        root = stock;
        return 1;
    }

    //from root , tree traversing
    ptr = root;
    
    for(;ptr;)
    {
        if (ID > ptr -> ID)
        {
            if (ptr -> right == NULL)
            {
                ptr -> right = stock;
                return 1;
            }
            ptr = ptr -> right;
        }
        else
        {
            if (ptr -> left == NULL)
            {
                ptr -> left = stock;
                return 1;
            }
            ptr = ptr -> left;
        }
    }

    return 0;
    //return -1;
}

// Traverse the tree
// and print requested data
void writeData(item *ptr, char *str)
{
    if (!str)
    {
        // for recording the data
        if (ptr)
        {
            fprintf(fp, "%d %d %d\n", ptr->ID, ptr->left_stock, ptr->price);
            
            writeData(ptr -> right, str);
            writeData(ptr -> left, str);
        }
    }
    else
    {
        //for showing the data
        if(ptr)
        {
            char tmpline[MAXLINE];

            P(&ptr -> mutex);
            (ptr -> readcnt)++;
            if (ptr -> readcnt == 1)
            {
                P(&ptr -> w);
            }
            V(&ptr -> mutex);

            sprintf(tmpline, "%d %d %d\n", ptr->ID, ptr->left_stock, ptr->price);
            strcat(str, tmpline);

            P(&ptr -> mutex);
            (ptr -> readcnt)--;
            if (ptr -> readcnt == 0)
                V(&ptr -> w);
            V(&ptr -> mutex);

            writeData(ptr -> right, str);
            writeData(ptr -> left, str);
        }
    }
}



// If SIGINT signal is sent, write modified data to stock.txt
// and free memory
void sig_int_handler(int sig)
{
    FILE* fp3;

    fp3 = fopen("stock.txt", "w");
    writeData(root, NULL);
    fclose(fp3);

    //free the memory of root => for no memory leack 
    free_mem(root);

    printf("\n");
    exit(0);
}

// Traverse tree
// and free memory
void free_mem(item *ptr)
{
    if (ptr !=NULL)
    {
        free_mem(ptr -> right);
        free_mem(ptr -> left);
        free(ptr);
    }
}