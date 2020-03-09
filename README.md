# Yari

<span style="color:darkred;">Yari</span> is a multi threaded in-memory key value store. It can scale based on the amount of parallelism required for the server. It is a super fast engine to serve the requests and doesn't require sharding within a single host as required by popular redis kind of key-value stores (hardware threads are real). 

###Build & Installation

- Clone Yari from github or download the zip. 

- Build instructions 

```
cd Yari
gmake;

# Yari server will be built and available in Yari/bin

# Start the server
cd bin
./yari_server

# It starts with 4 threads in parallel. Can be adjusted if required
```

###Client

- Yari has a simple client **yari_client** bundled together. 

    Invoke the same to try set and get. 

    ```
    cd Yari/bin
    ./yari_client
    yari > set key1 test1
    OK
    yari > set key2 simple_test2
    OK
    yari > get key1
    test1
    yari > get key2
    simple_test2
    ```

### Test

Yari includes a simple bench tests. 

```
cd Yari/bench;
export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:`pwd`/../lib
./kvbench -t yari
# operations     = 10000
# client threads = 4
test type = yari
yari : SET : time taken = 12046384         : avg/opn = 301.16
yari : GET : time taken = 15326872         : avg/opn = 383.17
```

Same command can be executed without -t option ( or with -t redis) with redis server running in the same host. 

