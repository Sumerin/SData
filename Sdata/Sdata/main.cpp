#define _CRT_SECURE_NO_WARNINGS
#pragma comment(lib,"ws2_32.lib")
#include <cstdio>
#include <WinSock2.h>
#include <string>
#include <windows.h>
#include <fstream>

#include <iostream> // temp 

#define ERROR WSAGetLastError()			// gives the error code 
#define PORT 1112						// port for connection
#define BROADCAST_ADDRES "127.0.0.1"	// need to be change to given from keyboard
#define KB 1024							// define size of KB/package

typedef int(_stdcall  * dir_s)(SOCKET, const char *, int, int);		// allows a pointer to send() 
typedef int(_stdcall  * dir_r)(SOCKET, char *, int, int);			// allows a pointer to recv()

using namespace std;


bool ipPattern(string IP)
{

	int counter = 0;





	for (int i = 0; i < 4; i++) // 4 BYTES
	{


		int number = 0;
		bool blank = true;

		if ((unsigned)counter < IP.size()) // in case there is less then 4
		{

			while (IP[counter] >= '0' && IP[counter] <= '9')// convert char -> int
			{

				number *= 10;
				number += IP[counter] - '0';

				counter++;


				blank = false; // to be sure if there is a number not just "10..0"
			}

			if (blank) // we don't want blank ip
			{
				return false;
			}


			if (IP[counter] != '.' && (unsigned)counter < IP.size()) // dot between Bytes if it isn't last one   
			{

				return false;
			}


			if (number > 255 || number < 0)// BYTE got value <255;0>
			{
				return false;
			}


			counter++; // next sign


		}
		else
		{
			return false;
		}

	}

	return true;
}


string getIP()
{
	string IP;
	do
	{
		printf("IP: ");
		getline(cin, IP); // scanf("%s",&IP); // why it gave me 0.0.1 from 127.0.0.1

	} while (!ipPattern(IP));

	return IP;
}



void check_errno(int err) // checking the error if occureed
{
	if (err < 0)
	{
		throw ERROR;
	}

}

template <typename lol>
void packet(SOCKET dest, char* data,int size, lol todo) // sending/receiving all bytes for sure
{
	int enumerator	= 0;
	int acc_data	= 0;

	while (acc_data < size)
	{
		enumerator = 0;

		enumerator = todo(dest, data + acc_data, size - acc_data, NULL); // sent rest of data if it wasn't sent fully

		check_errno(enumerator); 

		acc_data += enumerator;
		
	}
}



void sent_data(char* path)
{
	SOCKET reciver = socket(AF_INET, SOCK_STREAM, NULL);

	string	name = string(path);	// path to the file
	int		size;					// size of package
	int		size_NBO;				// size of backage in Network Byte Order
	char	callback[3];			// sth to storage callback
	char	buffer[KB];				// buffer for package
	int		read_byte;				// byte readed from file
	int		read_byte_NBO;			// the sam but in Network Byte Order
	int		result;					// result of conecting

	string address = getIP();
	sockaddr_in addr;
	int			size_addr = sizeof(addr);
	addr.sin_addr.s_addr = inet_addr(address.c_str()); // address of recv 
	addr.sin_port		 = htons(PORT);					// port of recv
	addr.sin_family		 = AF_INET;						// IPV4



	size_t found = name.find_last_of("\\"); // get just the filename
	name = name.substr(found + 1);

	try
	{

		result = connect(reciver, (sockaddr*)&addr, size_addr); 
		check_errno(result);

		size = name.size();
		size_NBO = htonl(size);
		packet<dir_s>(reciver, (char*)&size_NBO, sizeof(int), send);	// send size of name
		send(reciver, name.c_str(), size, NULL);						// send the name


		ifstream infile(path, ios_base::binary);	// open the file and get information about size
		infile.seekg(0, infile.end);

		size = infile.tellg();
		size_NBO = htonl(size);

		infile.seekg(0);


		packet<dir_s>(reciver, (char*)&size_NBO, sizeof(int), send); // send the total size of file

		while (size > 0)
		{

			if (size > KB)	// force package to be 1024 or less 
			{
				infile.read(buffer, KB);
				size -= KB;
				read_byte = KB;
			}
			else
			{
				infile.read(buffer, size);
				read_byte = size;
				size -= size;
			}
			printf("wysylanie pakietu: %d \n", read_byte);

			read_byte_NBO = htonl(read_byte);

			packet<dir_s>(reciver, (char*)&read_byte_NBO, sizeof(int), send);	// sent the size of package and data
			packet<dir_s>(reciver, buffer, read_byte, send);

			packet<dir_r>(reciver, callback, sizeof("ok"), recv);	// wait for response  

		}

	}
	catch (int a)
	{
		printf("%d", a);
	}
	printf("koniec Wysylania\n");
	closesocket(reciver);
}



void recv_data()
{
	
	
	SOCKET slisten = socket(AF_INET, SOCK_STREAM, NULL); // socket for queue
	SOCKET senter  = socket(AF_INET, SOCK_STREAM, NULL); // socket for data transmission


	int		result;			// result of binding/listening
	int		read_byte;		// byte in package
	int		total_data;		// total amount of data to recv
	int		sizeof_name;	// amount of bytes in package which contain name of file (just like read_byte)  
	char	*name;			// buffer for the name
	char	buffer[KB];		// buffer for reciving package

	
	SOCKADDR_IN addr;	
	int	size = sizeof(addr); 
	addr.sin_addr.s_addr = htons(INADDR_ANY); // every ip address 
	addr.sin_port		= htons(PORT);		  // PORT to be used
	addr.sin_family		= AF_INET;			  // IPV4


	try // check_errno throw execpction if sth went wrong
	{

		
		result = bind(slisten, (SOCKADDR*)&addr, size); // bind the address to slisten socket 
		check_errno(result);							

		result = listen(slisten, 1);					// start listening on slisten just 1 place in queue
		check_errno(result);


		senter = accept(slisten, (sockaddr*)&addr, &size); // waiting for your mate to connect
		check_errno(senter);

		packet<dir_r>(senter, (char*)&sizeof_name, sizeof(int), recv);	// get the size of filename, 
		sizeof_name = ntohl(sizeof_name);								// BE->LE if nessesary
		name = new char[sizeof_name + 1];								// reserve the memeory 
		name[sizeof_name] = '\0';										// and put the sign  "end of string" 



		packet<dir_r>(senter, name, sizeof_name, recv);			// get size of filename
		printf("nazwa pliku: %s \n", name);						// TODO: expand the interface by computer ip and name
		ofstream outfile(name, ios_base::binary);				// prepare the stream for output
		

		packet<dir_r>(senter, (char*)&total_data, sizeof(int), recv); // get the total size of file
		total_data = ntohl(total_data);								  // BE->LE if nessesary


		while (total_data > 0) // start receiving data in a package <= 1KB
		{
		
			packet<dir_r>(senter, (char*)&read_byte, sizeof(int), recv); // actual size of package
			read_byte = ntohl(read_byte);								// BE->LE if nessesary

			packet<dir_r>(senter, buffer, read_byte, recv); // get package

			packet<dir_s>(senter, "ok", sizeof("ok"), send); // send "ok" so senter can prepare another package

			printf("size: %d \n", read_byte); // TODO: change it to % and create thread for it (print on screen is slowing the process)
			outfile.write(buffer, read_byte); // save it (slow but gives time to senter for preparing another package)

			

			total_data -= read_byte; // less data need to be received
		}
		


		delete[] name;	// clr the buffer of name
		//delete[] buffer;// clr the buff for data

	}
	catch (int a)
	{
		printf("%d", a); // errors ocurred maybe
	}

	closesocket(slisten); // free the sockets
	closesocket(senter);

}

int main(int argc, char* argv[])
{
	WSADATA wsaData;
	WORD dllVersion = MAKEWORD(2, 1);

	if (WSAStartup(dllVersion, &wsaData) != 0) //
	{
		MessageBoxA(NULL, "WSAdata startup failed!!", "SData", MB_OK | MB_ICONERROR);
		return 1;
	}

	if (argc == 2)
	{
		sent_data(argv[1]);
	}
	else
	{
		recv_data();
	}
	
	WSACleanup();
	system("pause");
	return 0;
}

