#include <iostream>
#include <thread>
#include <vector>
#include <string>
#include <mutex>
#include <map>
#include <winsock2.h>
#include <queue>
#pragma comment(lib, "ws2_32.lib")

struct Message {
	int roomId;
	SOCKET senderSocket;
	std::string message;
};

std::mutex consoleMutex;
std::vector<SOCKET> clients;
std::map<int, std::vector<SOCKET>> rooms;
std::vector<std::thread> roomThreads;
std::mutex clientHandlerMutex;
std::mutex addClientMutex;

std::mutex messageQueueMutex;
std::condition_variable messageAvailableCondition;
std::queue<Message> messageQueue;

void addMessageToQueue(const Message& message) {
	{
		std::lock_guard<std::mutex> lock(messageQueueMutex);
		messageQueue.push(message);
	}
	messageAvailableCondition.notify_one();
}

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

int addClientToRoom(SOCKET clientSocket) {

	char idBuffer[3];
	
	int bytesReceived = recv(clientSocket, idBuffer, sizeof(idBuffer) - 1, 0);
	idBuffer[bytesReceived] = '\0';

	std::lock_guard<std::mutex> lock(consoleMutex);
	std::cout << "Client No " << clientSocket << " joined room with ID " << idBuffer << '\n';

	std::unique_lock<std::mutex> addClientLock(clientHandlerMutex);
	int roomID = std::atoi(idBuffer);
	rooms[roomID].push_back(clientSocket);

	addClientLock.unlock();

	return roomID;
}

void handleClient(SOCKET clientSocket) {

	//clients.push_back(clientSocket);

	int roomID = addClientToRoom(clientSocket);

	char buffer[4096];
	while (true) {
		int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
		if (bytesReceived <= 0) {

			std::lock_guard<std::mutex> lock(consoleMutex);
			std::cout << "Client " << clientSocket << " disconnected.\n";

			break;
		}

		std::string messageStr(buffer);

		if (messageStr == "/leave") {


			std::unique_lock<std::mutex> lock(clientHandlerMutex);
			auto socketIt = std::find(rooms[roomID].begin(), rooms[roomID].end(), clientSocket);

			if (socketIt != rooms[roomID].end()) {
				rooms[roomID].erase(socketIt);
			}
			lock.unlock();

			std::string msg = "/leave";
			send(clientSocket, msg.c_str(), msg.size() + 1, 0);

			roomID = addClientToRoom(clientSocket);

			continue;
		}

		Message message = { roomID, clientSocket, messageStr };
		addMessageToQueue(message);
	}
	closesocket(clientSocket);
}


void handleRoom() {

	while (true) {

		std::unique_lock<std::mutex> lock(messageQueueMutex);
		messageAvailableCondition.wait(lock, [] { return !messageQueue.empty(); });

		Message message = messageQueue.front();
		messageQueue.pop();

		lock.unlock();

		std::unique_lock<std::mutex> roomsLock(clientHandlerMutex);
		broadcastMessageInRoom(message.message, message.senderSocket, rooms[message.roomId]);
		roomsLock.unlock();
	}

}

int main() {
	rooms.insert(std::make_pair(0, std::vector<SOCKET>()));
	rooms.insert(std::make_pair(1, std::vector<SOCKET>()));
	rooms.insert(std::make_pair(2, std::vector<SOCKET>()));

	for (auto room : rooms) {
		std::thread roomThread(handleRoom);
		roomThread.detach();
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