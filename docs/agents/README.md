# 바이브 코딩 실험 계획

`codex/vfs-rework`는 사람이 직접 코드를 수정하지 않고 에이전트가 설계, 구현,
테스트, 문서화를 수행하는 바이브 코딩 실험용 브랜치입니다.

이 디렉터리는 실험 중 에이전트가 따라야 할 설계 초안, 구현 단계, 진행 상태,
검증 기준과 주요 판단을 보관합니다. 계획 문서는 작업에 맞춰 에이전트가
지속적으로 갱신합니다.

현재 코드와 공개 계약이 항상 우선하며, 구현이 안정화되면 확정된 내용은
`docs/opal/` 등 해당 기능의 정식 문서로 옮깁니다.

## 계획 목록

- [`plans/page-cache.md`](plans/page-cache.md): page cache, demand paging, file-backed mapping 및 COW
- [`plans/object-namespace.md`](plans/object-namespace.md): Linux형 VFS를 대체할 객체 네임스페이스와 단일 call ABI
