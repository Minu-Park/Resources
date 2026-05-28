# Resources Theme

현재 Resources는 단일 테마만 제공합니다.

- `qss/`: `Resources::installResources(app)`가 정해진 순서로 합쳐 적용하는 QSS 파트.
- 아이콘과 로고는 아직 `:/Resources/Icons/*`, `:/Resources/BASLER_Logo.png` 경로를 유지합니다.
- 모듈 단독 실행성을 지키기 위해 Camera, Gocator, GraphicsEngine이 Resources API를 직접 호출하지 않습니다.
- 호스트 애플리케이션만 `Resources::installResources(app)`를 호출해 외형을 입힙니다.
- 모듈은 `status`, `messageState`, objectName 같은 의미 정보만 노출하고 색상/여백/굵기는 이 테마가 소유합니다.

테마를 추가로 분기해야 할 때만 이 디렉터리 아래에 이름 있는 테마 구조를 도입합니다.
