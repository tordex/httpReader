#pragma once

class CHttpReader
{
public:
typedef enum 
{
  PROXY_HTTP		= 0,
  PROXY_HTTP_1_0	= 1,
  PROXY_SOCKS4		= 4,
  PROXY_SOCKS5		= 5,
  PROXY_SOCKS4A		= 6,
  PROXY_HOSTNAME	= 7
} PROXYTYPE;

typedef enum 
{
	NOPROXY			= 0,
	CUSTOMPROXY		= 1,
	IEPROXY			= 2
} USEPROXY;


private:
	LPSTR		m_url;
	USEPROXY	m_useProxy;
	LPSTR		m_proxyHost;
	long		m_proxyPort;
	PROXYTYPE	m_proxyType;
	LPSTR		m_proxyUser;
	LPSTR		m_proxyPassword;
	LPSTR		m_user;
	LPSTR		m_password;
	BOOL		m_SendCookies;
	DWORD		m_error;
	HANDLE		m_DumpThread;
	BOOL		m_stop;
	void*		m_session;
	LPSTR		m_userAgent;

	// stat
	size_t		m_szTotal;
	size_t		m_szDownloaded;
public:
	CHttpReader(void);
	virtual ~CHttpReader(void);

	void	Stop(void);
	DWORD	GetError()					{ return  m_error;			}
	void	AllowCookies(BOOL allow)	{ m_SendCookies = allow;	}
	DWORD	WaitForEnd(int timeout);
	void	SetIEProxy();
	void	SetProxy(USEPROXY useProxy, PROXYTYPE proxyType, LPCWSTR proxyServer, long proxyPort, LPCWSTR proxyUser, LPCWSTR proxyPassword);
	void	SetUserAgent(LPCWSTR userAgent);
public:
	BOOL	OpenURL(LPCWSTR url, LPCWSTR user = NULL, LPCWSTR password = NULL);
	virtual void OnData(void* data, size_t dataLen, size_t downloaded, size_t Total);
	virtual void OnStatus(DWORD status);
	virtual void OnError(DWORD error, LPWSTR errorString);
	virtual void OnFinish();

	static void Init();
	static void Uninit();

private:
	static DWORD WINAPI DumpTrdProc(LPVOID lpParam);
	void Dump();

	// callback functions
	static size_t	writeFunction( void *ptr, size_t size, size_t nmemb, void *stream);
	static int		progressFunction(void *clientp, double dltotal, double dlnow, double ultotal, double ulnow);
};
