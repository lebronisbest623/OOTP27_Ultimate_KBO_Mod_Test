# OOTP27 KBO Launcher

OOTP27은 KBO를 지원하지 않는다. 이 프로젝트가 그 빈자리를 채운다.

C# 런처가 게임 프로세스를 시작하고, C로 작성된 네이티브 패치 DLL을 주입해 KBO 고유 규칙들을 실시간으로 적용한다. OOTP의 소스코드 없이, 실행 중인 바이너리에 직접 훅을 걸어 동작한다.

English documentation: [`README.en.md`](README.en.md)

---

## 디스클레이머

이 프로젝트는 Out of the Park Baseball 27의 비공식 팬 제작 모드/런처다. OOTP Developments, Com2uS, KBO, 각 구단, 선수협 또는 Steam과 제휴하거나 승인받은 프로젝트가 아니다.

이 런처는 실행 중인 OOTP27 프로세스에 네이티브 DLL을 주입하고 게임 메모리와 런타임 동작을 패치한다. 그 특성상 OOTP 업데이트, 실행 환경, 세이브 상태, 백신/보안 정책에 따라 오작동, 충돌, 세이브 손상, 데이터 유실이 발생할 수 있다.

사용자는 반드시 중요한 세이브를 백업한 뒤 사용해야 한다. 이 프로젝트는 지원 빌드 확인, 로스터 마커 확인, 기본 주입 비활성화 등 안전장치를 제공하지만, 모든 환경에서 안전을 보장하지 않는다. 사용에 따른 책임은 사용자 본인에게 있다.

상업적 이용, 재배포, 타 프로젝트 포함, 공개 릴리즈 배포 전에는 포함된 폰트, 이미지, 시드 데이터, OOTP 관련 파일 및 기타 자산의 라이선스를 직접 확인해야 한다.

---

## 구현된 KBO 규칙

### 외국인 선수 보류권
시즌 종료 후 열리는 결정 창에서 팀이 외국인 선수에 대한 보류권을 행사한다. 보류권을 가진 팀만 해당 선수와 재계약이 가능하다. AI 팀도 자동으로 결정을 내린다.

### 군복무 대출 (군복무 선수단)
선수가 군복무 대상이 되면 군부대 야구단으로 임시 이적 처리된다. 복무 일수를 매일 차감하고, 복무 완료 시 원소속 팀으로 자동 복귀한다.

### 아시안 쿼터
외국인 선수 등록 제한과 별개로 아시아권 외국인 선수에 대한 쿼터를 별도로 관리한다. 활성 로스터, 콜업 한도 모두 KBO 규정대로 적용된다.

### 부상 대체 외국인 슬롯
등록된 외국인 선수가 부상자 명단에 오르면 임시 대체 외국인 선수 영입이 허용된다. 원 선수가 복귀하면 슬롯이 닫힌다.

### 아시안게임 국가대표 소집
커스텀 이벤트로 아시안게임 일정을 생성하고, 선발, 출국, 복귀 단계를 순서대로 처리한다. 각 구단별 차출 인원을 자동으로 배분하고 뉴스 기사도 생성한다.

### 단일 디비전 올스타
OOTP는 단일 디비전 리그의 올스타전을 기본 지원하지 않는다. 관련 분기 조건에 패치를 걸어 올스타 투표·선발·경기가 정상 진행되도록 한다.

---

## 구조

```
KBOLauncher.exe          ← C# 런처
  │
  ├─ OOTP27 프로세스 시작 (또는 실행 중인 프로세스에 attach)
  ├─ 시드 데이터 준비 (%LOCALAPPDATA%\OOTP-KBO\)
  └─ KBOFix.dll 주입
       │
       └─ 메모리 훅 / 패치
            ├─ native/src/foreign/          외국인 선수 정책
            │    ├─ rights/                 보류권 레코드 저장·로드
            │    ├─ signability/            서명·오퍼 차단 훅
            │    ├─ replacement_seed/       대체 선수 시드 해석
            │    ├─ injury/                 부상 대체 슬롯
            │    ├─ quota/                  아시안 쿼터 및 콜업 정책
            │    └─ roster_audit/           로스터 감사 (읽기 전용)
            ├─ native/src/military_service/ 군복무 대출
            │    └─ seed/                   선수 시드 해석
            ├─ native/src/custom_events/    아시안게임, 우선 협상 창
            └─ native/src/hotkey_window/    인게임 F2 허브 UI
```

네이티브 레이어는 단일 번역 단위로 빌드된다. `native/KBOFix.c`가 쉘 역할을 하고, 기능별 `.inc` 파일들을 의존성 순서대로 include한다.

---

## 요구사항

- **OOTP27** (Steam 기준 `ootp27.exe`)
- **.NET 8** — 런처 실행
- **GCC (MinGW-w64)** — 네이티브 DLL 빌드
- **WebView2 NuGet 패키지** — F2 허브 UI

---

## 빌드

```powershell
# 네이티브 DLL
powershell -ExecutionPolicy Bypass -File native\build.ps1

# C# 런처
dotnet build .\OOTP27-KBO-Launcher.sln
```

빌드 결과물:
- `native/bin/KBOFix.dll`
- `bin/Debug/net8.0/KBOLauncher.exe`

---

## 실행

### 일반 실행 (권장)

```bat
KBOLauncher.exe
```

DLL 주입 없이 OOTP를 시작한다. 주입을 활성화하려면 `%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json`에 단일 JSON 플래그 설정을 사용한다.

```json
{
  "enable_launcher_injection": true
}
```

### 직접 실행

```powershell
# OOTP 시작 + DLL 주입
KBOLauncher.exe --dll native\bin\KBOFix.dll

# 실행 중인 프로세스에 attach
KBOLauncher.exe --dll native\bin\KBOFix.dll --attach-existing

# 경로 지정
KBOLauncher.exe --ootp "D:\Steam\steamapps\common\Out of the Park Baseball 27\ootp27.exe" --dll native\bin\KBOFix.dll
```

### 주요 플래그 설정

런타임 동작은 `%LOCALAPPDATA%\OOTP-KBO\kbo_flags.json` 단일 JSON 파일로 제어한다.

| 설정 | 기본값 | 설명 |
|---|---|---|
| `enable_launcher_injection` | off | 런처 자동 주입 활성화 |
| `enable_foreign_waiver_ai` | on | AI 보류권 자동 결정 |
| `enable_single_division_allstar_events` | on | 단일 디비전 올스타전 |
| `kbo_league_id.txt` | 100 | KBO 리그 ID 오버라이드 |

---

## 런타임 데이터 경로

| 경로 | 설명 |
|---|---|
| `%LOCALAPPDATA%\OOTP-KBO\` | 글로벌 설정, 플래그, 시드 파일 |
| `<세이브폴더>/` | 세이브 스코프 데이터 (보류권, 군복무 기록 등) |
| `data/seeds/` | 번들 시드 CSV (빌드 결과물에 포함) |

세이브 스코프 파일이 먼저 읽히고, 없으면 글로벌 파일로 폴백한다.

---

## 아키텍처 상세

[`docs/ARCHITECTURE.md`](docs/ARCHITECTURE.md)

---

## 라이선스

이 프로젝트의 소스 코드는 **Mozilla Public License 2.0 (MPL-2.0)**에 따라 배포된다. 자세한 내용은 [`LICENSE`](LICENSE)를 확인한다.

MPL-2.0은 파일 단위 약한 copyleft 라이선스다. 이 프로젝트의 MPL 적용 소스 파일을 수정해 배포하는 경우, 해당 수정 파일의 소스 코드를 같은 라이선스 조건으로 제공해야 한다. 단, 별도 파일로 작성된 더 큰 프로그램 전체를 반드시 같은 라이선스로 공개해야 하는 것은 아니다.

번들 폰트, 이미지, 시드 데이터, OOTP/KBO 관련 명칭 및 기타 제3자 자산은 각자의 라이선스와 권리 관계를 따른다. 이 저장소의 라이선스는 해당 제3자 자산에 대한 별도 권리를 부여하지 않는다.
