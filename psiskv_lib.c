#include "psiskv.h"

int kv_connect(char * kv_server_ip, int kv_server_port) {

	int socket_fd;
	struct sockaddr_in server_addr;

	/* open socket */
	if(socket_fd = socket(AF_INET, SOCK_STREAM, 0) == -1) {
		perror("Creating socket");
		return -1;
	}

	/* fill struct */
	server_addr.sin_family = AF_INET;
	server_addr.sin_port = htons(kv_server_port);
	if(inet_aton(kv_server_ip, server_addr.sin_addr) == 0)  {
 		perror("inet_aton");
		return -1;
	}
	
	/* connect */
	int err = connect(socket_fd, (const struct sockaddr *) &server_addr, sizeof(server_addr));
	if (err == -1){
		perror("connect: ");
		return -1;
	}

	return socket_fd;

}


int kv_write(int kv_descriptor, uint32_t key, char * value, int value_length, int kv_overwrite) {

	kv_client2server message;
	message.op = 'w';	// para identificar ao server que é uma operação de write
	message.key = key;
	message.value_length = value_length;
	message.overwrite = kv_overwrite;

	/* send message header */
    int nbytes = send(kv_descriptor, &message, sizeof(message), 0);
	
	/* send message */
    nbytes = send(kv_descriptor, value, value_length, 0);
	if(nbytes != value_length) {
		perror("send failed");
		return -1;
	}
	
	return 0;
	
}


int kv_read(int kv_descriptor, uint32_t key, char * value, int value_length) {

	kv_client2server message;
	message.op = 'r';	// para identificar ao server que é uma operação de write
	message.key = key;
	
    int nbytes = send(kv_descriptor, &message, sizeof(message), 0);
	
	if(nbytes != sizeof(message)) {
		perror("send failed");
		return -1;
	}

	/* read size of message */
	nbytes = recv(kv_descriptor, &message, sizeof(kv_client2server), 0);

	if(nbytes != message.value_length) {
		perror("receive size of msg failed");
		return -1;
	}
	
	//If the values does not exist in dictionary
	if(message.error_code == -2)
		return -2;


	/* read message */
	nbytes = recv(kv_descriptor, value , value_length, 0);//only read up to param value_length
	
	//check length received
	if(nbytes != message.value_length) {
		perror("receive values failed");
		return -1;
	}

	return 0;

	
}

void kv_close(int kv_descriptor) {

	if(close(kv_descriptor) == -1) {
		perror("closing socket");
	}
	unlink(SOCK_ADDR);

}

int kv_delete(int kv_descriptor, uint32_t key) {
	
	kv_client2server message;
	message.op='d';
	
	//send order
	int nbytes = send(kv_descriptor, &message, sizeof(message), 0);
	
	if(nbytes != sizeof(message)) {
		perror("send failed");
		return -1;
	}
	
	//check if it was deleted
	nbytes = recv(kv_descriptor, &message, sizeof(kv_client2server), 0);

	if(message.error_code != 0) {
		perror("delete failed");
		return message.error_code;
	}

	return 0;	
}
