#include "Enemies/DTElite.h"

uint8 DTElite::Roll(EDTEnemyRole Role, int32 Count, float FavorWeight, FRandomStream& Rng) { return 0; }
EDTAffix DTElite::Lead(uint8 Mask) { return EDTAffix::Count; }
float DTElite::LightLevel(uint8 Mask, float Seconds) { return 0.0f; }
float DTElite::LightEmissive(uint8 Mask, float Seconds) { return 0.0f; }
bool DTElite::ParseMask(const FString& Text, uint8& OutMask) { return false; }
void DTElite::LabelKeys(uint8 Mask, TArray<FString>& OutKeys) {}
FText DTElite::LabelText(uint8 Mask) { return FText::GetEmpty(); }
void DTElite::LabelledElites(const FDTEnemyPool& Pool, const FVector2f& At, TArray<int32>& OutIndices) {}
void DTElite::BuildParts(const FDTEnemyPool& Pool, float Alpha, float Seconds, float MeshSizeUU, TArray<FDTElitePart>& Out) {}
FString DTElite::MaskNames(uint8 Mask) { return FString(); }
