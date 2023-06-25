#include "virtualNetLayer.h"

void virtualNetLayer::init(int portSend, int portReceive, int portTarget,double loss, double corrupt){
    this->portSend = portSend;
    this->portReceive = portReceive;
    this->loss = loss;
    this->corrupt = corrupt;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        cout << "WSAStartup failed!" << endl;
        return;
    }
    sockSend = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    sockReceive = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    u_long iMODE=1;
    ioctlsocket(sockReceive,FIONBIO,&iMODE);
    addrSend.sin_family = AF_INET;
    addrSend.sin_port = htons(portSend);
    addrSend.sin_addr.s_addr = inet_addr("127.0.0.1");

    addrReceive.sin_family = AF_INET;
    addrReceive.sin_port = htons(portReceive);
    addrReceive.sin_addr.s_addr = inet_addr("127.0.0.1");

    addrTarget.sin_family = AF_INET;
    addrTarget.sin_port = htons(portTarget);
    addrTarget.sin_addr.s_addr = inet_addr("127.0.0.1");

    bind(sockSend, (SOCKADDR*)&addrSend, sizeof(SOCKADDR));
    bind(sockReceive, (SOCKADDR*)&addrReceive, sizeof(SOCKADDR));
}

virtualNetLayer::~virtualNetLayer(){
    closesocket(sockSend);
    closesocket(sockReceive);
    WSACleanup();
}

void virtualNetLayer::send(char* packet, int size){
    srand((unsigned)time(NULL));
    if(rand()%100 < loss*100){
        cout<<"丢失"<<endl;
        return;
    }
    if(rand()%100 < corrupt*100){
        cout<<"损坏"<<endl;
        packet[0] = packet[0] + 1;
    }
    int len = sendto(sockSend, packet, size, 0, (SOCKADDR*)&addrTarget, sizeof(SOCKADDR));
    if (len == SOCKET_ERROR) {
        cout << "send failed!" << endl;
        cout << WSAGetLastError() << endl;
        return ;
    }
}

int virtualNetLayer::receive(char* packet, int size){
    return recvfrom(sockReceive, packet, size, 0, NULL, NULL);
}

