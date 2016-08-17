//
// Copyright (c) 2003-2013 Christopher M. Kohlhoff (chris at kohlhoff dot com)
//
// Distributed under the Boost Software License, Version 1.0. (See accompanying
// file LICENSE_1_0.txt or copy at http://www.boost.org/LICENSE_1_0.txt)
//

#include <fstream>
#include <algorithm>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <bounded_buffer.h>

#include "fps.h"
#include "monitor.h"
#include "params.h"
#include "config.h"

#include "NV_encoding.hpp"

using namespace std;
using namespace boost::asio;
using ip::tcp;

const int max_length = 1024;

LPCSTR lpSharedMemoryFileName = "FalconTexturesSharedMemoryArea";
HANDLE hMapFile;

typedef boost::shared_ptr<tcp::socket> socket_ptr;

bounded_buffer<RGBQUAD*> screenToSendQueue(2);

void copyRect(char* src, char* dest, int srcW, int x, int y, int w, int h) {
	int ws = srcW * 4;
	int wd = w * 4;
	int xd = x * 4;

	long cnt = 0;
	for (int n = 0; n < h; n++) {
		memcpy(dest + n * wd, src + (n + y) * ws + xd, wd);
	}
}

void threadCapture(int width, int height) {

	RGBQUAD* pPixels;
	FPS fps;

	hMapFile = OpenFileMapping(SECTION_MAP_READ, FALSE, lpSharedMemoryFileName);
	char* pBuf = (char*)MapViewOfFile(hMapFile, SECTION_MAP_READ, 0, 0, NULL);

	while(true){
			RGBQUAD* pixCopy = new RGBQUAD[width * height];
		//memcpy(pixCopy, pBuf + 128, width * height * sizeof(RGBQUAD));
		copyRect(pBuf + 128, (char*)pixCopy, 1200, 750, 750, 450, 450);
			screenToSendQueue.push_front(pixCopy);
		Sleep(20);
			fps.newFrame();
		}

	UnmapViewOfFile(pBuf);
	CloseHandle(hMapFile);
}

void sessionVideo(socket_ptr sock)
{	
	// get the height and width of the screen
	int height = 450;
	int width = 450;

	NV_encoding nv_encoding;
	nv_encoding.load(width, height, sock, 0);

	boost::thread t(boost::bind(threadCapture, width, height));

	FPS fps;
	RGBQUAD* pPixels;
	while(true){
		screenToSendQueue.pop_back(&pPixels);
		nv_encoding.write(width, height, pPixels);
		free(pPixels);
	}
	nv_encoding.close();
}

void session(socket_ptr sock)
{
	try
	{
		sock->set_option(tcp::no_delay(true));
		char data[max_length];

		boost::system::error_code error;
		size_t length = sock->read_some(buffer(data), error);
		if (error == error::eof)
			return; // Connection closed cleanly by peer.
		else if (error)
			throw boost::system::system_error(error); // Some other error.

		if (data[0] == 'a'){
			sessionVideo(sock);
		} else {
			cout << "Received a connection with a wrong identification buffer " << string(data, length) << endl;
		}
	}
	catch (exception& e)
	{
		cerr << "Exception in thread: " << e.what() << "\n";
	}
}

void server(io_service& io_service, short port)
{
	tcp::acceptor a(io_service, tcp::endpoint(tcp::v4(), port));
	for (;;)
	{
		socket_ptr sock(new tcp::socket(io_service));
		a.accept(*sock);
		boost::thread t(boost::bind(session, sock));
	}
}

int main(int argc, const char* argv[])
{
    cout << "Version 0.99 BMS" << endl;
	
	//socket_ptr sock;
	//sessionVideo(sock, params.monitor, screenCoordinates); // TODO test

	try
	{
		io_service io_service;
		server(io_service, 8080);
	}
	catch (exception& e)
	{
		cerr << "Exception: " << e.what() << "\n";
	}
	return 0;
}