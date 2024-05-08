#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>

#define CLEANUP_MSG_TYPE 6
#define MSG_KEY 1234

struct cleanup_msg {
    long msg_type;
    char msg_text[1];  
};

int main() {
    key_t key;
    int msg_id;
    struct cleanup_msg message;

    // Create a unique key for the cleanup message queue
    key = ftok("/tmp", MSG_KEY);
    if (key == -1) {
        perror("ftok");
        exit(EXIT_FAILURE);
    }

    // Get the cleanup message queue ID
    msg_id = msgget(key, IPC_CREAT | 0666);
    if (msg_id == -1) {
        perror("msgget");
        exit(EXIT_FAILURE);
    }

    printf("Cleanup Process: Connected to Cleanup Message Queue with key %d\n", key);

    while (1) {
        char response;
        printf("Cleanup Process: Want to terminate the application? Press Y (Yes) or N (No): ");
        scanf(" %c", &response);

        if (response == 'Y' || response == 'y') {
            // Send cleanup request to the load balancer
            message.msg_type = CLEANUP_MSG_TYPE;
            if (msgsnd(msg_id, &message, sizeof(message.msg_text), 0) == -1) {
                perror("msgsnd");
                exit(EXIT_FAILURE);
            }

            printf("Cleanup Process: Sent cleanup request to Load Balancer\n");

            // Cleanup and terminate
            if (msgctl(msg_id, IPC_RMID, NULL) == -1) {
                perror("msgctl");
                exit(EXIT_FAILURE);
            }

            printf("Cleanup Process: Cleanup Message Queue destroyed\n");
            printf("Cleanup Process: Exiting\n");

            exit(EXIT_SUCCESS);
        } else if (response == 'N' || response == 'n') {
            
            printf("Cleanup Process: Continuing...\n");
        } else {
            // Invalid input
            printf("Cleanup Process: Invalid input. Please enter Y or N.\n");
        }
    }

    return 0;
}

