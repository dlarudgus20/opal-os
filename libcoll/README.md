# libcoll

`libcoll`은 커널/저수준 코드에서 재사용 가능한 자료구조 라이브러리입니다.

## 제공 자료구조
- `arraylist`
- `linkedlist`
- `singlylist`
- `rbtree`
- `ringbuffer`

## 공개 헤더 (`<collections/...>`)
- [`collections/arraylist.h`](include/collections/arraylist.h)
- [`collections/linkedlist.h`](include/collections/linkedlist.h)
- [`collections/singlylist.h`](include/collections/singlylist.h)
- [`collections/rbtree.h`](include/collections/rbtree.h)
- [`collections/ringbuffer.h`](include/collections/ringbuffer.h)

## 빌드
```bash
make -C libcoll build CONFIG=debug PLATFORM=pc-x64
```

산출물:
- `build/pc-x64/<config>/libcoll.a`

## 테스트
```bash
make -C libcoll test CONFIG=debug PLATFORM=pc-x64
```

테스트 모드 링크 메모:
- `STATIC_REFS := libkc`
- `LDFLAGS_ON_TEST := -Wl,--exclude-libs=libkc`
- `TEST_SHARED_REFS := libpanicimpl`
