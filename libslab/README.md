# libslab

`libslab`은 slab 관련 실험/시연용 라이브러리입니다.

## 역할
- 현재는 최소 API와 테스트 경로를 유지하는 상태
- 향후 slab allocator 확장을 위한 자리

## 공개 헤더
- [`slab/slab.h`](include/slab/slab.h)

## 빌드
```bash
make -C libslab build CONFIG=debug PLATFORM=pc-x64
```

산출물:
- `build/pc-x64/<config>/libslab.a`

## 테스트
```bash
make -C libslab test CONFIG=debug PLATFORM=pc-x64
```
