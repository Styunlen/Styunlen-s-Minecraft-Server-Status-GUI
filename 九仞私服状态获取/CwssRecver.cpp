#include "CwssRecver.h"


/********************
	ȫ�ֱ���������
********************/
frame *pwin;
CwssRecver g_cr = CwssRecver();
/********************
	��������������
********************/
//����ǰ�˷���������json�ַ���
wstring GetJsonFieldFromJsonString(string json, string jsonField)
{
	return pwin->call_function("GetJsonFieldFromJsonString", json.c_str(), jsonField.c_str()).to_string().c_str();
}
wstring GetJsonFieldFromJsonString(wstring json, string jsonField)
{
	return pwin->call_function("GetJsonFieldFromJsonString", json.c_str(), jsonField.c_str()).to_string().c_str();
}

//��ȡ��ǰ����Ŀ¼
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
void varint_decode(string data, uint32_t* val, uint8_t len)
{
	int i = 0;
	int offset = 0;
	uint32_t result = 0;
	result |= ((uint32_t)data[len - 1]) << (7 * (len - 1));
	for (i = 0, offset = 0; i < len - 1; i++, offset += 7)
		result |= (uint32_t)(data[i] & 0x7f) << offset;
	*val = result;
}

string pack_data(string d)
{
	return pack_varint(d.length()) + d;
}

/********************
	�෽��������
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
//���ڻ�ȡ�����У���Щ����ʧ����Ҫֱ��return�����������߳���ֱ��ruturn���޷����´����еķ�������Ϣ����˽��˹��̶�����һ���º���
bool GetServerInfo(CwssRecver *cr, string serverAddr, string serverPort) {
	struct addrinfo *result = NULL, hints, *pres;
	ZeroMemory(&hints, sizeof(hints));
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = IPPROTO_TCP;
	int iResult;
	//�׽���������Ϣ
	iResult = getaddrinfo(serverAddr.c_str(), serverPort.c_str(), &hints, &result);
	if (iResult != 0) {
		string logstr = "��������ʧ�� : ";
		logstr += WcharToChar(gai_strerror(iResult));
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
		cr->iGetFlag = cr->FGHFAILED;
		cr->m_errorMsg = logstr;
		return false;
	}
	if (result == nullptr)
	{
		string logstr = "��������ʧ�� : ";
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
			string logstr = "�ѱ��������µ����н���������ʧ��";
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			cr->m_errorMsg = logstr;
			return false;
		}
		ClientSocket = socket(pres->ai_family, pres->ai_socktype, pres->ai_protocol);
		if (ClientSocket == INVALID_SOCKET) {
			string logstr = "�����׽���ʧ�� : ";
			logstr += WsGetErrorInfo(WSAGetLastError());
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
			/*���Ż�*/
			char addrStr[INET_ADDRSTRLEN];
			const char *paddrStr = inet_ntop(AF_INET, &((SOCKADDR_IN*)&(pres->ai_addr))->sin_addr, addrStr, sizeof(addrStr));
			logstr += WsGetErrorInfo(WSAGetLastError());
			logstr += paddrStr;
			logstr += " ����ʧ��";
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			closesocket(ClientSocket);
			cr->iGetFlag = cr->FCONFAILED;
			pres = pres->ai_next;
			continue;

		}
		else
		{
			pwin->call_function("DebugLog", "���ӳɹ���\n");
			break;
		}
	}
	freeaddrinfo(result);
	char sendbuf[512]{ 0 };
	char str[64]{ 0 };
	//HandShake Packet Э��Wiki https://wiki.vg/Server_List_Ping#1.6
	string packstr;
	packstr += '\x00';
	packstr += '\x00';
	packstr += pack_data(serverAddr.c_str()); //��װ������Ϣ
	char* portStrHex = new char[16]{ 0 };
	short port = short(atoi(serverPort.c_str()));
	//sprintf_s(portStrHex,15, "%x", port);
	portStrHex[0] = port >> 8;
	portStrHex[1] = port & 0x00FF;
	portStrHex[2] = '\x01';
	packstr += portStrHex;//��װ�˿���Ϣ
	delete[] portStrHex;
	packstr = pack_data(packstr);
	memcpy_s(str, 64, packstr.c_str(), packstr.length());
	iResult = send(ClientSocket, (char*)str, packstr.length(), 0);
	
	pwin->call_function("DebugLog", WsGetErrorInfo());
	cr->iGetFlag = cr->FSENDFAILED;
	if (iResult == SOCKET_ERROR) {
		string logstr = "����ʧ��! ";
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
		string logstr = "���ͳɹ�! ";
		logstr += iResult;
		logstr += " \n";
		pwin->call_function("DebugLog", logstr);
	}
	//string strRequest = pack_data("\x0");

	//iResult = send(ClientSocket, strRequest.c_str(), strRequest.length(), 0);

	byte sendRequest[2];//Request Packet
	sendRequest[0] = 0x1;
	sendRequest[1] = 0x0;
	iResult = send(ClientSocket, (char*)sendRequest, 2, 0);
	printf_s(WsGetErrorInfo());
	if (iResult == SOCKET_ERROR) {
		string logstr = "����ʧ��! ";
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
		string logstr = "���ͳɹ�! ";
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
	string ascAllRecv; //���ڽ���JSON�ַ���
	wstring allRecv; //���ڱ���ת�����ַ���
	iResult = 1;
	string msgLen; //���ݰ�����
				   //Server���ص����ݰ��л�������ݰ�����ǰ׺����δ����ʾȥ��ǰ׺��ֱ������json�ı���ͷ��{
	//������Response���������ֶ� Packet,Len Packet ID(IDһ��Ϊ0),Json Len
	for (int i = 0; iResult > 0; i++)
	{
		iResult = recv(ClientSocket, (char*)recvbuf, 1, 0);
		if (recvbuf[0] != '{') {
			msgLen += recvbuf[0];
		}
		else
		{
			break;
		}
		if (recvbuf[0] == 0x00)
			msgLen.clear();//���˵�Packet Length��Packet ID��ֻ����Json Length
	}
	uint8_t varintLen = varint_code_len(msgLen);
	unsigned int varintVal;
	varint_decode(msgLen, &varintVal, varintLen);
	ascAllRecv += recvbuf[0];
#ifdef  _DEBUG
	temp[tempIndex++] = recvbuf[0];
#endif
	iResult = recv(ClientSocket, (char*)recvbuf, 1024, 0);
	unsigned long recvTotLen = 1;//��ǰ�����Json Length��ʱ���Ѿ�������һ���ַ�'{'
	while (iResult > 0)
	{
		recvTotLen += iResult;
		string logstr = "���ճɹ��� iResult ";
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
			//BUG,��ͬ��MC������ͨѶЭ�鲻ͬ�����͵����ݰ��ܳ�������varint��֪
			//if (iResult < 100)//С��100�Ҳ��ǵ�һ�ν���˵�������һ�η���
			//	break;
		}
		if (recvTotLen >= varintVal) break;
		iResult = recv(ClientSocket, (char*)recvbuf, 1024, 0);
	}
	if (iResult == SOCKET_ERROR) {
		string logstr = "����ʧ��";
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
			if (_access(faviconPath.c_str(), 0) == -1)//�ļ��в�����ʱ�������ļ���
				_mkdir(faviconPath.c_str());
			faviconPath = faviconPath + serverAddr.c_str() + ".txt";//��imgд�뵽�ļ���
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
	if ((allRecv.length() != 0) && (allRecv.find(L"online") != allRecv.npos))//��ȡ��������Ϣ
	{
		cr->m_status.onlinePlayer = GetJsonFieldFromJsonString(GetJsonFieldFromJsonString(allRecv, "players"), "online").c_str();
		cr->m_status.maxPlayer = GetJsonFieldFromJsonString(GetJsonFieldFromJsonString(allRecv, "players"), "max").c_str();
		wstring motdJson = GetJsonFieldFromJsonString(allRecv, "description");
		cr->m_status.motd += GetJsonFieldFromJsonString(motdJson, "text");
		if (cr->m_status.motd == L"Can't find this field") //�еķ�����û��text�ֶΣ�����translate�ֶδ��棬����ȡ��translate�ֶ�
		{
			cr->m_status.motd.clear();
			cr->m_status.motd += GetJsonFieldFromJsonString(motdJson, "translate");
			cr->m_status.motd = cr->m_status.motd.substr(1, cr->m_status.motd.length() - 2);//ȥ����β����
		}
		else
		{
			cr->m_status.motd = (cr->m_status.motd == L"\"\"") ? L"" : cr->m_status.motd.substr(1, cr->m_status.motd.length() - 2);
			//���textΪ"",��ֱ����գ�����ȥ����β����

		}
		if (pwin->call_function("GetMotdType", motdJson) == L"array")
		{
			wstring motdExtra = pwin->call_function("TranslateExtraMotd", motdJson).to_string().c_str();
			cr->m_status.motd += motdExtra;
		}
	}
	wstring faviconBase64 = pwin->call_function("GetJsonFieldFromJsonString", allRecv.c_str(), "favicon").to_string().c_str();
	faviconBase64 = faviconBase64.substr(1, faviconBase64.length() - 2);//ȥ����β����
	int iBase64Begin = faviconBase64.find_first_of(',') + 1;
	faviconBase64 = faviconBase64.substr(iBase64Begin, faviconBase64.length() - iBase64Begin + 1);//ȡ��img�е�base64����
	iBase64Begin = faviconBase64.find(L"\\n");
	while (iBase64Begin != faviconBase64.npos)//�еķ�����������⣬���ص�favicon�а������з����˴���������ȥ�����з�
	{
		faviconBase64.replace(iBase64Begin, 2, L"");
		iBase64Begin = faviconBase64.find(L"\\n");
	}
	string imgData = base64_decode(WcharToChar(faviconBase64.c_str()));//base64����
	FILE *fp;
	string faviconPath = getCurrentWorkDir() + "\\ServerIcons\\";
	if (_access(faviconPath.c_str(), 0) == -1)//�ļ��в�����ʱ�������ļ���
		_mkdir(faviconPath.c_str());
	faviconPath = faviconPath + serverAddr.c_str() + ".png";//��imgд�뵽�ļ���
	fopen_s(&fp, faviconPath.c_str(), "wb");
	for (unsigned int i = 0; i < imgData.length(); i++)
	{
		fprintf_s(fp, "%c", imgData[i]);
	}
	fclose(fp);
	cr->m_status.favicon = faviconPath;
	OutputDebugStringW(faviconBase64.c_str());
	//fopen_s(&fp, "test.txt", "w");
	//fprintf_s(fp, "%ws", allRecv.c_str());
	//fclose(fp);
	closesocket(ClientSocket);
	cr->iGetFlag = cr->FSUCCESSFUL;
	return true;
}

void ThFunc_AsyncGet(CwssRecver *cr,string serverAddr,string serverPort, bool isRefresh = false/* �Ƿ�Ϊˢ�²��� */) {
	bool flag = true;
	cr->StatusClear();//�����һ�λ�ȡ����Ϣ
	srand(time(NULL));
	Sleep(rand() % 1000 + 1);
	flag = GetServerInfo(cr, serverAddr, serverPort);//��ȡ��Ϣ
	WaitForSingleObject(g_cr.is_getting, INFINITE); //�������߳�ͬʱ��ȡ��������Ϣʱ��ʾ���ҵ�����
	if(!isRefresh)
	{
		if (!flag)
		{
			string motdTemp = serverAddr + ":" + serverPort;
			motdTemp += " ��������Ϣ��ȡ�쳣 ";
			motdTemp += cr->GetLastStateInfo();
			pwin->call_function("AddServerInfo", "ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">����</span>", sciter::value(motdTemp), sciter::value("/ QWQ/"));
			pwin->call_function("DebugLog", cr->GetLastStateInfo());
			return;
		}
		if (cr->GetOnlinePlayer().length() == 0)
		{
			string motdTemp = serverAddr + ":" + serverPort;
			motdTemp += " ��������Ϣ������ ";
			motdTemp += cr->GetLastStateInfo();
			pwin->call_function("AddServerInfo", "ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">����</span>", sciter::value(motdTemp), sciter::value("/ QWQ/"));
		}
		else
		{
			wstring playerNum = cr->GetOnlinePlayer() + L" / " + cr->GetMaxPlayer();
			pwin->call_function("AddServerInfo", sciter::value(cr->GetFavicon()), "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:green;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">����</span>", sciter::value(cr->GetMotd()), playerNum);
		}
	}
	else
	{
		string val = serverAddr + ':' + serverPort;//�����ĵ�ַ�����������ĺϲ�����ַ���ڱ��Ҫˢ�µķ�����״̬ e.g. localhost:25565
													//�ϲ�����Ϊ���ֻ�ɴ��ĸ�������ǰ��,��ʵ���Դ�����������¸��汾��
		if (!flag)
		{
			string motdTemp = val;
			motdTemp += " ��������Ϣ��ȡ�쳣 ";
			motdTemp += cr->GetLastStateInfo();
			val += ",/QWQ/";
			pwin->call_function("EditServerInfo", sciter::value(val), "ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">����</span>", sciter::value(motdTemp));
			pwin->call_function("DebugLog", cr->GetLastStateInfo());
			return;
		}
		if (cr->GetOnlinePlayer().length() == 0)
		{
			string motdTemp = val;
			val += ",/QWQ/";	
			motdTemp += " ��������Ϣ������ ";
			pwin->call_function("EditServerInfo", sciter::value(val),"ImgErr.jpg", "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:red;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">����</span>", sciter::value(motdTemp));
		}
		else
		{
			wstring playerNum = cr->GetOnlinePlayer() + L" / " + cr->GetMaxPlayer();
			val += ',' + WcharToChar(playerNum.c_str());
			pwin->call_function("EditServerInfo", sciter::value(val), sciter::value(cr->GetFavicon()), "<div id=\"statusDot\" style=\"display:inline-block;position:relative;left:10px;width:10px;height:10px;border-radius:5px;background:green;\"></div><span style=\"text-align:left;padding-left:20px;display:inline-block;width:40px;\">����</span>", sciter::value(cr->GetMotd()));
		}
	}
	delete cr;
	ReleaseMutex(g_cr.is_getting);
}
bool CwssRecver::AsyncGet(string serverAddr, string serverPort, bool isRefresh = false/* �Ƿ�Ϊˢ�²��� */)
{
	//Debug Result: The limitation of thread appears to be uncertain.
	//���Խ������󲢷��߳�����Ӧ�������ƣ����������ܿ��ǣ��¸��汾�����󲢷��߳���������
	CwssRecver *cr = new CwssRecver();
	thread t = thread(ThFunc_AsyncGet, cr, serverAddr, serverPort, isRefresh);
	t.detach();
	return true;
}



sciter::value frame::DoTask(sciter::value serverAddr, sciter::value serverPort, sciter::value isRefresh/* �Ƿ�Ϊˢ�²��� */)
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
	return (strlen(json)==0)? sciter::value("NULL"): sciter::value(json);
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
		info = "һ������";
		break;
	case CwssRecver::FRAW:
		info = "��δ��ʼ��";
		break;
	case CwssRecver::FINITFAILED:
		info = "�׽��ֿ��ʼ��ʧ��";
		break;
	case CwssRecver::FCSFAILED:
		info = "�׽��ִ���ʧ��";
		break;
	case CwssRecver::FGHFAILED:
		info = "��������ʧ��";
		break;
	case CwssRecver::FCONFAILED:
		info = "����ʧ��";
		break;
	case CwssRecver::FSENDFAILED:
		info = "����ʧ��";
		break;
	case CwssRecver::FRECVFAILED:
		info = "����ʧ��";
		break;
	default:
		info = "QWQ���ұ�һ��״̬�жϸ��ѵ���";
		break;
	}
	info += " ��ϸ��Ϣ:" + m_errorMsg;
	m_errorMsg.clear();
	return info;
}

bool CwssRecver::init()
{
	if (!is_inited)//�����ظ���ʼ��
	{
		WSADATA wsaData;
		int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);//���ڼ�⺯��״̬
		if (iResult != 0) {
			string logstr = "�׽��ֿ��ʼ��ʧ�� : ";
			logstr += iResult;
			logstr += " \n";
			pwin->call_function("DebugLog", logstr);
			iGetFlag = FINITFAILED;
			m_errorMsg = logstr;
			return false;
		}
		ZeroMemory(&m_status, sizeof(m_status));
		iGetFlag = FSUCCESSFUL;
		pwin->call_function("DebugLog", "�ѳ�ʼ���׽��ֿ�\n");
		is_inited = true;
		return true;
	}
	return true;
}
