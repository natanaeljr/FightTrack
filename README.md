# FightTrack

## Build

~~~sh
mkdir build
cd build
cmake ..
make
~~~

## Run

Server:

~~~sh
./fight-track server 9124
~~~

Client1:

~~~sh
./fight-track client "127.0.0.1:9124" player1 > log1 2>&1; cat log1
~~~


Client2:

~~~sh
./fight-track client "127.0.0.1:9124" player2 > log2 2>&1; cat log2
~~~

