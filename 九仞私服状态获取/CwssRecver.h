#pragma once
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <vector>
#include <string>
#include <locale>
#include <codecvt>
#include "sciter-x-window.hpp"
#pragma comment(lib, "ws2_32.lib")
using namespace std;

char * WsGetErrorInfo();
string pack_varint(int d);
string pack_data(string d);



class frame : public sciter::window {
public:
	BEGIN_FUNCTION_MAP
		FUNCTION_0("DoTask", DoTask);
	END_FUNCTION_MAP
	sciter::value  DoTask();
	frame() : window(SW_TITLEBAR | SW_RESIZEABLE | SW_CONTROLS | SW_MAIN | SW_ENABLE_DEBUG) {}
};

extern frame *pwin;

class CwssRecver
{
public:
	CwssRecver();
	~CwssRecver();
	bool AsyncGet();
	string GetOnlinePlayer();
	string GetMaxPlayer();
	string GetMotd();
	string GetLastStateInfo();
	bool init();
private:
	struct { 
		string maxPlayer;
		string onlinePlayer;
		string motd;
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