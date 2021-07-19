#include "CwssRecver.h"


/********************
	全局变量声明区
********************/
frame *pwin;
CwssRecver g_cr = CwssRecver();
/********************
	公共方法定义区
********************/
//调用前端方法，解析json字符串
wstring GetJsonFieldFromJsonString(string json, string jsonField)
{
	return pwin->call_function("GetJsonFieldFromJsonString", json.c_str(), jsonField.c_str()).to_string().c_str();
}
wstring GetJsonFieldFromJsonString(wstring json, string jsonField)
{
	return pwin->call_function("GetJsonFieldFromJsonString", json.c_str(), jsonField.c_str()).to_string().c_str();
}

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

std::wstring CharToWchar(const char* c, size_t m_encode = CP_ACP)
{
	std::wstring str;
	int len = MultiByteToWideChar(m_encode, 0, c, strlen(c), NULL, 0);
	wchar_t*	m_wchar = new wchar_t[len + 1];
	MultiByteToWideChar(m_encode, 0, c, strlen(c), m_wchar, len);
	m_wchar[len] = '\0';
	str = m_wchar;
	delete m_wchar;
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
		sprintf_s(pBuf, bufSize, "%0.*ws", bufSize, pTemp);
		LocalFree((HLOCAL)pTemp);
	}
	return pBuf;
}

char * WsGetErrorInfo(DWORD err)
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
		err,
		LANG_NEUTRAL,
		(LPTSTR)&pTemp,
		0,
		NULL);
	if (retSize > 0) {
		pTemp[wcslen(pTemp) - 2] = '\0'; //remove cr and newline character
		sprintf_s(pBuf, bufSize, "%0.*ws", bufSize, pTemp);
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
uint8_t varint_code_len(string data)
{
	uint8_t i = 0;
	while (data[i++] >> 7);
	return i;
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
	is_getting = CreateMutex(NULL, FALSE, NULL);
}


CwssRecver::~CwssRecver()
{
	if (is_inited)
	{
		WSACleanup();
	}
	CloseHandle(is_getting);
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
		logstr += WcharToChar(gai_strerror(iResult));
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		cr->iGetFlag = cr->FGHFAILED;
		cr->m_errorMsg = logstr;
		return false;
	}
	if (result == nullptr)
	{
		string logstr = "域名解析失败 : ";
		logstr += "result:null";
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		cr->iGetFlag = cr->FGHFAILED;
		cr->m_errorMsg = logstr;
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
			cr->m_errorMsg = logstr;
			return false;
		}
		ClientSocket = socket(pres->ai_family, pres->ai_socktype, pres->ai_protocol);
		if (ClientSocket == INVALID_SOCKET) {
			string logstr = "创建套接字失败 : ";
			logstr +=  WsGetErrorInfo(WSAGetLastError());
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			freeaddrinfo(result);
			cr->iGetFlag = cr->FCSFAILED;
			cr->m_errorMsg = logstr;
			return false;
		}

		iResult = connect(ClientSocket, pres->ai_addr, (int)pres->ai_addrlen);
		if (iResult == SOCKET_ERROR) {
			string logstr;
			/*待优化*/
			char addrStr[INET_ADDRSTRLEN];
			const char *paddrStr = inet_ntop(AF_INET, &((SOCKADDR_IN*)&(pres->ai_addr))->sin_addr, addrStr, sizeof(addrStr));
			logstr += WsGetErrorInfo(WSAGetLastError());
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
	char* portStrHex = new char[16]{0};
	short port = short(atoi(serverPort.c_str()));
	//sprintf_s(portStrHex,15, "%x", port);
	portStrHex[0] = port >> 8;
	portStrHex[1] = port & 0x00FF;
	portStrHex[2] = '\x01';
	packstr += portStrHex;//包装端口信息
	delete[] portStrHex;
	packstr = pack_data(packstr);
	memcpy_s(str, 64, packstr.c_str(), packstr.length());
	iResult = send(ClientSocket, (char*)str, packstr.length(), 0);
	pwin->call_function("DebugLog", WsGetErrorInfo());
	cr->iGetFlag = cr->FSENDFAILED;
	if (iResult == SOCKET_ERROR) {
		string logstr = "发送失败! ";
		logstr += WsGetErrorInfo(WSAGetLastError());
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		cr->iGetFlag = cr->FSENDFAILED;
		cr->m_errorMsg = logstr;
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
		logstr += WsGetErrorInfo(WSAGetLastError());
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		cr->iGetFlag = cr->FSENDFAILED;
		cr->m_errorMsg = logstr;
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
#ifdef  _DEBUG
	byte* temp = new byte[1024 * 64];
	unsigned int tempIndex = 0;
#endif
	string ascAllRecv; //用于接收字符串
	wstring allRecv; //用于保存转码后的字符串
	iResult = 1;
	string msgLen; //数据包长度
	//Server返回的数据包中会包含数据包长度前缀，这段代码表示去除前缀，直到遇到json文本开头的{
	for (int i = 0; iResult > 0 && recvbuf[0] != '{'; i++)
	{
		iResult = recv(ClientSocket, (char*)recvbuf, 1, 0);
		msgLen += recvbuf[0];
	}
	uint8_t varintLen = varint_code_len(msgLen);
	int varintVal;
	ascAllRecv += recvbuf[0];
#ifdef  _DEBUG
	temp[tempIndex++] = recvbuf[0];
#endif
	iResult = recv(ClientSocket, (char*)recvbuf, 1024, 0);
	bool bIsFirstRecv = true;
	while (iResult > 0)
	{
		string logstr = "接收成功！ iResult";
		logstr += to_string(iResult);
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		for (int i = 0; i < iResult; i++)
		{
			ascAllRecv += recvbuf[i];
#ifdef  _DEBUG
			temp[tempIndex] = recvbuf[i];
			tempIndex++;
#endif
		}
		if (iResult < 100 && !bIsFirstRecv)//小于100且不是第一次接收说明是最后一次发送
			break;
		iResult = recv(ClientSocket, (char*)recvbuf, 1024, 0);
		bIsFirstRecv = false;
	}
	if (iResult == SOCKET_ERROR) {
		string logstr = "接收失败";
		logstr += WsGetErrorInfo(WSAGetLastError());
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		closesocket(ClientSocket);
		cr->iGetFlag = cr->FRECVFAILED;
		cr->m_errorMsg = logstr;
		return false;
	}
#ifdef  _DEBUG
	{
		FILE *fp;
		string faviconPath = getCurrentWorkDir() + "\\ServerIcons\\";
		if (_access(faviconPath.c_str(), 0) == -1)//文件夹不存在时，创建文件夹
			_mkdir(faviconPath.c_str());
		faviconPath = faviconPath + serverAddr.c_str() + ".txt";//将img写入到文件中
		fopen_s(&fp, faviconPath.c_str(), "wb");
		for (unsigned int i = 0; i < tempIndex; i++)
		{
			fprintf_s(fp, "%c", temp[i]);
		}
		fclose(fp);
	}
#endif
	allRecv = UTF8ToUnicode(ascAllRecv);
	pwin->call_function("DebugLog", "\n-----------------------------\n");
	pwin->call_function("DebugLog", sciter::value(allRecv));
	pwin->call_function("DebugLog", "\n-----------------------------\n");
#ifdef  _DEBUG
	delete[] temp;
#endif
	if ((allRecv.length() != 0) && (allRecv.find(L"online") != allRecv.npos))//获取服务器信息
	{
		cr->m_status.onlinePlayer = GetJsonFieldFromJsonString(GetJsonFieldFromJsonString(allRecv, "players"),"online").c_str();
		cr->m_status.maxPlayer = GetJsonFieldFromJsonString(GetJsonFieldFromJsonString(allRecv, "players"), "max").c_str();
		wstring motdJson = GetJsonFieldFromJsonString(allRecv, "description");
		cr->m_status.motd += GetJsonFieldFromJsonString(motdJson, "text");
		if(cr->m_status.motd == L"Can't find this field") //有的服务器没有text字段，而用translate字段代替，这里取出translate字段
		{
			cr->m_status.motd.clear();
			cr->m_status.motd += GetJsonFieldFromJsonString(motdJson, "translate");
			cr->m_status.motd = cr->m_status.motd.substr(1, cr->m_status.motd.length() - 2);//去除首尾引号
		}
		else
		{
			cr->m_status.motd = (cr->m_status.motd == L"\"\"")? L"": cr->m_status.motd.substr(1, cr->m_status.motd.length() - 2);
			//如果text为"",则直接清空，否则去除首尾引号
			
		}
		if (pwin->call_function("GetMotdType", motdJson) == L"array")
		{
			wstring motdExtra = pwin->call_function("TranslateExtraMotd", motdJson).to_string().c_str();
			cr->m_status.motd += motdExtra;
		}
	}
	wstring faviconBase64 = pwin->call_function("GetJsonFieldFromJsonString", allRecv.c_str(), "favicon").to_string().c_str();
	faviconBase64 = faviconBase64.substr(1, faviconBase64.length() - 2);//去除首尾引号
	int iBase64Begin = faviconBase64.find_first_of(',') + 1;
	faviconBase64 = faviconBase64.substr(iBase64Begin, faviconBase64.length() - iBase64Begin + 1);//取出img中的base64部分
	iBase64Begin = faviconBase64.find(L"\\n");
	while(iBase64Begin != faviconBase64.npos)//有的服务器真的奇葩，返回的favicon中包含换行符，此处代码用于去除换行符
	{
		faviconBase64.replace(iBase64Begin, 2, L"");
		iBase64Begin = faviconBase64.find(L"\\n");
	}
	string imgData = base64_decode(WcharToChar(faviconBase64.c_str()));//base64解码
	FILE *fp;
	string faviconPath = getCurrentWorkDir() + "\\ServerIcons\\";
	if (_access(faviconPath.c_str(), 0) == -1)//文件夹不存在时，创建文件夹
		_mkdir(faviconPath.c_str());
	faviconPath = faviconPath + serverAddr.c_str() + ".png";//将img写入到文件中
	fopen_s(&fp, faviconPath.c_str(), "wb");
	for (unsigned int i = 0; i < imgData.length(); i++)
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

void ThFunc_AsyncGet(CwssRecver *cr,string serverAddr,string serverPort, bool isRefresh = false/* 是否为刷新操作 */) {
	WaitForSingleObject(cr->is_getting, INFINITE); //解决多个线程同时获取服务器信息时显示错乱的问题
	bool flag = true;
	cr->StatusClear();//清空上一次获取的信息
	flag = GetServerInfo(cr, serverAddr, serverPort);//获取信息
	if(!isRefresh)
	{
		if (!flag)
		{
			string motdTemp = serverAddr + ":" + serverPort;
			motdTemp += " 服务器信息获取异常 ";
			motdTemp += cr->GetLastStateInfo();
			pwin->call_function("AddServerInfo", "ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">离线</span>", sciter::value(motdTemp), sciter::value("/ QWQ/"));
			pwin->call_function("DebugLog", cr->GetLastStateInfo());
			return;
		}
		if (cr->GetOnlinePlayer().length() == 0)
		{
			string motdTemp = serverAddr + ":" + serverPort;
			motdTemp += " 服务器信息不完整 ";
			motdTemp += cr->GetLastStateInfo();
			pwin->call_function("AddServerInfo", "ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">离线</span>", sciter::value(motdTemp), sciter::value("/ QWQ/"));
		}
		else
		{
			wstring playerNum = cr->GetOnlinePlayer() + L" / " + cr->GetMaxPlayer();
			pwin->call_function("AddServerInfo", sciter::value(cr->GetFavicon()), "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:green;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">在线</span>", sciter::value(cr->GetMotd()), playerNum);
		}
	}
	else
	{
		string val = serverAddr + ':' + serverPort;//完整的地址与在线人数的合并，地址用于标记要刷新的服务器状态 e.g. localhost:25565
													//合并是因为后端只可传四个参数给前端,其实可以传多个参数，下个版本搞
		if (!flag)
		{
			string motdTemp = val;
			motdTemp += " 服务器信息获取异常 ";
			motdTemp += cr->GetLastStateInfo();
			val += ",/QWQ/";
			pwin->call_function("EditServerInfo", sciter::value(val), "ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">离线</span>", sciter::value(motdTemp));
			pwin->call_function("DebugLog", cr->GetLastStateInfo());
			return;
		}
		if (cr->GetOnlinePlayer().length() == 0)
		{
			string motdTemp = val;
			val += ",/QWQ/";	
			motdTemp += " 服务器信息不完整 ";
			pwin->call_function("EditServerInfo", sciter::value(val),"ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">离线</span>", sciter::value(motdTemp));
		}
		else
		{
			wstring playerNum = cr->GetOnlinePlayer() + L" / " + cr->GetMaxPlayer();
			val += ',' + WcharToChar(playerNum.c_str());
			pwin->call_function("EditServerInfo", sciter::value(val), sciter::value(cr->GetFavicon()), "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:green;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">在线</span>", sciter::value(cr->GetMotd()));
		}
	}
	ReleaseMutex(cr->is_getting);
}
bool CwssRecver::AsyncGet(string serverAddr, string serverPort, bool isRefresh = false/* 是否为刷新操作 */)
{
	thread t = thread(ThFunc_AsyncGet,this, serverAddr, serverPort, isRefresh);
	t.detach();
	return true;
}



sciter::value frame::DoTask(sciter::value serverAddr, sciter::value serverPort, sciter::value isRefresh/* 是否为刷新操作 */)
{
	if (!g_cr.init())
	{
		pwin->call_function("DebugLog", g_cr.GetLastStateInfo());
	}
	bool bRefresh = (isRefresh.to_string() == L"F") ? false : true;
	g_cr.AsyncGet(WcharToChar(serverAddr.to_string().c_str()), WcharToChar(serverPort.to_string().c_str()), bRefresh);
	return sciter::value("End Task");
}

sciter::value frame::SaveServerList(sciter::value json)
{
	FILE* fp;
	string jsonPath = getCurrentWorkDir() + "\\serverList.json";
	fopen_s(&fp, jsonPath.c_str(), "wb");
	fprintf_s(fp, "%ws", json.to_string().c_str());
	fclose(fp);
	return sciter::value();
}

sciter::value frame::GetServerList()
{
	string jsonPath = getCurrentWorkDir() + "\\serverList.json";
	if (_access(jsonPath.c_str(), 0) == -1)
	{
		return sciter::value("NULL");
	}
	FILE* fp;
	fopen_s(&fp, jsonPath.c_str(), "rb");
	char json[1024*8];
	fscanf_s(fp, "%s", json,1024*8);
	fclose(fp);
	return sciter::value(json);
}



wstring CwssRecver::GetOnlinePlayer()
{
	return m_status.onlinePlayer;
}

wstring CwssRecver::GetMaxPlayer()
{
	return m_status.maxPlayer;
}

wstring CwssRecver::GetMotd()
{
	return m_status.motd;
}

string CwssRecver::GetFavicon()
{
	return m_status.favicon;;
}

void CwssRecver::StatusClear()
{
	m_status.favicon.clear();
	m_status.maxPlayer.clear();
	m_status.motd.clear();
	m_status.onlinePlayer.clear();
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
	info += " 详细信息:" + m_errorMsg;
	m_errorMsg.clear();
	return info;
}

bool CwssRecver::init()
{
	if (!is_inited)//避免重复初始化
	{
		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);//用于检测函数状态
		if (iResult != 0) {
			string logstr = "套接字库初始化失败 : ";
			logstr += iResult;
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			iGetFlag = FINITFAILED;
			m_errorMsg = logstr;
			return false;
		}
		ZeroMemory(&m_status, sizeof(m_status));
		iGetFlag = FSUCCESSFUL;
		pwin->call_function("DebugLog", "已初始化套接字库\n");
		is_inited = true;
		return true;
	}
	return true;
}
