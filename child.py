import os
import sys
import tensorflow as tf
import time
import numpy as np
from tensorflow.keras.models import Sequential
from tensorflow.keras.layers import Dense
from tensorflow.keras.utils import to_categorical

fd_read = int(sys.argv[1])
fd_write = int(sys.argv[2])
client_id = int(sys.argv[3])
print(f"client id {client_id}")


fd_file_read = os.fdopen(fd_read, 'r') 
fd_file_write = os.fdopen(fd_write, 'w')

print(fd_file_read.readline() + ' from child ' + str(os.getpid()) + "with fd_write: " + str(fd_write))



fashion_mnist = tf.keras.datasets.fashion_mnist

(train_images, train_labels), (test_images, test_labels) = fashion_mnist.load_data()

class_names = ['T-shirt/top', 'Trouser', 'Pullover', 'Dress', 'Coat',
               'Sandal', 'Shirt', 'Sneaker', 'Bag', 'Ankle boot']

train_images = train_images / 255.0

test_images = test_images / 255.0

model = tf.keras.Sequential([
    tf.keras.layers.Flatten(input_shape=(28, 28)),
    tf.keras.layers.Dense(128, activation='relu'),
    tf.keras.layers.Dense(10)
])


# lets split data by its lables , each label will be for a client


def flatten(old_weights):
    weights = []
    for i in old_weights:
        weights += [str(j) for j in i.flatten().tolist()]
    return weights


weights = model.get_weights()

# Initialize the global model's weights

global_weights = model.get_weights()
num_epochs = 5

# Federated Learning Loop
client_model = None

shapes = []

# split the data to clients
num_clients = 5

batch_size = train_images.shape[0] // num_clients

for epoch in range(num_epochs):
    print(f"\nEpoch {epoch + 1}/{num_epochs} from client {client_id}")

    # Distribute global model's weights to all clients
    # Simulate data distribution to clients (replace with actual data)
    client_data = train_images[client_id * batch_size : (client_id + 1) * batch_size]
    client_labels = train_labels[client_id * batch_size : (client_id + 1) * batch_size]

    # Update client's model with global weights
    client_model = tf.keras.models.clone_model(model)

    client_model.compile(optimizer='adam',
          loss=tf.keras.losses.SparseCategoricalCrossentropy(from_logits=True),
          metrics=['accuracy'])
    
    client_model.set_weights(global_weights)

    # Train the client's model on its local data
    client_model.fit(client_data, client_labels, epochs=1, verbose=0)

    # Get the updated weights from the client's model
    new_local_weights = client_model.get_weights()
    shapes = [i.shape for i in new_local_weights]

    weights = flatten(new_local_weights)
    print(f"sending weights from client {client_id} with size {len(weights)}")
    message = ",".join(weights) + "\n"
    print(f"sent message from client {client_id} with size {len(message)}")
    fd_file_write.write(message)


    new_global_message = fd_file_read.readline()
    print(f"new message recived by client {client_id} with size {len(new_global_message)}")
    new_global_weights = new_global_message.split(",")
    print(f"new weights size recived by client {client_id} with size {len(new_global_weights)}")
    new_global_weights = list(map(float, new_global_weights))

    # convert the weights back to the original shape
    layer_weights_sizes = [i[0] * i[1] if len(i)>1 else i[0] for i in shapes]
    new_weights_seprated_by_layer = []
    start = 0
    for i in layer_weights_sizes:
        new_weights_seprated_by_layer.append(new_global_weights[start:start + i])
        start += i
    
    global_weights = [np.array(new_weights_seprated_by_layer[i]).reshape(shapes[i]) for i in range(len(new_weights_seprated_by_layer))]

    test_loss, test_acc = client_model.evaluate(test_images,  test_labels, verbose=2)
    print('\nTest accuracy:', test_acc)

          
test_loss, test_acc = client_model.evaluate(test_images,  test_labels, verbose=2)
print(' final \nTest accuracy:', test_acc)




