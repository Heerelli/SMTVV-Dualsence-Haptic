# SMTVV DualSense Haptic

**진·여신전생 V Vengeance (Steam PC)** 에서 듀얼센스의 **햅틱 피드백(보이스코일 진동)** 을 쓸 수 있게 해 주는 모드입니다.

[snm01234 님의 SMT5V Haptics (PC-SMT5V-VIB)](https://github.com/snm01234/PC-SMT5V-VIB) 는 게임에 없는 진동을 전투·필드 이벤트에 추가해 줍니다. 다만 일반 모터 진동(XInput)이라, 듀얼센스로 해도 단순한 "웅~" 진동만 납니다.
이 모드는 그 앞에 끼워져서, 같은 이벤트를 **이벤트마다 다른 햅틱 파형**으로 바꿔 줍니다.

> ⚠️ **현재 상태: v0.2 테스트 버전 — 빌드는 자동으로 확인되지만, 실제 게임에서는 아직 테스트 전입니다.**

## 빠른 설치 (3단계)

빌드할 필요 없습니다. [Releases](https://github.com/Heerelli/SMTVV-Dualsence-Haptic/releases) 에서 받으세요.

1. **원본 모드 설치:** [SMT5V Haptics v0.6](https://github.com/snm01234/PC-SMT5V-VIB/releases) 을 원래 방법대로 게임의 `Project\Binaries\Win64\` 에 넣습니다.
2. **이 모드 설치 (둘 중 하나):**
   - **Reloaded-II를 쓴다면:** `SMTVV-DualSense-Haptic-ReloadedII.zip` 을 Reloaded-II 창에 끌어다 놓고, 모드를 체크한 뒤 Reloaded-II에서 실행합니다.
   - **Reloaded-II가 없다면:** 원본 `xinput1_3.dll` 이름을 `SMT5VHapticsCore.dll` 로 바꾸고, `SMTVV-DualSense-Haptic-Manual.zip` 안의 `xinput1_3.dll` 을 같은 폴더에 넣습니다.
3. **듀얼센스를 USB 케이블로 연결**하고 게임을 실행합니다. 게임 중 **F11** 을 누르면 햅틱 테스트가 나옵니다.

INI 설정은 손대지 않아도 기본값으로 동작합니다. 자세한 내용은 아래를 보세요.

---

## 특징

- **이벤트별 전용 햅틱 파형**: 일반 피격, 크리티컬, 약점, 가드, 필드 이벤트가 각각 다른 느낌으로 울립니다.
- **원본 모드 설정 그대로 사용**: 게임 내 `진동 세기` 0~10 게이지와 8·9·10단계 지속시간 연장(120/125/130%)이 그대로 반영됩니다.
- **원본 모드를 수정하지 않음**: 원본 DLL은 이름만 바꿔서 그대로 사용합니다. 원본이 하던 일은 모두 원본이 합니다.
- **안전한 대체 동작**: 듀얼센스(USB)가 없으면 진동을 원래대로 넘깁니다. 엑스박스 패드 등은 지금처럼 쓸 수 있습니다.
- **게임 창이 비활성일 때 자동 음소거** (설정으로 끌 수 있음)
- **두 가지 설치 방식**: Reloaded-II 모드로 설치하거나, 게임 폴더에 직접 넣는 방식 중 고를 수 있습니다.

## 햅틱이 나오는 때

진동 타이밍은 원본 모드(v0.6)의 판정을 그대로 따릅니다. 게임 원본에는 진동이 없습니다.

| 구분 | 상황 | 종류 | 햅틱 느낌 |
| --- | --- | --- | --- |
| 전투 | 일반 데미지 | normal | 중간 굵기의 "퍽" |
| 전투 | 크리티컬 | critical | 날카로운 클릭 뒤에 묵직한 저음 여운 |
| 전투 | 약점 공격 | weak | 왼쪽 → 오른쪽 더블탭 |
| 전투 | 가드 | guard | 짧은 "탁" 뒤에 부드러운 저음 |
| 필드 | 적 심볼 공격 적중 (`fld_encount_hitstamp`) | normal | 전투 일반 피격과 같음 |
| 필드 | 아이템 상자 파괴 (`fld_chest_break3`) | normal | 전투 일반 피격과 같음 |
| 필드 | 적 심볼과 접촉 (`fld_encount`) | normal | 전투 일반 피격과 같음 |

- 회복, 빗나감, 무효에는 진동이 없습니다 (원본 모드와 동일).
- 필드 이벤트의 종류는 `SMT5V-Haptics.ini`의 `[SoundRules]`에서 바꿀 수 있습니다. 예를 들어 `fld_chest_break3=sound`로 바꾸면 상자 파괴만 넓게 퍼지는 울림이 됩니다.

## 요구 사항

| 항목 | 내용 |
| --- | --- |
| 게임 | 진·여신전생 V Vengeance, Windows Steam판 (원본 모드 v0.6 확인 환경: 1.0.3a, Build ID 17140961) |
| 원본 모드 | [SMT5V Haptics v0.6](https://github.com/snm01234/PC-SMT5V-VIB/releases) |
| 컨트롤러 | 듀얼센스 / 듀얼센스 엣지, **USB 유선 연결 필수** |
| OS | Windows 10/11 64bit |
| 모드 로더 (선택) | [Reloaded-II](https://github.com/Reloaded-Project/Reloaded-II) — Reloaded-II 방식으로 설치할 때만 필요 |
| 빌드 | Visual Studio 2022 (C++ 데스크톱 개발 워크로드) |

> 블루투스로 연결하면 햅틱 오디오 채널이 없어서 동작하지 않습니다. 이때는 진동이 원래대로 전달됩니다.
> USB로 연결하면 Windows 사운드 설정의 재생 장치에 **"Wireless Controller"** 가 보여야 합니다.

## 직접 빌드 (개발자용)

Releases의 zip은 GitHub Actions(`.github/workflows/build.yml`)가 자동으로 만듭니다. 직접 빌드하려면:


1. 시작 메뉴에서 **"x64 Native Tools Command Prompt for VS 2022"** 를 엽니다.
2. 이 저장소 폴더로 이동해서, 설치 방식에 맞는 스크립트를 실행합니다.

| 설치 방식 | 실행 | 결과물 |
| --- | --- | --- |
| Reloaded-II | `build_reloaded.bat` | `out\reloaded\heerelli.smtvv.dualsense.haptic\` 폴더 (`SMTVVDualSenseHaptic.dll` + `ModConfig.json`) |
| 게임 폴더 직접 | `build.bat` | `out\xinput1_3.dll` |

## 설치

Releases의 zip을 쓰는 경우, Reloaded-II용은 `SMTVV-DualSense-Haptic-ReloadedII.zip`, 직접 설치용은 `SMTVV-DualSense-Haptic-Manual.zip` 입니다.

두 방식 중 **하나만** 쓰세요. 둘 다 설치하면 Reloaded-II 쪽이 오류를 남기고 동작하지 않습니다.

### 방법 A: Reloaded-II (추천)

원본 모드 파일을 건드리지 않아서 설치와 제거가 가장 간단합니다.

1. **원본 모드 설치:** 게임을 종료하고 [SMT5V Haptics v0.6](https://github.com/snm01234/PC-SMT5V-VIB/releases) 을 원래 방법대로 설치합니다 (`Project\Binaries\Win64\` 에 `xinput1_3.dll`, `SMT5V-Haptics.ini`). **이름은 바꾸지 않습니다.**
2. **Reloaded-II에 게임 추가:** 아직 안 했다면 Reloaded-II에서 `+` (Add an Application) 를 누르고 `SMT5V\Project\Binaries\Win64\SMT5V-Win64-Shipping.exe` 를 선택합니다.
3. **모드 넣기:** `SMTVV-DualSense-Haptic-ReloadedII.zip` 을 Reloaded-II 창에 끌어다 놓습니다. 직접 빌드했다면 `heerelli.smtvv.dualsense.haptic` 폴더를 `Reloaded-II\Mods\` 에 복사합니다.
4. **모드 켜기:** Reloaded-II에서 게임을 선택하고 모드 목록의 **SMTVV DualSense Haptic** 을 체크합니다.
5. (선택) `Win64\SMT5V-Haptics.ini` 맨 아래에 `SMT5V-Haptics.DualSense-section.ini` 내용을 붙여 넣습니다. 안 넣어도 기본값으로 동작합니다.
6. 듀얼센스를 USB로 연결하고 **Reloaded-II에서 Launch Application** 으로 실행합니다.

```text
Reloaded-II/Mods/heerelli.smtvv.dualsense.haptic/
├─ ModConfig.json
└─ SMTVVDualSenseHaptic.dll

SMT5V/Project/Binaries/Win64/
├─ SMT5V-Win64-Shipping.exe
├─ xinput1_3.dll           ← 원본 모드 (그대로)
└─ SMT5V-Haptics.ini       ← 원본 설정 (+ [DualSense] 섹션)
```

> Reloaded-II 방식에서도 설정 파일(`SMT5V-Haptics.ini`)과 로그(`SMT5V-DualSense.log`)는 게임의 `Win64` 폴더에 있습니다.
> 원본 모드를 찾지 못하면 로그가 Reloaded-II 모드 폴더에 남습니다.

### 방법 B: 게임 폴더에 직접 넣기

Reloaded-II 없이 쓰는 방식입니다.

1. 게임을 완전히 종료합니다.
2. 원본 모드 v0.6을 먼저 설치합니다 (`Project\Binaries\Win64\` 에 `xinput1_3.dll`, `SMT5V-Haptics.ini`).
3. 같은 폴더에서 원본 모드의 `xinput1_3.dll` 이름을 **`SMT5VHapticsCore.dll`** 로 바꿉니다.
4. `SMTVV-DualSense-Haptic-Manual.zip` 안의 `xinput1_3.dll` (직접 빌드했다면 `out\xinput1_3.dll`) 을 같은 폴더에 복사합니다.
5. (선택) `SMT5V-Haptics.ini` 맨 아래에 `SMT5V-Haptics.DualSense-section.ini` 내용을 붙여 넣습니다.
6. 듀얼센스를 USB로 연결하고 Steam에서 실행합니다.

```text
SMT5V/Project/Binaries/Win64/
├─ SMT5V-Win64-Shipping.exe
├─ xinput1_3.dll           ← 이 모드 (햅틱 브리지)
├─ SMT5VHapticsCore.dll    ← 원본 모드 (이름만 변경)
└─ SMT5V-Haptics.ini       ← 원본 설정 + [DualSense] 섹션
```

> **Steam Input**: 켜 둬도 동작합니다. 기본 설정(`PassThroughRumble=0`)에서는 기존 모터 진동을 꺼서 이중으로 울리지 않게 합니다.

## 동작 확인

- 같은 폴더의 **`SMT5V-DualSense.log`** 를 엽니다.
  - `HAPTICS ACTIVE` 줄이 있으면 햅틱이 켜진 것입니다.
  - `core patched: ... WriteFile IAT=1` (1 이상)이어야 이벤트별 파형이 동작합니다.
  - `no DualSense audio endpoint` 가 나오면 USB 연결과 "Wireless Controller" 장치를 확인하세요.
- 게임 중 **F11** 을 누를 때마다 6가지 파형(normal → critical → weak → guard → sound → heal)을 차례로 재생합니다.
- 원본 모드의 F6~F8 모터 테스트는 `MotorToHaptics=1` 일 때만 햅틱으로 나옵니다.

## 설정 (`SMT5V-Haptics.ini` 의 `[DualSense]`)

| 키 | 기본값 | 설명 |
| --- | --- | --- |
| `Enabled` | `1` | 0이면 햅틱 브리지를 끄고 원본 모드만 동작 |
| `DeviceName` | `Wireless Controller` | 이 이름이 들어간 오디오 장치를 듀얼센스로 인식 (`DualSense`도 자동 인식) |
| `Gain` | `1.0` | 전체 햅틱 세기 (0.0 ~ 2.0) |
| `PassThroughRumble` | `0` | 1이면 기존 모터 진동도 같이 보냄 |
| `MotorToHaptics` | `0` | 1이면 이벤트가 아닌 모터 값(F6~F8 테스트 등)도 햅틱으로 변환 |
| `MotorLowHz` / `MotorHighHz` | `55` / `160` | 모터 값을 변환할 때 쓰는 저역/고역 주파수 |
| `MotorDelayMs` | `12` | 모터 값을 변환하기 전에 이벤트 로그를 기다리는 시간 |
| `BufferMs` | `10` | 오디오 버퍼 길이 (3 ~ 50). 작을수록 지연이 적음 |
| `LeftChannel` / `RightChannel` | `2` / `3` | 햅틱 채널 번호 (0부터 셈) |
| `MuteWhenUnfocused` | `1` | 게임 창이 비활성일 때 햅틱 끄기 |
| `TestHotkey` | `1` | F11 파형 테스트 사용 |

원본 모드의 설정(`[General]`, `[SoundRules]`)은 그대로 원본 모드가 읽습니다.

## 문제 해결

| 증상 | 확인할 것 |
| --- | --- |
| 햅틱이 전혀 안 남 | USB 유선 연결인지, 사운드 설정에 "Wireless Controller"가 있는지, 로그에 `HAPTICS ACTIVE`가 있는지 |
| 이벤트와 상관없이 같은 느낌만 남 | 로그의 `WriteFile IAT` 값이 0이면 이벤트 판정이 안 되는 상태 |
| 진동이 두 번 울림 | `PassThroughRumble=0` 인지 확인 |
| 게임이 실행되지 않음 / 원본 모드가 안 됨 | (방법 B) `SMT5VHapticsCore.dll` 이름이 정확한지, 로그에 `SMT5VHapticsCore.dll missing` 이 있는지 |
| Reloaded-II에서 햅틱이 안 남 | Reloaded-II에서 실행했는지, 모드가 체크돼 있는지, 원본 모드 `xinput1_3.dll` 이 `Win64` 에 있는지. Reloaded-II 모드 폴더에 `original mod not loaded` 로그가 있으면 원본 모드가 없는 것 |
| 로그에 `original mod already patched` | 방법 A와 B를 함께 설치한 상태. 하나만 남기세요 |
| 너무 세거나 약함 | 게임 내 `진동 세기` 게이지 또는 `Gain` 조절 |

## 제거

**방법 A (Reloaded-II):** Reloaded-II에서 모드 체크를 끄거나 `Reloaded-II\Mods\heerelli.smtvv.dualsense.haptic` 폴더를 지웁니다. 원본 모드는 그대로 동작합니다.

**방법 B (직접 설치):**
1. 이 모드의 `xinput1_3.dll` 을 삭제합니다.
2. `SMT5VHapticsCore.dll` 을 다시 `xinput1_3.dll` 로 이름을 바꿉니다.

공통 (선택): `SMT5V-Haptics.ini` 에서 `[DualSense]` 섹션과 `SMT5V-DualSense.log` 를 지웁니다.

## 동작 원리 (개발자용)

방법 B 기준입니다. 방법 A에서는 Reloaded-II가 이 모드를 불러오고, 이 모드가 이미 로드된 원본 모드를 찾아(자체 export `SMT5VHapticsPoCStatus` 로 식별) 같은 방식으로 패치합니다.

```text
게임 ──► xinput1_3.dll (이 모드: 모든 export를 원본으로 전달)
            │
            ▼
         SMT5VHapticsCore.dll (원본 모드)
            ├─ XInputSetState 호출 ──► 가로채서 기존 모터는 끄고 값만 기록
            └─ 로그 "EVENT kind=..." ──► 가로채서 이벤트별 햅틱 파형 재생
            ▼
         System32\xinput1_3.dll

햅틱 파형 ──► WASAPI ──► 듀얼센스 USB 오디오 4채널 중 3·4번 채널 (좌/우 보이스코일)
```

- 원본 모드의 내부 주소를 하드코딩하지 않습니다. 원본 DLL의 import 테이블(`GetProcAddress`, `WriteFile`)을 패치하고, 이미 확보된 `XInputSetState` 포인터를 찾아 교체합니다.
- export 이름과 ordinal은 원본 v0.6과 똑같이 맞췄습니다 (`src/exports.def`).
- 하나의 소스(`src/dllmain.cpp`)에서 `SMT5V_RELOADED` 정의 여부로 두 방식을 빌드합니다. Reloaded-II 방식은 네이티브 모드 진입점 `ReloadedStart` 를 export 합니다.

## 앞으로 할 일

- [x] 자동 빌드 및 Releases 배포
- [ ] 실제 게임에서 테스트
- [ ] 효과음 이름별 햅틱 규칙 (발소리, 메뉴 커서, 아이템 획득 등)
- [ ] 전투 파형을 더 짧고 날카롭게 다듬기
- [ ] 엔진 컷씬 햅틱 (가능한 경우) + 컷씬 햅틱 끄기 옵션

## 크레딧 · 라이선스

- 원본 진동 모드: [snm01234 / PC-SMT5V-VIB](https://github.com/snm01234/PC-SMT5V-VIB). 원본 모드 파일은 원작자의 것이며 이 저장소에 포함되지 않습니다.
- 이 저장소의 코드: MIT License (`LICENSE`)
- 진·여신전생 V Vengeance는 ATLUS의 상표입니다. 이 모드는 비공식 팬 모드입니다.
