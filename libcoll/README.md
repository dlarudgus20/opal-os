# libcoll

`libcoll`은 커널에서 사용하는 자료구조 라이브러리입니다.

## 제공 자료구조
- `arraylist`
- `linkedlist`
- `singlylist`
- `rbtree`
- `ringbuffer`

## 빌드
```bash
make -C libcoll build CONFIG=debug PLATFORM=pc-x64
```

## 테스트
```bash
make -C libcoll test CONFIG=debug PLATFORM=pc-x64
```
