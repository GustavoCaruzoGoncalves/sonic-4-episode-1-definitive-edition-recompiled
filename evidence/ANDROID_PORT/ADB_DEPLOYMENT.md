# Instalação no POCO F7

`adb` usado em todos os passos:

```
C:\Users\Administrator\Downloads\platform-tools-latest-windows\platform-tools\adb.exe
```

## APK

```
adb install -r C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk
```

`install -r` manteve `files/game`, `files/user` e `files/cache`. Um `adb uninstall` apaga isso.

## Dados do jogo

Não usar `/sdcard/Android/data/com.s4ep1recomp.sonic4/`. O app não consegue dar `stat` nesses arquivos.

```
adb push C:\S4EP1Recomp\game\extracted /data/local/tmp/sonic4game
adb shell "cd /data/local/tmp/sonic4game && tar -cf - . | run-as com.s4ep1recomp.sonic4 tar -xf - -C files/game"
```

O `tar` tem de rodar no aparelho. Um pipe de tar do Windows pelo `adb shell` quebra com `Illegal seek`.

Confira:

```
adb shell "run-as com.s4ep1recomp.sonic4 ls -l files/game/default.xex files/game/data.cpk"
```

## Subir o jogo

```
adb shell am force-stop com.s4ep1recomp.sonic4
adb shell am start -n com.s4ep1recomp.sonic4/com.s4ep1recomp.sonic4.Sonic4Activity
```

Log do runtime:

```
adb exec-out run-as com.s4ep1recomp.sonic4 cat files/user/logs/sonic4_003.log
```

O índice do arquivo sobe a cada execução.

Screenshot binário (não usar `Set-Content` no PowerShell):

```
cmd /c "adb exec-out screencap -p > C:\S4EP1Recomp\evidence\ANDROID_PORT\game.png"
```
