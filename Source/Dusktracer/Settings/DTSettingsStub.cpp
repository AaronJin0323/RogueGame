// Temporary stubs: lets the T-SET tests compile and fail before the implementation lands.
#include "Enemies/DTEnemyBulletRenderer.h"
#include "Offers/DTOfferScreen.h"
#include "Offers/DTOfferSubsystem.h"
#include "Player/DTInputSet.h"
#include "Settings/DTMenuScreen.h"
#include "Settings/DTMenuSubsystem.h"
#include "Settings/DTSettingsSubsystem.h"
FDTUserSettings::FDTUserSettings() {}
bool FDTUserSettings::operator==(const FDTUserSettings&) const { return false; }
EDTMotion FDTUserSettings::GetMotion() const { return EDTMotion::Custom; }
bool FDTBindings::operator==(const FDTBindings&) const { return false; }
FDTBindings FDTBindings::Defaults() { return FDTBindings(); }
namespace DTBindings
{
	EDTBindGroup GroupOf(EDTBindSlot) { return EDTBindGroup::InPlay; }
	bool IsRebindable(EDTBindSlot, EDTBindDevice) { return true; }
	bool IsReserved(const FKey&, EDTBindGroup) { return false; }
	bool IsOfDevice(const FKey&, EDTBindDevice) { return false; }
	EDTRebindResult Rebind(FDTBindings&, EDTBindSlot, EDTBindDevice, const FKey&, EDTBindSlot*) { return EDTRebindResult::Reserved; }
	bool IsLegal(const FDTBindings&, EDTBindDevice, EDTBindGroup) { return false; }
	FText Glyph(const FKey&) { return FText::GetEmpty(); }
	const TCHAR* ActionKey(EDTBindSlot) { return TEXT(""); }
}
namespace DTSettings
{
	FDTUserSettings& Live() { static FDTUserSettings S; return S; }
	float TextScale() { return 0.0f; }
	bool AllowShake() { return false; }
	bool AllowFlash() { return false; }
	bool DangerOutline() { return false; }
	FString Serialize(const FDTUserSettings&) { return FString(); }
	FDTUserSettings Parse(const FString&) { return FDTUserSettings(); }
	bool Save(const FString&, const FDTUserSettings&) { return false; }
	bool Load(const FString&, FDTUserSettings&) { return true; }
	FString DefaultPath() { return FString(); }
	bool UsesFile(bool, bool) { return false; }
	bool StepRow(FDTUserSettings&, EDTSettingRow, int32) { return false; }
	int32 ScalabilityLevel(EDTGraphics) { return -1; }
	const TCHAR* CultureName(EDTLanguage) { return TEXT(""); }
}
void FDTMenuState::Open() {}
void FDTMenuState::Close() {}
int32 FDTMenuState::NumRows() const { return 0; }
FDTMenuOutcome FDTMenuState::Apply(EDTMenuCommand, FDTUserSettings&) { return FDTMenuOutcome(); }
FDTMenuOutcome FDTMenuState::Capture(const FKey&, FDTUserSettings&) { return FDTMenuOutcome(); }
namespace DTMenuInput
{
	const TArray<FKey>& Keys() { static TArray<FKey> K; return K; }
	EDTMenuCommand ForKey(const FKey&) { return EDTMenuCommand::None; }
	bool CancelsCapture(const FKey&) { return false; }
}
namespace DTMenuLayout
{
	float RowHeight(float) { return 0.0f; }
	float ContentHeight(int32, float) { return 100000.0f; }
}
namespace DTMenuText
{
	TArray<FString> AllKeys() { return {}; }
	FText RowHint(EDTSettingRow) { return FText::GetEmpty(); }
	FText RowLabel(EDTSettingRow) { return FText::GetEmpty(); }
	FText RowValue(EDTSettingRow, const FDTUserSettings&) { return FText::GetEmpty(); }
	FText StatusLine(const FDTMenuState&) { return FText::GetEmpty(); }
}
namespace DTMenuScreen { void Draw(UCanvas*, const FDTMenuState&, const FDTUserSettings&) {} }
bool UDTMenuSubsystem::ShouldCreateSubsystem(UObject*) const { return false; }
void UDTMenuSubsystem::Tick(float) {}
TStatId UDTMenuSubsystem::GetStatId() const { RETURN_QUICK_DECLARE_CYCLE_STAT(UDTMenuSubsystem, STATGROUP_Tickables); }
void UDTMenuSubsystem::RequestPause() {}
void UDTMenuSubsystem::OpenAt(EDTMenuScreen) {}
void UDTMenuSubsystem::Close() {}
void UDTMenuSubsystem::Command(EDTMenuCommand) {}
void UDTMenuSubsystem::CaptureKey(const FKey&) {}
void UDTMenuSubsystem::RunLanguageSwitch() {}
void UDTMenuSubsystem::Draw(UCanvas*) {}
void UDTMenuSubsystem::HandleOutcome(const FDTMenuOutcome&) {}
void UDTMenuSubsystem::SetPaused(bool) {}
void UDTMenuSubsystem::PollKeys() {}
void UDTMenuSubsystem::RefreshHeroBindings() {}
#if !UE_BUILD_SHIPPING
void UDTMenuSubsystem::TickDevShot() {}
#endif
void UDTSettingsSubsystem::Initialize(FSubsystemCollectionBase& Collection) { Super::Initialize(Collection); }
void UDTSettingsSubsystem::Persist() {}
FString UDTSettingsSubsystem::ActivePath() { return TEXT("stub"); }
void UDTSettingsSubsystem::ApplyVolume(const FDTUserSettings&) {}
void UDTSettingsSubsystem::ApplyDisplay(const FDTUserSettings&) {}
void UDTSettingsSubsystem::ApplyCulture(const FDTUserSettings&) {}
FText DTOfferInput::ToolGlyphs(EDTOfferAction) { return FText::GetEmpty(); }
void UDTOfferSubsystem::ResumeAfterMenu(double) {}
double UDTOfferSubsystem::GetOpenedWall(int32) const { return 0.0; }
bool UDTOfferSubsystem::IsGuarded(int32) const { return false; }
void UDTInputSet::ApplyBindings(const FDTBindings&) {}
int32 ADTEnemyBulletRenderer::GetOutlineCount() const { return 0; }
