# libslab

`libslab`은 실험/시연용 라이브러리입니다.

## 상태
- 현재는 최소 API/테스트 경로를 유지하는 데 목적이 있습니다.

## 공개 헤더
- [`include/slab/slab.h`](include/slab/slab.h)

## 빌드
```bash
make -C libslab build CONFIG=debug PLATFORM=pc-x64
```

## 테스트
```bash
make -C libslab test CONFIG=debug PLATFORM=pc-x64
```
