all: myWget

myWget: main.o 
	g++ -std=c++11 -o myWget main.o -L. -lCNM -lCustomTcpServer -lboost_regex

main.o: main.cpp 
	g++ -std=c++11 -c -o main.o main.cpp -I.

clear:
	rm *.o
