# AGENTS Guide

이 문서는 `opal-os` 저장소에서 작업하는 인간/에이전트를 위한 운영 가이드입니다.
권장사항 중심으로 작성되어 있으며, 반복적으로 발생한 빌드/테스트 이슈를 빠르게 피하는 데 목적이 있습니다.

## **중요** Markdown 규칙
Markdown 문서를 편집할 때 아래 규칙을 준수해야 합니다.
- Ordered list 아래 Unordered list를 중첩할 때, 하위 불릿(`-`)은 상위 항목의 본문 시작 위치와 정렬합니다.
- 즉, 고정 공백 수를 강제하지 않고 “상위 항목 텍스트 시작 컬럼 정렬”을 기준으로 합니다.
- 예시:

```md
1. 상위 항목
   - 하위 항목 A
   - 하위 항목 B

10. 상위 항목
    - 하위 항목 A
    - 하위 항목 B
```

## 적용 범위
- 루트 및 모든 하위 서브프로젝트

## 응답 언어
- 사용자와의 모든 응답은 한국어로 작성합니다.
- `/review` 명령 결과를 전달하거나 요약할 때도 반드시 한국어로 작성합니다.

## 프로젝트 맥락
- 타깃은 `pc-x64`입니다.
- 커널 산출물은 `kernel/build/.../kernel.elf`와 `kernel/build/.../kernel.sys`입니다.
- 커널 테스트의 산출물은 `kernel/build/test/...`에 위치합니다.

## 빌드 및 테스트
- dry-run처럼 stdout 자체가 목적이 아닌 `make` 실행은 stdout을 억제합니다. 실패 시 stderr와 종료 코드를 확인합니다.
- QEMU 실행 전 빌드를 별도로 수행합니다. 다음처럼 빌드하고 시리얼 출력을 확인합니다.

```bash
make iso TEST=1 >/dev/null
make test QEMU_DISPNONE=1
```

- 테스트는 headless로 실행합니다.
- `QEMU_FLAGS`를 직접 지정할 때는 `-display none`과 `-boot order=dc`를 포함합니다.
- `==== test end ====` 요약을 확인한 다음 Ctrl+C로 QEMU를 수동 종료합니다.

## 예시 명령
```bash
make build >/dev/null
make iso >/dev/null
make clean >/dev/null
make clean-test >/dev/null
make fullclean >/dev/null
```

## 작업 원칙 (권장)
1. 변경 전에 영향 범위 확인
   - `rg`, `find`, 기존 `docs/*.md`로 관련 파일을 먼저 확인합니다.

2. 빌드 시스템 변경은 묶어서 점검
   - Makefile을 건드리면 최소한 아래를 함께 확인합니다.
     - `mkfiles/config.mk`
     - `mkfiles/rules.mk`
   - 해당 서브프로젝트 `Makefile`/`makefile`

3. 변경 후 최소 검증
   - 가능한 경우 최소 1개 이상 빌드/테스트를 실행하고 결과를 남깁니다.
   - 여러 `make` 명령을 동시에 실행하지 않습니다. 이 빌드 시스템은 독립 `make` 프로세스 간 산출물 쓰기를 serialize하지 않습니다.
   - 여러 산출물을 검증할 때는 루트 `make`/`make iso`처럼 한 make invocation 안의 의존성 그래프를 사용하거나 순차 실행합니다.
   - 소스 수집 여부나 Make 규칙 포함 여부만 확인할 때는 `make -B -n` 같은 강제 dry-run을 우선 사용합니다.
   - 단순 `make -n`은 타깃이 up-to-date이면 실제 recipe를 보여주지 않을 수 있습니다.
   - `make -B` 전체 강제 빌드는 실제 산출물 재생성 검증이 필요할 때만 사용합니다.

4. 큰 구조 변경 시 문서 동반 업데이트
   - `README.md`와 `docs/` 내 문서들을 우선 검토합니다.

### *중요* `docs/opal/memory-map.md 수정 시 유의점
- `1. 가상 주소 공간 요약` 부분에 텍스트로 그려진 표 양식은 함부로 바꾸지 말 것.
  - 표 내용을 바꿔야 할 땐 기존 양식 엄수

## 자주 발생한 이슈와 예방
### 1) C23 `constexpr` compound literal 관련 오탐 방지
- 이 저장소는 C 소스에서 C23 `constexpr` compound literal 사용을 허용합니다.
- 예: `((constexpr struct singlylist){ { NULL } })`
- 해당 문법은 현재 툴체인에서 유효하므로, "C++ 전용 문법"으로 자동 판단하지 않습니다.
- 문법 이슈를 제기하려면 최소 1회 실제 컴파일 실패 로그를 첨부합니다.

### 2) release 빌드 false positive 경고 처리 원칙
- release 빌드에서 false positive 경고나 오탐처럼 보이는 경고가 나와도, 경고를 잠재우기 위한 임시 초기화/캐스트/분기 추가를 먼저 하지 않습니다.
- 먼저 실제 타입 계약, 에러 값 표현, signed/unsigned 변환, narrow cast, 수명/소유권 경로를 조사해서 분석기가 혼란스러워한 근본 원인을 확인합니다.
- 경고가 false positive로 결론나더라도, 가능하면 코드의 계약을 더 명확히 표현하는 헬퍼나 타입 경계 정리로 해결합니다.
- 단순 우회가 필요하다고 판단한 경우에는 왜 근본 수정이 불가능하거나 과도한지 근거를 남깁니다.

## 커널 테스트 작성 가이드
- 테스트 등록: `DEFINE_TEST(name)`
- 검증 매크로: `TEST_EXPECT_*`, `TEST_ASSERT_*`
- 실패 모델:
  - `EXPECT_*`: 실패 누적
  - `ASSERT_*`: 현재 테스트 즉시 종료
- 테스트는 작고 독립적으로 유지합니다.

## 커밋/PR 전 체크리스트
- [ ] 관련 빌드 최소 1개 성공
- [ ] 관련 테스트 최소 1개 통과
- [ ] 문서 갱신 필요 여부 확인
- [ ] 명령/경로/변수 오타 확인
- [ ] 의도 범위 외 파일 변경 없는지 확인

## 권장 보고 포맷
1. 변경 요약
2. 근거/의도
3. 검증 명령과 결과
4. 남은 리스크/후속 작업

## 참고 문서
- `README.md`
- `docs/README.md`
- `docs/build-system.md`
- `docs/kernel-test.md`
