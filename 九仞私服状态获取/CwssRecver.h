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
#include "stringTools.h"
#include "sciter-x-window.hpp"
#include <io.h>
#pragma comment(lib, "ws2_32.lib")
using namespace std;

char * WsGetErrorInfo();
string pack_varint(int d);
string pack_data(string d);



class frame : public sciter::window {
public:
	BEGIN_FUNCTION_MAP
		FUNCTION_3("DoTask", DoTask);
	END_FUNCTION_MAP
	sciter::value DoTask(sciter::value serverAddr, sciter::value serverPort, sciter::value isRefresh/* 是否为刷新操作 */);
	frame() : window(SW_TITLEBAR | SW_RESIZEABLE | SW_CONTROLS | SW_MAIN | SW_ENABLE_DEBUG) {}
};

extern frame *pwin;

class CwssRecver
{
public:
	CwssRecver();
	~CwssRecver();
	bool AsyncGet(string serverAddr, string serverPort, bool isRefresh);
	void SetFlag(int i);
	wstring GetOnlinePlayer();
	wstring GetMaxPlayer();
	wstring GetMotd();
	string GetLastStateInfo();
	string GetFavicon();
	bool init();
	friend void ThFunc_AsyncGet(CwssRecver *cr, string serverAddr, string serverPort, bool isRefresh/* 是否为刷新操作 */);
	friend bool GetServerInfo(CwssRecver *cr, string serverAddr, string serverPort);
private:
	struct { 
		wstring maxPlayer;
		wstring onlinePlayer;
		wstring motd;
		string favicon;
	} m_status;
	bool is_inited = false;
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

/* Sciter的脚本已经提供了诸多方法，这个文件用于前后端之间的方法交换，
意思是说后端的一些功能实现起来过于复杂，还不如用前端的脚本来帮助后端实现一些复杂功能
比如JSON字符串的读取
*/

wstring GetJsonFieldFromJsonString(string json, string jsonField);
wstring GetJsonFieldFromJsonString(wstring json, string jsonField);