// Tiny JSON line builder for telemetry records (02_Systems/telemetry.md §Data). Internal to Telemetry/.
#pragma once

#include "CoreMinimal.h"

/**
 * Builds one JSON object field by field, in call order (deterministic output). Floats use a fixed precision so the same
 * sim gives the same bytes (R-TEL-08). NAME_None and empty optional strings can be written as null.
 */
class FDTTelJson
{
public:
	/** An empty object "{". */
	FDTTelJson() : Text(TEXT("{")) {}
	/** Continues an object already opened with fields (a record head "{"t":..,"owner":n"). */
	explicit FDTTelJson(FString OpenWithFields) : Text(MoveTemp(OpenWithFields)), bFirst(false) {}

	/** A JSON string literal (quoted, escaped). */
	static FString Quote(const FString& In)
	{
		FString Out;
		Out.Reserve(In.Len() + 2);
		Out += TEXT('"');
		for (const TCHAR Ch : In)
		{
			if (Ch == TEXT('"') || Ch == TEXT('\\'))
			{
				Out += TEXT('\\');
				Out += Ch;
			}
			else if (Ch < TEXT(' '))
			{
				Out += FString::Printf(TEXT("\\u%04x"), static_cast<int32>(Ch));
			}
			else
			{
				Out += Ch;
			}
		}
		Out += TEXT('"');
		return Out;
	}

	/** Fixed-precision number. */
	static FString Real(double Value) { return FString::Printf(TEXT("%.4f"), Value); }

	FDTTelJson& Raw(const FString& Key, const FString& Json)
	{
		Text += bFirst ? TEXT("") : TEXT(",");
		bFirst = false;
		Text += Quote(Key);
		Text += TEXT(':');
		Text += Json;
		return *this;
	}
	FDTTelJson& Int(const FString& Key, int64 Value) { return Raw(Key, FString::Printf(TEXT("%lld"), static_cast<long long>(Value))); }
	FDTTelJson& Num(const FString& Key, double Value) { return Raw(Key, Real(Value)); }
	FDTTelJson& Bool(const FString& Key, bool bValue) { return Raw(Key, bValue ? TEXT("true") : TEXT("false")); }
	FDTTelJson& Str(const FString& Key, const FString& Value) { return Raw(Key, Quote(Value)); }
	/** NAME_None -> null. */
	FDTTelJson& Name(const FString& Key, FName Value) { return Raw(Key, Value.IsNone() ? FString(TEXT("null")) : Quote(Value.ToString())); }

	/** The object, closed. */
	FString Close() const { return Text + TEXT("}"); }
	/** The object, closed, as one JSONL line. */
	FString Line() const { return Text + TEXT("}\n"); }

private:
	FString Text;
	bool bFirst = true;
};
