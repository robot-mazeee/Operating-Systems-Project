<h1>Part 1</h1>

The first part of this project consists of implementing an interactive Hash Table called IST-KVS, which stores key-value pairs. To do so, the program reads a directory where each file (with extension .job) has a sequence of commands, such as writting, reading and deleting keys from IST-KVS, as well as perfomimg backups on the current state of the table. In order to do this efficiently, mechanisms like multi-threading and concurrency are implemented. This project taught me how to manage file systems, work with locks, synchronize multiple processes and threads, and much more.

This repository provides two sets of automatic tests for part 1, which can be ran with:

cd Part1
<br/>
make
<br/>
bash ./tests-public/run_ex1.sh kvs
<br/>
bash ./tests-public/run_ex2.sh kvs

<h1>Part 2</h1>

For part 2, the goal is to make IST-KVS accessible to clients. Now, the application developed in part 1 behaves as a server and, added to the previous functionality, also handles clients' requests. Each client can subscribe to keys to monitor changes in the table, as well as unsubscribing and disconnecting from the server. This mechanism is implemented using named pipes. The server has a single pipe that receives connection requests from clients. This pipe is operated by the host thread. When a new client is connected, a worker thread is activated to handle requests from that client until they decide to disconnect, or disconnect unexpectedly. Each client has three named pipes: one for sending requests, one for receiving server responses, and one for receiving notifications about subscribed keys. The last one is handled by a separate notifications thread. Furthermore, a SIGUSR1 signal can be received at any point; this signal will be handled by the host thread and disconnect all clients without killing the server.

The server can be launched with the following commands: 

cd Part2
<br/>
make
<br/>
./src/server/kvs directory_path max_threads max_backups server_named_pipe
<br/>
(Example: ./src/server/kvs ./src/server/jobs 3 3 /tmp/server)

<h6>directory_path</h6> - path of the directory
<br/>
<h6>max_threads</h6> - maximum number of threads that can be active, reading .job files
<br/>
<h6>max_backups</h6> - maximum number of backups that can be performed simultaneously
<br/>
<h6>server_named_pipe</h6> - name of the named pipe operated by the server
<br/>
<br/>

A client can be launched with the following command:

./src/client/client client_id server_named_pipe
<br/>
(Example: ./src/client/client 1 /tmp/server)

<h6>client_id</h6> - unique client identifier
<br/>
<h6>server_named_pipe</h6> - name of the named pipe operated by the server
<br>
<br/>
Once connected, you can use the following commands:
<br/>
<br/>
SUBSCRIBE [key]
<br/>
UNSUBSCRIBE [key]
<br/>
DISCONNECT
<br/>
<br/>
Example:
<br/>
<br/>
SUBSCRIBE [a]
<br/>
SUBSCRIBE [b]
<br/>
UNSUBSCRIBE [b]
<br/>
DISCONNECT
