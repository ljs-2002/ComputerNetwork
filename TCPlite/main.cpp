#include "transportlayer.h"

using namespace std;

int main(int argc, char *argv[]){
    double loss = 0,correct = 0;
    if(argc!=2){
        cout<<"Usage: "<<argv[0]<<" -s/-r"<<endl;
        return 0;
    }
    if(strcmp(argv[1],"-s")==0){
        TcpLite sender(23455,23456,23457,0.3,0.3);
        sender.start();
        while(true){
            string s;
            cout<<"input:";
            cin >> s;
            cout<<endl;
            if(s=="exit") break;
            sender.send(s);
        }
        sender.close();
    }
    else if(strcmp(argv[1],"-r")==0){
        TcpLite receiver(23458,23457,23456,0,0);
        char buf[100];
        cout<<"start receive"<<endl;
        receiver.start();
        int realSize;
        while(true){
            int size = receiver.receive(buf,100,&realSize);
            buf[realSize] = '\0';
            cout << buf << endl;
            if(size==-1) break;
        }
    }
    else{
        cout<<"Usage: "<<argv[0]<<" -s/-r"<<endl;
        return 0;
    }

}