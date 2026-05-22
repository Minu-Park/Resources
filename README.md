# Resources

> Playground 제품군 및 결합 모듈 전반에 걸쳐 공통으로 사용되는 Qt 스타일시트(QSS), 브랜드 자산 및 아이콘 팩을 제공하는 리소스 통합 모듈입니다.

이 모듈은 개별 센서 모듈이나 렌더링 라이브러리에 스타일시트와 그래픽 자원이 파편화되어 흩어지는 것을 차단하고, 전역 테마 및 자산을 일괄 제어하는 오케스트레이션 스타일 가이드 역할을 합니다.

---

## 📌 주요 특징 (Key Features)
- **전역 자원 통합 관리 (QRC Prefix `:/Resources`)**: `Resources.qrc` 번들을 소유하며, 브랜드 아이덴티티 및 아이콘 팩을 단일 가상 경로를 통해 모듈 전역에 배포합니다.
- **통합 스타일 시트 (`Style.qss`)**: `GraphicsEngine`, `QCameraWidget`, `QGocatorWidget` 등 각 컴포넌트의 스타일 선택자(Selector) 및 전역 다크 테마/브랜드 컬러를 단일 스타일시트 내부에서 일괄 정의합니다.
- **원스톱 초기화 인터페이스 (`installResources` API)**: 호스트 애플리케이션 시작부에 `Resources::installResources(app)` 한 줄만 호출하면 전역 QRC 리소스를 동적으로 등록하고, 스타일시트를 로드하여 애플리케이션 전반에 테마가 자동 적용되도록 설계되었습니다.

## 🛠️ 요구 사양 및 의존성 (Prerequisites & Dependencies)
- **OS**: macOS / Windows / Linux (Qt가 지원하는 모든 OS 공통)
- **언어 표준**: C++17 이상
- **필수 의존성**:
  - **CMake**: 버전 3.16 이상
  - **Qt6**: 버전 6.4 이상 (Core, Gui, Widgets 라이브러리 필수)

## 🚀 시작하기 (Quick Start & Build)

### 1. 빌드 방법
상위 CMake 프로젝트에서 모듈을 포함하고 타겟을 프라이빗 라이브러리로 링크합니다.

```cmake
# CMakeLists.txt 예시
add_subdirectory(modules/Resources)
target_link_libraries(<your_target> PRIVATE Resources::Resources)
```

### 2. 사용 예제 (API Usage)
```cpp
#include "Resources.h"
#include <QApplication>
#include <QMainWindow>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 애플리케이션 리소스 등록 및 스타일시트(Style.qss) 로드/적용
    Resources::installResources(app);

    QMainWindow mainWindow;
    // ... 메인 윈도우 구성 ...
    mainWindow.show();

    return app.exec();
}
```

---

## 📂 디렉토리 구조 (Directory Structure)

```text
Resources/
├── CMakeLists.txt         # Qt MOC/AUTORCC 설정 및 Resources 타겟 빌드 구성
├── README.md              # 모듈 리소스 규칙 및 바인딩 설명서
├── Resources.qrc          # Qt 가상 리소스 루트 정의 파일 (:/Resources Prefix)
├── Resources.rc           # Windows 실행 파일 전용 빌드 메타데이터 리소스 파일
├── Resources.h/.cpp       # installResources API 구현 클래스
├── Style.qss              # 메인 UI 테마, 위젯 커스터마이징, 상태별 QSS 정의 파일
├── BASLER_Logo.png        # Basler 브랜드 에셋 리소스
├── Icon.ico/Icon.png      # 애플리케이션 전역 런쳐 아이콘 자산
└── Icons/                 # 모듈 내에서 참조하여 호출하는 공용 UI 액션용 아이콘 디렉토리
```

---

## ⚠️ 아키텍처 규칙 및 제약 (Boundaries & Rules)
- **자산 격리 규칙**: 이 모듈에는 카메라, Gocator 등의 장치 제어 로직이나 GraphicsEngine 렌더링 파이프라인 연동 코드와 같은 물리적 소스코드가 절대 포함될 수 없습니다. 순수 자원 자산과 이 자산을 주입하는 초기화 클래스만 존재해야 합니다.
- **스타일 및 에셋 중앙화**: UI 마크업에 사용될 신규 아이콘 추가, QSS 스타일시트 디자인 가이드 개정 등 모든 시각 요소에 대한 변경 요구는 오직 본 리포지토리 안에서 수정되어야 하며, 다른 소스 모듈에서 복사본을 만들어 참조하면 안 됩니다.
- **아이콘 가상 주소 바인딩**: 모듈 내에서 아이콘 파일을 가져다 쓸 때는 파일 시스템의 로컬 상대 경로가 아닌, QRC 가상 주소인 `:/Resources/Icons/[아이콘_파일명].png` 경로를 독점 사용해야 합니다.

## 📝 라이선스 (License)
본 리소스 및 브랜드 자산은 독점 상용 라이선스를 따르며, 허가되지 않은 도용과 상업용 이용을 불허합니다.
