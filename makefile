all: myWget

myWget: main.o libCustomNetworkManager.a libCustomTcpServer.a
	g++ -std=c++11 -o myWget main.o -L. -lCustomNetworkManager -lCustomTcpServer -lboost_regex

main.o: main.cpp 
	g++ -std=c++11 -c -o main.o main.cpp -I. 


libCustomNetworkManager.a: CustomNetworkManager.o
	ar cr libCustomNetworkManager.a CustomNetworkManager.o

CustomNetworkManager.o: customnetworkmanager.cpp
	g++ -std=c++11 -c -o CustomNetworkManager.o customnetworkmanager.cpp -I. 

libCustomTcpServer.a: CustomTcpServer.o TcpSocket.o
	ar cr libCustomTcpServer.a CustomTcpServer.o TcpSocket.o

CustomTcpServer.o: customtcpserver.cpp
	g++ -std=c++11 -c -o CustomTcpServer.o customtcpserver.cpp -I. 

TcpSocket.o: tcpsocket.cpp
	g++ -std=c++11 -c -o TcpSocket.o tcpsocket.cpp -I.
clear:
	rm *.o *.a myWget
