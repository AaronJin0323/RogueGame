// Settings and the pause, settings and controls screens (02_Systems/settings.md T-SET-01..18).
// Run headless: UnrealEditor-Cmd.exe Dusktracer.uproject -ExecCmds="Automation RunTests Dusktracer.Settings; Quit" -unattended -nullrhi

#include "Misc/AutomationTest.h"
#include "Bot/DTBotSubsystem.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Core/DTConstantsGen.h"
#include "Core/DTPaletteGen.h"
#include "Core/DTRunState.h"
#include "Core/DTSimSubsystem.h"
#include "Director/DTDirector.h"
#include "Enemies/DTEnemyBulletRenderer.h"
#include "Enemies/DTTelegraphRenderer.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "InputMappingContext.h"
#include "Internationalization/Culture.h"
#include "Internationalization/Internationalization.h"
#include "Internationalization/StringTable.h"
#include "Internationalization/StringTableCore.h"
#include "Internationalization/Regex.h"
#include "Internationalization/StringTableRegistry.h"
#include "Misc/App.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Offers/DTOfferScreen.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTHeroSubsystem.h"
#include "Player/DTInputSet.h"
#include "Settings/DTMenuScreen.h"
#include "Settings/DTMenuState.h"
#include "Settings/DTMenuSubsystem.h"
#include "Settings/DTSettingsSubsystem.h"
#include "Settings/DTUserSettings.h"
#include "Telemetry/DTTelemetrySubsystem.h"
#include "UObject/Package.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace DTSettingsTest
{
	constexpr EAutomationTestFlags Flags = EAutomationTestFlags_ApplicationContextMask | EAutomationTestFlags::ProductFilter;
	constexpr int32 BotSeconds = 45;

	/** Puts the live settings back whatever a test does. */
	struct FLiveGuard
	{
		FDTUserSettings Saved = DTSettings::Live();
		~FLiveGuard() { DTSettings::Live() = Saved; }
	};

	static FString TempFile(const TCHAR* Name)
	{
		const FString Dir = FPaths::Combine(FPaths::ProjectIntermediateDir(), TEXT("SettingsTests"));
		IFileManager::Get().MakeDirectory(*Dir, true);
		const FString Path = FPaths::Combine(Dir, Name);
		IFileManager::Get().Delete(*Path, false, true, true);
		return Path;
	}

	static FDTUserSettings AllChanged()
	{
		FDTUserSettings S;
		S.Language = EDTLanguage::ZhHans;
		S.VolumePct = DTConst::SETTINGS_VOLUME_STEP_PCT * 3;
		S.Graphics = EDTGraphics::Low;
		S.Window = EDTWindow::Windowed;
		S.TextPct = DTConst::SETTINGS_TEXT_MAX_PCT;
		S.bShake = false;
		S.bFlash = false;
		S.bDangerOutline = true;
		S.Bindings.Set(EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::LeftShift);
		S.Bindings.Set(EDTBindSlot::UiLock, EDTBindDevice::Pad, EKeys::Gamepad_LeftShoulder);
		return S;
	}

	struct FRunResult
	{
		uint32 InputHash = 0;
		int64 Steps = 0;
		FVector2f Pos = FVector2f::ZeroVector;
		float Hp = 0.0f;
		int32 Picks = 0;
		FString Csv;
		FString Jsonl;
	};

	static FString StripWall(const FString& In)
	{
		const FRegexPattern Pattern(TEXT(",?\"wall[a-z_]*\":(\"([^\"\\\\]|\\\\.)*\"|[-+0-9.eE]+)"));
		FRegexMatcher Matcher(Pattern, In);
		FString Out;
		int32 Last = 0;
		while (Matcher.FindNext())
		{
			Out += In.Mid(Last, Matcher.GetMatchBeginning() - Last);
			Last = Matcher.GetMatchEnding();
		}
		Out += In.Mid(Last);
		return Out;
	}

	struct FTestWorld
	{
		UWorld* World = nullptr;
		FTestWorld()
		{
			FWorldContext& Context = GEngine->CreateNewWorldContext(EWorldType::Game);
			World = UWorld::CreateWorld(EWorldType::Game, false, MakeUniqueObjectName(nullptr, UWorld::StaticClass(), TEXT("DTSettingsTestWorld")));
			Context.SetCurrentWorld(World);
			World->InitializeActorsForPlay(FURL());
			World->BeginPlay();
		}
		~FTestWorld()
		{
			for (AActor* Actor : FActorRange(World))
			{
				if (Actor)
				{
					Actor->RouteEndPlay(EEndPlayReason::LevelTransition);
				}
			}
			World->DestroyWorld(true);
			GEngine->DestroyWorldContext(World);
		}
	};

	static FRunResult Play(int32 Seed)
	{
		FRunResult Result;
		FTestWorld Test;
		UDTSimSubsystem* Sim = Test.World->GetSubsystem<UDTSimSubsystem>();
		UDTBotSubsystem* Bot = Test.World->GetSubsystem<UDTBotSubsystem>();
		UDTTelemetrySubsystem* Telemetry = Test.World->GetSubsystem<UDTTelemetrySubsystem>();
		UDTDirector* Director = Test.World->GetSubsystem<UDTDirector>();
		const UDTHeroSubsystem* Heroes = Test.World->GetSubsystem<UDTHeroSubsystem>();
		if (Sim && Bot && Telemetry && Director && Heroes)
		{
			Sim->StartRun(Seed, 1);
			Telemetry->StartSession(Seed, 1);
			Director->StartRun();
			FDTBotConfig Config;
			Config.Policy = EDTBotPolicy::Steered;
			Config.Path = EDTPath::Storm;
			Config.Archetype = FName(TEXT("Tempest"));
			Config.Version = 2;
			Config.Skill = 0.6f;
			Config.Seed = Seed;
			Bot->Activate(Config);
			const int32 Steps = BotSeconds * DTConst::SIM_FIXED_HZ;
			while (Sim->GetStepIndex() < Steps && !Bot->IsRunFinished())
			{
				if (Bot->RunSteps(Steps - static_cast<int32>(Sim->GetStepIndex())) == 0)
				{
					break;
				}
			}
			Telemetry->EndSession(TEXT("test"));
			const FString CsvPath = Telemetry->GetWriter().GetCsvPath();
			const FString JsonlPath = Telemetry->GetWriter().GetJsonlPath();
			FFileHelper::LoadFileToString(Result.Csv, *CsvPath);
			FFileHelper::LoadFileToString(Result.Jsonl, *JsonlPath);
			IFileManager::Get().Delete(*CsvPath, false, true, true);
			IFileManager::Get().Delete(*JsonlPath, false, true, true);
			Result.InputHash = Bot->GetInputHash();
			Result.Steps = Sim->GetStepIndex();
			Result.Picks = Bot->GetBrain().GetStats().Picks;
			if (const FDTHeroState* State = Heroes->GetState(0))
			{
				Result.Pos = State->Pos;
				Result.Hp = State->Hp;
			}
			Bot->Deactivate();
		}
		return Result;
	}

	// Machado, Oliveira, Fernandes 2009, full severity (the source's method).
	static const double ProtanM[3][3] = { { 0.152286, 1.052583, -0.204868 }, { 0.114503, 0.786281, 0.099216 }, { -0.003882, -0.048116, 1.051998 } };
	static const double DeutanM[3][3] = { { 0.367322, 0.860646, -0.227968 }, { 0.280085, 0.672501, 0.047413 }, { -0.011820, 0.042940, 0.968881 } };
	static const double TritanM[3][3] = { { 1.255528, -0.076749, -0.178779 }, { -0.078411, 0.930809, 0.147602 }, { 0.004733, 0.691367, 0.303900 } };

	static FVector3d Lab(const FColor& Color, const double (*Matrix)[3])
	{
		const FLinearColor L = FLinearColor::FromSRGBColor(Color);
		FVector3d Rgb(L.R, L.G, L.B);
		if (Matrix)
		{
			const FVector3d In = Rgb;
			for (int32 Row = 0; Row < 3; ++Row)
			{
				Rgb[Row] = FMath::Clamp(Matrix[Row][0] * In.X + Matrix[Row][1] * In.Y + Matrix[Row][2] * In.Z, 0.0, 1.0);
			}
		}
		const double X = (0.4124 * Rgb.X + 0.3576 * Rgb.Y + 0.1805 * Rgb.Z) / 0.95047;
		const double Y = 0.2126 * Rgb.X + 0.7152 * Rgb.Y + 0.0722 * Rgb.Z;
		const double Z = (0.0193 * Rgb.X + 0.1192 * Rgb.Y + 0.9505 * Rgb.Z) / 1.08883;
		auto F = [](double T) { return T > 0.008856 ? FMath::Pow(T, 1.0 / 3.0) : 7.787 * T + 16.0 / 116.0; };
		return FVector3d(116.0 * F(Y) - 16.0, 500.0 * (F(X) - F(Y)), 200.0 * (F(Y) - F(Z)));
	}

	static double DeltaE(const FColor& A, const FColor& B, const double (*Matrix)[3])
	{
		return FVector3d::Distance(Lab(A, Matrix), Lab(B, Matrix));
	}

	static int32 CountColor(const UInstancedStaticMeshComponent* Mesh, int32 Stride, const FLinearColor& Color)
	{
		int32 Count = 0;
		const TArray<float>& Data = Mesh->PerInstanceSMCustomData;
		for (int32 Index = 0; Index + 2 < Data.Num(); Index += Stride)
		{
			Count += FMath::IsNearlyEqual(Data[Index], Color.R) && FMath::IsNearlyEqual(Data[Index + 1], Color.G) && FMath::IsNearlyEqual(Data[Index + 2], Color.B) ? 1 : 0;
		}
		return Count;
	}

	static bool ContextMaps(const UInputMappingContext* Context, const FKey& Key)
	{
		for (const FEnhancedActionKeyMapping& Mapping : Context->GetMappings())
		{
			if (Mapping.Key == Key)
			{
				return true;
			}
		}
		return false;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetSimUntouched, "Dusktracer.Settings.SimUntouched", DTSettingsTest::Flags) // T-SET-01
bool FDTSetSimUntouched::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	FLiveGuard Guard;
	const FString Culture = FInternationalization::Get().GetCurrentCulture()->GetName();
	DTSettings::Live() = FDTUserSettings();
	const FRunResult Base = Play(31);
	DTSettings::Live() = AllChanged();
	UDTSettingsSubsystem::ApplyCulture(DTSettings::Live());
	UDTSettingsSubsystem::ApplyVolume(DTSettings::Live());
	const FRunResult Changed = Play(31);
	FInternationalization::Get().SetCurrentCulture(Culture);
	FApp::SetVolumeMultiplier(1.0f);

	TestTrue(TEXT("the base run played"), Base.Steps > 0 && Base.Picks >= 1 && !Base.Csv.IsEmpty());
	TestEqual(TEXT("same steps"), Changed.Steps, Base.Steps);
	TestEqual(TEXT("same input stream"), Changed.InputHash, Base.InputHash);
	TestEqual(TEXT("same hero position"), Changed.Pos, Base.Pos);
	TestEqual(TEXT("same HP"), Changed.Hp, Base.Hp);
	// The culture tag is the one field that names a setting (R-SET-01): the run under it must match byte for byte.
	const auto SameCulture = [](const FString& Text) { return Text.Replace(TEXT("zh-Hans"), TEXT("en")); };
	TestTrue(TEXT("the tag did change"), Changed.Csv != Base.Csv);
	TestEqual(TEXT("identical telemetry CSV"), SameCulture(Changed.Csv), SameCulture(Base.Csv));
	TestTrue(TEXT("identical telemetry JSONL except wall fields"), SameCulture(StripWall(Changed.Jsonl)) == SameCulture(StripWall(Base.Jsonl)));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetReaders, "Dusktracer.Settings.Readers", DTSettingsTest::Flags) // T-SET-02
bool FDTSetReaders::RunTest(const FString& Parameters)
{
	const FString Root = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer")));
	const TArray<FString> Allowed = {
		TEXT("Game/DTHUD.cpp"), TEXT("Offers/DTOfferScreen.cpp"), TEXT("Player/DTHero.cpp"), TEXT("Player/DTInputSet.cpp"),
		TEXT("Enemies/DTTelegraphRenderer.cpp"), TEXT("Enemies/DTEnemyBulletRenderer.cpp") };
	TArray<FString> Files;
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.h"), true, false);
	IFileManager::Get().FindFilesRecursive(Files, *Root, TEXT("*.cpp"), true, false, false);
	TestTrue(TEXT("found the source"), Files.Num() > 100);
	int32 Readers = 0;
	for (const FString& File : Files)
	{
		FString Rel = File;
		FPaths::MakePathRelativeTo(Rel, *(Root + TEXT("/")));
		if (Rel.StartsWith(TEXT("Settings/")) || Rel.StartsWith(TEXT("Tests/Settings/")))
		{
			continue;
		}
		FString Text;
		FFileHelper::LoadFileToString(Text, *File);
		if (Text.Contains(TEXT("#include \"Settings/")))
		{
			++Readers;
			TestTrue(FString::Printf(TEXT("%s may read settings"), *Rel), Allowed.Contains(Rel));
		}
	}
	TestEqual(TEXT("each listed reader exists"), Readers, Allowed.Num());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetFile, "Dusktracer.Settings.File", DTSettingsTest::Flags) // T-SET-03
bool FDTSetFile::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	const FString Path = TempFile(TEXT("RoundTrip.ini"));
	const FDTUserSettings Written = AllChanged();
	TestTrue(TEXT("saved"), DTSettings::Save(Path, Written));
	FDTUserSettings Read;
	TestTrue(TEXT("loaded"), DTSettings::Load(Path, Read));
	TestTrue(TEXT("every field round-trips"), Read == Written);

	FString Text;
	FFileHelper::LoadFileToString(Text, *Path);
	TestTrue(TEXT("the version leads"), Text.StartsWith(FString::Printf(TEXT("Version=%d"), DTConst::SETTINGS_FILE_VERSION)));
	TestTrue(TEXT("plain text with key=value lines"), Text.Contains(TEXT("Language=zh-Hans")) && Text.Contains(TEXT("Bind.Dash.Kbm=LeftShift")));
	TestTrue(TEXT("the default path is under Saved/Config"), DTSettings::DefaultPath().EndsWith(TEXT("Saved/Config/DTSettings.ini")));

	// A failed write leaves the old file: the temporary sibling is blocked by a directory of that name.
	const FString Blocked = TempFile(TEXT("Blocked.ini"));
	TestTrue(TEXT("first write"), DTSettings::Save(Blocked, Written));
	const FString Tmp = Blocked + TEXT(".tmp");
	IFileManager::Get().MakeDirectory(*Tmp, true);
	AddExpectedError(TEXT("settings"), EAutomationExpectedErrorFlags::Contains, 0);
	TestFalse(TEXT("second write fails"), DTSettings::Save(Blocked, FDTUserSettings()));
	IFileManager::Get().DeleteDirectory(*Tmp, false, true);
	FDTUserSettings Kept;
	TestTrue(TEXT("still readable"), DTSettings::Load(Blocked, Kept));
	TestTrue(TEXT("the old values stand"), Kept == Written);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetBadFile, "Dusktracer.Settings.BadFile", DTSettingsTest::Flags) // T-SET-04
bool FDTSetBadFile::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	const FDTUserSettings Defaults;
	FDTUserSettings Out = AllChanged();
	TestFalse(TEXT("a missing file reports false"), DTSettings::Load(TempFile(TEXT("Missing.ini")), Out));
	TestTrue(TEXT("a missing file gives the defaults"), Out == Defaults);
	TestTrue(TEXT("empty text"), DTSettings::Parse(TEXT("")) == Defaults);
	TestTrue(TEXT("garbage"), DTSettings::Parse(TEXT("\x7F\x01=== not a settings file\nLanguage\n=\n")) == Defaults);
	TestTrue(TEXT("an unknown key is ignored"), DTSettings::Parse(TEXT("Version=99\nFutureRow=7\n")) == Defaults);

	const FDTUserSettings Mixed = DTSettings::Parse(TEXT("Language=klingon\nVolume=500\nText=999\nGraphics=Ultra\nWindow=zh\nShake=maybe\nFlash=0\n"));
	TestEqual(TEXT("a bad language stays default"), Mixed.Language, Defaults.Language);
	TestEqual(TEXT("a volume out of range clamps"), Mixed.VolumePct, DTConst::SETTINGS_VOLUME_MAX_PCT);
	TestEqual(TEXT("a text size out of range clamps"), Mixed.TextPct, DTConst::SETTINGS_TEXT_MAX_PCT);
	TestEqual(TEXT("a bad graphics value stays default"), Mixed.Graphics, Defaults.Graphics);
	TestEqual(TEXT("a bad window value stays default"), Mixed.Window, Defaults.Window);
	TestEqual(TEXT("a bad bool stays default"), Mixed.bShake, Defaults.bShake);
	TestFalse(TEXT("one good row beside bad ones still reads"), Mixed.bFlash);
	TestEqual(TEXT("volume snaps to its step"), DTSettings::Parse(TEXT("Volume=33\n")).VolumePct, 30);
	TestEqual(TEXT("text size snaps to its step"), DTSettings::Parse(TEXT("Text=130\n")).TextPct, 125);

	// A duplicate, a reserved key and a wrong-device key each undo their device and group, and only those.
	const FDTBindings Good = FDTBindings::Defaults();
	const FDTUserSettings Dup = DTSettings::Parse(TEXT("Bind.Dash.Kbm=Q\nBind.UiReroll.Kbm=T\n"));
	TestEqual(TEXT("a duplicate undoes the in-play keys"), Dup.Bindings.Get(EDTBindSlot::Dash, EDTBindDevice::Kbm), Good.Get(EDTBindSlot::Dash, EDTBindDevice::Kbm));
	TestEqual(TEXT("another group's change stands"), Dup.Bindings.Get(EDTBindSlot::UiReroll, EDTBindDevice::Kbm), FKey(EKeys::T));
	TestTrue(TEXT("a fixed key undoes"), DTSettings::Parse(TEXT("Bind.Dash.Kbm=Escape\n")).Bindings == Good);
	TestTrue(TEXT("a pad key in a keyboard slot undoes"), DTSettings::Parse(TEXT("Bind.Dash.Kbm=Gamepad_FaceButton_Left\n")).Bindings == Good);
	TestTrue(TEXT("a key that does not exist undoes"), DTSettings::Parse(TEXT("Bind.Dash.Kbm=NoSuchKey\n")).Bindings == Good);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetToolRuns, "Dusktracer.Settings.ToolRuns", DTSettingsTest::Flags) // T-SET-05
bool FDTSetToolRuns::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("a player's process uses the file"), DTSettings::UsesFile(false, false));
	TestFalse(TEXT("a tool run does not"), DTSettings::UsesFile(true, false));
	TestTrue(TEXT("a tool run naming a file does"), DTSettings::UsesFile(true, true));
	TestTrue(TEXT("this very run has no active file"), UDTSettingsSubsystem::ActivePath().IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetLanguage, "Dusktracer.Settings.LanguageOnePress", DTSettingsTest::Flags) // T-SET-06
bool FDTSetLanguage::RunTest(const FString& Parameters)
{
	FDTUserSettings S;
	FDTMenuState Menu;
	Menu.Open();
	Menu.Apply(EDTMenuCommand::Down, S);
	Menu.Apply(EDTMenuCommand::Accept, S); // Settings; Language is its first row
	TestEqual(TEXT("on the settings screen"), Menu.Screen, EDTMenuScreen::Settings);
	int32 Requests = 0;
	for (int32 Press = 0; Press < 8; ++Press)
	{
		Requests += Menu.Apply(EDTMenuCommand::Accept, S).bLanguageRequested ? 1 : 0;
	}
	TestEqual(TEXT("eight presses queue one switch"), Requests, 1);
	TestTrue(TEXT("the language changed once"), S.Language == EDTLanguage::ZhHans);
	TestTrue(TEXT("the switch is pending"), Menu.bLanguagePending);
	Menu.bLanguagePending = false;
	TestTrue(TEXT("the next press works once it ran"), Menu.Apply(EDTMenuCommand::Right, S).bLanguageRequested);
	TestTrue(TEXT("back to English"), S.Language == EDTLanguage::En);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetVolume, "Dusktracer.Settings.Volume", DTSettingsTest::Flags) // T-SET-07
bool FDTSetVolume::RunTest(const FString& Parameters)
{
	FDTUserSettings S;
	TestEqual(TEXT("default is the ceiling"), S.VolumePct, DTConst::SETTINGS_VOLUME_MAX_PCT);
	TestFalse(TEXT("the ceiling holds"), DTSettings::StepRow(S, EDTSettingRow::Volume, 1));
	TestTrue(TEXT("steps down"), DTSettings::StepRow(S, EDTSettingRow::Volume, -1));
	TestEqual(TEXT("by one step"), S.VolumePct, DTConst::SETTINGS_VOLUME_MAX_PCT - DTConst::SETTINGS_VOLUME_STEP_PCT);
	while (DTSettings::StepRow(S, EDTSettingRow::Volume, -1))
	{
	}
	TestEqual(TEXT("the floor is silence"), S.VolumePct, 0);
	const float Was = FApp::GetVolumeMultiplier();
	S.VolumePct = DTConst::SETTINGS_VOLUME_MAX_PCT / 2;
	UDTSettingsSubsystem::ApplyVolume(S);
	TestEqual(TEXT("the application volume follows"), FApp::GetVolumeMultiplier(), 0.5f);
	FApp::SetVolumeMultiplier(Was);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetDisplay, "Dusktracer.Settings.Display", DTSettingsTest::Flags) // T-SET-08
bool FDTSetDisplay::RunTest(const FString& Parameters)
{
	TestEqual(TEXT("Epic is level 3"), DTSettings::ScalabilityLevel(EDTGraphics::Epic), 3);
	TestEqual(TEXT("High is level 2"), DTSettings::ScalabilityLevel(EDTGraphics::High), 2);
	TestEqual(TEXT("Medium is level 1"), DTSettings::ScalabilityLevel(EDTGraphics::Medium), 1);
	TestEqual(TEXT("Low is level 0"), DTSettings::ScalabilityLevel(EDTGraphics::Low), 0);
	FDTUserSettings S;
	TSet<int32> Seen;
	for (int32 Step = 0; Step < static_cast<int32>(EDTGraphics::Num); ++Step)
	{
		Seen.Add(static_cast<int32>(S.Graphics));
		TestTrue(TEXT("graphics steps"), DTSettings::StepRow(S, EDTSettingRow::Graphics, 1));
	}
	TestEqual(TEXT("every level is reached"), Seen.Num(), static_cast<int32>(EDTGraphics::Num));
	TestEqual(TEXT("and it wraps"), S.Graphics, EDTGraphics::Epic);
	TestTrue(TEXT("window steps"), DTSettings::StepRow(S, EDTSettingRow::Window, 1));
	TestEqual(TEXT("windowed"), S.Window, EDTWindow::Windowed);
	TestTrue(TEXT("window steps back"), DTSettings::StepRow(S, EDTSettingRow::Window, -1));
	TestEqual(TEXT("fullscreen"), S.Window, EDTWindow::Fullscreen);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetMotion, "Dusktracer.Settings.Motion", DTSettingsTest::Flags) // T-SET-09
bool FDTSetMotion::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	FLiveGuard Guard;
	FDTUserSettings& S = DTSettings::Live();
	S = FDTUserSettings();
	TestEqual(TEXT("default Full"), S.GetMotion(), EDTMotion::Full);
	TestTrue(TEXT("read points on"), DTSettings::AllowShake() && DTSettings::AllowFlash());
	TestTrue(TEXT("steps"), DTSettings::StepRow(S, EDTSettingRow::Motion, 1));
	TestEqual(TEXT("Reduced"), S.GetMotion(), EDTMotion::Reduced);
	TestFalse(TEXT("shake off"), DTSettings::AllowShake());
	TestFalse(TEXT("flash off"), DTSettings::AllowFlash());
	TestTrue(TEXT("one row by hand"), DTSettings::StepRow(S, EDTSettingRow::Flash, 1));
	TestEqual(TEXT("a mix reads Custom"), S.GetMotion(), EDTMotion::Custom);
	TestTrue(TEXT("flash follows its row alone"), DTSettings::AllowFlash() && !DTSettings::AllowShake());
	TestTrue(TEXT("right from Custom"), DTSettings::StepRow(S, EDTSettingRow::Motion, 1));
	TestEqual(TEXT("goes Full"), S.GetMotion(), EDTMotion::Full);
	S.bFlash = false;
	TestTrue(TEXT("left from Custom"), DTSettings::StepRow(S, EDTSettingRow::Motion, -1));
	TestEqual(TEXT("goes Reduced"), S.GetMotion(), EDTMotion::Reduced);
	TestTrue(TEXT("hints name the missing effects"), !DTMenuText::RowHint(EDTSettingRow::Shake).IsEmpty() && !DTMenuText::RowHint(EDTSettingRow::Flash).IsEmpty());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetOutline, "Dusktracer.Settings.DangerOutline", DTSettingsTest::Flags) // T-SET-10
bool FDTSetOutline::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	FLiveGuard Guard;
	DTSettings::Live() = FDTUserSettings();
	FTestWorld Test;
	const FLinearColor Light = DTPal::Linear(DTPal::Assist_Light);
	const FLinearColor Dark = DTPal::Linear(DTPal::Assist_Dark);

	FDTTelegraph Band;
	Band.Shape = EDTTelegraphShape::Band;
	Band.Origin = FVector2f::ZeroVector;
	Band.Dir = FVector2f(1.0f, 0.0f);
	Band.Length = 400.0f;
	Band.Width = 80.0f;
	const TArray<FDTTelegraph> Shapes = { Band };

	ADTTelegraphRenderer* Telegraphs = Test.World->SpawnActor<ADTTelegraphRenderer>();
	Telegraphs->InitRender(DTPal::Linear(DTPal::Telegraph), 1.0f);
	Telegraphs->SetTelegraphs(Shapes);
	const int32 PlainCount = Telegraphs->GetInstanceCount();
	TestEqual(TEXT("off: the usual layers"), PlainCount, ADTTelegraphRenderer::InstancesDrawn(Band));
	TestEqual(TEXT("off: no light band"), CountColor(Telegraphs->GetInstances(), ADTTelegraphRenderer::NumCustomData, Light), 0);

	ADTTelegraphRenderer* Cues = Test.World->SpawnActor<ADTTelegraphRenderer>();
	Cues->InitRender(DTPal::Linear(DTPal::Keeper_Tide), 1.0f);
	Cues->SetEdged(false);

	ADTEnemyBulletRenderer* Rounds = Test.World->SpawnActor<ADTEnemyBulletRenderer>();
	Rounds->InitRender(DTPal::Linear(DTPal::Enemy_Bullet), 1.0f, DTPal::Linear(DTPal::Enemy_BulletCore));
	const float RoundScale = 20.0f / Rounds->GetMeshRadius();
	const TArray<FTransform> RoundTransforms = {
		FTransform(FQuat::Identity, FVector(10.0, 0.0, 50.0), FVector(RoundScale)), FTransform(FQuat::Identity, FVector(90.0, 0.0, 50.0), FVector(RoundScale)) };
	Rounds->SetInstances(RoundTransforms);
	TestEqual(TEXT("off: no disc"), Rounds->GetOutlineCount(), 0);

	DTSettings::Live().bDangerOutline = true;
	Telegraphs->SetTelegraphs(Shapes);
	TestEqual(TEXT("on: one more quad for the band"), Telegraphs->GetInstanceCount(), PlainCount + 1);
	TestEqual(TEXT("on: it is the assist token"), CountColor(Telegraphs->GetInstances(), ADTTelegraphRenderer::NumCustomData, Light), 1);
	TestEqual(TEXT("on: the red's footprint is unchanged"),
		CountColor(Telegraphs->GetInstances(), ADTTelegraphRenderer::NumCustomData, DTPal::Linear(DTPal::Telegraph)), 2);
	FTransform Outer;
	Telegraphs->GetInstances()->GetInstanceTransform(0, Outer, true);
	TestEqual(TEXT("on: the band sits outside the dark edge"), static_cast<float>(Outer.GetScale3D().Y) * Telegraphs->GetMeshSizeUU(),
		Band.Width + 2.0f * static_cast<float>(DTConst::PRE_TELEGRAPH_EDGE_UU + DTConst::SETTINGS_OUTLINE_BAND_UU), 0.01f);

	Cues->SetTelegraphs(Shapes);
	TestEqual(TEXT("a cue renderer gains nothing"), Cues->GetInstanceCount(), 1);

	Rounds->SetInstances(RoundTransforms);
	TestEqual(TEXT("on: two discs per round"), Rounds->GetOutlineCount(), 4);
	TestEqual(TEXT("light discs"), CountColor(Rounds->GetOutlines(), ADTEnemyBulletRenderer::NumOutlineData, Light), 2);
	TestEqual(TEXT("dark discs"), CountColor(Rounds->GetOutlines(), ADTEnemyBulletRenderer::NumOutlineData, Dark), 2);
	TestEqual(TEXT("the rounds themselves stay"), Rounds->GetInstanceCount(), 2);

	DTSettings::Live().bDangerOutline = false;
	Telegraphs->SetTelegraphs(Shapes);
	Rounds->SetInstances(RoundTransforms);
	TestEqual(TEXT("off again: telegraph as before"), Telegraphs->GetInstanceCount(), PlainCount);
	TestEqual(TEXT("off again: no disc"), Rounds->GetOutlineCount(), 0);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetColourVision, "Dusktracer.Settings.ColourVision", DTSettingsTest::Flags) // T-SET-11
bool FDTSetColourVision::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	struct FToken { const TCHAR* Name; FColor Color; };
	const FToken Player[] = { { TEXT("Core"), DTPal::Player_Core }, { TEXT("Neutral"), DTPal::Player_TrailNeutral }, { TEXT("Storm"), DTPal::Keeper_Storm },
		{ TEXT("Forge"), DTPal::Keeper_Forge }, { TEXT("Tide"), DTPal::Keeper_Tide }, { TEXT("Shade"), DTPal::Keeper_Shade },
		{ TEXT("Radiance"), DTPal::Keeper_Radiance }, { TEXT("Quicksilver"), DTPal::Keeper_Quicksilver } };
	struct FVision { const TCHAR* Name; const double (*Matrix)[3]; };
	const FVision Vision[] = { { TEXT("normal"), nullptr }, { TEXT("protan"), ProtanM }, { TEXT("deutan"), DeutanM }, { TEXT("tritan"), TritanM } };
	struct FEnemy { const TCHAR* Name; FColor Color; double Min; };
	const FEnemy Enemies[] = { { TEXT("round"), DTPal::Enemy_Bullet, DTConst::SETTINGS_CVD_DELTA_E_MIN }, { TEXT("telegraph"), DTPal::Telegraph, DTConst::SETTINGS_CVD_DELTA_E_MIN },
		{ TEXT("muzzle"), DTPal::Enemy_Muzzle, DTConst::SETTINGS_CVD_MUZZLE_DELTA_E_MIN } };

	// The debts of R-SET-11, by name: a new failure fails this test, and so does a debt that no longer exists.
	TSet<FString> Debts = { TEXT("telegraph/Forge/deutan"), TEXT("muzzle/Forge/deutan") };
	for (const FEnemy& Enemy : Enemies)
	{
		for (const FToken& Token : Player)
		{
			for (const FVision& Mode : Vision)
			{
				const double D = DeltaE(Enemy.Color, Token.Color, Mode.Matrix);
				const FString Pair = FString::Printf(TEXT("%s/%s/%s"), Enemy.Name, Token.Name, Mode.Name);
				if (D < Enemy.Min && Debts.Remove(Pair) == 0)
				{
					AddError(FString::Printf(TEXT("%s: delta E %.1f under %.1f"), *Pair, D, Enemy.Min));
				}
			}
		}
	}
	TestEqual(TEXT("every named debt is still a debt"), Debts.Num(), 0);
	TestEqual(TEXT("the method reproduces the source's Forge figure"), DeltaE(DTPal::Enemy_Muzzle, DTPal::Keeper_Forge, DeutanM), 8.5, 0.1);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetTextSize, "Dusktracer.Settings.TextSize", DTSettingsTest::Flags) // T-SET-12
bool FDTSetTextSize::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	FLiveGuard Guard;
	FDTUserSettings& S = DTSettings::Live();
	S = FDTUserSettings();
	TestEqual(TEXT("default is the smallest"), S.TextPct, DTConst::SETTINGS_TEXT_MIN_PCT);
	TestEqual(TEXT("scale at the default"), DTSettings::TextScale(), 1.0f);
	TestFalse(TEXT("the floor holds"), DTSettings::StepRow(S, EDTSettingRow::TextSize, -1));
	TestTrue(TEXT("one step"), DTSettings::StepRow(S, EDTSettingRow::TextSize, 1));
	TestEqual(TEXT("125 %"), DTSettings::TextScale(), 1.25f);
	TestTrue(TEXT("two steps"), DTSettings::StepRow(S, EDTSettingRow::TextSize, 1));
	TestEqual(TEXT("150 %"), DTSettings::TextScale(), 1.5f);
	TestFalse(TEXT("the ceiling holds"), DTSettings::StepRow(S, EDTSettingRow::TextSize, 1));

	FDTMenuState Menu;
	Menu.Screen = EDTMenuScreen::Controls;
	TestTrue(TEXT("the longest screen fits a 1080-line canvas at the largest size"), DTMenuLayout::ContentHeight(Menu.NumRows(), 1.5f) <= 1080.0f);
	TestTrue(TEXT("rows grow with the text"), DTMenuLayout::RowHeight(1.5f) >= 1.5f * DTMenuLayout::RowHeight(1.0f) - 0.01f);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetBindingTable, "Dusktracer.Settings.BindingTable", DTSettingsTest::Flags) // T-SET-13
bool FDTSetBindingTable::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	FLiveGuard Guard;
	DTSettings::Live() = FDTUserSettings();
	const FDTBindings D = FDTBindings::Defaults();
	TestEqual(TEXT("Dash on keyboard"), D.Get(EDTBindSlot::Dash, EDTBindDevice::Kbm), FKey(EKeys::SpaceBar));
	TestEqual(TEXT("Dash on pad"), D.Get(EDTBindSlot::Dash, EDTBindDevice::Pad), FKey(EKeys::Gamepad_RightShoulder));
	TestEqual(TEXT("Intercession on keyboard"), D.Get(EDTBindSlot::Intercession, EDTBindDevice::Kbm), FKey(EKeys::Q));
	TestEqual(TEXT("Intercession on pad"), D.Get(EDTBindSlot::Intercession, EDTBindDevice::Pad), FKey(EKeys::Gamepad_LeftShoulder));
	TestEqual(TEXT("Up"), D.Get(EDTBindSlot::MoveUp, EDTBindDevice::Kbm), FKey(EKeys::W));
	TestFalse(TEXT("the left stick is fixed"), DTBindings::IsRebindable(EDTBindSlot::MoveUp, EDTBindDevice::Pad));
	TestFalse(TEXT("the D-pad is fixed"), DTBindings::IsRebindable(EDTBindSlot::UiPrev, EDTBindDevice::Pad));
	TestEqual(TEXT("Hold on pad"), D.Get(EDTBindSlot::UiLock, EDTBindDevice::Pad), FKey(EKeys::Gamepad_RightShoulder));

	UDTInputSet* Set = UDTInputSet::Create(GetTransientPackage());
	TestTrue(TEXT("the default context maps Space"), ContextMaps(Set->Kbm, EKeys::SpaceBar));
	DTSettings::Live().Bindings.Set(EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::LeftShift);
	DTSettings::Live().Bindings.Set(EDTBindSlot::Intercession, EDTBindDevice::Pad, EKeys::Gamepad_FaceButton_Top);
	Set->ApplyBindings(DTSettings::Live().Bindings);
	TestTrue(TEXT("the keyboard context follows"), ContextMaps(Set->Kbm, EKeys::LeftShift) && !ContextMaps(Set->Kbm, EKeys::SpaceBar));
	TestTrue(TEXT("the pad context follows"), ContextMaps(Set->Pad, EKeys::Gamepad_FaceButton_Top) && !ContextMaps(Set->Pad, EKeys::Gamepad_LeftShoulder));
	TestTrue(TEXT("Pause stays"), ContextMaps(Set->Kbm, EKeys::Escape) && ContextMaps(Set->Pad, EKeys::Gamepad_Special_Right));

	DTSettings::Live().Bindings.Set(EDTBindSlot::UiReroll, EDTBindDevice::Kbm, EKeys::T);
	TestEqual(TEXT("the offer screen follows"), DTOfferInput::ForKey(EKeys::T).Action, EDTOfferAction::Reroll);
	TestEqual(TEXT("the old key is free"), DTOfferInput::ForKey(EKeys::R).Action, EDTOfferAction::None);
	TestEqual(TEXT("fixed Left stays"), DTOfferInput::ForKey(EKeys::Left).Action, EDTOfferAction::Prev);
	TestTrue(TEXT("the prompt follows"), DTOfferInput::ToolGlyphs(EDTOfferAction::Reroll).ToString().StartsWith(TEXT("T / ")));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetRebind, "Dusktracer.Settings.Rebind", DTSettingsTest::Flags) // T-SET-14
bool FDTSetRebind::RunTest(const FString& Parameters)
{
	FDTBindings B = FDTBindings::Defaults();
	EDTBindSlot Other = EDTBindSlot::Num;
	TestEqual(TEXT("a free key binds"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::LeftShift), EDTRebindResult::Bound);
	TestEqual(TEXT("a held key swaps"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::Q, &Other), EDTRebindResult::Swapped);
	TestEqual(TEXT("with the holder"), Other, EDTBindSlot::Intercession);
	TestEqual(TEXT("which takes the old key"), B.Get(EDTBindSlot::Intercession, EDTBindDevice::Kbm), FKey(EKeys::LeftShift));
	TestEqual(TEXT("Esc is fixed"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::Escape), EDTRebindResult::Reserved);
	TestEqual(TEXT("Enter is fixed"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::Enter), EDTRebindResult::Reserved);
	TestEqual(TEXT("pad Menu is fixed"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Pad, EKeys::Gamepad_Special_Right), EDTRebindResult::Reserved);
	TestEqual(TEXT("pad B is fixed"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Pad, EKeys::Gamepad_FaceButton_Right), EDTRebindResult::Reserved);
	TestEqual(TEXT("Space is free in play"), DTBindings::Rebind(B, EDTBindSlot::MoveUp, EDTBindDevice::Kbm, EKeys::SpaceBar), EDTRebindResult::Bound);
	TestEqual(TEXT("but picks cards at level-up"), DTBindings::Rebind(B, EDTBindSlot::UiLock, EDTBindDevice::Kbm, EKeys::SpaceBar), EDTRebindResult::Reserved);
	TestEqual(TEXT("number keys pick cards"), DTBindings::Rebind(B, EDTBindSlot::UiLock, EDTBindDevice::Kbm, EKeys::Three), EDTRebindResult::Reserved);
	TestEqual(TEXT("a pad key on the keyboard column"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Kbm, EKeys::Gamepad_FaceButton_Left), EDTRebindResult::WrongDevice);
	TestEqual(TEXT("a keyboard key on the pad column"), DTBindings::Rebind(B, EDTBindSlot::Dash, EDTBindDevice::Pad, EKeys::T), EDTRebindResult::WrongDevice);
	TestEqual(TEXT("groups do not conflict: Q in both"), DTBindings::Rebind(B, EDTBindSlot::UiLock, EDTBindDevice::Kbm, EKeys::Q), EDTRebindResult::Bound);

	// Any sequence leaves every group legal.
	FRandomStream Rng(7);
	const FKey Pool[] = { EKeys::E, EKeys::F, EKeys::G, EKeys::Q, EKeys::R, EKeys::T, EKeys::Y, EKeys::Escape, EKeys::W, EKeys::S };
	for (int32 Try = 0; Try < 2000; ++Try)
	{
		const EDTBindSlot Slot = static_cast<EDTBindSlot>(Rng.RandHelper(static_cast<int32>(EDTBindSlot::Num)));
		DTBindings::Rebind(B, Slot, EDTBindDevice::Kbm, Pool[Rng.RandHelper(UE_ARRAY_COUNT(Pool))]);
	}
	TestTrue(TEXT("in-play keys stay legal"), DTBindings::IsLegal(B, EDTBindDevice::Kbm, EDTBindGroup::InPlay));
	TestTrue(TEXT("level-up keys stay legal"), DTBindings::IsLegal(B, EDTBindDevice::Kbm, EDTBindGroup::LevelUp));

	FDTUserSettings S;
	S.Bindings = B;
	FDTMenuState Menu;
	Menu.Open();
	Menu.Screen = EDTMenuScreen::Controls;
	Menu.Focus = FDTMenuState::ResetRow();
	const FDTMenuOutcome Reset = Menu.Apply(EDTMenuCommand::Accept, S);
	TestTrue(TEXT("reset reports the change"), Reset.bBindingsChanged);
	TestTrue(TEXT("reset restores every slot"), S.Bindings == FDTBindings::Defaults());
	TestEqual(TEXT("reset says so"), Menu.Status, EDTMenuStatus::ResetDone);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetCapture, "Dusktracer.Settings.Capture", DTSettingsTest::Flags) // T-SET-15
bool FDTSetCapture::RunTest(const FString& Parameters)
{
	FDTUserSettings S;
	FDTMenuState Menu;
	Menu.Open();
	Menu.Screen = EDTMenuScreen::Controls;
	Menu.Focus = static_cast<int32>(EDTBindSlot::Dash);
	Menu.Apply(EDTMenuCommand::Accept, S);
	TestTrue(TEXT("Accept starts capture"), Menu.bCapturing);
	TestEqual(TEXT("status"), Menu.Status, EDTMenuStatus::Capturing);
	const FDTMenuOutcome Ignored = Menu.Apply(EDTMenuCommand::Down, S);
	TestEqual(TEXT("navigation does nothing during capture"), Menu.Focus, static_cast<int32>(EDTBindSlot::Dash));
	TestFalse(TEXT("nothing changed"), Ignored.bBindingsChanged);

	const FDTMenuOutcome Cancelled = Menu.Capture(EKeys::Escape, S);
	TestFalse(TEXT("Esc cancels"), Menu.bCapturing);
	TestFalse(TEXT("no change"), Cancelled.bBindingsChanged);
	TestEqual(TEXT("status cancelled"), Menu.Status, EDTMenuStatus::Cancelled);
	TestTrue(TEXT("table unchanged"), S.Bindings == FDTBindings::Defaults());

	Menu.Apply(EDTMenuCommand::Accept, S);
	const FDTMenuOutcome Bound = Menu.Capture(EKeys::LeftShift, S);
	TestTrue(TEXT("a key binds"), Bound.bBindingsChanged && !Menu.bCapturing);
	TestEqual(TEXT("status bound"), Menu.Status, EDTMenuStatus::Bound);
	TestEqual(TEXT("the key landed"), S.Bindings.Get(EDTBindSlot::Dash, EDTBindDevice::Kbm), FKey(EKeys::LeftShift));

	Menu.Apply(EDTMenuCommand::Accept, S);
	Menu.Capture(EKeys::Gamepad_FaceButton_Left, S);
	TestEqual(TEXT("a pad key on the keyboard column"), Menu.Status, EDTMenuStatus::WrongDeviceKbm);
	Menu.Apply(EDTMenuCommand::Accept, S);
	Menu.Capture(EKeys::Enter, S);
	TestEqual(TEXT("a fixed key"), Menu.Status, EDTMenuStatus::Reserved);

	Menu.Focus = static_cast<int32>(EDTBindSlot::MoveUp);
	Menu.Apply(EDTMenuCommand::Right, S);
	TestEqual(TEXT("right moves to the pad column"), Menu.Column, EDTBindDevice::Pad);
	Menu.Apply(EDTMenuCommand::Accept, S);
	TestFalse(TEXT("a fixed cell does not capture"), Menu.bCapturing);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetPauseOverHand, "Dusktracer.Settings.PauseOverHand", DTSettingsTest::Flags) // T-SET-16
bool FDTSetPauseOverHand::RunTest(const FString& Parameters)
{
	using namespace DTSettingsTest;
	FLiveGuard Guard;
	FTestWorld Test;
	UDTMenuSubsystem* Menu = Test.World->GetSubsystem<UDTMenuSubsystem>();
	UDTOfferSubsystem* Offers = Test.World->GetSubsystem<UDTOfferSubsystem>();
	if (!TestTrue(TEXT("subsystems"), Menu && Offers))
	{
		return false;
	}
	TArray<bool> Pauses;
	Menu->SetPauseOverride([&Pauses](bool bPaused) { Pauses.Add(bPaused); });

	Menu->RequestPause();
	TestTrue(TEXT("Pause opens the screen"), Menu->IsOpen());
	TestEqual(TEXT("on the pause screen"), Menu->GetState().Screen, EDTMenuScreen::Pause);
	TestEqual(TEXT("and takes the world pause"), Pauses, TArray<bool>{ true });
	Menu->RequestPause();
	TestEqual(TEXT("a second Pause does not re-take it"), Pauses.Num(), 1);
	Menu->Command(EDTMenuCommand::Accept); // Resume
	TestFalse(TEXT("Resume closes"), Menu->IsOpen());
	TestEqual(TEXT("and releases the pause"), Pauses, TArray<bool>{ true, false });

	Offers->UpdateFlow();
	const FDTOfferHand* Hand = Offers->GetOpenHand(0);
	TestNotNull(TEXT("the Patron hand is open under the menu"), Hand);
	const double OpenedBefore = Offers->GetOpenedWall(0);
	Menu->RequestPause();
	TestTrue(TEXT("the hand takes no input under the menu"), Offers->IsInputHeld());
	Offers->ResumeAfterMenu(12.5);
	TestFalse(TEXT("input returns"), Offers->IsInputHeld());
	TestEqual(TEXT("menu time is not decision time"), Offers->GetOpenedWall(0), OpenedBefore + 12.5, 0.001);
	TestTrue(TEXT("the confirm guard restarted"), Offers->IsGuarded(0));
	Menu->Close();
	TestNotNull(TEXT("closing leaves the hand open"), Offers->GetOpenHand(0));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetNavigation, "Dusktracer.Settings.Navigation", DTSettingsTest::Flags) // T-SET-17
bool FDTSetNavigation::RunTest(const FString& Parameters)
{
	// Each device list holds every command on its own.
	const TArray<FKey> KeyboardKeys = { EKeys::Up, EKeys::Down, EKeys::Left, EKeys::Right, EKeys::Enter, EKeys::Escape };
	const TArray<FKey> PadKeys = { EKeys::Gamepad_DPad_Up, EKeys::Gamepad_DPad_Down, EKeys::Gamepad_DPad_Left, EKeys::Gamepad_DPad_Right,
		EKeys::Gamepad_FaceButton_Bottom, EKeys::Gamepad_FaceButton_Right };
	const EDTMenuCommand Commands[] = { EDTMenuCommand::Up, EDTMenuCommand::Down, EDTMenuCommand::Left, EDTMenuCommand::Right, EDTMenuCommand::Accept, EDTMenuCommand::Back };
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Commands); ++Index)
	{
		TestEqual(TEXT("keyboard"), DTMenuInput::ForKey(KeyboardKeys[Index]), Commands[Index]);
		TestEqual(TEXT("pad"), DTMenuInput::ForKey(PadKeys[Index]), Commands[Index]);
		TestTrue(TEXT("the key is polled"), DTMenuInput::Keys().Contains(KeyboardKeys[Index]) && DTMenuInput::Keys().Contains(PadKeys[Index]));
	}
	TestEqual(TEXT("left stick moves"), DTMenuInput::ForKey(EKeys::Gamepad_LeftStick_Down), EDTMenuCommand::Down);
	TestEqual(TEXT("Space accepts"), DTMenuInput::ForKey(EKeys::SpaceBar), EDTMenuCommand::Accept);
	TestEqual(TEXT("S moves"), DTMenuInput::ForKey(EKeys::S), EDTMenuCommand::Down);
	TestEqual(TEXT("pad Menu backs out"), DTMenuInput::ForKey(EKeys::Gamepad_Special_Right), EDTMenuCommand::Back);

	FDTUserSettings S;
	FDTMenuState Menu;
	Menu.Open();
	TestEqual(TEXT("opens on Pause"), Menu.Screen, EDTMenuScreen::Pause);
	TestEqual(TEXT("Pause has its rows"), Menu.NumRows(), static_cast<int32>(EDTPauseRow::Num));
	Menu.Apply(EDTMenuCommand::Up, S);
	TestEqual(TEXT("focus wraps up"), Menu.Focus, Menu.NumRows() - 1);
	Menu.Apply(EDTMenuCommand::Accept, S);
	TestEqual(TEXT("Settings"), Menu.Screen, EDTMenuScreen::Settings);

	// Every settings row answers: value rows to left or right, Controls to Accept.
	for (int32 Row = 0; Row < Menu.NumRows(); ++Row)
	{
		TestEqual(TEXT("focus reaches the row"), Menu.Focus, Row);
		if (Row == static_cast<int32>(EDTSettingRow::Controls))
		{
			Menu.Apply(EDTMenuCommand::Accept, S);
			TestEqual(TEXT("Controls opens"), Menu.Screen, EDTMenuScreen::Controls);
			for (int32 Slot = 0; Slot < Menu.NumRows(); ++Slot)
			{
				TestEqual(TEXT("focus reaches the slot"), Menu.Focus, Slot);
				Menu.Apply(EDTMenuCommand::Down, S);
			}
			TestEqual(TEXT("focus wraps down"), Menu.Focus, 0);
			Menu.Apply(EDTMenuCommand::Back, S);
			TestEqual(TEXT("Back returns to Settings"), Menu.Screen, EDTMenuScreen::Settings);
			TestEqual(TEXT("on the Controls row"), Menu.Focus, Row);
		}
		else
		{
			FDTUserSettings Before = S;
			const FDTMenuOutcome Left = Menu.Apply(EDTMenuCommand::Left, S);
			const FDTMenuOutcome Right = Menu.Apply(EDTMenuCommand::Right, S);
			Menu.bLanguagePending = false;
			TestTrue(FString::Printf(TEXT("row %d changes"), Row), Left.bSettingsChanged || Right.bSettingsChanged);
			S = Before;
		}
		Menu.Apply(EDTMenuCommand::Down, S);
	}
	Menu.Apply(EDTMenuCommand::Back, S);
	TestEqual(TEXT("Back returns to Pause"), Menu.Screen, EDTMenuScreen::Pause);
	TestEqual(TEXT("on the Settings row"), Menu.Focus, static_cast<int32>(EDTPauseRow::Settings));
	const FDTMenuOutcome Closed = Menu.Apply(EDTMenuCommand::Back, S);
	TestTrue(TEXT("Back from Pause closes"), Closed.bClosed);
	TestEqual(TEXT("no screen"), Menu.Screen, EDTMenuScreen::None);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDTSetTextKeys, "Dusktracer.Settings.TextKeys", DTSettingsTest::Flags) // T-SET-18
bool FDTSetTextKeys::RunTest(const FString& Parameters)
{
	TArray<FString> Keys = DTMenuText::AllKeys();
	TestTrue(TEXT("the menu has its keys"), Keys.Num() > 40);
	for (const FString& Key : Keys)
	{
		const FStringTableConstPtr Table = FStringTableRegistry::Get().FindStringTable(TEXT("ST_UI"));
		FStringTableEntryConstPtr Entry = Table.IsValid() ? Table->FindEntry(Key) : nullptr;
		TestTrue(FString::Printf(TEXT("%s is in ST_UI"), *Key), Entry.IsValid());
	}
	const FString Dir = FPaths::ConvertRelativePathToFull(FPaths::Combine(FPaths::GameSourceDir(), TEXT("Dusktracer/Settings")));
	for (const TCHAR* Name : { TEXT("DTMenuScreen.cpp"), TEXT("DTMenuSubsystem.cpp") })
	{
		FString Text;
		TestTrue(Name, FFileHelper::LoadFileToString(Text, *FPaths::Combine(Dir, Name)));
		TestTrue(TEXT("colour comes from the tokens"), Text.Contains(TEXT("DTPal::")) == (FString(Name) == TEXT("DTMenuScreen.cpp")));
		TestFalse(TEXT("no literal colour"), Text.Contains(TEXT("FColor(")) || Text.Contains(TEXT("FLinearColor(")) || Text.Contains(TEXT("FromHex")));
		TestFalse(TEXT("no enemy or telegraph token in UI"), Text.Contains(TEXT("DTPal::Enemy")) || Text.Contains(TEXT("DTPal::Telegraph")));
	}
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
