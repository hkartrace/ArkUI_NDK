# SDK d.ts patches (machine-local stand-ins until T9 codegen)

DevEco's ArkTS compiler resolves `@ohos.*` kit typings ONLY from the SDK on
disk and ignores project-local ambient module augmentations. Until T9 ships
generated typings (`aurora-effects.d.ts`), the custom-API typing is applied by
copying these files over the DevEco SDK.

The DevEco SDK location is NOT hardcoded anywhere - resolve it the same way
the backend does (see AuroraGraph\backend\paths.js):

1. `AURORA_DEVECO` env var, or `%LOCALAPPDATA%\AuroraGraph\config.json`
   `{ "devecoHome": "..." }`, or the standard `DEVECO_SDK_HOME` env var, or
   auto-detection (machine-wide/per-user install dirs, registry, PATH).
   The backend logs the resolved home at launch and exposes it at
   `/api/health` -> `paths.devecoHome` / `paths.sdkHome`.
2. Then (PowerShell, from this folder):

       copy .\@ohos.graphics.uiEffect.d.ts "<devecoHome>\sdk\default\openharmony\ets\api\" /Y

   e.g. with the resolved home in `$home`:

       Copy-Item .\@ohos.graphics.uiEffect.d.ts (Join-Path $home 'sdk\default\openharmony\ets\api') -Force

- `@ohos.graphics.uiEffect.d.ts` — adds `uiEffect.applyCustomEffect(name,
  params?): Filter` (namespace-level static, exported by the patched uiEffect
  NAPI kit; previewer runtime has the real implementation).
- Re-apply after any DevEco Studio / SDK upgrade (upgrades restore originals).
  The backend WARNS at launch when the on-disk SDK typing lacks
  `applyCustomEffect` (see paths.js sdkTypingPatched).
- Original backup: `%TEMP%\AuroraGraph\uiEffect.d.ts.orig` (also the pristine
  stub is recoverable from the SDK installer).
