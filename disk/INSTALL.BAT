echo off
if not %2p==p goto ok
echo ドライブの指定がちがいます
echo install　転送元:　転送先:
goto end
:ok
echo ..
echo 「ハードディスクに倉庫番をインストールします」
echo ..
md %2\sbs30
copy %1*.BGM %2\sbs30 > nul
copy %1*.VOI %2\sbs30 > nul
copy %1*.CG* %2\sbs30 > nul
copy %1*.DAT %2\sbs30 > nul
copy %1*.EXE %2\sbs30 > nul
copy %1mmd2.sys %2\ > nul
copy %1mouse.sys %2\ > nul
copy %1soko.bat %2\ > nul
copy %1sbp98.add %2\ > nul
echo ..
echo 「インストールが終わりました」
echo ..
:end
