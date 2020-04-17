#include "CwssRecver.h"


/********************
	全局变量声明区
********************/
frame *pwin;

/********************
	公共方法定义区
********************/

char * WsGetErrorInfo()
{
	static CHAR pBuf[1024] = { 0 };
	const ULONG bufSize = 1024;
	DWORD retSize;
	LPTSTR pTemp = NULL;

	pBuf[0] = '0';

	retSize = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
		FORMAT_MESSAGE_FROM_SYSTEM |
		FORMAT_MESSAGE_ARGUMENT_ARRAY,
		NULL,
		GetLastError(),
		LANG_NEUTRAL,
		(LPTSTR)&pTemp,
		0,
		NULL);
	if (retSize > 0) {
		pTemp[strlen(pTemp) - 2] = '\0'; //remove cr and newline character
		sprintf_s(pBuf, bufSize, "%0.*s", bufSize, pTemp);
		LocalFree((HLOCAL)pTemp);
	}
	return pBuf;
}

string pack_varint(int d) {
	static string o = "";
	o.clear();
	while (1)
	{
		int b = d & 0x7F;
		d >>= 7;
		o += b | (d > 0 ? 0x80 : 0);
		if (d == 0)
			break;
	}

	return o;
}

string pack_data(string d)
{
	return pack_varint(d.length()) + d;
}

/********************
	类方法定义区
********************/

CwssRecver::CwssRecver()
{
}


CwssRecver::~CwssRecver()
{
	WSACleanup();
}

sciter::value frame::DoTask()
{
	CwssRecver cr = CwssRecver();
	if (!cr.init())
	{
		pwin->call_function("DebugLog", cr.GetLastStateInfo());
		return sciter::value("初始化失败");
	}
		
	if (!cr.AsyncGet())
	{
		pwin->call_function("DebugLog", cr.GetLastStateInfo());
		pwin->call_function("SetSerStatus", sciter::value("<div id=\"statusDot\" style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"padding-left:10px;display:inline-block;width:80px;\">离线</span>"));
		pwin->call_function("SetPlayerNum", sciter::value("/ QWQ"), sciter::value(cr.GetMaxPlayer()));
		pwin->call_function("SetMotd", sciter::value(cr.GetLastStateInfo()));
		return sciter::value(cr.GetLastStateInfo());
	}
	if(cr.GetOnlinePlayer().length() == 0)
		pwin->call_function("SetSerStatus", sciter::value("<div id=\"statusDot\" style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"padding-left:10px;display:inline-block;width:80px;\">离线</span>"));
	else
		pwin->call_function("SetSerStatus", sciter::value("<div id=\"statusDot\" style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:green;\"></div><span style=\"padding-left:10px;display:inline-block;width:80px;\">在线</span>"));
	pwin->call_function("SetPlayerNum", sciter::value(cr.GetOnlinePlayer()), sciter::value(cr.GetMaxPlayer()));
	pwin->call_function("SetMotd", sciter::value(cr.GetMotd()));
	return sciter::value("获取成功");
}

bool CwssRecver::AsyncGet()
{
	struct addrinfo *result = NULL, hints;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	int iResult;
	//套接字连接信息
	iResult = getaddrinfo("cn-xz-bgp.sakurafrp.com", "65051", &hints, &result);
	if (iResult != 0) {
		string logstr = "域名解析失败 : ";
		logstr += iResult;
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		iGetFlag = FGHFAILED;
		return 0;
	}
	SOCKET ClientSocket = INVALID_SOCKET;

	ClientSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
	if (ClientSocket == INVALID_SOCKET) {
		string logstr = "创建套接字失败 : ";
		logstr += WSAGetLastError();
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		freeaddrinfo(result);
		iGetFlag = FCSFAILED;
		return 0;
	}

	iResult = connect(ClientSocket, result->ai_addr, (int)result->ai_addrlen);
	if (iResult == SOCKET_ERROR) {
		string logstr = "连接失败 : ";
		logstr += WSAGetLastError();
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		iGetFlag = FCONFAILED;
		return 0;
	}
	else
		pwin->call_function("DebugLog", "连接成功！\n");
	freeaddrinfo(result);
	char sendbuf[512]{ 0 };
	byte egMcPingPacket[] =
	{
		0xf, 0x0, 0x0, 0x9, 0x6c, 0x6f, 0x63, 0x61, 0x6c, 0x68,
		0x6f, 0x73, 0x74, 0x63, 0xdd, 0x1,
	};
	char str[64]{ 0 };
	string packstr;
	packstr += '\x00';
	packstr += '\x00';
	packstr += pack_data("cn-xz-bgp.sakurafrp.com"); //包装域名信息
	packstr += "\xfe\x1b\x01";//端口65051的十六进制表示
	string debug = pack_data(packstr);
	packstr = pack_data(packstr);
	memcpy_s(str, 64, packstr.c_str(), packstr.length());
	iResult = send(ClientSocket, (char*)str, packstr.length(), 0);
	pwin->call_function("DebugLog", WsGetErrorInfo());
	iGetFlag = FSENDFAILED;
	if (iResult == SOCKET_ERROR) {
		string logstr = "发送失败! ";
		logstr += WSAGetLastError();
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		iGetFlag = FSENDFAILED;
		return 0;
	}
	else
	{
		string logstr = "发送成功! ";
		logstr += iResult;
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
	}
	//string strRequest = pack_data("\x0");

	//iResult = send(ClientSocket, strRequest.c_str(), strRequest.length(), 0);

	byte sendRequest[2];
	sendRequest[0] = 0x1;
	sendRequest[1] = 0x0;
	iResult = send(ClientSocket, (char*)sendRequest, 2, 0);
	printf_s(WsGetErrorInfo());
	if (iResult == SOCKET_ERROR) {
		string logstr = "发送失败! ";
		logstr += WSAGetLastError();
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		iGetFlag = FSENDFAILED;
		return 0;
	}
	else
	{
		string logstr = "发送成功! ";
		logstr += iResult;
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
	}
	byte recvbuf[1025]{ 0 };
	recvbuf[1024] = '\0';
	string allRecv;
	iResult = 1;
	for (int i = 0; iResult > 0 && recvbuf[0] != '{'; i++)
		iResult = recv(ClientSocket, (char*)recvbuf, 1, 0);
	iResult = recv(ClientSocket, (char*)recvbuf, 1024, 0);
	while (iResult > 0)
	{
		string logstr = "接收成功！";
		logstr += iResult;
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		for (int i = 0; i < iResult; i++)
		{
			//if (recvbuf[i] > 128) //传输时用的是UTF8编码
			//{
				//string chrU16BE;
				//chrU16BE += recvbuf[i] & 0xff;
				//chrU16BE += recvbuf[i + 1] & 0xff;
				//wchar_t test = (recvbuf[i] << 8) | (recvbuf[i + 1]);
				//wstring chr = std::wstring_convert< std::codecvt_utf8<wchar_t>, wchar_t >{}.from_bytes(chrU16BE);
				//setlocale(LC_ALL, "");
				//printf_s("%ls", chr.c_str());
				//pwin->call_function("DebugLog", chr.c_str());
				//allRecv += std::wstring_convert< std::codecvt_utf8<wchar_t>, wchar_t >{}.to_bytes(chr);
				//i++;
				//continue;
			//}
			pwin->call_function("DebugLog", sciter::value((char)recvbuf[i]));
			allRecv += recvbuf[i];

		}
		pwin->call_function("DebugLog", "\n");
		if (iResult != 1024)
			break;
		iResult = recv(ClientSocket, (char*)recvbuf, 1024, 0);
	}
	if (iResult == SOCKET_ERROR) {
		string logstr = "接收失败";
		logstr += WSAGetLastError();
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		iGetFlag = FRECVFAILED;
		return 0;
	}
	pwin->call_function("DebugLog", "\n-----------------------------\n");
	if ((allRecv.length() != 0) && (allRecv.find("online") != allRecv.npos))
	{
		short iPos, iOnlinePos, iMaxPos, iMotdPos;
		iOnlinePos = allRecv.find("online");
		iPos = iOnlinePos + 8;
		pwin->call_function("DebugLog", "当前在线人数: ");
		while (allRecv[iPos] != '}' && allRecv[iPos] != ',')
		{
			m_status.onlinePlayer += allRecv[iPos];
			iPos++;
		}
		pwin->call_function("DebugLog", m_status.onlinePlayer);

		iMaxPos = allRecv.find("max");
		iPos = iMaxPos + 5;
		pwin->call_function("DebugLog", " / ");
		while (allRecv[iPos] != '}' && allRecv[iPos] != ',')
		{
			m_status.maxPlayer += allRecv[iPos];
			iPos++;
		}
		pwin->call_function("DebugLog", m_status.maxPlayer);

		pwin->call_function("DebugLog", "\n服务器标语: ");
		iMotdPos = allRecv.find("text");
		iPos = iMotdPos + 7;
		while (allRecv[iPos] != '}' && allRecv[iPos] != '\"')
		{
			if ((byte(allRecv[iPos]) == 0xc2) && (byte(allRecv[iPos + 1]) == 0xa7)) //过滤掉UTF-8字符§和颜色代码
			{
				iPos = iPos + 3;
				continue;
			}
			m_status.motd += allRecv[iPos];
			iPos++;
		}
		pwin->call_function("DebugLog", m_status.motd);
		pwin->call_function("DebugLog", "\n-----------------------------\n");
	}
	//printf_s("%s %d", WsGetErrorInfo(), WSAGetLastError());
	pwin->call_function("DebugLog", "获取成功！\n");
	closesocket(ClientSocket);
	iGetFlag = FSUCCESSFUL;
	iGetFlag = FSUCCESSFUL;
	return true;
}

string CwssRecver::GetOnlinePlayer()
{
	return m_status.onlinePlayer;
}

string CwssRecver::GetMaxPlayer()
{
	return m_status.maxPlayer;
}

string CwssRecver::GetMotd()
{
	return m_status.motd;
}

string CwssRecver::GetLastStateInfo()
{
	string info;
	switch (iGetFlag)
	{
	case CwssRecver::FSUCCESSFUL:
		info = "一切正常";
		break;
	case CwssRecver::FRAW:
		info = "尚未初始化";
		break;
	case CwssRecver::FINITFAILED:
		info = "套接字库初始化失败";
		break;
	case CwssRecver::FCSFAILED:
		info = "套接字创建失败";
		break;
	case CwssRecver::FGHFAILED:
		info = "域名解析失败";
		break;
	case CwssRecver::FCONFAILED:
		info = "连接失败";
		break;
	case CwssRecver::FSENDFAILED:
		info = "发送失败";
		break;
	case CwssRecver::FRECVFAILED:
		info = "接收失败";
		break;
	default:
		info = "QWQ，我被一个状态判断给难倒了";
		break;
	}
	return info;
}

bool CwssRecver::init()
{
	WSADATA wsaData;
	int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);//用于检测函数状态
	if (iResult != 0) {
		string logstr = "套接字库初始化失败 : ";
		logstr += iResult;
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		iGetFlag = FINITFAILED;
		return false;
	}
	ZeroMemory(&m_status, sizeof(m_status));
	iGetFlag = FSUCCESSFUL;
	pwin->call_function("DebugLog", "已初始化套接字库\n");
	return true;
}
