Dusktracer validation build (host notes; the full steps are in Docs/Validation/S3_kit.md)

First time on a PC: run Engine\Extras\Redist\en-us\vc_redist.x64.exe if the game reports a missing runtime.
Start:             Play_en.bat or Play_zh-Hans.bat. Plug a pad in first. The game opens in Vigil, not in a fight: Begin starts a run and each run comes back there.
One person, one save: the save and the run files belong to the Windows account. Give each validator their own PC or Windows account, never the owner's.
Run files land in: %LOCALAPPDATA%\Dusktracer\Saved\Playtests
Hand them over:    Export_Logs.bat makes one dated zip on the desktop and prints where it went. Send that file.
                   It starts no game and hides the Windows user and PC names. Nothing is ever sent by this build.
