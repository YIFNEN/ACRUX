# 빌드 및 검증 기록

## 확인 환경

2026년 10월 23일 Windows의 Visual Studio MSVC 도구와 Windows SDK를 사용해 x86 대상으로 확인했습니다. 빌드 스크립트는 Visual Studio Installer의 `vswhere.exe`로 C++ 도구 설치 위치를 찾습니다.

Visual Studio에서 C++를 사용한 데스크톱 개발 도구와 Windows SDK를 설치한 뒤 저장소 루트에서 PowerShell을 실행합니다. 기본 빌드에는 Python, 게임 엔진이나 별도 패키지 설치가 필요하지 않습니다.

## 이번에 실행한 확인

| 항목 | 명령 또는 방법 | 결과 |
| --- | --- | --- |
| 원본과 코드 일치 | `src`와 `tests`의 25개 파일을 SHA-256으로 대조 | 모두 일치했습니다. |
| 핵심 규칙 검사 | `& .\scripts\verify-iteration.ps1 -Profile Logic` | Debug 테스트가 통과했습니다. |
| Release 빌드와 크기 | `& .\scripts\verify-iteration.ps1 -Profile Size` | 빌드와 크기 검사가 통과했습니다. |
| 실행 파일 크기 | `out/game-release-x86/ACRUX.exe` | 971,776바이트입니다. 원본 개발 폴더의 기존 Release 실행 파일과 크기가 같습니다. |
| 용량 여유 | `scripts/release-policy.json`의 내부 기준과 비교 | 내부 기준 1,350,000바이트 중 378,224바이트가 남았습니다. |

테스트에는 길의 연결과 파손, 설치 및 회수 수량, 새 밤의 재료 보충, 자원 중복 방지, 동물 행동, 생존 상태와 합성 오디오 등을 확인하는 사례가 포함되어 있습니다.

## 빌드 명령

```powershell
# 규칙 테스트
& .\scripts\verify-iteration.ps1 -Profile Logic

# Release 게임 빌드와 크기 검사
& .\scripts\verify-iteration.ps1 -Profile Size

# Debug 게임이 필요한 경우
& .\scripts\build.ps1 -Target Game -Configuration Debug -Arch x86
```

출력은 `out` 아래에 생성되며 Git 추적 대상에서 제외합니다. 생성된 이미지 include 파일을 그대로 빌드하므로 기본 확인 과정에서 자산 변환 스크립트를 실행할 필요는 없습니다.

## 확인하지 않은 범위

이번 공개 작업에서는 게임 코드를 변경하지 않았습니다. 새로 직접 플레이하거나 화면과 조작감을 평가하지 않았으며, 재미나 난이도가 개선되었다고 판단하지 않습니다.

제출 후보를 새로 만들지 않았습니다. 배포 ZIP의 재해제 검사, 깨끗한 Windows 환경의 실행과 완주를 이번 결과에 포함하지 않습니다. 별도의 제출 후보가 필요한 경우 기존 릴리스 절차와 [오프라인 점검표](release/offline-checklist.md)에 따라 확인해야 합니다.

이 기록의 실행 파일 크기는 저장소 전체 크기와 다릅니다. 저장소에는 읽고 수정할 소스, 원본 자산과 검사 도구가 포함됩니다.
