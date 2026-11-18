# Como gerar o APK ARM64

Não há script ARM32. Não há APK universal.

## Ferramentas usadas

- Android SDK em `%LOCALAPPDATA%\Android\Sdk`
- NDK `28.2.13676358` (Clang 19.0.1)
- CMake 4.5 e Ninja
- Temurin JDK 21 (`JAVA_HOME`)
- Empacotamento direto: `javac`, `d8`, `aapt2`, `zipalign`, `apksigner`. Gradle não está no caminho e não é necessário.
- Build-tools mais novos do SDK (o `aapt2` acabou compilando contra `android-37`; o manifesto pede target 35)

Variáveis `ANDROID_HOME`, `ANDROID_SDK_ROOT` e `NDK_HOME` estavam vazias. Os scripts apontam para `%LOCALAPPDATA%\Android\Sdk`.

## Comando

Na raiz do repositório:

```
build-android-arm64.bat
```

O script limpa `CMAKE_LINKER_TYPE` (um cache antigo injetava `MSVC`, que o NDK rejeita), configura `pilot` com o toolchain legado do NDK e chama `android\package-arm64.ps1`.

Configuração equivalente:

```
cmake -S C:\S4EP1Recomp\pilot -B C:\S4EP1Recomp\android\build-arm64 -G Ninja ^
  -DCMAKE_TOOLCHAIN_FILE=%LOCALAPPDATA%\Android\Sdk\ndk\28.2.13676358\build\cmake\android.toolchain.cmake ^
  -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-28 -DANDROID_STL=c++_shared ^
  -DREXSDK_DIR=C:\S4EP1Recomp\third_party\rexglue-sdk -DCMAKE_BUILD_TYPE=Release
cmake --build C:\S4EP1Recomp\android\build-arm64 --target sonic4 --parallel 4
powershell -File C:\S4EP1Recomp\android\package-arm64.ps1
```

Saída: `C:\S4EP1Recomp\android\dist\Sonic4-arm64-v8a.apk`

ABI no APK: só `lib/arm64-v8a`. Bibliotecas sem compressão, alinhamento zip 16 KB. Assinatura debug: `android\debug.keystore`, senha `android`, alias `androiddebugkey`.

## Pacote

- id `com.s4ep1recomp.sonic4`
- activity `com.s4ep1recomp.sonic4.Sonic4Activity`
- `minSdk` 28, `targetSdk` 35, `versionName` 1.0
- Permissão: `VIBRATE` apenas
- `android:debuggable="true"` neste APK de desenvolvimento
- Feature Vulkan obrigatória

Os ~189 MB de `game\extracted` não entram no APK. Ver `ADB_DEPLOYMENT.md`.

## Release

O keystore atual é de debug. Para uma loja, gere um keystore próprio e troque os argumentos do `apksigner` em `android\package-arm64.ps1`. Não há tarefa de release separada.

## Windows

Não configure `pilot\out\build\win-amd64-release` com o toolchain do NDK. Os `if(ANDROID)` não alteram o binário Windows, mas essa árvore não foi recompilada nesta sessão.
