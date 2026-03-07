# rbtree

## 개요
- intrusive 레드-블랙 트리입니다.
- 공통 코어(`rbtree_init`, `rbtree_remove`, `rbtree_first`, `rbtree_next`)와
  템플릿 매크로(`RBTREE_TEMPLATE`)를 조합해 타입별 API를 생성합니다.

## 핵심 타입
- `struct rbtree_node`: 트리 링크/색상 메타데이터
- `struct rbtree`: 루트 포인터
- `struct rbtree_find_result`:
  - `lower`: key 이하의 가장 가까운 노드
  - `upper`: key 이상의 가장 가까운 노드
  - exact match면 `lower == upper`

## 템플릿 매크로 계약
- `RBTREE_TEMPLATE(type, keytype, node_, comp, comp_to, postfix, ...)`
  - `comp(type* a, type* b)`:
    - `<0`: `a < b`
    - `>0`: `a > b`
    - `0`: 동일 key (중복 삽입 거부)
  - `comp_to(type* a, keytype key)`:
    - `<0`: `a->key < key`
    - `>0`: `a->key > key`
    - `0`: 동일 key
- 생성 함수:
  - `rbtree_insert_<postfix>(tree, data)`
  - `rbtree_find_<postfix>(tree, key)`

## 중복 key 동작
- `insert`는 `comp(...) == 0`이면 삽입하지 않고 즉시 반환합니다.
- 즉 현재 구현은 **중복 key를 허용하지 않습니다**.

## 사용 예시
```c
struct mynode {
    int key;
    struct rbtree_node node;
};

static int cmp_node(const struct mynode* a, const struct mynode* b) {
    if (a->key < b->key) return -1;
    if (a->key > b->key) return 1;
    return 0;
}

static int cmp_key(const struct mynode* a, int key) {
    if (a->key < key) return -1;
    if (a->key > key) return 1;
    return 0;
}

RBTREE_TEMPLATE(struct mynode, int, node, cmp_node, cmp_key, my, static)
```
