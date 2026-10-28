# How to send your logs (tester README line)

For the package README, in both languages:

**Sending your logs:** after you play, paste `%LOCALAPPDATA%\RogueHero\Saved\` into the File Explorer address bar and press
Enter. Right-click the `Playtests` folder, choose *Send to → Compressed (zipped) folder*, and send us the zip. It holds only
game events and stats: no personal data.

**发送日志：** 玩完后，在文件资源管理器的地址栏粘贴 `%LOCALAPPDATA%\RogueHero\Saved\` 并按回车，右键点击 `Playtests` 文件夹，
选择“发送到 → 压缩(zipped)文件夹”，然后把压缩包发给我们。里面只有游戏事件和统计数据，不含任何个人信息。

On our side: `python Tools/playtest_report.py <the zips> --out <dir>` (one zip per tester, named after them).
