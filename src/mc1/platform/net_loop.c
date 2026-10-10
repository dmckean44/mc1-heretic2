// Networking for a single player game: only the loopback between the local client and server.

#include "qcommon.h"

#define MAX_LOOPBACK	4
#define NUM_SOCKETS		3

typedef struct
{
	byte data[MAX_MSGLEN];
	int datalen;
} loopmsg_t;

typedef struct
{
	loopmsg_t msgs[MAX_LOOPBACK];
	int get;
	int send;
} loopback_t;

static loopback_t loopbacks[NUM_SOCKETS];

qboolean NET_CompareAdr(const netadr_t* a, const netadr_t* b)
{
	return (a->type == b->type && a->type == NA_LOOPBACK);
}

qboolean NET_CompareBaseAdr(const netadr_t* a, const netadr_t* b)
{
	return NET_CompareAdr(a, b);
}

qboolean NET_IsLocalAddress(const netadr_t* a)
{
	return (a->type == NA_LOOPBACK);
}

char* NET_AdrToString(const netadr_t* a)
{
	static char s[16];

	strcpy_s(s, sizeof(s), (a->type == NA_LOOPBACK ? "loopback" : "unknown"));
	return s;
}

qboolean NET_StringToAdr(const char* s, netadr_t* a)
{
	memset(a, 0, sizeof(netadr_t));

	if (strcmp(s, "localhost") != 0 && strcmp(s, "loopback") != 0)
		return false;

	a->type = NA_LOOPBACK;
	return true;
}

qboolean NET_GetPacket(const netsrc_t sock, netadr_t* n_from, sizebuf_t* n_message)
{
	loopback_t* loop = &loopbacks[sock];

	if (loop->send - loop->get > MAX_LOOPBACK)
		loop->get = loop->send - MAX_LOOPBACK;

	if (loop->get >= loop->send)
		return false;

	const loopmsg_t* msg = &loop->msgs[loop->get & (MAX_LOOPBACK - 1)];
	loop->get++;

	memcpy(n_message->data, msg->data, msg->datalen);
	n_message->cursize = msg->datalen;
	memset(n_from, 0, sizeof(*n_from));
	n_from->type = NA_LOOPBACK;

	return true;
}

void NET_SendPacket(const netsrc_t sock, const int length, const void* data, const netadr_t* to)
{
	if (to->type != NA_LOOPBACK)
		return;

	loopback_t* loop = &loopbacks[sock ^ 1];
	loopmsg_t* msg = &loop->msgs[loop->send & (MAX_LOOPBACK - 1)];
	loop->send++;

	memcpy(msg->data, data, length);
	msg->datalen = length;
}

void NET_Config(const qboolean multiplayer)
{
}

void NET_Init(void)
{
}

void NET_Shutdown(void)
{
}
