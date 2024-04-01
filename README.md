# Federated-Learning
In this project, we aim to implement an example of Federated Learning, which is a form of Distributed Learning Machine. The key difference is that instead of multiple systems, we have several processes, and instead of network communication, we use communication between processes. Each process has access to a portion of the data, and during each epoch, the main process sends the weights to other processes. These child processes complete one epoch of training and send the updated weights back to the main process. The main process aggregates these weights, typically by averaging them, and the process is repeated until the model weights converge.

## Main Process
The main process which is created by `main.c` file, is responsible for creating the child processes and communicating with them. Main process creates 2 `pipe`(which is 4 file descriptors) for each child process. One of the pipes is used for sending the aggregated weights to the child process and the other one is used for receiving the updated weights from the child process. A thread is created for each child process to listen to the pipe for receiving the updated weights and send back the aggregated weights.

## Child Processes
Each child process is responsible for loading the data which is assigned to it, training the model and sending the updated weights to the main process and receiving the aggregated weights from the main process. `child.c` file recieves 3 arguments from the main process. The first argument is read pipe file descriptor, the second argument is write pipe file descriptor and the third argument is the child number.

## Learning
We used `TensorFlow.keras` library for creating the model and training it. We used `fashion_mnist` dataset for training the model. The model is a simple neural network with 2 hidden layers. The model is trained for 5 epochs.

## Running the code
To run the code, you need to run the following commands:
```
chmod +x child.py
gcc main.c -o main
./main
```
## Notes
- You can change the number of child processes by changing the `NUM_CHILD` variable in the `main.c` file. be careful to also change the `num_clients` variable in the `child.py` file.
- You can change the number of epochs by changing the `num_epochs` variable in the `child.py` file.
- you can see part of learning logs in the `result.png` file.
- I thank [Nima Najafi]() for his help in implementing this project.

