#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>

#define BUFFER_SIZE 40000000
#define SMALL_BUFFER_SIZE 500000
#define NUM_CHILDREN 5


struct CommunicationContext {
    int readPipe;
    int writePipe;
};

struct TwoWayCommunicationLine {
    struct CommunicationContext mainToChild;
    struct CommunicationContext childToMain;
    int client_id;
};

double weights[BUFFER_SIZE] = {0.0};
pthread_mutex_t lock; 


int convertStringToArray(char *string_numbers, double* fun_weights) {
    char *token = strtok(string_numbers, ",");
    int i = 0;
    while (token != NULL) {
        fun_weights[i++] = atof(token);
        token = strtok(NULL, ",");
    }
    return i;
}

void convertArrayToString(char *answer, double* new_weight, int weight_size) {
    char s = ',';
    for(int i = 0; i < weight_size; i++) {
        int len = snprintf(NULL, 0, "%f", new_weight[i]);
        char *result = malloc(len + 1);
        snprintf(result, len + 1, "%f", new_weight[i]);
        if (i == 0)
            strcpy(answer, result);
        else
            strncat(answer, result, len);
        if (i != weight_size - 1)    
            strncat(answer, &s, 1);
        free(result);
    }
    s = '\n';
    strncat(answer, &s, 1);
}


void initializeForChildCommunication(struct TwoWayCommunicationLine *line, int pipe_fd_main_to_child[2],
 int pipe_fd_child_to_main[2], int client_id) {
    line->mainToChild.readPipe = pipe_fd_main_to_child[0];
    line->mainToChild.writePipe = pipe_fd_main_to_child[1];
    line->childToMain.readPipe = pipe_fd_child_to_main[0];
    line->childToMain.writePipe = pipe_fd_child_to_main[1];

    close(line->mainToChild.writePipe);
    close(line->childToMain.readPipe);
    char readPipe[100];
    sprintf(readPipe, "%d", line->mainToChild.readPipe);
    char writePipe[100];
    sprintf(writePipe, "%d",  line->childToMain.writePipe);
    char client_id_str[100];
    sprintf(client_id_str, "%d", client_id);
    char *args[]={"python3","./child.py", readPipe, writePipe ,client_id_str ,NULL};
    int status = execvp(args[0],args);
    if (status == -1) {
        perror("Exec failed");
        exit(EXIT_FAILURE);
    }
}
void initializeForMainCommunication(struct TwoWayCommunicationLine *line, int pipe_fd_main_to_child[2], int pipe_fd_child_to_main[2]) {
    line->mainToChild.readPipe = pipe_fd_main_to_child[0];
    line->mainToChild.writePipe = pipe_fd_main_to_child[1];
    line->childToMain.readPipe = pipe_fd_child_to_main[0];
    line->childToMain.writePipe = pipe_fd_child_to_main[1];

    close(line->mainToChild.readPipe);
    close(line->childToMain.writePipe);
}

void sendMainMessage(struct TwoWayCommunicationLine *line, const char *message) {
    // Write a message to the pipe
    ssize_t bytes_written = write(line->mainToChild.writePipe, message, strlen(message));
    if (bytes_written == -1) {
        perror("Write to pipe failed");
        exit(EXIT_FAILURE);
    }
}
void listen(struct TwoWayCommunicationLine *line) {

    printf("thread started to listen from client %d\n", line->client_id);

    char *big_buffer = (char*) malloc(BUFFER_SIZE);
    double* new_weights = (double*) malloc(BUFFER_SIZE);
    char *buffer = (char*) malloc(SMALL_BUFFER_SIZE);
    ssize_t bytes_read;
    long long int total_bytes_read = 0;
    int w_size = 0;
        
    while(1){
        int message_length = 0;
        int number_of_rounds = 0;
        total_bytes_read = 0;
        big_buffer[0] = '\0';
        while((bytes_read = read(line->childToMain.readPipe, buffer, SMALL_BUFFER_SIZE)) > 0) {
            if (buffer[bytes_read-1] == '\n') {
                strncat(big_buffer, buffer, bytes_read - 1);
                total_bytes_read += (bytes_read - 1);
                break;
            }
            strncat(big_buffer, buffer, bytes_read);
            total_bytes_read += bytes_read;            
        }
        if (total_bytes_read > 0) {
            big_buffer[total_bytes_read] = '\0';  // Null-terminate the string
            printf("%d bytes received from client %d\n", total_bytes_read, line->client_id);
            pthread_mutex_lock(&lock);
            w_size = convertStringToArray(big_buffer,new_weights);
            printf("%d weights received from client %d\n", w_size, line->client_id);
            for(int i=0; i< w_size; i++){
                weights[i] = (0.5 * new_weights[i] + 0.5 * weights[i]);
            }
            convertArrayToString(big_buffer , weights, w_size);
            pthread_mutex_unlock(&lock);
            sendMainMessage(line, big_buffer);
        }
    }
}

int main() {

    int pipe_fd_main_to_child[NUM_CHILDREN][2];  // File descriptors for main process to communicate with children
    int pipe_fd_child_to_main[NUM_CHILDREN][2];  // File descriptors for children to communicate with main process

    pid_t child_pid[NUM_CHILDREN];
    struct CommunicationContext MainToChildren[NUM_CHILDREN];
    struct CommunicationContext ChildrenToMain[NUM_CHILDREN];
    struct TwoWayCommunicationLine lines[NUM_CHILDREN];
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        lines[i].mainToChild = MainToChildren[i];
        lines[i].childToMain = ChildrenToMain[i];
        lines[i].client_id = i;
    }
    // intilize the lock
    pthread_mutex_init(&lock, NULL);

    // Create pipes and child processes
    for (int i = 0; i < NUM_CHILDREN; ++i) {
        if (pipe(pipe_fd_main_to_child[i]) == -1) {
            perror("Pipe creation failed");
            exit(EXIT_FAILURE);
        }
        if (pipe(pipe_fd_child_to_main[i]) == -1) {
            perror("Pipe creation failed");
            exit(EXIT_FAILURE);
        }

        if ((child_pid[i] = fork()) == -1) {
            perror("Fork failed");
            exit(EXIT_FAILURE);
        }


        if (child_pid[i] == 0) {  // Child process
            initializeForChildCommunication(&(lines[i]), pipe_fd_main_to_child[i], pipe_fd_child_to_main[i],i);
            break;  // Child process does not need to create more children
        } else {  // Main process
            initializeForMainCommunication(&(lines[i]), pipe_fd_main_to_child[i], pipe_fd_child_to_main[i]);
        }
    }
    pthread_t threads[NUM_CHILDREN];
    if (child_pid[0] != 0) {  // Main process
        // Send messages to the child processes
        for (int i = 0; i < NUM_CHILDREN; ++i) {
            char writePipe[100];
            sprintf(writePipe, "%d\n",  lines[i].childToMain.writePipe);
            sendMainMessage(&(lines[i]), writePipe);
        }

        // create a thread for each child to read from the pipe 
        for (int i = 0; i < NUM_CHILDREN; ++i) {
            pthread_create(&threads[i], NULL, listen, &(lines[i]));
        }

        for (int i = 0; i < NUM_CHILDREN; ++i) {
            pthread_join(threads[i], NULL);
        }
    }

    return 0;
}
