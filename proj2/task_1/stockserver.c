#include "csapp.h"

//data structure
typedef struct                   // Represents a pool of connected descriptors
{
    int maxfd;                   // Largest descriptor in read_set
    fd_set read_set;             // Set of all active descriptors
    fd_set ready_set;            // Subset of descriptors ready for reading
    int nready;                  // Number of ready descriptors from select

    int maxi;                    // High water index into client array
    int clientfd[FD_SETSIZE];    // Set of active descriptors
    rio_t clientrio[FD_SETSIZE]; // Set of active read buffers
}pool;

typedef struct _item             // Contain Information of stock
{
    int ID;                      
    int left_stock;             
    int price;                   
    struct _item *left, *right;  // item 's child left, right pointer
}item;

typedef struct cmdInput           // info of command
{
    char cmd[6];                 // Command
    long ID;
    long num;
}cmdInput;


/* --------- Function Prototyes ---------- */

/*   Event-based server management   */
void init_pool(int listenfd, pool *p);
void add_client(int connfd, pool *p);
void check_clients(pool *p);

/*   Data Structure management   */
item* find_ID(int ID);
int read_data();
void writeData(item *ptr, char *str);
int insert(int ID, int left_stock, int price);
void free_mem(item *ptr);

/*   Installed SIGINT handler   */
void sig_int_handler(int sig);



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
    static pool pool;
    char client_hostname[MAXLINE], client_port[MAXLINE];

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
    listenfd = Open_listenfd(argv[1]);

    init_pool(listenfd, &pool);
    while (1) 
    {
        // Wait for listening/connected descriptor(s) to become ready
        pool.ready_set = pool.read_set;
        pool.nready = Select(pool.maxfd+1, &pool.ready_set, NULL, NULL, NULL);

        // If listening descriptor ready, add new client to pool
        if (FD_ISSET(listenfd, &pool.ready_set))
        {
            clientlen = sizeof(struct sockaddr_storage);
            connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
            Getnameinfo((SA *) &clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
            add_client(connfd, &pool);
            printf("\033[0;32mConnected\033[0m to (%s, %s)\n", client_hostname, client_port);
        }
        
        check_clients(&pool);
    }
}
/* $end echoserverimain */

void init_pool(int listenfd, pool *p)
{
    // Initially, there are no connected desriptors
    int i;
    p -> maxi = -1;
    for (i = 0; i < FD_SETSIZE; i++)
        p -> clientfd[i] = -1;

    // Initially, listenfd is only member of select read set
    p->maxfd = listenfd;
    FD_ZERO(&p -> read_set);
    FD_SET(listenfd, &p -> read_set);
}

void add_client(int connfd, pool *p)
{
    int i;
    p -> nready--;
    for (i = 0; i < FD_SETSIZE; i++) // Find an available slot
    {
        if (p -> clientfd[i] < 0)
        {
            // Add connected descriptor to the pool
            p -> clientfd[i] = connfd;
            Rio_readinitb(&p -> clientrio[i], connfd);

            // Add the descriptor to the descriptor set
            FD_SET(connfd, &p -> read_set);

            // Update max descriptor and pool high water mark
            if (connfd > p -> maxfd)
                p -> maxfd = connfd;
            if (i > p -> maxi)
                p -> maxi = i;
            break;
        }
    }
    if (i == FD_SETSIZE)
        app_error("add_client error: Too many clients");

    return;
}

void check_clients(pool *p)
{
    int i, connfd, n;
    char buf[MAXLINE];
    cmdInput argv;
    rio_t rio;
    item* ptr;

    for (i = 0; (i <= p->maxi) && (p -> nready > 0); i++)
    {
        connfd = p -> clientfd[i];
        rio = p -> clientrio[i];

        // If the descriptor is ready, echo a text line from it
        if ((connfd > 0) && (FD_ISSET(connfd, &p -> ready_set)))
        {
            p -> nready--;
            if ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0)
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
                    if(ptr -> left_stock < argv.num)
                        Rio_writen(connfd,"Not enough left stock\n", MAXLINE);
                    else{
                        ptr -> left_stock = ptr->left_stock - argv.num;
                        Rio_writen(connfd, "[buy] \033[0;32msuccess\033[0m\n", MAXLINE);
                    }
                }
                else if(!strcmp("exit",argv.cmd))
                {
                    Close(connfd);
                    FD_CLR(connfd, &p -> read_set);
                    p -> clientfd[i] = -1;
                }
                else
                {
                    ptr = find_ID(argv.ID);
                    if (!ptr)
                        unix_error("Stock not in list");
                    ptr -> left_stock = ptr ->left_stock + argv.num;
                    Rio_writen(connfd, "[sell] \033[0;32msuccess\033[0m\n", MAXLINE);
                }
                
            }
            // EOF => remove descriptor
            else
            {
                Close(connfd);
                FD_CLR(connfd, &p -> read_set);
                p -> clientfd[i] = -1;
            }
        }
    }
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
            sprintf(tmpline, "%d %d %d\n", ptr->ID, ptr->left_stock, ptr->price);
            strcat(str, tmpline);
            
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