# Votelly GUI


## Requirements

The general requirements for building the project are

- CMake 3.8 or newer

- Qt Kit 6.x or newer


## Build from the Linux command line

To build the project from the command line, first construct a `build` folder within the project's folder

```
mkdir build
cd build
```

Run `cmake` while properly referencing the location of the Qt Kit available on your operating system.

```
cmake -DCMAKE_PREFIX_PATH=/home/ruser/Qt/6.x.x/gcc_64 ..

cmake -DBOOST_ROOT=/usr/local/cellar/boost@1.76 -DOPENSSL_ROOT_DIR=/usr/local/cellar/openssl@1.1 ..

cmake -DOPENSSL_ROOT_DIR=/usr/local/cellar/openssl@1.1 ..

make
```

A successful build should produce, among other files, an executable named `Votelly-Gui`

To run the executable

```
./Votelly-Gui
```


## Build from Qt Creator

Although the project can be built like any CMake project, one specific way is building from within Qt Creator (v4.15 or newer).

1. Load the project within Qt Creator by loading the CMakeLists.txt file

File -> Open File or Project ... -> Navigate to where the repository has been cloned -> Select the CMakeLists.txt file -> Open

2. When loading the project for the first time:

configure the project by selecting any 6.x version of Qt Kit -> Configure Project

3. Build the project

Build -> Build Project "Votelly-GUI"

4. Run the project

Build -> Run


## Starting the GUI

The first time that the GUI starts, the user will be welcomed by Así.  Así will assist the user with connecting their GUI to the Votelly- contract deployed on a smart contract platform such as EOSIO or Peerplays.

### Votelly- Contract on EOSIO

Connectivity to the Votelly- smart contract deployed on an EOSIO blockchain is possible by providing Así with the server's address (e.g. https://myblockchainnode.com:8080/api).

### Votelly- Contract on Peerplays

Connectivity to the Votelly- smart contract deployed on a Peerplays blockchain is under development as of January 2022.
