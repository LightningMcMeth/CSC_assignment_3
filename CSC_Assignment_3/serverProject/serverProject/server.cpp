#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")

std::mutex consoleMutex;
std::vector<SOCKET> clients;
std::map<int, std::vector<SOCKET>> rooms;
std::vector<std::thread> roomThreads;

void broadcastMessage(const std::string& message, SOCKET senderSocket) {

	std::lock_guard<std::mutex> lock(consoleMutex);
	std::cout << "Client " << senderSocket << ": " << message << std::endl;

	for (SOCKET client : clients) {
		if (client != senderSocket) {
			send(client, message.c_str(), message.size() + 1, 0);
		}
	}
}

void broadcastMessageInRoom(const std::string& message, SOCKET senderSocket, std::vector<SOCKET>& clients) {

	std::lock_guard<std::mutex> lock(consoleMutex);
	std::cout << "Client " << senderSocket << ": " << message << std::endl;

	for (SOCKET client : clients) {
		if (client != senderSocket) {
			send(client, message.c_str(), message.size() + 1, 0);
		}
	}
}

void handleClient(SOCKET clientSocket) {

	clients.push_back(clientSocket);

	char roomID[2];
	recv(clientSocket, roomID, sizeof(roomID), 0);
	std::cout << "Client No " << clientSocket << " joined room with ID " << roomID;
	rooms[std::atoi(roomID)].push_back(clientSocket);

	char buffer[4096];
	while (true) {
		int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {

			std::lock_guard<std::mutex> lock(consoleMutex);
			std::cout << "Client " << clientSocket << " disconnected.\n";

			break;
		}

		//buffer[bytesReceived] = '\0';
		//std::string message(buffer);

		//broadcastMessage(message, clientSocket);
	}
	closesocket(clientSocket);
}


void handleRoom(std::pair<int, std::vector<SOCKET>>& room) {
	//This stuff should be looped and message queue vars should be declared here, before the loop

	std::string message;
	while (true) {
		std::getline(std::cin, message);

		//broadcastMessageInRoom(message, );
	}

}

int main() {
	//creating rooms. Num of rooms and their ids are hardcoded
	rooms.insert(std::make_pair(0, std::vector<SOCKET>()));
	rooms.insert(std::make_pair(1, std::vector<SOCKET>()));
	rooms.insert(std::make_pair(2, std::vector<SOCKET>()));

	//creating threads here for the rooms
	for (auto& room : rooms) {
		std::thread roomThread(handleRoom, room);
		roomThread.detach();
		//roomThreads.push_back(roomThread); //I think storing them is literally useless lol
	}

	WSADATA wsaData;
	if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
		std::cerr << "WSAStartup failed.\n";
		return 1;
	}
	SOCKET serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET) {
		std::cerr << "Socket creation failed.\n";
		WSACleanup();
		return 1;
	}

	sockaddr_in serverAddr;
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY;
	serverAddr.sin_port = htons(8080);

	if (bind(serverSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) ==
		SOCKET_ERROR) {
		std::cerr << "Bind failed.\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	if (listen(serverSocket, SOMAXCONN) == SOCKET_ERROR) {
		std::cerr << "Listen failed.\n";
		closesocket(serverSocket);
		WSACleanup();
		return 1;
	}

	std::cout << "Server is listening on port 8080...\n";

	while (true) {

		SOCKET clientSocket = accept(serverSocket, nullptr, nullptr);

		if (clientSocket == INVALID_SOCKET) {
			std::cerr << "Accept failed.\n";
			closesocket(serverSocket);
			WSACleanup();
			return 1;
		}

		std::lock_guard<std::mutex> lock(consoleMutex);
		std::cout << "Client " << clientSocket << " connected.\n";

		std::thread clientThread(handleClient, clientSocket);
		clientThread.detach(); // Detach the thread to allow handling multiple clients concurrently
	}
	closesocket(serverSocket);
	WSACleanup();
	return 0;
}

//TODO:
	//1) Finish implementing multiple rooms.
	//2) Implement message queues.

	//I understand what I didn't previously:
	//Each client already has their own thread, so is it really necessary to create a thread per room?
	//I can simply add them to the appropriate room inside of handleClient.
	//What would be the difference in implementing rooms with separate threads per rooma and without and is it mandatory?