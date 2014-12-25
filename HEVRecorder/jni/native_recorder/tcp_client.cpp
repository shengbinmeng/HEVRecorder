/*
 * tcp_client.cpp
 *
 *  Created on: Dec 25, 2014
 *      Author: shengbin
 */

#include "tcp_client.h"
#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/stat.h>
#include "jni_utils.h"

#define LOG_TAG "TcpClient"

#define BUFFER_SIZE 1024

TcpClient::TcpClient() {
	dataSocket = 0;
}

int TcpClient::openConnection(const char *host, int port) {

	// SOCK_STREAM Provides sequenced, reliable, two-way, connection-
    // based byte streams. i.e. TCP
	if ((dataSocket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
		LOGE("create socket failed\n");
		return -1;
	}

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = inet_addr(host);
    addr.sin_port = htons(port);
	if (connect(dataSocket, (struct sockaddr *) &addr, sizeof(struct sockaddr)) == -1) {
		LOGE("connect to remote host failed\n");
		return -1;
	}

	return 0;
}

int TcpClient::sendData(char *buff, int size) {

	int sent = 0, result = 0;
	while (sent < size) {
		result = send(dataSocket, buff + sent, size - sent, 0);
		if (result == -1) {
			return -1;
		}
		sent += result;
	}
	return sent;
}

int TcpClient::receiveData(char **pbuff, int size) {
	int recvnum = 0, result = 0;
	char buff[256];

	*pbuff = NULL;

	while (recvnum < size || size == 0) {
		result = recv(dataSocket, buff, 256, 0);
		if (result <= 0)
			break;
		recvnum += result;

		if (*pbuff == NULL) {
			*pbuff = (char*) malloc(recvnum);
			if (*pbuff == NULL) {
				return -1;
			}
		} else {
			*pbuff = (char*) realloc(*pbuff, recvnum);
			if (*pbuff == NULL) {
				return -1;
			}
		}

		memcpy(*pbuff + recvnum - result, buff, result);
	}

	return recvnum;
}

int TcpClient::closeConnection() {
	int ret = shutdown(dataSocket, 2);
	return ret;
}

int TcpClient::sendFile(char* filename) {
	unsigned int filesize = -1;
	FILE *file;
	int read_len, send_len;
	char buf[BUFFER_SIZE];
	struct stat statbuff;
	if (stat(filename, &statbuff) < 0) {
		return -1;
	} else {
		filesize = statbuff.st_size;
	}
	file = fopen(filename, "r");

	int ret = sendData((char*) &filesize, sizeof(unsigned int));
	if (ret < 0) {
		LOGE("Send file size failed\n");
		return -1;
	}

	while ((read_len = fread(buf, sizeof(char), BUFFER_SIZE, file)) > 0) {
		send_len = send(dataSocket, buf, read_len, 0);
		if (send_len < 0) {
			LOGE("Send file failed\n");
			return -1;
		}
		bzero(buf, BUFFER_SIZE);
	}

	return 0;
}
