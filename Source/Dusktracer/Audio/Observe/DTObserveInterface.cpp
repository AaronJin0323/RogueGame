#include "Audio/Observe/DTObserveInterface.h"
#include "Engine/World.h"
#include "Hub/DTHubSubsystem.h"
#include "Offers/DTOfferSubsystem.h"
#include "Settings/DTMenuSubsystem.h"

namespace DTInterfaceEvents
{
	int32 Depth(EDTMenuScreen Screen)
	{
		switch (Screen)
		{
		case EDTMenuScreen::None:     return 0;
		case EDTMenuScreen::Pause:    return 1;
		case EDTMenuScreen::RunOver:  return 1;
		default:                      return 2; // Settings, Controls, Confirm
		}
	}

	static bool IsRefusal(EDTMenuStatus Status)
	{
		return Status == EDTMenuStatus::Reserved || Status == EDTMenuStatus::WrongDevicePad || Status == EDTMenuStatus::WrongDeviceKbm;
	}

	void Diff(const FDTInterfaceView& Was, const FDTInterfaceView& Now, TFunctionRef<void(DTCat::ESoundId Sound, int32 Count)> Raise)
	{
		using DTCat::ESoundId;
		// The pause, settings, controls, confirmation and run-over screens.
		if (Now.Screen != Was.Screen)
		{
			const bool bOpen = Now.Screen != EDTMenuScreen::None
				&& (Was.Screen == EDTMenuScreen::None || Now.Screen == EDTMenuScreen::RunOver || Depth(Now.Screen) >= Depth(Was.Screen));
			Raise(bOpen ? ESoundId::Snd_UI_Open : ESoundId::Snd_UI_Back, 1);
		}
		else if (Now.Screen != EDTMenuScreen::None && (Now.MenuFocus != Was.MenuFocus || Now.MenuColumn != Was.MenuColumn))
		{
			Raise(ESoundId::Snd_UI_Focus, 1);
		}
		if (Now.Screen != EDTMenuScreen::None && Now.MenuStatus != Was.MenuStatus && IsRefusal(Now.MenuStatus))
		{
			Raise(ESoundId::Snd_UI_Refuse, 1);
		}

		// The hub's pages (vigil.md R-VIG-03): entering it, or leaving the root for a page, opens; coming back to the root is Back.
		if (Now.bHub && !Was.bHub)
		{
			Raise(ESoundId::Snd_UI_Open, 1);
		}
		else if (Now.bHub && Was.bHub)
		{
			if (Now.HubPage != Was.HubPage)
			{
				Raise(Now.HubPage == EDTHubPage::Root ? ESoundId::Snd_UI_Back : ESoundId::Snd_UI_Open, 1);
			}
			else if (Now.HubFocus != Was.HubFocus)
			{
				Raise(ESoundId::Snd_UI_Focus, 1);
			}
		}

		// The offer hand (offers.md): a new hand that is not the reroll of the one on show opens; the focus moves within one hand.
		if (Now.HandId != INDEX_NONE && Now.HandId != Was.HandId && !(Was.HandId != INDEX_NONE && Now.RerollOf == Was.HandId))
		{
			Raise(ESoundId::Snd_UI_HandOpen, 1);
		}
		else if (Now.HandId != INDEX_NONE && Now.HandId == Was.HandId && Now.HandFocus != Was.HandFocus)
		{
			Raise(ESoundId::Snd_UI_Focus, 1);
		}
		const auto Grew = [&](int32 Before, int32 After, ESoundId Sound)
		{
			if (After > Before)
			{
				Raise(Sound, After - Before);
			}
		};
		Grew(Was.Decisions, Now.Decisions, ESoundId::Snd_UI_Pick);
		Grew(Was.Rerolls, Now.Rerolls, ESoundId::Snd_UI_Reroll);
		Grew(Was.Banishes, Now.Banishes, ESoundId::Snd_UI_Banish);
		Grew(Was.Locks, Now.Locks, ESoundId::Snd_UI_Lock);
	}

	FDTInterfaceView Read(const UWorld& World)
	{
		FDTInterfaceView View;
		if (const UDTMenuSubsystem* Menu = World.GetSubsystem<UDTMenuSubsystem>())
		{
			const FDTMenuState& State = Menu->GetState();
			View.Screen = State.Screen;
			View.MenuFocus = State.Focus;
			View.MenuColumn = State.Column;
			View.MenuStatus = State.Status;
		}
		if (const UDTHubSubsystem* Hub = World.GetSubsystem<UDTHubSubsystem>())
		{
			View.bHub = Hub->IsActive();
			View.HubPage = Hub->GetState().Page;
			View.HubFocus = Hub->GetState().Focus;
		}
		if (const UDTOfferSubsystem* Offers = World.GetSubsystem<UDTOfferSubsystem>())
		{
			constexpr int32 LocalOwner = 0; // the solo player, whose hand the offer screen shows (offers.md)
			if (const FDTOfferHand* Hand = Offers->GetOpenHand(LocalOwner))
			{
				View.HandId = Hand->HandId;
				View.RerollOf = Hand->RerollOf;
				View.HandFocus = Offers->GetFocus(LocalOwner);
			}
			if (const FDTOfferPlayerStats* Stats = Offers->GetStats(LocalOwner))
			{
				View.Decisions = Stats->Decisions;
				View.Rerolls = Stats->Rerolls;
				View.Banishes = Stats->Banishes;
				View.Locks = Stats->Locks;
			}
		}
		return View;
	}
}

void FDTObserveInterface::OnFrame(const UWorld& World, IDTAudioRequests& Out)
{
	const FDTInterfaceView Now = DTInterfaceEvents::Read(World);
	DTInterfaceEvents::Diff(Last, Now, [&Out](DTCat::ESoundId Sound, int32 Count) { Out.Ui(static_cast<int32>(Sound), Count); });
	Last = Now;
}
