# OpenXRay Android ARM64 Port — Roadmap & TODO

This document tracks the prerequisites, architecture decisions, and remaining implementation tasks for porting OpenXRay to Android ARM64 (`aarch64-linux-android`) using the native OpenGL ES 3.2 / 3.0 renderer.

---

## 1. Prerequisites & Installation

### Host Tools & Dependencies (atleast on Debian / Ubuntu)
Run the following all-in-one command on the Linux host:

```bash
sudo apt update && sudo apt install -y \
    build-essential \
    cmake \
    ninja-build \
    clang \
    lld \
    pkg-config \
    git \
    curl \
    wget \
    unzip \
    zip \
    default-jdk \
    glslang-tools \
    libsdl2-dev \
    libopenal-dev \
    libogg-dev \
    libvorbis-dev \
    libtheora-dev \
    liblzo2-dev \
    libjpeg-dev
```

### Android NDK (r26b)
Install the official Google Android NDK to `/opt/android-ndk`:

```bash
curl -Lo /tmp/android-ndk-r26b.zip https://dl.google.com/android/repository/android-ndk-r26b-linux.zip && \
sudo unzip -q /tmp/android-ndk-r26b.zip -d /opt && \
sudo ln -sfn /opt/android-ndk-r26b /opt/android-ndk && \
rm /tmp/android-ndk-r26b.zip
```

Add to `~/.bashrc`:
```bash
export ANDROID_NDK_HOME=/opt/android-ndk
```

---

## 2. Implementation TODO Checklist

### Phase 1: Decouple OpenGL ES from WebAssembly in Engine
The GLES 3.0 renderer and ESSL shader translator already exist in `xrRenderGL` (from the WebGL2 work in PR #2153), but are hardcoded behind `#ifdef XR_PLATFORM_WEB`.

- [ ] **1.1. Introduce `XR_RENDER_GLES` macro** in `src/Common/Platform.hpp`:
  - Define `XR_RENDER_GLES` when either `XR_PLATFORM_WEB` or `XR_PLATFORM_ANDROID` is set.
- [ ] **1.2. Configure SDL2 context for GLES on Android** in `src/Layers/xrRenderGL/glHW.cpp`:
  - In `CHW::SetPrimaryAttributes`: request `SDL_GL_CONTEXT_PROFILE_ES` with major `3` and minor `2` (or `0`).
  - In `CHW::CreateDevice`: call `gladLoadGLES2(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress))` on Android instead of desktop `gladLoadGL`.
- [ ] **1.3. Route shader compiler through ESSL rewriter on Android** in `src/Layers/xrRenderPC_GL/rgl_shaders.cpp`:
  - Invoke `essl::rewrite()` from `rgl_essl_web.h` whenever `XR_RENDER_GLES` is active.
  - Shaders will output `#version 300 es` with location attributes and precision qualifiers.
- [ ] **1.4. Migrate GLES buffer and texture workarounds** from `XR_PLATFORM_WEB` to `XR_RENDER_GLES`:
  - `src/Layers/xrRenderGL/glBufferUtils.cpp` (buffer flags, base-vertex calls).
  - `src/Layers/xrRenderGL/glState.cpp` (texture border colors, LOD bias, sampler state).
  - `src/Layers/xrRenderGL/glTexture.cpp` (`gli::gl::PROFILE_ES30` and format swizzling).
  - `src/Layers/xrRenderGL/glSH_RT.cpp` (FBO attachment and viewport masks).
  - `src/Layers/xrRenderGL/glr_constants_cache.h` (`UniformShadow` state cache).
- [ ] **1.5. Validate all shaders** with `misc/web/tools/essl_check.sh` to ensure 100% ESSL compatibility.

---

### Phase 2: Android ARM64 Target Dependencies
Cross-compiling with NDK requires target ARM64 (`aarch64-linux-android`) libraries.

- [ ] **2.1. Prepare ARM64 target libraries** for:
  - `SDL2` (v2.28+ / v2.30+)
  - `OpenAL Soft` (with OpenSL ES / AAudio backends)
  - `libogg`
  - `libvorbis` & `libvorbisfile`
  - `libtheora`
  - `liblzo2`
- [ ] **2.2. Automate dependency building** via a CMake helper or script (e.g. `scripts/build_android_deps.sh`).

---

### Phase 3: Android Engine Architecture & Entry Point
Android cannot execute standalone ELF binaries; it loads native `.so` shared libraries inside an APK.

- [ ] **3.1. Update `src/xr_3da/CMakeLists.txt`**:
  - When `CMAKE_SYSTEM_NAME STREQUAL "Android"`, compile `xr_3da` as a shared library (`add_library(main SHARED ...)` or `libmain.so`).
- [ ] **3.2. Implement `SDL_main` entry point** in `src/xr_3da/entry_point.cpp`:
  - Add `#if defined(XR_PLATFORM_ANDROID)` exporting `extern "C" int SDL_main(int argc, char* argv[])`.
  - Forward arguments directly to `entry_point(commandLine)`.
- [ ] **3.3. Preserve library naming conventions**:
  - Ensure all shared library targets keep the `lib` prefix (e.g. `libxrRender_GL.so`, `libxrGame.so`, `libxrCore.so`) so the Android APK packager does not discard them.

---

### Phase 4: Test Cross-Compilation for Android ARM64
Verify the full engine compiles cleanly with the NDK.

- [ ] **4.1. Run CMake configure**:
  ```bash
  cmake -B build-android-arm64 -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE=/opt/android-ndk/build/cmake/android.toolchain.cmake \
      -DANDROID_ABI=arm64-v8a \
      -DANDROID_PLATFORM=android-26 \
      -DCMAKE_BUILD_TYPE=ReleaseMasterGold \
      -DXRAY_USE_LUAJIT=ON \
      -DMEMORY_ALLOCATOR=standard
  ```
- [ ] **4.2. Build with Ninja**:
  ```bash
  ninja -C build-android-arm64
  ```
- [ ] **4.3. Fix any Bionic libc / Clang warnings or linker errors**.

---

### Phase 5: Android Application Wrapper (Gradle & Launcher)
Create a unified APK that runs **Shadow of Chernobyl**, **Clear Sky**, and **Call of Pripyat**. 
NOTE remi: ask me for gamefiles, i'm not gonna provide it publicly here otherwise GSC will be after our asses

- [ ] **5.1. Setup Android project structure** (`android/`):
  - `android/app/build.gradle` (NDK CMake integration, packaging `.so` libs).
  - `android/app/src/main/AndroidManifest.xml` (landscape orientation, storage permissions).
- [ ] **5.2. Create `XRayActivity`** extending `org.libsdl.app.SDLActivity`.
- [ ] **5.3. Create `LauncherActivity` (Game Selector)**:
  - Frontend UI allowing the player to select:
    - S.T.A.L.K.E.R.: Shadow of Chernobyl (`-soc`)
    - S.T.A.L.K.E.R.: Clear Sky (`-cs`)
    - S.T.A.L.K.E.R.: Call of Pripyat (`-cop`)
    - Custom Mod / Sandbox (`-unlock_game_mode`)
  - Game data directory selector (browsing for `fsgame.ltx` and `.db*` archives).
  - Launches `XRayActivity` with the chosen command line flags.

---

### Phase 6: Mobile UX, Touch Controls & Device Testing
### (NOTE remi): im ngl this WILL need work because this game is annoying to play on PC anyways. slightly better than CWR i guess)
Make the game fully playable on a mobile touchscreen without requiring a physical keyboard or gamepad.

- [ ] **6.1. Physical Gamepad Verification**:
  - Test Bluetooth/USB controller (already supported via SDL GameController API).
- [ ] **6.2. On-Screen Touch Controls Overlay**:
  - Virtual movement stick (left) + look/aim touch zone (right).
  - Essential action buttons: Fire, Aim, Reload, Jump, Crouch, Sprint, Use, Flashlight, PDA, Inventory, Quick Save/Load.
- [ ] **6.3. Device Deployment & Validation**:
  - Install APK via `adb install -r stalker-mobile.apk`.
  - Copy game data files (`fsgame.ltx`, `gamedata/` or `.db` archives) to `/sdcard/Android/data/.../files/`.
  - Verify 3D rendering, audio, save/load, and gameplay performance on ARM64 hardware.
