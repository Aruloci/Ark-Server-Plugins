#include "UnicodeRcon.h"

void Hook_RCONClientConnection_SendMessageW(RCONClientConnection* _this, int id, int type, FString* out_going_message)
{
	std::string msg = ArkApi::Tools::Utf8Encode(**out_going_message);
	const size_t len = msg.length();
	DWORD64 packet = reinterpret_cast<DWORD64>(FMemory::Malloc(len + 14));

	*(int *)packet = len + 10;
	*(int *)(packet + 4) = id;
	*(int *)(packet + 8) = type;

	memcpy((void *)(packet + 12), msg.data(), len);

	*(char *)(len + packet + 12) = 0;
	*(char *)(len + packet + 13) = 0;

	auto socket = static_cast<FSocketBSD*>(_this->SocketField());

	int bytes_sent = 0;
	if (!socket->Send((char*)packet, len + 14, &bytes_sent))
		_this->IsClosedField() = true;

	FMemory::Free(reinterpret_cast<void*>(packet));
}

bool Hook_FSocketBSD_Recv(FSocketBSD* _this, char* Data, int BufferSize, int* BytesRead, ESocketReceiveFlags::Type Flags)
{
	bool ret = FSocketBSD_Recv_original(_this, Data, BufferSize, BytesRead, Flags);
	if (RCONPORT != -1 && _this->GetPortNo() == RCONPORT && BufferSize > 14)
	{
		int PacketID;
		memcpy(&PacketID, Data + 8, 4);
		if (PacketID == 2) RconCMDData = ArkApi::Tools::Utf8Decode(Data + 12).data();
	}
	return ret;
}

void Hook_RCONClientConnection_ProcessRCONPacket(RCONClientConnection* _this, RCONPacket* packet,
	UWorld* in_world)
{
	if (_this->IsAuthenticatedField()) packet->Body = RconCMDData;
	RCONClientConnection_ProcessRCONPacket_original(_this, packet, in_world);
}

bool _cdecl Hook_AShooterGameMode_HandleNewPlayer(AShooterGameMode* _this, AShooterPlayerController* Player, UPrimalPlayerData* PlayerData, AShooterCharacter* PlayerCharacter, bool bIsFromLogin)
{
	if (!LoadedRconPort) LoadServerRconPort();
	return AShooterGameMode_HandleNewPlayer_original(_this, Player, PlayerData, PlayerCharacter, bIsFromLogin);
}

void Load()
{
	if (!LoadedRconPort) LoadServerRconPort();
	ArkApi::GetHooks().SetHook("RCONClientConnection.SendMessageW", &Hook_RCONClientConnection_SendMessageW,
	                           &RCONClientConnection_SendMessageW_original);
	ArkApi::GetHooks().SetHook("FSocketBSD.Recv", &Hook_FSocketBSD_Recv,
							   &FSocketBSD_Recv_original);
	ArkApi::GetHooks().SetHook("RCONClientConnection.ProcessRCONPacket", &Hook_RCONClientConnection_ProcessRCONPacket,
							   &RCONClientConnection_ProcessRCONPacket_original);
	ArkApi::GetHooks().SetHook("AShooterGameMode.HandleNewPlayer_Implementation", &Hook_AShooterGameMode_HandleNewPlayer,
							   &AShooterGameMode_HandleNewPlayer_original);
}

void Destory()
{
	ArkApi::GetHooks().DisableHook("RCONClientConnection.SendMessageW", &Hook_RCONClientConnection_SendMessageW);
	ArkApi::GetHooks().DisableHook("FSocketBSD.Recv", &Hook_FSocketBSD_Recv);
	ArkApi::GetHooks().DisableHook("RCONClientConnection.ProcessRCONPacket", &Hook_RCONClientConnection_ProcessRCONPacket);
	ArkApi::GetHooks().DisableHook("AShooterGameMode.HandleNewPlayer_Implementation", &Hook_AShooterGameMode_HandleNewPlayer);
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
		Load();
		break;
	case DLL_PROCESS_DETACH:
		Destory();
		break;
	}
	return TRUE;
}
