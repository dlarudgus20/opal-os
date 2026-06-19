# opsh

`opsh`는 사용자 공간 shell 프로그램이다.

## 실행
- `uinit`은 `/opsh`를 열고 `exec(fd)`로 실행한다.

## 빌드/배치
- `make iso`는 `opsh`를 빌드해 initramfs의 `/opsh`로 복사한다.
