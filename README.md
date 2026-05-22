# Resources

> 전역 다크 테마 QSS 스타일시트, 아이콘 에셋, 로고 파일을 Qt 가상 리소스로 바인딩하여 제공하는 공유 자원 모듈입니다.

---

## 📌 주요 특징
- **전역 테마 관리**: 통합 스타일시트(`Style.qss`)를 통해 각 위젯 컴포넌트의 비주얼 테마와 스타일 규칙을 한곳에서 일괄 관리합니다.
- **리소스 중앙화**: 아이콘 팩 및 로고 파일 등 비주얼 에셋을 가상 경로(`:/Resources`)를 통해 배포하여 타 코드 모듈과 완전히 분리합니다.
- **동적 자원 주입**: 애플리케이션 진입부에서 `Resources::installResources(app)` 호출 한 줄만으로 스타일시트와 에셋을 자동 로딩하여 반영합니다.

## 🛠️ 요구 사양 및 의존성
- **OS**: Qt6를 지원하는 모든 OS 공통
- **언어 표준**: C++17 이상
- **의존 라이브러리**:
  - Qt6 (Core, Gui, Widgets - 최소 6.4 이상 권장)
  - CMake 3.16+

## 🚀 빌드 및 사용 예제

### 1. 빌드 방법
상위 CMake 프로젝트에서 하위 타겟으로 빌드하여 링크합니다.
```cmake
add_subdirectory(modules/Resources)
target_link_libraries(<target> PRIVATE Resources::Resources)
```

### 2. 사용 예제
```cpp
#include "Resources.h"
#include <QApplication>

int main(int argc, char* argv[])
{
    QApplication app(argc, argv);

    // 공용 리소스 등록 및 Style.qss 전역 적용
    Resources::installResources(app);

    // ... 애플리케이션 UI 실행 ...
    return app.exec();
}
```

## ⚠️ 개발 주의사항
- **비즈니스 로직 제한**: 본 모듈은 순수 디자인 에셋과 QSS 파일 및 동적 로더 클래스로만 구성되어야 하며, 센서 기동이나 렌더링에 관한 물리적 비즈니스 로직을 포함하지 않습니다.
- **에셋 주소 규격**: 가상 디렉토리 내 에셋을 호출할 때는 로컬 경로 대신 반드시 QRC 주소(`:/Resources/Icons/[파일명].png`)를 사용하여 통일합니다.
