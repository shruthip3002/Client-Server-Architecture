#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <pthread.h>

#define MAX_NODES 100
#define RESULT_TYPE 8

#define SECONDARY_SERVER_1_MSG_TYPE 4
#define CLEANUP_MSG_TYPE 6 

#define MAX_MSG_SIZE 256
#define MSG_KEY 1234
#define SHM_KEY 5678

struct msg_buffer
{
    long msg_type;
    char msg_text[MAX_MSG_SIZE];
};

long count = 1;

// Structure to pass arguments to the thread function
struct ThreadArgs
{
    int startNode;
    int n;
    int adjacencyMatrix[MAX_NODES][MAX_NODES];
};

// Structure to hold results for DFS
struct DFSResult
{
    long res_type;
    int vertices[MAX_NODES];
};

// Function to perform Breadth-First Search in a thread
void *BFS(void *args)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int startNode = threadArgs->startNode-1;
    int n = threadArgs->n;
    int adjacencyMatrix[MAX_NODES][MAX_NODES];

    // Copy the adjacency matrix to the local thread data
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            adjacencyMatrix[i][j] = threadArgs->adjacencyMatrix[i][j];
        }
    }

    // Queue for BFS
    int queue[MAX_NODES];
    int front = -1, rear = -1;
    bool visited[MAX_NODES];

    // Initialize visited array to false
    for (int i = 0; i < n; i++)
    {
        visited[i] = false;
    }

    // Enqueue the startNode and mark it as visited
    queue[++rear] = startNode;
    visited[startNode] = true;

    while (front != rear)
    {
        // Dequeue a vertex and add it to the result
        int currentVertex = queue[++front];
        struct DFSResult res;
        res.vertices[count++] = currentVertex;
        printf("%d ", currentVertex+1);

        // Enqueue adjacent vertices of the dequeued vertex that are not visited
        for (int i = 0; i < n; i++)
        {
            if (adjacencyMatrix[currentVertex][i] == 1 && !visited[i])
            {
                queue[++rear] = i;
                visited[i] = true;
            }
        }
    }

    printf("\n");

    // Exit the thread
    pthread_exit(NULL);
}

// Recursive function for Depth-First Search
void DFSRecursive(int currentVertex, int n, int adjacencyMatrix[][MAX_NODES], bool visited[], struct DFSResult *result)
{
    // Mark the current vertex as visited
    visited[currentVertex] = true;

    // Recursive call for all the adjacent vertices
    for (int i = 0; i < n; i++)
    {
        if (adjacencyMatrix[currentVertex][i] == 1 && !visited[i])
        {
            DFSRecursive(i, n, adjacencyMatrix, visited, result);
        }
    }

    // Add the current vertex to the result
        int sum = 0;
        for(int i=0;i<n;i++){
            if(adjacencyMatrix[currentVertex][i] == 1){
                sum++;
            }
        }
        if(sum == 1){
            result->vertices[count++] = currentVertex;
            
        }
}

// Function to perform Depth-First Search in a thread
void *DFS(void *args)
{
    struct ThreadArgs *threadArgs = (struct ThreadArgs *)args;
    int startNode = threadArgs->startNode-1;
    int n = threadArgs->n;
    int adjacencyMatrix[MAX_NODES][MAX_NODES];

    // Copy the adjacency matrix to the local thread data
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            adjacencyMatrix[i][j] = threadArgs->adjacencyMatrix[i][j];
        }
    }

    bool visited[MAX_NODES];

    // Initialize visited array to false
    for (int i = 0; i < n; i++)
    {
        visited[i] = false;
    }

    // Message structure to hold DFS result
    struct DFSResult dfsResult;                    
    // Call the recursive DFS function
    DFSRecursive(startNode, n, adjacencyMatrix, visited, &dfsResult);
 // Changed now
    key_t key;
    int msg_id;
    //struct msg_buffer message;

    key = ftok("/tmp", MSG_KEY);
    if(key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    msg_id = msgget(key, 0666);
    if (msg_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

     printf("DFS: ");
    // dfsResult.vertices[count] = '\0';
    dfsResult.vertices[0] = count-1;
     for (int i = count-1; i >= 1; i--) {
         printf("%d ", dfsResult.vertices[i]);
     }
    count = 1;
     
    dfsResult.res_type = RESULT_TYPE;
    
    if(msgsnd(msg_id, &dfsResult, sizeof(struct DFSResult), 0)==-1){
        printf("Error in msgsnd\n");
    }
    
    printf("Msg sent to client");


    printf("\n");

    // Exit the thread
    pthread_exit(NULL);
}

void *handleTraversal(void *arg)
{
    struct msg_buffer *request = (struct msg_buffer *)arg;

    int seq_no, op_no;
    char filename[10];
    sscanf(request->msg_text, "%d %d %s", &seq_no, &op_no, filename);

    key_t shmkey = ftok("/tmp", SHM_KEY);

    int shmid = shmget(shmkey, 0, 0);
    if (shmid == -1)
    {
        perror("shmget");
        exit(EXIT_FAILURE);
    }

    char *shared_memory = (char *)shmat(shmid, NULL, 0);
    if (shared_memory == (void *)-1)
    {
        perror("shmat");
        exit(EXIT_FAILURE);
    }

    char *ptr;
    int startNode = strtol(shared_memory, &ptr, 10);

    char path[100];
    sprintf(path, "graphs/%s", filename);
    FILE *file = fopen(path, "r");
    if (file == NULL)
    {
        perror("Error opening file");
        return EXIT_FAILURE;
    }

    int n;
    fscanf(file, "%d", &n);

    int adjacencyMatrix[MAX_NODES][MAX_NODES];

    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            fscanf(file, "%d", &adjacencyMatrix[i][j]);
        }
    }

    fclose(file);

    // Create thread arguments
    struct ThreadArgs threadArgs = {startNode, n};
    for (int i = 0; i < n; i++)
    {
        for (int j = 0; j < n; j++)
        {
            threadArgs.adjacencyMatrix[i][j] = adjacencyMatrix[i][j];
        }
    }

    // Create threads for BFS and DFS
    pthread_t bfsThread, dfsThread;

    if (op_no == 3)
    {
        printf("Secondary Server 1: Performing DFS, start node: %d\n", startNode);

        // Create DFS thread
        if (pthread_create(&dfsThread, NULL, DFS, (void *)&threadArgs) != 0)
        {
            perror("Error creating DFS thread");
            return EXIT_FAILURE;
        }

        // Wait for DFS thread to finish
        if (pthread_join(dfsThread, NULL) != 0)
        {
            perror("Error joining DFS thread");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    else if (op_no == 4)
    {
        printf("Secondary Server 1: Performing BFS, start node: %d\n", startNode);

        // Create BFS thread
        if (pthread_create(&bfsThread, NULL, BFS, (void *)&threadArgs) != 0)
        {
            perror("Error creating BFS thread");
            return EXIT_FAILURE;
        }

        // Wait for BFS thread to finish
        if (pthread_join(bfsThread, NULL) != 0)
        {
            perror("Error joining BFS thread");
            return EXIT_FAILURE;
        }
        return EXIT_SUCCESS;
    }
    else
    {
        printf("Secondary Server 1: Unknown Operation: %d\n", op_no);
    }

    if (shmdt(shared_memory) == -1)
    {
        perror("shmdt");
        exit(EXIT_FAILURE);
    }
    free(request);
    pthread_exit(NULL);
}

void cleanupHandler()
{
    
    printf("Secondary Server 1: Cleanup initiated. Performing cleanup actions...\n");

    // Exit the secondary server
    printf("Secondary Server 1: Exiting\n");
    exit(EXIT_SUCCESS);
}

int main()
{
    key_t key;
    int msg_id;
    struct msg_buffer message;

    key = ftok("/tmp", MSG_KEY);
    if (key == -1)
    {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    msg_id = msgget(key, 0666);
    if (msg_id == -1)
    {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    printf("Secondary Server 1: Connected to Message Queue with key %d\n", key);

    count = 1;
    while (1)
    {
        if (msgrcv(msg_id, &message, sizeof(message.msg_text), SECONDARY_SERVER_1_MSG_TYPE, 0) == -1)
        {
            perror("msgrcv");
            exit(EXIT_FAILURE);
        }

        printf("Secondary Server 1: Received message: %s\n", message.msg_text);

        // Check for cleanup request
        if (message.msg_type == CLEANUP_MSG_TYPE)
        {
            cleanupHandler();
        }

        pthread_t tid;
        struct msg_buffer *request = malloc(sizeof(struct msg_buffer));
        if (request == NULL)
        {
            perror("malloc");
            exit(EXIT_FAILURE);
        }

        memcpy(request, &message, sizeof(struct msg_buffer));
        if (pthread_create(&tid, NULL, handleTraversal, (void *)request))
        {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
        count = 1;

        pthread_detach(tid);
    }

    printf("Secondary Server 1: Exiting\n");
    return 0;
}

