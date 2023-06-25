#ifndef __MYTRANSPORT_H_
#define __MYTRANSPORT_H_

#include <iostream>
#include <string>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <future>
#include <thread>
#include <chrono>
#include <map>
#include <thread>
#include <algorithm>
#include "virtualNetLayer.h"

using namespace std;

enum class TransportPacketType :uint8_t {
    DATA,
    ACK,
    FIN
};

//数据段最大长度
const int MAX_SEGMENT_SIZE = 1024;
const int MAX_PACKAGE_SIZE = MAX_SEGMENT_SIZE+sizeof(int)*2+sizeof(unsigned short)+sizeof(TransportPacketType);
//最大序列号，能够表示的最大序列号为5M
const int MAX_SEQ_NUM = 5*1024*1024;
//窗口大小
const int SEND_WINDOW_SIZE = 128;
const int RECEIVE_WINDOW_SIZE = 3;
//偏移量
const int OFFSET_SEQ_NEM = sizeof(TransportPacketType)*2;
const int OFFSET_ACK_NUM = OFFSET_SEQ_NEM+sizeof(int);
const int OFFSET_CHECKSUM = OFFSET_ACK_NUM+sizeof(int);
const int OFFSET_DATA = OFFSET_CHECKSUM + sizeof(unsigned short);
//超时时间(ms)
const int TIMEOUT = 100;

class Timer;

struct TransportPacket {
    TransportPacketType type;
    TransportPacketType padding;
    int seqNum;
    int ackNum;
    unsigned short checksum;
    string data;
};

class TcpLite{
public:
    TcpLite(int portSend, int portReceive, int portTarget,double loss, double corrupt);
    ~TcpLite();
    void start();
    void send(string data);
    int receive(char* buf, int maxSize, int* realSize);
    void setReSend();
    void close();
    bool reSend;

private:
    //下一个加入发送窗口的包的序列号
    int seqNum;
    //下一个要发送的包的序列号
    int nextSeqNum;
    //接收窗口的最大序列号
    int maxReceiveSeqNum;
    //接收窗口的最小序列号
    int minReceiveSeqNum;
    //接收窗口的最大确认号
    int ackNum;
    //发送窗口的线程是否结束
    bool sendWindowThreadEnd;
    //接收窗口的线程是否结束
    bool receiveWindowThreadEnd;
    //是否收到FIN包
    bool receiveFIN;
    //是否重新发送
    //bool reSend;
    //网络层
    virtualNetLayer netLayer;
    //计时器
    Timer *timer;
    //待发送缓存
    deque<TransportPacket> packetCache;
    // //接收窗口
    // deque<TransportPacket> receiveWindow;
    //发送窗口
    //map<int, TransportPacket> sendWindow;
    //已发送窗口
    map<int, TransportPacket> sendWindow;
    //接收窗口
    map<int, TransportPacket> receiveWindow;
    //发送窗口的互斥锁
    mutex sendWindowMutex;
    //发送缓存的互斥锁
    mutex packetCacheMutex;
    //接收窗口的互斥锁
    mutex receiveWindowMutex;
    //发送窗口的条件变量
    condition_variable sendWindowCV;
    //发送缓存的条件变量
    condition_variable packetCacheCV;
    //接收窗口的条件变量
    condition_variable receiveWindowCV;
    //发送窗口的线程
    future<void> sendFutureThread;
    //接收窗口的线程
    future<void> receiveFutureThread;
    //计算整个数据包的checksum
    unsigned short calculateChecksum(char *packet, int sizes);
    //检查checksum
    bool checkChecksum(char *packet, int sizes);
    //从TransportPacket构建数据包
    void buildPacket(char *buffer, TransportPacket transportPacket);
    //从数据包构建TransportPacket
    TransportPacket buildTransportPacket(char *buffer, int size);
    //发送的线程函数
    void sendLoop();
    //接收的线程函数
    void receiveLoop();
    //用于发送数据包的函数
    void sendPacket(TransportPacket packet);
};

struct TimerEvent {
    chrono::time_point<chrono::high_resolution_clock> t;
    int seqNum;
};

class Timer {
public:
    Timer(TcpLite *tcpLite) : tcpLite(tcpLite), stopFlag(false), nowSeqNum(0) {}
    void add(int seqNum, int timeout_ms);
    void remove(int seqNum);
    void cancel();
    void run();
    void close();
private:
    bool stopFlag = false;
    int nowSeqNum;
    TcpLite *tcpLite;
    deque<TimerEvent> events;
    mutex mtx;
    condition_variable cv;
    future<void> futureObj;
    void timerThread();
};

#endif