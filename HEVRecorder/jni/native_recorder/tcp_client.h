/*
 * tcp_client.h
 *
 *  Created on: Dec 25, 2014
 *      Author: shengbin
 */

#ifndef TCP_CLIENT_H_
#define TCP_CLIENT_H_

#include <netinet/in.h>

class TcpClient {
public:
	TcpClient();
	int openConnection(const char *host, int port);
	int receiveData(char **buff, int size);
	int sendData(char *buff, int size);
	int closeConnection();
	int sendFile(char* filename);

private:
	int dataSocket;
};

#endif /* TCP_CLIENT_H_ */
