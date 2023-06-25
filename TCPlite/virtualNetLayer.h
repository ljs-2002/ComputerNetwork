#ifndef __MYVIRTUALNETLAYER_H_
#define __MYVIRTUALNETLAYER_H_

#include <iostream>
#include <cstring>
#include <ctime>
#include <cstdlib>
#include <winsock2.h>

using namespace std;

class virtualNetLayer{
public:
    ~virtualNetLayer();
    void init(int portSend, int portReceive, int portTarget,double loss, double corrupt);
    void send(char* packet, int size);
    int receive(char* packet, int size);
private:
    SOCKET sockSend;
    SOCKET sockReceive;
    SOCKADDR_IN addrSend;
    SOCKADDR_IN addrReceive;
    SOCKADDR_IN addrTarget;
    int portReceive;
    int portSend;
    double loss;
    double corrupt;

};

#endif