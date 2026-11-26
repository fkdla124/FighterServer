#include "SendPacket.h"
#include "Debug.h"
#include "MsgBuf.h"
#include "Session.h"
#include "List.h"

#include "RPC_ServerProxy.h"

List<Session*> sessionList;

void DisconnectSession(Session* session)
{
	DebugPrint("[Delete Mark] ID: %d\n", session->id);

	session->del = true;
}

void SweepMarkedSession()
{
	for (List<Session*>::Iterator it = sessionList.begin(); it != sessionList.end();)
	{
		if ((*it)->del == true)
		{
			DebugPrint("[Sweep Marked Session] ID: %d\n", (*it)->id);

			List<Session*> sendList;
			for (const auto s : sessionList)
				if (s != (*it))
					sendList.push_back(s);

			RPC::SendDeletePlayer(sendList, (*it)->id);
			closesocket((*it)->sock);
			delete (*it);
			it = sessionList.erase(it);
		}
		else
			++it;
	}
}