#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>
#include <thread>
#include <direct.h>
#include "base64.h"
#include "sciter-x-window.hpp"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

char * WsGetErrorInfo();
string pack_varint(int d);
string pack_data(string d);



class frame : public sciter::window {
public:
	BEGIN_FUNCTION_MAP
		FUNCTION_2("DoTask", DoTask);
	END_FUNCTION_MAP
	sciter::value  DoTask(sciter::value serverAddr, sciter::value serverPort);
	frame() : window(SW_TITLEBAR | SW_RESIZEABLE | SW_CONTROLS | SW_MAIN | SW_ENABLE_DEBUG) {}
};

extern frame *pwin;

class CwssRecver
{
public:
	CwssRecver();
	~CwssRecver();
	bool AsyncGet(string serverAddr, string serverPort);
	void SetFlag(int i);
	string GetOnlinePlayer();
	string GetMaxPlayer();
	string GetMotd();
	string GetLastStateInfo();
	string GetFavicon();
	bool init();
	friend void ThFunc_AsyncGet(CwssRecver *cr, string serverAddr, string serverPort);
	friend bool GetServerInfo(CwssRecver *cr, string serverAddr, string serverPort);
private:
	struct { 
		string maxPlayer;
		string onlinePlayer;
		string motd;
		string favicon;
	} m_status;
	/*标志获取状态
		FSUCCESSFUL,	一切正常
		FRAW,			类刚初始化时的状态
		FINITFAILED,	套接字库初始化失败
		FCSFAILED,		创建套接字失败
		FGHFAILED,		域名解析失败
		FCONFAILED,		连接失败
		FSENDFAILED,	发送失败
		FRECVFAILED,	接收失败
	*/
	enum { FSUCCESSFUL, FRAW, FINITFAILED, FCSFAILED, FGHFAILED, FCONFAILED, FSENDFAILED, FRECVFAILED} iGetFlag = FRAW;
};