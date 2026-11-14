# SMT5V DualSense 햅틱 브리지 v0.1

SMT5V Haptics v0.6의 진동을 듀얼센스 햅틱(보이스코일)으로 바꿔 주는 앞단 DLL입니다.
원본 모드는 그대로 두고, 그 앞에 끼워서 동작합니다.

> **원본 모드가 필요합니다.** 이 저장소에는 원본 모드 파일이 들어 있지 않습니다.
> 원본: [snm01234/PC-SMT5V-VIB](https://github.com/snm01234/PC-SMT5V-VIB) (SMT5V Haptics v0.6 기준)
>
> **상태:** v0.1, 아직 빌드·실기 테스트 전입니다.

## 동작 방식
- 원본 `xinput1_3.dll`은 `SMT5VHapticsCore.dll`로 이름을 바꿔 둡니다. 이 브리지가 그 파일을 불러옵니다.
- 원본 모드가 로그에 남기는 `EVENT kind=...` 줄을 읽어서 이벤트별 전용 파형을 재생합니다.
  - normal: 중간 굵기의 "퍽"
  - critical: 날카로운 클릭 뒤에 묵직한 저음 여운
  - weak: 좌→우 더블탭
  - guard: 짧은 "탁"과 부드러운 저음
  - sound: 필드 이벤트용 넓은 울림
  - heal: 가벼운 상승음
- 게임 원본에는 진동이 없으므로 기본값은 이벤트 파형만 사용합니다. `MotorToHaptics=1`로 바꾸면 모터 값(F6~F8 테스트 등)도 저역 55Hz / 고역 160Hz 햅틱으로 변환합니다.
- 진동 세기 게이지(0~10)와 8·9·10단계의 지속시간 연장도 그대로 반영됩니다.

## 요구 사항
- Windows, **듀얼센스 USB 유선 연결** (블루투스는 햅틱 오디오 채널이 없음)
- Windows 사운드 설정에 "Wireless Controller" 4채널 장치가 보여야 합니다.
- Steam Input은 켜도 되지만, 이중 진동이 생기면 `PassThroughRumble=0`(기본값)을 유지하세요.

## 빌드
"x64 Native Tools Command Prompt for VS 2022"에서 `build.bat`을 실행하면 `out\xinput1_3.dll`이 생깁니다.

## 설치 (`Project/Binaries/Win64/`)
1. 게임을 종료합니다.
2. 원본 모드의 `xinput1_3.dll`을 **`SMT5VHapticsCore.dll`로 이름 변경**합니다.
3. 빌드한 `out\xinput1_3.dll`을 복사합니다.
4. `SMT5V-Haptics.ini` 맨 아래에 `SMT5V-Haptics.DualSense-section.ini` 내용을 붙여 넣습니다.

```text
Win64/
├─ SMT5V-Win64-Shipping.exe
├─ xinput1_3.dll          ← 브리지
├─ SMT5VHapticsCore.dll   ← 원본 모드 (이름만 변경)
└─ SMT5V-Haptics.ini
```

## 확인
- `SMT5V-DualSense.log`에 `HAPTICS ACTIVE`가 나오면 정상입니다.
- `core patched: ... WriteFile IAT=1` 이상이어야 이벤트별 파형이 동작합니다.
- 게임 중 **F11**을 누르면 6가지 파형을 차례로 테스트합니다. F6~F8(원본 모드의 모터 테스트)은 `MotorToHaptics=1`일 때만 햅틱으로 나옵니다.
- 듀얼센스가 없으면 진동이 원래대로 전달됩니다. 엑스박스 패드도 그대로 쓸 수 있습니다.

## 제거
`xinput1_3.dll`(브리지)을 지우고 `SMT5VHapticsCore.dll`을 다시 `xinput1_3.dll`로 바꾸면 됩니다.

## 라이선스
브리지 코드는 MIT 라이선스입니다 (`LICENSE`). 원본 모드는 원작자의 소유이며 이 저장소에 포함되지 않습니다.
