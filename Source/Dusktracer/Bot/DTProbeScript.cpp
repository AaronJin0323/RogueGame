#include "Bot/DTProbeScript.h"
#include "Bot/DTBotBrain.h"
#include "Telemetry/DTTelemetrySources.h"

bool FDTProbeScript::Parse(const FString& Text, FString& OutError)
{
	*this = FDTProbeScript();
	TArray<FString> Lines;
	Text.ParseIntoArrayLines(Lines);
	for (const FString& Raw : Lines)
	{
		TArray<FString> Words;
		Raw.ParseIntoArrayWS(Words);
		if (Words.Num() == 0)
		{
			continue;
		}
		const FString& Key = Words[0];
		const bool bOneArg = Words.Num() == 2;
		if (Key == TEXT("patron") && bOneArg)
		{
			Patron = FDTBotConfig::ParsePath(Words[1]);
		}
		else if (Key == TEXT("second") && bOneArg)
		{
			Second = FDTBotConfig::ParsePath(Words[1]);
		}
		else if (Key == TEXT("bless") && bOneArg)
		{
			Blessings.Add(FName(*Words[1]));
		}
		else if (Key == TEXT("weapon") && Words.Num() >= 3)
		{
			FDTProbeWeapon& Weapon = Weapons.AddDefaulted_GetRef();
			Weapon.Id = FName(*Words[1]);
			Weapon.Level = FCString::Atoi(*Words[2]);
			for (int32 Index = 3; Index < Words.Num(); ++Index)
			{
				Weapon.Mods.Add(FName(*Words[Index]));
			}
		}
		else if (Key == TEXT("field") && bOneArg)
		{
			Field = FCString::Atoi(*Words[1]);
		}
		else if (Key == TEXT("hp_scale") && bOneArg)
		{
			HpScale = FCString::Atof(*Words[1]);
		}
		else if (Key == TEXT("warmup_s") && bOneArg)
		{
			WarmupS = FCString::Atof(*Words[1]);
		}
		else if (Key == TEXT("window_s") && bOneArg)
		{
			WindowS = FCString::Atof(*Words[1]);
		}
		else if (Key == TEXT("cast_at_s") && bOneArg)
		{
			CastAtS = FCString::Atof(*Words[1]);
		}
		else
		{
			OutError = FString::Printf(TEXT("bad line: %s"), *Raw);
			return false;
		}
	}
	if (Patron == EDTPath::None)
	{
		OutError = TEXT("no patron");
		return false;
	}
	return true;
}

FString FDTProbeScript::ToText() const
{
	FString Out = FString::Printf(TEXT("patron %s\n"), FDTTelemetrySources::PathName(Patron));
	if (Second != EDTPath::None)
	{
		Out += FString::Printf(TEXT("second %s\n"), FDTTelemetrySources::PathName(Second));
	}
	for (const FName& Id : Blessings)
	{
		Out += FString::Printf(TEXT("bless %s\n"), *Id.ToString());
	}
	for (const FDTProbeWeapon& Weapon : Weapons)
	{
		Out += FString::Printf(TEXT("weapon %s %d"), *Weapon.Id.ToString(), Weapon.Level);
		for (const FName& Mod : Weapon.Mods)
		{
			Out += TEXT(" ") + Mod.ToString();
		}
		Out += TEXT("\n");
	}
	return Out;
}

FDTProbeScript FDTProbeScript::FromPlayer(const FDTPlayerRunState& Player)
{
	FDTProbeScript Script;
	Script.Patron = Player.Keepers.Primary;
	Script.Second = Player.Keepers.bCommitDone ? Player.Keepers.Second : EDTPath::None;
	Script.Blessings = Player.Keepers.Owned;
	for (const FDTWeaponInstance& Owned : Player.Weapons)
	{
		FDTProbeWeapon& Weapon = Script.Weapons.AddDefaulted_GetRef();
		Weapon.Id = Owned.Id;
		Weapon.Level = Owned.Level;
		for (const FDTWeaponInstance::FSocket& Socket : Owned.Sockets)
		{
			Weapon.Mods.Add(Socket.ModId);
		}
	}
	return Script;
}
