# Resources

> Shared Qt resource module for the Playground light theme, icons, QSS, and logo assets.

---

## Features
- **Light theme QSS**: `Style.qss` owns the shared application visual rules.
- **Centralized assets**: Icons and logo files are exposed through `:/Resources` QRC paths.
- **Host installation API**: `Resources::installResources(app)` registers compiled resources, applies QSS, and sets the app icon.
- **Shared device status styling**: Camera and Gocator status bubbles use the same QSS selectors and dynamic `status` properties.
- **Brand asset**: `BASLER_Logo.png` is available for host chrome such as the Playground MDI watermark.

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

## Development Notes
- Keep this module limited to reusable QSS, icons, logos, and resource installation helpers.
- Do not add sensor runtime, rendering, or host-window logic here.
- Use QRC paths such as `:/Resources/Icons/[name].png` or `:/Resources/BASLER_Logo.png`; avoid filesystem paths in application code.
