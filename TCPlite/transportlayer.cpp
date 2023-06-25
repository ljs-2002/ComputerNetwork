#include "transportlayer.h"

using namespace std;

TcpLite::TcpLite(int portSend, int portReceive, int portTarget, double loss, double corrupt)
{
    timer = new Timer(this);
    this->netLayer.init(portSend, portReceive, portTarget, loss, corrupt);
}

TcpLite::~TcpLite()
{
    this->netLayer.~virtualNetLayer();
    timer->close();
    delete timer;
}

void TcpLite::start()
{
    // 初始化下一个加入发送窗口的包的序列号
    seqNum = 0;
    // 初始化下一个要发送的包的序列号
    nextSeqNum = 0;
    // 初始化接收窗口的下一个序列号
    ackNum = 0;
    minReceiveSeqNum = 0;
    maxReceiveSeqNum = (RECEIVE_WINDOW_SIZE - 1) * MAX_SEGMENT_SIZE;
    sendWindowThreadEnd = false;
    receiveWindowThreadEnd = false;
    receiveFIN = false;
    reSend = false;
    // //启动发送窗口的线程
    sendFutureThread = async(launch::async, &TcpLite::sendLoop, this);
    receiveFutureThread = async(launch::async, &TcpLite::receiveLoop, this);
    timer->run();
}

unsigned short TcpLite::calculateChecksum(char *packet, int size)
{
    // 按反码加法计算16位checksum
    unsigned int checksum = 0;
    int i = 0;
    while (i < size - 1)
    {
        unsigned short *p = (unsigned short *)(packet + i);
        i += 2;
        checksum += *p;
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
        checksum += checksum >> 16;
    }
    if (size % 2 == 1)
    {
        unsigned char *p = (unsigned char *)(packet + size - 1);
        checksum += ((*p) << 8);
        checksum = (checksum & 0xFFFF) + (checksum >> 16);
        checksum += checksum >> 16;
    }
    return (unsigned short)(~checksum);
}

bool TcpLite::checkChecksum(char *packet, int size)
{
    unsigned short checksum = calculateChecksum(packet, size);
    return checksum == 0;
}

void TcpLite::buildPacket(char *buffer, TransportPacket transportPacket)
{
    int zero = 0;
    memset(buffer, 0, MAX_PACKAGE_SIZE);
    memcpy(buffer, reinterpret_cast<char *>(&transportPacket.type), sizeof(TransportPacketType));
    memcpy(buffer + OFFSET_SEQ_NEM, reinterpret_cast<char *>(&transportPacket.seqNum), sizeof(int));
    memcpy(buffer + OFFSET_ACK_NUM, reinterpret_cast<char *>(&transportPacket.ackNum), sizeof(int));
    memcpy(buffer + OFFSET_CHECKSUM, reinterpret_cast<char *>(&zero), sizeof(unsigned short));
    memcpy(buffer + OFFSET_DATA, transportPacket.data.c_str(), transportPacket.data.size());
    unsigned short checksum = calculateChecksum(buffer, transportPacket.data.size() + OFFSET_DATA);
    memcpy(buffer + OFFSET_CHECKSUM, reinterpret_cast<char *>(&checksum), sizeof(unsigned short));
}

TransportPacket TcpLite::buildTransportPacket(char *buffer, int size)
{
    TransportPacket transportPacket;
    memcpy(reinterpret_cast<char *>(&transportPacket.type), buffer, sizeof(TransportPacketType));
    memcpy(reinterpret_cast<char *>(&transportPacket.seqNum), buffer + OFFSET_SEQ_NEM, sizeof(int));
    memcpy(reinterpret_cast<char *>(&transportPacket.ackNum), buffer + OFFSET_ACK_NUM, sizeof(int));
    memcpy(reinterpret_cast<char *>(&transportPacket.checksum), buffer + OFFSET_CHECKSUM, sizeof(unsigned short));
    transportPacket.data = string(buffer + OFFSET_DATA, size - OFFSET_DATA);
    return transportPacket;
}

void TcpLite::send(string data)
{
    int size = data.size();
    char *content = new char[size];
    char *p = content;
    memcpy(content, data.c_str(), size);
    while (size > 0)
    {
        int packetSize = min(size, MAX_SEGMENT_SIZE);
        char *packet = new char[packetSize];
        memcpy(packet, content, packetSize);
        TransportPacket transportPacket;
        transportPacket.type = TransportPacketType::DATA;
        transportPacket.seqNum = seqNum;
        transportPacket.ackNum = ackNum;
        transportPacket.data = string(packet, packetSize);
        transportPacket.checksum = 0;
        sendPacket(transportPacket);
        seqNum = (seqNum + packetSize) % MAX_SEQ_NUM;
        size -= packetSize;
        content += packetSize;
        delete[] (packet);
    }
    delete[] (p);
}

int TcpLite::receive(char* buf, int maxSize, int* realSize)
{
    unique_lock<mutex> lock(receiveWindowMutex);
    // DONE:receiveWindow设置三个指针：最小序号指针，最大序号指针，最大确认序号指针
    // DONE:等待receiveLoop通知receiveWindowCV，取出receiveWindow中所有数据并清空receiveWindow，更新三个指针若有FIN标志则退出
    while (!receiveFIN && receiveWindow.empty())
    {
        receiveWindowCV.wait(lock);
    }
    int size = 0;
    // 取出receiveWindow中所有数据
    for (auto it = receiveWindow.begin(); it != receiveWindow.end();)
    {
        TransportPacket transportPacket = it->second;
        // 若当前包的数据长度加上已取出的数据长度未超过max_size，则将当前包的数据放入buf中，并从 receiveWindow中删除当前包
        if (size + transportPacket.data.size() <= maxSize)
        {
            memcpy(buf + size, transportPacket.data.c_str(), transportPacket.data.size());
            size += transportPacket.data.size();
            it = receiveWindow.erase(it);
        }
        else
        {
            break;
        }
    }
    // 更新三个指针
    minReceiveSeqNum = (minReceiveSeqNum + size) % MAX_SEQ_NUM;
    maxReceiveSeqNum = (maxReceiveSeqNum + size) % MAX_SEQ_NUM;
    ackNum = max(minReceiveSeqNum, ackNum);
    receiveWindowCV.notify_one();
    *realSize = size;
    // 若收到FIN包则关闭接收和发送线程
    if (receiveFIN)
    {
        close();
        // 若已取出接收窗口中的所有数据，则返回-1
        if (receiveWindow.empty()){
            return -1;
        }
    }
    return size;
}

void TcpLite::close()
{
    TransportPacket transportPacket;
    transportPacket.type = TransportPacketType::FIN;
    transportPacket.seqNum = seqNum;
    transportPacket.ackNum = ackNum;
    transportPacket.data = "";
    transportPacket.checksum = 0;
    sendPacket(transportPacket);
    sendWindowThreadEnd = true;
    receiveWindowThreadEnd = true;
    packetCacheCV.notify_one();
    receiveWindowCV.notify_one();
    sendFutureThread.get();
    receiveFutureThread.get();
}

void TcpLite::sendLoop()
{
    // TODO:发送一个包后注册一个定时器，若超时则重传，可以使用一个队列D保存已发送包的序号，只使用一个计时器，每次计时器到期就重发sendWindow中序号小于队列D中第一个序号的所有包
    // 并将计时器设定为队列D中下一个序号的超时时间，将第一个序号从队列D中弹出
    char *buffer = new char[MAX_PACKAGE_SIZE];
    while (!sendWindowThreadEnd)
    {
        unique_lock<mutex> lock(packetCacheMutex);
        while ((packetCache.empty() && !sendWindowThreadEnd&&!reSend) || sendWindow.size()>=SEND_WINDOW_SIZE)
        {
            packetCacheCV.wait(lock);
        }
        if (packetCache.empty() && sendWindowThreadEnd &&!reSend)
        {
            if(sendWindow.empty()){
                // 检查是否持有lock，若持有则释放
                if (lock.owns_lock())
                {
                    lock.unlock();
                }
                break;
            }
            packetCacheCV.notify_one();
            continue;
        }
        if (reSend)
        {
            //将sendCache中的所有包移动到sendWindow的首部
            cout<<"resend all packets in sendWindow"<<endl;
            unique_lock<mutex> lock2(sendWindowMutex);
            for(auto it = sendWindow.begin();it!=sendWindow.end();){
                packetCache.push_front(it->second);
                it = sendWindow.erase(it);
            }
            reSend=false;
            packetCacheCV.notify_one();
            sendWindowCV.notify_one();
            continue;
        }
        TransportPacket transportPacket = packetCache.front();
        if (transportPacket.type == TransportPacketType::FIN)
        {
            cout << "send FIN packet" << endl;
        }
        packetCache.pop_front();
        packetCacheCV.notify_one();
        //将包放入发送缓存中
        unique_lock<mutex> lock2(sendWindowMutex);
        sendWindow[transportPacket.seqNum] = transportPacket;
        sendWindowCV.notify_one();
        buildPacket(buffer, transportPacket);
        netLayer.send(buffer, transportPacket.data.size() + OFFSET_DATA);
        if(transportPacket.type == TransportPacketType::DATA){
            timer->add(transportPacket.seqNum, TIMEOUT);
        }
    }
    delete[] (buffer);
    // cout<<"sendLoop end"<<endl;
}

void TcpLite::sendPacket(TransportPacket packet)
{
    unique_lock<mutex> lock(packetCacheMutex);
    //真正的发送窗口是sendCache，即已发送未确认的包
    while (sendWindow.size()>= SEND_WINDOW_SIZE)
    {
        packetCacheCV.wait(lock);
    }
    packetCache.push_back(packet);
    //packetCache[packet.seqNum] = packet;
    packetCacheCV.notify_one();
}

void TcpLite::receiveLoop()
{
    // DONE:对于每个接到的ACK包，弹出sendWindow中序号小于等于ACK包的序号的所有包
    // DONE:对于接收到的FIN包，设置receiveFIN为true并通知receiveWindowCV，等待receive()检查receiveFIN并退出
    // DONE:当接收窗口满或或接到FIN包时，通知receiveWindowCV，让receive()取出所有数据
    // DONE:当接到的包在窗口范围内，并且不是FIN包且没有收到过，则放入receiveWindow中，若不是FIN包但是已收到过，则丢弃。
    // DONE:每次放入receiveWindow之后都更新ackNum为从receiveWindow中第一个序号开始连续的序号最大值
    char *buffer = new char[MAX_PACKAGE_SIZE];
    while (!receiveWindowThreadEnd)
    {
        int size;
        size = netLayer.receive(buffer, MAX_PACKAGE_SIZE);
        if (size == -1)
        {
            continue;
        }
        // 检查校验和
        if (!checkChecksum(buffer, size))
        {
            cout << "checksum error, ignore this packet." << endl;
            continue;
        }
        TransportPacket transportPacket = buildTransportPacket(buffer, size);
        if (transportPacket.type == TransportPacketType::ACK)
        {
            cout << "receive ACK packet with ackNum " << transportPacket.ackNum << endl;
            timer->remove(transportPacket.ackNum);
            // 将sendWindow中序号小于等于ACK包的序号的所有包弹出
            unique_lock<mutex> lock(sendWindowMutex);
            sendWindow.erase(sendWindow.begin(), sendWindow.upper_bound(transportPacket.ackNum));
            sendWindowCV.notify_one();
        }else if(transportPacket.type == TransportPacketType::FIN){
            cout << "receive FIN packet" << endl;
            receiveFIN = true;
            receiveWindowCV.notify_one();
            continue;
        }
        else if(transportPacket.seqNum<minReceiveSeqNum||transportPacket.seqNum>maxReceiveSeqNum){
            cout<<"receive packet with seqNum "<<transportPacket.seqNum<<" out of receive window"<<endl;
            continue;
        }else if(receiveWindow.find(transportPacket.seqNum)!=receiveWindow.end()){
            cout<<"receive packet with seqNum "<<transportPacket.seqNum<<" has been received"<<endl;
            continue;
        }
        else
        {
            //ackNum = transportPacket.seqNum + transportPacket.data.size();
            if(ackNum == transportPacket.seqNum){
                ackNum = transportPacket.seqNum + transportPacket.data.size();
                while(ackNum<RECEIVE_WINDOW_SIZE && receiveWindow.find(ackNum)!=receiveWindow.end()){
                    ackNum = ackNum + receiveWindow[ackNum].data.size();
                }
            }
            cout<<"now min,ack,max is "<<minReceiveSeqNum<<" "<<ackNum<<" "<<maxReceiveSeqNum<<endl;
            TransportPacket ackPacket;
            ackPacket.type = TransportPacketType::ACK;
            ackPacket.seqNum = seqNum;
            ackPacket.ackNum = ackNum;
            ackPacket.data = "";
            ackPacket.checksum = 0;
            sendPacket(ackPacket);
            unique_lock<mutex> lock(receiveWindowMutex);
            receiveWindow[transportPacket.seqNum] = transportPacket;
            lock.unlock();
            if(receiveWindow.size()>=RECEIVE_WINDOW_SIZE||receiveFIN)
                receiveWindowCV.notify_one();
        }
    }
    delete[] (buffer);
}

void TcpLite::setReSend(){
    reSend = true;
    packetCacheCV.notify_one();
}

void Timer::add(int seqNum, int timeout_ms)
{
    auto t = chrono::high_resolution_clock::now() + chrono::milliseconds(timeout_ms);
    bool notify = events.empty() || t < events.front().t;
    events.push_back({t, seqNum});
    if (notify)
    {
        cv.notify_one();
    }
}

void Timer::remove(int seqNum)
{
    unique_lock<mutex> lock(mtx);
    events.erase(remove_if(events.begin(), events.end(), [seqNum](const TimerEvent &e)
                           { return e.seqNum > seqNum; }),
                 events.end());
    lock.unlock();
    if(nowSeqNum<=seqNum){
        cancel();
    }
}

void Timer::cancel()
{
    unique_lock<mutex> lock(mtx);
    if (events.empty())
    {
        return;
    }
    cout << "cancel" << endl;
    cv.notify_one();
}

void Timer::run()
{
    futureObj = async(launch::async, &Timer::timerThread, this);
}

void Timer::close()
{
    {
        lock_guard<mutex> lock(mtx);
        events.clear();
    }
    stopFlag = true;
    cv.notify_one();
    futureObj.wait();
}

void Timer::timerThread()
{
    while (!stopFlag)
    {
        unique_lock<mutex> lock(mtx);
        if (events.empty())
        {
            cv.wait(lock);
        }
        else
        {
            auto &e = events.front();
            nowSeqNum = e.seqNum;
            auto now = chrono::high_resolution_clock::now();
            if (now < e.t)
            {
                cv.wait_until(lock, e.t);
            }
            int seqNum = e.seqNum;
            now = chrono::high_resolution_clock::now();
            events.pop_front();
            if (now >= e.t)
            {
                cout << "seqNum " << seqNum << " timeout" << endl;
                
                lock.unlock();
                tcpLite->setReSend();
                //弹出超时队列中所有大于等于seqNum的包
                remove(seqNum);
            }
        }
    }
}