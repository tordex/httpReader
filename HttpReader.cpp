#include "curl/curl.h"
#include "HttpReader.h"
#include <Wininet.h>
#include <process.h>
#include <ShlObj.h>
#include <Shlwapi.h>

///////////////////////////// chBEGINTHREADEX Macro ///////////////////////////


// This macro function calls the C runtime's _beginthreadex function. 
// The C runtime library doesn't want to have any reliance on Windows' data 
// types such as HANDLE. This means that a Windows programmer needs to cast
// values when using _beginthreadex. Since this is terribly inconvenient, 
// I created this macro to perform the casting.
typedef unsigned (__stdcall *PTHREAD_START) (void *);

#define chBEGINTHREADEX(psa, cbStack, pfnStartAddr, \
	pvParam, fdwCreate, pdwThreadId)                 \
	((HANDLE)_beginthreadex(                      \
	(void *)        (psa),                     \
	(unsigned)      (cbStack),                 \
	(PTHREAD_START) (pfnStartAddr),            \
	(void *)        (pvParam),                 \
	(unsigned)      (fdwCreate),               \
	(unsigned *)    (pdwThreadId)))


CHttpReader::CHttpReader(void)
{
	m_session		= NULL;
	m_stop			= FALSE;
	m_DumpThread	= NULL;
	m_url			= NULL;
	m_useProxy		= NOPROXY;
	m_proxyHost		= NULL;
	m_proxyPort		= 0;
	m_proxyType		= PROXY_HTTP;
	m_proxyUser		= NULL;
	m_proxyPassword	= NULL;
	m_user			= NULL;
	m_password		= NULL;
	m_SendCookies	= FALSE;
	m_error			= 0;
	m_userAgent		= NULL;
}

CHttpReader::~CHttpReader(void)
{
	if(m_url)			delete m_url;
	if(m_proxyHost)		delete m_proxyHost;
	if(m_proxyUser)		delete m_proxyUser;
	if(m_proxyPassword)	delete m_proxyPassword;
	if(m_user)			delete m_user;
	if(m_password)		delete m_password;
	if(m_userAgent)		delete m_userAgent;
	WaitForEnd(INFINITE);
}

size_t CHttpReader::writeFunction( void *ptr, size_t size, size_t nmemb, void *stream )
{
	CHttpReader* pThis = (CHttpReader*) stream;
	pThis->m_szDownloaded += size * nmemb;
	if(!pThis->m_szTotal)
	{
		double sz = 0;
		curl_easy_getinfo((CURL*) pThis->m_session, CURLINFO_CONTENT_LENGTH_DOWNLOAD, &sz);
		pThis->m_szTotal = (size_t) sz;
	}
	pThis->OnData(ptr, size * nmemb, pThis->m_szDownloaded, pThis->m_szTotal);
	return size * nmemb;
}

void CHttpReader::OnData( void* data, size_t dataLen, size_t downloaded, size_t Total )
{
	
}

void CHttpReader::OnStatus( DWORD status )
{

}

void CHttpReader::OnError( DWORD error, LPWSTR errorString )
{

}

void CHttpReader::OnFinish()
{

}

DWORD WINAPI CHttpReader::DumpTrdProc( LPVOID lpParam )
{
	CHttpReader* pThis = (CHttpReader*) lpParam;
	pThis->Dump();
	return 0;
}

void CHttpReader::Stop( void )
{
	m_stop = TRUE;
}

int CHttpReader::progressFunction( void *clientp, double dltotal, double dlnow, double ultotal, double ulnow )
{
	CHttpReader* pThis = (CHttpReader*) clientp;
	return pThis->m_stop;
}

void CHttpReader::SetProxy( USEPROXY useProxy, PROXYTYPE proxyType, LPCWSTR proxyServer, long proxyPort, LPCWSTR proxyUser, LPCWSTR proxyPassword )
{
	if(m_proxyHost)		delete m_proxyHost;
	if(m_proxyUser)		delete m_proxyUser;
	if(m_proxyPassword) delete m_proxyPassword;
	m_proxyHost		= NULL;
	m_proxyUser		= NULL;
	m_proxyPassword = NULL;
	m_useProxy		= useProxy;
	m_proxyPort		= proxyPort;
	m_proxyType		= proxyType;

	if(useProxy == CUSTOMPROXY)
	{
		if(proxyServer && proxyServer[0])
		{
			m_proxyHost = new char[lstrlenW(proxyServer) + 1];
			WideCharToMultiByte(CP_ACP, 0, proxyServer, -1, m_proxyHost, lstrlenW(proxyServer) + 1, NULL, NULL);
		}
	} else if(useProxy == IEPROXY)
	{
		INTERNET_PER_CONN_OPTION_LIST    List;
		INTERNET_PER_CONN_OPTION         Option[5];
		unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

		Option[0].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
		Option[1].dwOption = INTERNET_PER_CONN_AUTODISCOVERY_FLAGS;
		Option[2].dwOption = INTERNET_PER_CONN_FLAGS;
		Option[3].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
		Option[4].dwOption = INTERNET_PER_CONN_PROXY_SERVER;

		List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
		List.pszConnection = NULL;
		List.dwOptionCount = 5;
		List.dwOptionError = 0;
		List.pOptions = Option;

		InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, &nSize);

		if((Option[2].Value.dwValue & PROXY_TYPE_PROXY) && Option[4].Value.pszValue != NULL)
		{
			char* dots = strstr(Option[4].Value.pszValue, ":");
			if(dots)
			{
				int len = (int) (dots - Option[4].Value.pszValue + 1);
				m_proxyHost = new char[len + 1];
				lstrcpyn(m_proxyHost, Option[4].Value.pszValue, len);
				m_proxyHost[len] = 0;
				m_proxyPort = atoi(dots + 1);
			} else
			{
				m_proxyHost = new char[lstrlen(Option[4].Value.pszValue) + 1];
				lstrcpy(m_proxyHost, Option[4].Value.pszValue);
				m_proxyPort = 0;
			}
			m_useProxy = CUSTOMPROXY;
		} else 
		{
			m_useProxy = NOPROXY;
		}
	}
	if(proxyUser && proxyUser[0])
	{
		m_proxyUser = new char[lstrlenW(proxyUser) + 1];
		WideCharToMultiByte(CP_ACP, 0, proxyUser, -1, m_proxyUser, lstrlenW(proxyUser) + 1, NULL, NULL);
	}
	if(proxyPassword && proxyPassword[0])
	{
		m_proxyPassword = new char[lstrlenW(proxyPassword) + 1];
		WideCharToMultiByte(CP_ACP, 0, proxyPassword, -1, m_proxyPassword, lstrlenW(proxyPassword) + 1, NULL, NULL);
	}
}

DWORD CHttpReader::WaitForEnd( int timeout )
{
	if(m_DumpThread)
	{
		DWORD ret = WaitForSingleObject(m_DumpThread, timeout);
		CloseHandle(m_DumpThread);
		m_DumpThread = NULL;
		return ret;
	}
	return WAIT_OBJECT_0;
}

void CHttpReader::SetUserAgent( LPCWSTR userAgent )
{
	if(userAgent && userAgent[0])
	{
		if(m_userAgent)
		{
			delete m_userAgent;
		}
		m_userAgent = new char[lstrlenW(userAgent) + 1];
		WideCharToMultiByte(CP_ACP, 0, userAgent, -1, m_userAgent, lstrlenW(userAgent) + 1, NULL, NULL);
	}
}

void CHttpReader::SetIEProxy()
{
/*
	INTERNET_PER_CONN_OPTION_LIST    List;
	INTERNET_PER_CONN_OPTION         Option[5];
	unsigned long                    nSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);

	Option[0].dwOption = INTERNET_PER_CONN_AUTOCONFIG_URL;
	Option[1].dwOption = INTERNET_PER_CONN_AUTODISCOVERY_FLAGS;
	Option[2].dwOption = INTERNET_PER_CONN_FLAGS;
	Option[3].dwOption = INTERNET_PER_CONN_PROXY_BYPASS;
	Option[4].dwOption = INTERNET_PER_CONN_PROXY_SERVER;

	List.dwSize = sizeof(INTERNET_PER_CONN_OPTION_LIST);
	List.pszConnection = NULL;
	List.dwOptionCount = 5;
	List.dwOptionError = 0;
	List.pOptions = Option;

	InternetQueryOption(NULL, INTERNET_OPTION_PER_CONNECTION_OPTION, &List, &nSize);

	if(Option[0].Value.pszValue != NULL)
		printf("%s\n", Option[0].Value.pszValue);

	if((Option[2].Value.dwValue & PROXY_TYPE_AUTO_PROXY_URL) == PROXY_TYPE_AUTO_PROXY_URL)
		printf("PROXY_TYPE_AUTO_PROXY_URL\n");

	if((Option[2].Value.dwValue & PROXY_TYPE_AUTO_DETECT) == PROXY_TYPE_AUTO_DETECT)
		printf("PROXY_TYPE_AUTO_DETECT\n");

	INTERNET_VERSION_INFO      Version;
	nSize = sizeof(INTERNET_VERSION_INFO);

	InternetQueryOption(NULL, INTERNET_OPTION_VERSION, &Version, &nSize);

	if(Option[0].Value.pszValue != NULL)
		GlobalFree(Option[0].Value.pszValue);

	if(Option[3].Value.pszValue != NULL)
		GlobalFree(Option[3].Value.pszValue);

	if(Option[4].Value.pszValue != NULL)
		GlobalFree(Option[4].Value.pszValue);
*/
}

void CHttpReader::Init()
{
	curl_global_init(CURL_GLOBAL_ALL);
}

void CHttpReader::Uninit()
{
	curl_global_cleanup();
}

/*
void WriteLog(char* str)
{
	CHAR flName[MAX_PATH];
	SHGetFolderPathA(NULL, CSIDL_DESKTOPDIRECTORY, NULL, SHGFP_TYPE_CURRENT, flName);
	PathAddBackslashA(flName);
	lstrcatA(flName, "tlblog.log");
	FILE* fl = fopen(flName, "at");
	fwrite(str, 1, strlen(str), fl);
	fwrite("\n", 1, strlen("\n"), fl);
	fclose(fl);
}
*/



void CHttpReader::Dump()
{
	CURL* curl = curl_easy_init();

	curl_easy_setopt(curl, CURLOPT_URL,		m_url);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, (long) 1);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, (long) 10);
	if(m_user)
	{
		curl_easy_setopt(curl, CURLOPT_USERNAME,	m_user);
	}
	if(m_password)
	{
		curl_easy_setopt(curl, CURLOPT_PASSWORD,	m_password);
	}
	if(m_useProxy == CUSTOMPROXY)
	{
		curl_easy_setopt(curl, CURLOPT_PROXY,			m_proxyHost);
		curl_easy_setopt(curl, CURLOPT_PROXYPORT,		m_proxyPort);
		curl_easy_setopt(curl, CURLOPT_PROXYTYPE,		m_proxyType);
		if(m_proxyUser)
		{
			curl_easy_setopt(curl, CURLOPT_PROXYUSERNAME,	m_proxyUser);
		}
		if(m_proxyPassword)
		{
			curl_easy_setopt(curl, CURLOPT_PROXYPASSWORD,	m_proxyPassword);
		}
	}

	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION,		CHttpReader::writeFunction);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA,			this);

	curl_easy_setopt(curl, CURLOPT_NOPROGRESS,			(long) 0);

	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_LIMIT,		(long) 512);
	curl_easy_setopt(curl, CURLOPT_LOW_SPEED_TIME,		(long) 30);

	curl_easy_setopt(curl, CURLOPT_PROGRESSFUNCTION,	CHttpReader::progressFunction);
	curl_easy_setopt(curl, CURLOPT_PROGRESSDATA,		this);

	curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER,		(long) 0);

	CHAR errMessageA[1024];
	errMessageA[0] = 0;

	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER,		errMessageA);
	curl_easy_setopt(curl, CURLOPT_FAILONERROR,		(long) 1);
	if(m_userAgent)
	{
		curl_easy_setopt(curl, CURLOPT_USERAGENT,	m_userAgent);
	} else
	{
		curl_easy_setopt(curl, CURLOPT_USERAGENT,	"Mozilla/5.0 (Windows; U; Windows NT 6.1; en-US) AppleWebKit/534.13 (KHTML, like Gecko) Chrome/9.0.597.47 Safari/534.13");
	}

	m_session = curl;
	CURLcode res = curl_easy_perform(curl);

	long httpCode = 0;

	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	m_session = NULL;
	curl_easy_cleanup(curl);

	if(res)
	{
		WCHAR errMessageW[256];
		MultiByteToWideChar(CP_ACP, 0, errMessageA, -1, errMessageW, 256);
		OnError(res, errMessageW);
	}
	OnFinish();
}

BOOL CHttpReader::OpenURL( LPCWSTR url, LPCWSTR user /*= NULL*/, LPCWSTR password /*= NULL*/ )
{
	if(m_url)		delete m_url;
	if(m_user)		delete m_user;
	if(m_password)	delete m_password;
	m_user		= NULL;
	m_password	= NULL;

	m_url = new char[lstrlenW(url) + 1];
	WideCharToMultiByte(CP_ACP, 0, url, -1, m_url, lstrlenW(url) + 1, NULL, NULL);

	if(password) 
	{
		m_password = new char[lstrlenW(password) + 1];
		WideCharToMultiByte(CP_ACP, 0, password, -1, m_password, lstrlenW(password) + 1, NULL, NULL);
	}
	if(user) 
	{
		m_user = new char[lstrlenW(user) + 1];
		WideCharToMultiByte(CP_ACP, 0, user, -1, m_user, lstrlenW(user) + 1, NULL, NULL);
	}

	WaitForEnd(INFINITE);
	
	m_szTotal		= 0;
	m_szDownloaded	= 0;
	m_stop = FALSE;
	DWORD trdID = 0;
	m_DumpThread = chBEGINTHREADEX(NULL, 0, CHttpReader::DumpTrdProc, (LPVOID) this, 0, &trdID);

	return TRUE;
}
