#include "CwssRecver.h"


/********************
	全局变量声明区
********************/
frame *pwin;
CwssRecver g_cr = CwssRecver();
/********************
	公共方法定义区
********************/
//获取当前工作目录
string	getCurrentWorkDir()
{
	static string ApplicationPath;
	if (ApplicationPath.length() != 0)
		return ApplicationPath;
	char * buffer = new char[MAX_PATH];
	_getcwd(buffer, MAX_PATH);
	ApplicationPath = buffer;
	delete[] buffer;
	return ApplicationPath;
}

std::string WcharToChar(const wchar_t* wp, size_t m_encode = CP_ACP)
{
	std::string str;
	int len = WideCharToMultiByte(m_encode, 0, wp, wcslen(wp), NULL, 0, NULL, NULL);
	char	*m_char = new char[len + 1];
	WideCharToMultiByte(m_encode, 0, wp, wcslen(wp), m_char, len, NULL, NULL);
	m_char[len] = '\0';
	str = m_char;
	delete m_char;
	return str;
}

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
		pTemp[wcslen(pTemp) - 2] = '\0'; //remove cr and newline character
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

void CwssRecver::SetFlag(int i)
{
}
//由于获取过程中，有些步骤失败需要直接return，而若在子线程中直接ruturn则无法更新窗口中的服务器信息，因此将此过程独立成一个新函数
bool GetServerInfo(CwssRecver *cr, string serverAddr, string serverPort) {
	struct addrinfo *result = NULL, hints, *pres;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	int iResult;
	//套接字连接信息
	iResult = getaddrinfo(serverAddr.c_str(), serverPort.c_str(), &hints, &result);
	if (iResult != 0) {
		string logstr = "域名解析失败 : ";
		logstr += iResult;
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		cr->iGetFlag = cr->FGHFAILED;
		return false;
	}
	if (result == nullptr)
	{
		string logstr = "域名解析失败 : ";
		logstr += "result:null";
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		cr->iGetFlag = cr->FGHFAILED;
		return false;
	}
	SOCKET ClientSocket = INVALID_SOCKET;
	pres = result;
	while (true)
	{
		if (pres == nullptr)
		{
			freeaddrinfo(result);
			string logstr = "已遍历域名下的所有解析，连接失败";
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			return false;
		}
		ClientSocket = socket(pres->ai_family, pres->ai_socktype, pres->ai_protocol);
		if (ClientSocket == INVALID_SOCKET) {
			string logstr = "创建套接字失败 : ";
			logstr += WSAGetLastError();
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			freeaddrinfo(result);
			cr->iGetFlag = cr->FCSFAILED;
			return false;
		}

		iResult = connect(ClientSocket, pres->ai_addr, (int)pres->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			string logstr;
			/*待优化*/
			char addrStr[INET_ADDRSTRLEN];
			const char *paddrStr = inet_ntop(AF_INET, &((SOCKADDR_IN*)&(pres->ai_addr))->sin_addr, addrStr, sizeof(addrStr));
			logstr += WSAGetLastError();
			logstr += paddrStr;
			logstr += " 连接失败";
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			closesocket(ClientSocket);
			cr->iGetFlag = cr->FCONFAILED;
			pres = pres->ai_next;
			continue;

		}
		else
		{
			pwin->call_function("DebugLog", "连接成功！\n");
			break;
		}
	}
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
	packstr += pack_data(serverAddr.c_str()); //包装域名信息
	packstr += "\xfe\x1b\x01";//端口65051的十六进制表示
	short test = 65051;
	packstr = pack_data(packstr);
	memcpy_s(str, 64, packstr.c_str(), packstr.length());
	iResult = send(ClientSocket, (char*)str, packstr.length(), 0);
	pwin->call_function("DebugLog", WsGetErrorInfo());
	cr->iGetFlag = cr->FSENDFAILED;
	if (iResult == SOCKET_ERROR) {
		string logstr = "发送失败! ";
		logstr += WSAGetLastError();
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		cr->iGetFlag = cr->FSENDFAILED;
		return false;
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
		cr->iGetFlag = cr->FSENDFAILED;
		return false;
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
	wstring allRecv;
	iResult = 1;
	//Server返回的数据包中会包含数据包长度前缀，这段代码表示去除前缀，直到遇到json文本开头的{
	for (int i = 0; iResult > 0 && recvbuf[0] != '{'; i++)
	{
		iResult = recv(ClientSocket, (char*)recvbuf, 1, 0);
	}
	allRecv += recvbuf[0];
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
			//pwin->call_function("DebugLog", sciter::value((char)recvbuf[i]));
			allRecv += recvbuf[i];

		}
		//pwin->call_function("DebugLog", "\n");
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
		cr->iGetFlag = cr->FRECVFAILED;
		return false;
	}
	pwin->call_function("DebugLog", "\n-----------------------------\n");
	if ((allRecv.length() != 0) && (allRecv.find(L"online") != allRecv.npos))
	{
		short iPos, iOnlinePos, iMaxPos, iMotdPos;
		iOnlinePos = allRecv.find(L"online");
		iPos = iOnlinePos + 8;
		pwin->call_function("DebugLog", "当前在线人数: ");
		while (allRecv[iPos] != '}' && allRecv[iPos] != ',')
		{
			cr->m_status.onlinePlayer += allRecv[iPos];
			iPos++;
		}
		pwin->call_function("DebugLog", cr->m_status.onlinePlayer);

		iMaxPos = allRecv.find(L"max");
		iPos = iMaxPos + 5;
		pwin->call_function("DebugLog", " / ");
		while (allRecv[iPos] != '}' && allRecv[iPos] != ',')
		{
			cr->m_status.maxPlayer += allRecv[iPos];
			iPos++;
		}
		pwin->call_function("DebugLog", cr->m_status.maxPlayer);

		pwin->call_function("DebugLog", "\n服务器标语: ");
		iMotdPos = allRecv.find(L"text");
		iPos = iMotdPos + 7;
		while (allRecv[iPos] != '}' && allRecv[iPos] != '\"')
		{
			if ((byte(allRecv[iPos]) == 0xc2) && (byte(allRecv[iPos + 1]) == 0xa7)) //过滤掉UTF-8字符§和颜色代码
			{
				iPos = iPos + 3;
				continue;
			}
			cr->m_status.motd += allRecv[iPos];
			iPos++;
		}
		pwin->call_function("DebugLog", cr->m_status.motd);
		pwin->call_function("DebugLog", "\n-----------------------------\n");
	}
	//printf_s("%s %d", WsGetErrorInfo(), WSAGetLastError());
	pwin->call_function("DebugLog", "获取成功！\n");
	wstring faviconBase64 = pwin->call_function("GetJsonFieldFromJsonString", allRecv.c_str(), "favicon").to_string().c_str();
	faviconBase64 = faviconBase64.substr(1, faviconBase64.length() - 2);//去除首尾引号
	int iBase64Begin = faviconBase64.find_first_of(',') + 1;
	faviconBase64 = faviconBase64.substr(iBase64Begin, faviconBase64.length() - iBase64Begin + 1);//取出img中的base64部分
	string imgData = base64_decode(WcharToChar(faviconBase64.c_str()));
	FILE *fp;
	string faviconPath = getCurrentWorkDir() + "\\ServerIcons\\" + serverAddr.c_str() + ".png";
	fopen_s(&fp, faviconPath.c_str(), "wb");
	for (int i = 0; i < imgData.length(); i++)
	{
		fprintf_s(fp, "%c", imgData[i]);
	}
	fclose(fp);
	cr->m_status.favicon = faviconPath;
	OutputDebugStringW(faviconBase64.c_str());
	fopen_s(&fp,"test.txt", "w");
	fprintf_s(fp, "%ws", allRecv.c_str());
	fclose(fp);
	closesocket(ClientSocket);
	cr->iGetFlag = cr->FSUCCESSFUL;
	return true;
}
void ThFunc_AsyncGet(CwssRecver *cr,string serverAddr,string serverPort) {
	bool flag = true;
	flag = GetServerInfo(cr, serverAddr, serverPort);
	if (!flag)
	{
		pwin->call_function("DebugLog", cr->GetLastStateInfo());
		pwin->call_function("SetSerStatus", sciter::value("<div id=\"statusDot\" style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"padding-left:10px;display:inline-block;width:80px;\">离线</span>"));
		pwin->call_function("SetPlayerNum", sciter::value("/ QWQ"), sciter::value(cr->GetMaxPlayer()));
		pwin->call_function("SetMotd", sciter::value(cr->GetLastStateInfo()));
		return;
	}
	if (cr->GetOnlinePlayer().length() == 0)
		pwin->call_function("SetSerStatus", sciter::value("<div id=\"statusDot\" style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"padding-left:10px;display:inline-block;width:80px;\">离线</span>"));
	else
		pwin->call_function("SetSerStatus", sciter::value("<div id=\"statusDot\" style=\"display:inline-block;width:10px;height:10px;border-radius:5px;background:green;\"></div><span style=\"padding-left:10px;display:inline-block;width:80px;\">在线</span>"));
	pwin->call_function("SetPlayerNum", sciter::value(cr->GetOnlinePlayer()), sciter::value(cr->GetMaxPlayer()));
	pwin->call_function("SetMotd", sciter::value(cr->GetMotd()));
	pwin->call_function("SetFavicon", sciter::value(cr->GetFavicon()));

}
bool CwssRecver::AsyncGet(string serverAddr, string serverPort)
{
	thread t = thread(ThFunc_AsyncGet,this, serverAddr, serverPort);
	t.detach();
	return true;
}



sciter::value frame::DoTask(sciter::value serverAddr, sciter::value serverPort)
{
	if (!g_cr.init())
	{
		pwin->call_function("DebugLog", g_cr.GetLastStateInfo());
	}
	g_cr.AsyncGet(WcharToChar(serverAddr.to_string().c_str()), WcharToChar(serverPort.to_string().c_str()));
	return sciter::value("End Task");
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

string CwssRecver::GetFavicon()
{
	return m_status.favicon;;
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
