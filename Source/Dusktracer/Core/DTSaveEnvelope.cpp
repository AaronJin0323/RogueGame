#include "Core/DTSaveEnvelope.h"
#include "Misc/Crc.h"

namespace DTSave
{
	static constexpr uint8 Magic[4] = { 'D', 'T', 'S', 'V' };
	static constexpr int32 HeaderSize = 16;

	static void WriteU32(TArray<uint8>& Out, uint32 Value)
	{
		for (int32 Shift = 0; Shift < 32; Shift += 8)
		{
			Out.Add(static_cast<uint8>(Value >> Shift));
		}
	}

	static uint32 ReadU32(const TArray<uint8>& In, int32 Offset)
	{
		uint32 Value = 0;
		for (int32 Byte = 0; Byte < 4; ++Byte)
		{
			Value |= static_cast<uint32>(In[Offset + Byte]) << (8 * Byte);
		}
		return Value;
	}
}

TArray<uint8> FDTSaveEnvelope::Wrap(const TArray<uint8>& Payload, int32 Version)
{
	TArray<uint8> Out;
	Out.Reserve(DTSave::HeaderSize + Payload.Num());
	Out.Append(DTSave::Magic, UE_ARRAY_COUNT(DTSave::Magic));
	DTSave::WriteU32(Out, static_cast<uint32>(Version));
	DTSave::WriteU32(Out, static_cast<uint32>(Payload.Num()));
	DTSave::WriteU32(Out, FCrc::MemCrc32(Payload.GetData(), Payload.Num()));
	Out.Append(Payload);
	return Out;
}

EDTSaveLoadResult FDTSaveEnvelope::Unwrap(const TArray<uint8>& Bytes, TArray<uint8>& OutPayload, int32& OutLoadedVersion,
	int32 CurrentVersion) const
{
	OutPayload.Reset();
	OutLoadedVersion = INDEX_NONE;
	if (Bytes.Num() < DTSave::HeaderSize || FMemory::Memcmp(Bytes.GetData(), DTSave::Magic, UE_ARRAY_COUNT(DTSave::Magic)) != 0)
	{
		return EDTSaveLoadResult::BadHeader;
	}

	const int32 Version = static_cast<int32>(DTSave::ReadU32(Bytes, 4));
	const uint32 Size = DTSave::ReadU32(Bytes, 8);
	const uint32 Crc = DTSave::ReadU32(Bytes, 12);
	if (static_cast<int64>(Size) != Bytes.Num() - DTSave::HeaderSize)
	{
		return EDTSaveLoadResult::Corrupt;
	}
	TArray<uint8> Payload(Bytes.GetData() + DTSave::HeaderSize, static_cast<int32>(Size));
	if (FCrc::MemCrc32(Payload.GetData(), Payload.Num()) != Crc)
	{
		return EDTSaveLoadResult::Corrupt;
	}
	OutLoadedVersion = Version;
	if (Version > CurrentVersion)
	{
		return EDTSaveLoadResult::TooNew;
	}

	for (int32 From = Version; From < CurrentVersion; ++From)
	{
		const FDTSaveMigration* Step = Migrations.Find(From);
		if (!Step)
		{
			return EDTSaveLoadResult::MigrationMissing;
		}
		if (!(*Step)(Payload))
		{
			return EDTSaveLoadResult::MigrationFailed;
		}
	}
	OutPayload = MoveTemp(Payload);
	return EDTSaveLoadResult::Ok;
}
