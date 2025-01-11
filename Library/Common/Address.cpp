#include "pch.h"
#include "Address.h"

#ifndef WIN32
   #include <unistd.h>
   #include <cstdlib>
   #include <cstring>
   #include <netdb.h>
   #include <arpa/inet.h>
   #include <sys/types.h>
   #include <sys/socket.h>
   #include <netinet/in.h>
   #include <fcntl.h>
   #define SOCKET_ERROR (-1)
   #define closesocket close
   #define WSAGetLastError() errno
   #define SOCKET int
   #define INVALID_SOCKET (-1)
#ifdef __APPLE__
    #include "errno.h"
#endif
#else
   #include <winsock2.h>
   #include <ws2tcpip.h>
   #include <wspiapi.h>
#endif

CAddress::CAddress(EAF eAF)
{
	m_eAF = eAF;
	memset(m_IP, 0, 16);
}

void CAddress::Set(const byte* IP)
{
	memcpy(m_IP, IP, Len());

//#ifdef _DEBUG
//	m_Str = ToString();
//#endif
}

#ifdef USING_QT
CAddress::CAddress(const QString& Src)
{
	QString Str = Src;
	if(Str.left(1) == "[" && Str.right(1) == "]")
	{
		Str.remove(0, 1);
		Str.truncate(Str.length() - 1);
	}

	if(!FromString(Str.toStdString()))
	{
		m_eAF = EAF::UNSPEC;
		memset(m_IP, 0, 16);
	}
}
#endif

CAddress::CAddress(const byte* IP)
{
	m_eAF = EAF::INET6;
	Set(IP);
}

CAddress::CAddress(uint32 IP) // must be same as with Qt
{
	m_eAF = EAF::INET;
	IP = _ntohl(IP);
	Set((byte*)&IP);
}

uint32 CAddress::ToIPv4() const // must be same as with Qt
{
	uint32 ip = 0;
	if(m_eAF == EAF::INET)
		memcpy(&ip, m_IP, Len());
	return _ntohl(ip);
}

size_t CAddress::Len() const
{
	return m_eAF == EAF::INET6 ? 16 : 4;
}

int CAddress::AF() const
{
	return m_eAF == EAF::INET6 ? AF_INET6 : AF_INET;
}

std::wstring CAddress::ToWString() const
{
	wchar_t Dest[65] = {'\0'};
	if(m_eAF != EAF::UNSPEC)
		_winet_ntop(AF(), m_IP, Dest, 65);
	return Dest;
}

bool CAddress::FromWString(const std::wstring& Str)
{
	if(Str.find(L".") != std::wstring::npos)
		m_eAF = EAF::INET;
	else if(Str.find(L":") != std::wstring::npos)
		m_eAF = EAF::INET6;
	else
		return false;

	if (_winet_pton(AF(), Str.c_str(), m_IP) != 1)
		return false;

//#ifdef _DEBUG
//	m_Str = ToString();
//#endif

	return true;
}

std::string CAddress::ToString() const
{
	char Dest[65] = {'\0'};
	if(m_eAF != EAF::UNSPEC)
		_inet_ntop(AF(), m_IP, Dest, 65);
	return Dest;
}

bool CAddress::FromString(const std::string& Str)
{
	if(Str.find(".") != std::string::npos)
		m_eAF = EAF::INET;
	else if(Str.find(":") != std::string::npos)
		m_eAF = EAF::INET6;
	else
		return false;
	if(_inet_pton(AF(), Str.c_str(), m_IP) != 1)
		return false;

//#ifdef _DEBUG
//	m_Str = ToString();
//#endif

	return true;
}

#ifdef USING_QT
CAddress::EAF CAddress::FromArray(const QString& Array)
{
	if(Array.size() == 16)
		m_eAF = EAF::INET6;
	else if(Array.size() == 4)
		m_eAF = EAF::INET;
	else
		return EAF::UNSPEC;
	Set(Array.data());
	return m_eAF;
}

QString CAddress::ToQString(bool bForUrl) const
{
	char Dest[65] = {'\0'};
	if(m_eAF != EAF::UNSPEC)
		_inet_ntop(AF(), m_IP, Dest, 65);
	if(bForUrl && m_eAF == EAF::INET6)
		return QString("[%1]").arg(Dest);
	return Dest;
}
#endif

void CAddress::FromSA(const sockaddr* sa, int sa_len, uint16* pPort) 
{
	switch(sa->sa_family)
	{
		case AF_INET:
		{
			ASSERT(sizeof(sockaddr_in) == sa_len);
			sockaddr_in* sa4 = (sockaddr_in*)sa;

			m_eAF = EAF::INET;
			Set((const byte*)&sa4->sin_addr.s_addr);
			if(pPort)
				*pPort = _ntohs(sa4->sin_port);
			break;
		}
		case AF_INET6:
		{
			ASSERT(sizeof(sockaddr_in6) == sa_len);
			sockaddr_in6* sa6 = (sockaddr_in6*)sa;

			m_eAF = EAF::INET6;
			Set((const byte*)&sa6->sin6_addr);
			if(pPort)
				*pPort  = _ntohs(sa6->sin6_port);
			break;
		}
		default:
			m_eAF = EAF::UNSPEC;
	}
}

void CAddress::ToSA(sockaddr* sa, int *sa_len, uint16 uPort) const
{
	switch((EAF)m_eAF)
	{
		case EAF::INET:
		{
			ASSERT(sizeof(sockaddr_in) <= *sa_len);
			sockaddr_in* sa4 = (sockaddr_in*)sa;
			*sa_len = sizeof(sockaddr_in);
			memset(sa, 0, *sa_len);

			sa4->sin_family = AF_INET;
			sa4->sin_addr.s_addr = *((uint32*)m_IP);
			sa4->sin_port = _ntohs(uPort);
			break;
		}
		case EAF::INET6:
		{
			ASSERT(sizeof(sockaddr_in6) <= *sa_len);
			sockaddr_in6* sa6 = (sockaddr_in6*)sa;
			*sa_len = sizeof(sockaddr_in6);
			memset(sa, 0, *sa_len);

			sa6->sin6_family = AF_INET6;
			memcpy(&sa6->sin6_addr, m_IP, 16);
			sa6->sin6_port = _ntohs(uPort);
			break;
		}
		default:
			ASSERT(0);
	}
}

bool CAddress::IsNull() const
{
	switch(m_eAF)
	{
		case EAF::UNSPEC:	return true;
		case EAF::INET:		return *((uint32*)m_IP) == INADDR_ANY;
		case EAF::INET6:	return ((uint64*)m_IP)[0] == 0 && ((uint64*)m_IP)[1] == 0;
	}
	return true;
}

bool CAddress::Convert(EAF eAF)
{
	if(eAF == m_eAF)
		return true;
	switch(eAF)
	{
	case EAF::INET6:
		if(m_eAF != EAF::INET)
			return false;

		m_IP[12] = m_IP[0];
		m_IP[13] = m_IP[1];
		m_IP[14] = m_IP[2];
		m_IP[15] = m_IP[3];

		m_IP[10] = m_IP[11] = 0xFF;
		m_IP[0] = m_IP[1] = m_IP[2] = m_IP[3] = m_IP[4] = m_IP[5] = m_IP[6] = m_IP[7] = m_IP[8] = m_IP[9] = 0;
		break;
	
	case EAF::INET:
		if(!IsMappedIPv4())
			return false;

		m_IP[0] = m_IP[12];
		m_IP[1] = m_IP[13];
		m_IP[2] = m_IP[14];
		m_IP[3] = m_IP[15];
		
		m_IP[4] = m_IP[5] = m_IP[6] = m_IP[7] = m_IP[8] = m_IP[9] = m_IP[10] = m_IP[11] = m_IP[12] = m_IP[13] = m_IP[14] = m_IP[14] = 0;
		break;

	default:
		return false;
	}
	m_eAF = eAF;
	return true;
}

bool CAddress::IsMappedIPv4() const
{
	return m_eAF == EAF::INET6 
	 && m_IP[10] == 0xFF && m_IP[11] == 0xFF 
	 && !m_IP[0] && !m_IP[1] && !m_IP[2] && !m_IP[3] && !m_IP[4] && !m_IP[5] && !m_IP[6] && !m_IP[7] && !m_IP[8] && !m_IP[9];
}

bool IsLanIPv4(uint32 IPv4)
{
	byte uIP[4];
	(*(uint32*)(&uIP)) = IPv4;

	// LAN IP's
	// -------------------------------------------
	//	0.*								"This" Network
	//	10.0.0.0 - 10.255.255.255		Class A
	//	172.16.0.0 - 172.31.255.255		Class B
	//	192.168.0.0 - 192.168.255.255	Class C

	if (uIP[3]==192 && uIP[2]==168) // check this 1st, because those LANs IPs are mostly spreaded
		return true;

	if (uIP[3]==172 && uIP[2]>=16 && uIP[2]<=31)
		return true;

	if (uIP[3]==0 || uIP[3]==10)
		return true;

	return false;
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// from BSD sources: http://code.google.com/p/plan9front/source/browse/sys/src/ape/Library/bsd/?r=320990f52487ae84e28961517a4fa0d02d473bac

char* _inet_ntop(int af, const void *src, char *dst, int size)
{
	unsigned char *p;
	char *t;
	int i, c, l;

	if(af == AF_INET){
		if(size < INET_ADDRSTRLEN){
			//errno = ENOSPC;
			return 0;
		}
		p = (unsigned char*)&(((struct in_addr*)src)->s_addr);
		sprintf_s(dst, size, "%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
		return dst;
	}

	if(af != AF_INET6){
		//errno = EAFNOSUPPORT;
		return 0;
	}
	if(size < INET6_ADDRSTRLEN){
		//errno = ENOSPC;
		return 0;
	}

	p = (unsigned char*)((struct in6_addr*)src)->s6_addr;
	c = size;
	t = dst;
	for(i=0; i<16; i += 2){
		unsigned int w;

		if (i > 0) {
			c--;
			*t++ = ':';
		}
		w = p[i]<<8 | p[i+1];
		sprintf_s(t, c, "%x", w);
		l = (int)strlen(t);;
		c -= l;
		t += l;
	}
	return dst;
}

wchar_t* _winet_ntop(int af, const void *src, wchar_t *dst, size_t size)
{
    unsigned char *p;
    wchar_t *t;
    int i;

    if (af == AF_INET) {
        if (size < INET_ADDRSTRLEN) {
            //errno = ENOSPC;
            return NULL;
        }
        p = (unsigned char*)&(((struct in_addr*)src)->s_addr);
        swprintf(dst, size, L"%d.%d.%d.%d", p[0], p[1], p[2], p[3]);
        return dst;
    }

    if (af != AF_INET6) {
        //errno = EAFNOSUPPORT;
        return NULL;
    }
    if (size < INET6_ADDRSTRLEN) {
        //errno = ENOSPC;
        return NULL;
    }

    p = (unsigned char*)((struct in6_addr*)src)->s6_addr;
    t = dst;
    for (i = 0; i < 16; i += 2) {
        unsigned int w;

        if (i > 0)
            *t++ = L':';
        w = p[i] << 8 | p[i + 1];
        swprintf(t, size - (t - dst), L"%x", w);
        t += wcslen(t);
    }
    return dst;
}


//////////////////////////////////////////////////////////////////////////////////////

#define CLASS(x)        (x[0]>>6)

int _inet_aton(const char *from, struct in_addr *in)
{
	unsigned char *to;
	unsigned long x;
	char *p;
	int i;

	in->s_addr = 0;
	to = (unsigned char*)&in->s_addr;
	if(*from == 0)
		return 0;
	for(i = 0; i < 4 && *from; i++, from = p){
		x = strtoul(from, &p, 0);
		if(x != (unsigned char)x || p == from)
			return 0;       /* parse error */
		to[i] = (unsigned char)x;
		if(*p == '.')
			p++;
		else if(*p != 0)
			return 0;       /* parse error */
	}

	switch(CLASS(to)){
	case 0: /* class A - 1 byte net */
	case 1:
		if(i == 3){
			to[3] = to[2];
			to[2] = to[1];
			to[1] = 0;
		} else if (i == 2){
			to[3] = to[1];
			to[1] = 0;
		}
		break;
	case 2: /* class B - 2 byte net */
		if(i == 3){
			to[3] = to[2];
			to[2] = 0;
		}
		break;
	}
	return 1;
}

int _winet_aton(const wchar_t *from, struct in_addr *in)
{
    unsigned char *to;
    unsigned long x;
    wchar_t *p;
    int i;

    in->s_addr = 0;
    to = (unsigned char*)&in->s_addr;
    if (*from == L'\0')
        return 0;
    for (i = 0; i < 4 && *from; i++, from = p) {
        x = wcstoul(from, &p, 0);
        if (x != (unsigned char)x || p == from)
            return 0;       /* parse error */
        to[i] = (unsigned char)x;
        if (*p == L'.')
            p++;
        else if (*p != L'\0')
            return 0;       /* parse error */
    }

    switch (CLASS(to)) {
    case 0: /* class A - 1 byte net */
    case 1:
        if (i == 3) {
            to[3] = to[2];
            to[2] = to[1];
            to[1] = 0;
        } else if (i == 2) {
            to[3] = to[1];
            to[1] = 0;
        }
        break;
    case 2: /* class B - 2 byte net */
        if (i == 3) {
            to[3] = to[2];
            to[2] = 0;
        }
        break;
    }
    return 1;
}


static int ipcharok(int c)
{
	return c == ':' || isascii(c) && isxdigit(c);
}

static int delimchar(int c)
{
	if(c == '\0')
		return 1;
	if(c == ':' || isascii(c) && isalnum(c))
		return 0;
	return 1;
}

int _inet_pton(int af, const char *src, void *dst)
{
	int i, elipsis = 0;
	unsigned char *to;
	unsigned long x;
	const char *p, *op;

	if(af == AF_INET){
		return _inet_aton(src, (struct in_addr*)dst);
		ASSERT(0); // todo what is correct?
		/*struct in_addr temp;
		int ret = _inet_aton(src, &temp);
		*((ULONG*)dst) = _ntohl(temp.S_un.S_addr);
		return ret;*/
	}
	
	if(af != AF_INET6){
		//errno = EAFNOSUPPORT;
		return -1;
	}

	to = ((struct in6_addr*)dst)->s6_addr;
	memset(to, 0, 16);

	p = src;
	for(i = 0; i < 16 && ipcharok(*p); i+=2){
		op = p;
		x = strtoul(p, (char**)&p, 16);

		if(x != (unsigned short)x || *p != ':' && !delimchar(*p))
			return 0;                       /* parse error */

		to[i] = (unsigned char)(x >> 8);
		to[i+1] = (unsigned char)x;
		if(*p == ':'){
			if(*++p == ':'){        /* :: is elided zero short(s) */
				if (elipsis)
					return 0;       /* second :: */
				elipsis = i+2;
				p++;
			}
		} else if (p == op)             /* strtoul made no progress? */
			break;
	}
	if (p == src || !delimchar(*p))
		return 0;                               /* parse error */
	if(i < 16){
		memmove(&to[elipsis+16-i], &to[elipsis], i-elipsis);
		memset(&to[elipsis], 0, 16-i);
	}
	return 1;
}

int _winet_pton(int af, const wchar_t *src, void *dst)
{
    int i, elipsis = 0;
    unsigned char *to;
    unsigned long x;
    const wchar_t *p, *op;

    if (af == AF_INET) {
        // Use the _winet_aton function for IPv4.
        return _winet_aton(src, (struct in_addr*)dst);
    }

    if (af != AF_INET6) {
        //errno = EAFNOSUPPORT;
        return -1;
    }

    to = ((struct in6_addr*)dst)->s6_addr;
    memset(to, 0, 16);

    p = src;
    for (i = 0; i < 16 && ipcharok(*p); i += 2) {
        op = p;
        x = wcstoul(p, (wchar_t**)&p, 16);

        if (x != (unsigned short)x || *p != L':' && !delimchar(*p))
            return 0; /* parse error */

        to[i] = (unsigned char)(x >> 8);
        to[i + 1] = (unsigned char)x;
        if (*p == L':') {
            if (*++p == L':') { /* :: is elided zero short(s) */
                if (elipsis)
                    return 0; /* second :: */
                elipsis = i + 2;
                p++;
            }
        }
        else if (p == op) /* wcstoul made no progress? */
            break;
    }
    if (p == src || !delimchar(*p))
        return 0; /* parse error */
    if (i < 16) {
        memmove(&to[elipsis + 16 - i], &to[elipsis], i - elipsis);
        memset(&to[elipsis], 0, 16 - i);
    }
    return 1;
}

int SubNetToInt(ULONG subnet)
{
    int value = 0;
    for (int i = 31; i >= 0 && ((int)subnet & 1 << i) != 0; i--)
        value++;
    return value;
}

ULONG IntSubNet(ULONG value)
{
    ULONG subnet = 0;
    for (int i = 31; (long)i >= (long)(32U - value); i--)
        subnet |= (ULONG)(1 << i);
    return subnet;
}

bool TestV4RangeOrder(IN_ADDR Begin, IN_ADDR End)
{
    return _ntohl(Begin.S_un.S_addr) > _ntohl(End.S_un.S_addr);
}

bool TestV6RangeOrder(IN6_ADDR Begin, IN6_ADDR End)
{
    for (int i = 0; i < 16; i++)
    {
        if (Begin.u.Byte[i] < End.u.Byte[i])
            return true;
        if (Begin.u.Byte[i] > End.u.Byte[i])
            return false;
    }
    return false;
}

