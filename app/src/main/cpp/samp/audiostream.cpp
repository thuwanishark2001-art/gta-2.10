#include <memory>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include "audiostream.h"
#include "servers.h"
#include "log.h"
#include "game/game.h"
#include "../main.h"
#include "../vendor/bass/bass.h"

extern CGame* pGame;

HSTREAM bassStream = 0;
char g_szAudioStreamUrl[256+1];
float g_fAudioStreamX;
float g_fAudioStreamY;
float g_fAudioStreamZ;
float g_fAudioStreamRadius;
bool g_audioStreamUsePos;
bool g_bAudioStreamStop;
bool g_bAudioStreamThreadWorked;

// A path inside the content folder: letters, digits, _ - . / only, no
// hidden parts, no "..". Same rule as the launcher's ContentSync.isSafePath.
static bool IsSafeContentPath(const std::string& path)
{
	if (path.empty() || path.size() > 128 || path.back() == '/') return false;
	size_t partStart = 0;
	for (size_t i = 0; i <= path.size(); i++)
	{
		if (i == path.size() || path[i] == '/')
		{
			if (i == partStart || path[partStart] == '.') return false;
			partStart = i + 1;
			continue;
		}
		char c = path[i];
		bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' || c == '.';
		if (!ok) return false;
	}
	return path.find("..") == std::string::npos;
}

// A stream from the server's content service (http://host:KM_CONTENT_PORT/path)
// that the launcher has already downloaded: the local file's path, else "".
std::string AudioStream_LocalContentPath(const char* url)
{
	if (!url || !g_pszStorage || strncmp(url, "http://", 7) != 0) return "";
	const char* host = url + 7;
	const char* slash = strchr(host, '/');
	if (!slash) return "";
	std::string hostPort(host, slash - host);
	size_t colon = hostPort.rfind(':');
	if (colon == std::string::npos || hostPort.substr(colon + 1) != std::to_string(KM_CONTENT_PORT)) return "";
	std::string path(slash + 1);
	if (!IsSafeContentPath(path)) return "";
	std::string local = std::string(g_pszStorage) + "content/" + path;
	struct stat st;
	if (stat(local.c_str(), &st) != 0 || !S_ISREG(st.st_mode)) return "";
	return local;
}

void* audioStreamThread(void*)
{
	g_bAudioStreamThreadWorked = true;
	if (bassStream)
	{
		BASS_ChannelStop(bassStream);
		bassStream = 0;
	}

	std::string local = AudioStream_LocalContentPath(g_szAudioStreamUrl);
	if (!local.empty())
	{
		bassStream = BASS_StreamCreateFile(FALSE, local.c_str(), 0, 0, 0);
		if (bassStream) FLog("AudioStream: playing the downloaded %s", local.c_str());
		else FLog("AudioStream: can't open %s (BASS error %d); streaming instead", local.c_str(), BASS_ErrorGetCode());
	}
	if (!bassStream)
		bassStream = BASS_StreamCreateURL(g_szAudioStreamUrl, 0, 0x940000, 0,0);
	if (!bassStream)
		FLog("AudioStream: can't open %s (BASS error %d)", g_szAudioStreamUrl, BASS_ErrorGetCode());
	else if (!BASS_ChannelPlay(bassStream, 0))
		FLog("AudioStream: can't play (BASS error %d)", BASS_ErrorGetCode());
	else
		FLog("AudioStream: playing");

	//BASS_ChannelSetSync(bassStream, 4, 0, 0, 0);
	//BASS_ChannelSetSync(bassStream, 12, 0, 0, 0);

	while (!g_bAudioStreamStop)
	{
		// ..
		usleep(2000);
	}

	BASS_ChannelStop(bassStream);
	bassStream = 0;
	g_bAudioStreamThreadWorked = false;
	pthread_exit(0);
}

CAudioStream::CAudioStream()
{
	m_bInited = false;
}

bool CAudioStream::Initialize()
{
	bassStream = 0;
	//BASS_Free();
	//if (!BASS_Init(-1, 44100, 0)) return false;		//if (!BASS_Init(-1, 48000, 0)) return false;

	BASS_SetConfigPtr(16, "SA-MP/0.3");
	//BASS_SetConfig(5, ) volume
	BASS_SetConfig(21, 1);			// BASS_CONFIG_NET_PLAYLIST
	BASS_SetConfig(11, 10000);		// BASS_CONFIG_NET_TIMEOUT

	m_bInited = true;
	return true;
}

void CAudioStream::Process()
{
	if (g_bAudioStreamThreadWorked)
	{
		// ..
		if (pGame->IsGamePaused())
		{
			BASS_SetConfig(5, 0);
		}
		else
		{
			BASS_SetConfig(5, 5000);
		}
	}
}

bool CAudioStream::Play(const char* szUrl, float fX, float fY, float fZ, float fRadius, bool bUsePos)
{
	FLog("Play: %s", szUrl);

	if (!m_bInited) return false;
	Stop(true);

	// BASS is normally started by the voice module; start it here if that
	// didn't happen, so server audio streams still play.
	if (BASS_GetDevice() == (DWORD) -1)
	{
		if (BASS_Init(-1, 44100, 0, nullptr, nullptr))
			FLog("AudioStream: started BASS");
		else
			FLog("AudioStream: BASS_Init failed (BASS error %d)", BASS_ErrorGetCode());
	}

	if (bassStream)
	{
		BASS_ChannelStop(bassStream);
		bassStream = 0;
	}

	memset(g_szAudioStreamUrl, 0, sizeof(g_szAudioStreamUrl));
	strncpy(g_szAudioStreamUrl, szUrl, 256);

	g_fAudioStreamX = fX;
	g_fAudioStreamY = fY;
	g_fAudioStreamZ = fZ;
	g_fAudioStreamRadius = fRadius;
	g_audioStreamUsePos = bUsePos;

	g_bAudioStreamStop = false;

	pthread_t thread;
	pthread_create(&thread, 0, audioStreamThread, 0);
	return true;
}
// 0.3.7
bool CAudioStream::Stop(bool bWaitThread)
{
	FLog("Stop: %s", g_szAudioStreamUrl);
	if (!m_bInited || !g_bAudioStreamThreadWorked)
		return false;

	g_bAudioStreamStop = true;

	if (bWaitThread)
	{
		while (g_bAudioStreamThreadWorked)
			usleep(200);
	}

	BASS_StreamFree(bassStream);
	bassStream = 0;
	return true;
}