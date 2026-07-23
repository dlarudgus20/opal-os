# Object Namespace 작업 계획

상태: 초안

대상 브랜치: `codex/vfs-rework`

## 1. 목표

현재 Linux형 VFS의 `path_entry -> inode -> file` 강제 계층을 범용 객체
네임스페이스로 교체합니다.

- 파일, 디렉터리, 장치, 프로세스, 이벤트와 메모리 객체를 같은 이름 트리에
  바인딩
- 기존 `path_entry`의 공통 경로 트리 역할은 `ns_entry`로 일반화해 유지
- namespace상의 위치와 실제 backing object의 정체성을 분리
- 열린 상태가 필요할 때만 별도의 stateful `ns_object` 생성
- subtype별 read/write/ioctl syscall을 `iid`와 `mid` 기반 object call로 통합
- VFS directory semantic과 filesystem directory inode semantic을 분리
- filesystem과 동적 객체 트리를 비공개 `ns_provider` 계약으로 연결

이 설계는 “모든 것이 파일이다”를 목표로 하지 않습니다. 모든 사용자 접근
대상은 `ns_object`가 될 수 있지만, 각 객체는 실제로 의미가 있는 interface만
지원합니다.

## 2. 현재 기준선

현재 구현은 다음 구조를 가집니다.

- `struct path_entry`가 부모, 이름, 자식 cache와 `struct inode *`를 가집니다.
- `struct inode`가 filesystem node의 공유 정체성과 `inode_ops`를 가집니다.
- `struct file`이 열린 세션의 mode, position과 `file_ops`를 가집니다.
- 프로세스의 `struct filetable`은 FD를 `struct file *`에 매핑합니다.
- CPIO와 FAT는 `superblock`, `inode`, `file` 계층을 각각 구현합니다.
- `kobjfs`는 HID와 framebuffer console 같은 커널 객체를 가짜 inode/file로
  감싸 `/dev`에 노출합니다.
- 익명 pipe도 inode와 file을 만들지만 경로 트리에는 연결하지 않습니다.
- 디렉터리 read는 VFS가 `inode_ops.iterate_dir()`를 호출하는 특례입니다.
- mount point처럼 `path_entry`에만 존재하는 자식은 현재 디렉터리 read 결과에
  나타나지 않습니다.
- 사용자 ABI는 `open`, `stat`, `read`, `write`, `ioctl`, `mount`, `pipe`처럼
  객체 종류와 연산별 syscall로 나뉩니다.
- 커널의 `int 0x80` 진입 경로는 syscall 번호 외에 5개 인자
  (`rdi`, `rsi`, `rdx`, `r10`, `r8`)를 전달할 수 있습니다.
- `libopalsys`의 syscall wrapper는 현재 그중 4개 인자까지만 전달합니다.
- 사용자 포인터 검증과 복구 가능한 user-memory access는 아직 구현되어 있지
  않습니다.

## 3. 핵심 구조

### 3.1 `nsstr`: namespace 이름

새 namespace는 기존 `fs/hstr.h`에 의존하지 않습니다.
`kernel/include/opal/ns/nsstr.h`와 `kernel/src/ns/nsstr.c`에 `struct nsstr`과
구현을 둡니다.

`nsstr`은 현재 `hstr`의 다음 계약을 가져옵니다.

- 짧은 문자열 inline 저장과 긴 문자열 pointer 저장
- 문자열 길이와 32비트 DJB2 계열 hash cache
- owned 문자열과 호출 동안만 유효한 borrowed stack 문자열
- alloc, free, stack, clone, dup, equality와 rehash 연산

공개 함수와 상수는 `NSSTR_SHORT_LEN`, `NSSTR_NULL`, `NSSTR_EMPTY`와
`nsstr_alloc()`, `nsstr_free()`, `nsstr_stack()`, `nsstr_is_null()`,
`nsstr_rehash()`, `nsstr_get()`, `nsstr_len()`, `nsstr_hash()`,
`nsstr_clone()`, `nsstr_dup()`, `nsstr_equal()`로 명명합니다.

- `nsstr_alloc()`으로 얻은 writable 문자열은 내용을 채운 뒤
  `nsstr_rehash()`를 호출해야 key로 사용할 수 있습니다.
- `nsstr_stack()`은 borrowed buffer의 길이와 hash를 즉시 계산합니다.
- 완성된 `nsstr`은 항상 유효한 cached hash를 가집니다.
- equality는 hash와 길이가 모두 같을 때만 문자열 byte를 비교합니다.
- 초기 구현은 per-boot salt나 cryptographic hash를 추가하지 않습니다.

기존 `fs/hstr.h`와 `kernel/src/fs/hstr.c`는 새 namespace와 구 VFS가 병행되는
동안 유지합니다. 구 VFS 제거 단계에서 기존 소비자와 함께 삭제하며 `hstr`에서
`nsstr`로 향하는 alias나 compatibility wrapper는 만들지 않습니다.

### 3.2 `ns_entry`: 공통 namespace tree

namespace는 VFS core가 관리하는 공통 `ns_entry` tree입니다. provider마다 서로
다른 경로 트리를 노출하거나 공통 트리 자체를 없애지 않습니다.

```text
(parent ns_entry, name) -> child ns_entry -> backing ns_object
```

개념적인 구조는 다음과 같습니다.

```c
enum ns_binding_source {
    NS_BINDING_EXPLICIT,
    NS_BINDING_PROVIDER,
};

struct ns_entry {
    struct ns_entry *parent;
    struct nsstr name;

    struct ns_object *object;
    struct ns_provider *provider;
    enum ns_binding_source binding_source;
    struct hashtable children;

    struct hashtable_link sibling_link;
    unsigned refcount;
};
```

- `ns_entry`는 부모, 이름과 backing object 사이의 바인딩입니다.
- `ns_entry`는 `object` 참조를 하나 소유합니다.
- 같은 `ns_object`는 여러 `ns_entry`에 동시에 바인딩될 수 있습니다.
- `ns_object`에는 단일 parent나 이름을 넣지 않습니다.
- 이름을 제거해도 다른 binding이나 열린 object-table 참조가 있으면 객체는
  살아 있습니다.
- `binding_source`는 VFS가 직접 설치한 binding/mount와 provider lookup에서
  materialize한 cache entry를 구분합니다.
- `binding_source`는 이 이름 binding의 출처이고, `provider`는 이 entry 자체가
  디렉터리일 때 그 아래 backend를 해석할 provider이므로 서로 다른 정보입니다.
- `entry->provider != NULL`이면 namespace상 디렉터리입니다.
- filesystem directory는 해당 filesystem provider를 가집니다.
- `/dev` 같은 synthetic directory와 순수 memory directory도 memory provider를
  가집니다.
- 빈 디렉터리도 provider가 있으므로 디렉터리 여부를 cached child 개수로
  추론하지 않습니다.
- `provider == NULL`인 entry는 namespace leaf이며 `children`을 사용하지
  않습니다.
- provider lookup의 negative entry는 cache하지 않습니다.
- `children`은 explicit binding과 materialized provider entry를 같은 이름
  hash table에 저장합니다.
- child lookup은 `nsstr_hash()`로 bucket을 고른 뒤 collision chain에서
  `nsstr_equal()`로 이름을 확인합니다.
- parent/child active ref 규칙은 구현 전에 소유권 표로 확정합니다.

`nsstr` hash table은 이름으로 찾는 mapping에만 사용합니다. 정수 index로 직접
접근하는 process object table, 순서를 표현하는 wait/event list와 FAT on-disk
directory scan에는 적용하지 않습니다.

같은 이름의 explicit VFS binding이나 mount는 provider cache entry를 항상
shadow합니다.

- shadow 설치는 기존 entry의 `object`와 `provider`를 제자리에서 바꾸지 않습니다.
- 기존 entry를 parent mapping에서 detach하고 새 explicit entry를 설치합니다.
- 기존 handle이 detach된 entry를 retain하고 있으면 해당 entry와 object는 계속
  살아 있습니다.
- explicit entry를 제거한 뒤에는 provider를 다시 lookup해 backend binding을
  materialize합니다.
- covered binding stack은 두지 않으며, shadow된 entry 아래의 VFS-only topology를
  나중에 복원하지 않습니다.

모든 entry는 backing `ns_object`를 가집니다. `/dev` 같은 순수 synthetic
directory에는 VFS가 최소 core object를 backing으로 연결할 수 있습니다. 이
backing object가 directory interface를 구현할 필요는 없습니다.

### 3.3 `ns_object`: 실제 호출 대상

`ns_object`는 refcount와 object-implemented interface 집합을 가진 실제 커널
객체입니다.

```text
filesystem node
opened file session
HID device
framebuffer console
directory read session
pipe endpoint
event
process
```

위 대상은 모두 필요에 따라 `ns_object`가 될 수 있습니다.

중요한 원칙은 열린 상태도 별도의 generic handle context가 아니라 실제
`ns_object`라는 것입니다.

- regular filesystem node는 `IID_STREAMABLE`을 구현할 수 있습니다.
- `IID_STREAMABLE.OPEN`은 position을 가진 새로운 `IID_STREAM` 객체를 만듭니다.
- 별도 상태가 없는 HID는 namespace에 바인딩된 객체가 직접 `IID_STREAM`을
  구현할 수 있습니다.
- reader별 cursor가 필요한 HID는 open 시 별도의 HID reader/File 객체를 만들
  수 있습니다.
- pipe의 reader와 writer endpoint도 각각 독립된 객체로 표현할 수 있습니다.

별도의 `struct ns_handle` 객체나 모든 객체에 공통인 file position/context를
도입하지 않습니다.

### 3.4 Process object table

사용자에게 보이는 handle은 process object table의 정수 index입니다.

```c
typedef int32_t ns_handle_id_t;

struct ns_object_slot {
    struct ns_object *object;
    struct ns_entry *entry;
};
```

- `object`는 object call의 실제 대상입니다.
- `entry`는 namespace 경로를 통해 얻은 handle에만 설정됩니다.
- 직접 생성된 익명 객체는 `entry == NULL`일 수 있습니다.
- slot은 별도의 heap object나 열린 세션 객체가 아닙니다.
- slot은 mutable file position, mode 또는 interface별 context를 갖지 않습니다.
- `dup()`은 같은 object와 optional entry를 retain한 새 slot을 만듭니다.
- `close()`는 slot이 가진 참조를 해제합니다.
- fork는 동일 object와 위치 문맥을 retain하므로 stateful STREAM 객체의
  position도
  기존 `dup()`과 같은 방식으로 공유됩니다.
- `open_file()`이 STREAMABLE을 새로 열면 새로운 STREAM 객체가 생기므로 별도
  open끼리 position이 독립적입니다.

process는 namespace의 시작점을 root entry로 보유합니다.

```c
struct process {
    struct ns_entry *ns_root;
    struct ns_entry *ns_cwd;
    /* ... */
};
```

- 절대 경로는 `ns_root`에서 시작합니다.
- 상대 경로는 syscall이 지정한 directory entry 또는 `ns_cwd`에서 시작합니다.
- `..`는 parent로 이동하되 해당 process의 `ns_root` 위로 올라가지 않습니다.
- fork는 `ns_root`와 `ns_cwd` 참조를 공유합니다.
- 서로 다른 namespace가 필요하면 서로 다른 entry tree의 root를 보유합니다.
- 서로 다른 entry tree가 같은 backing object와 provider를 공유할 수 있습니다.
- namespace를 나타내는 별도 wrapper 구조나 mount side table을 두지 않습니다.

## 4. Interface 모델

### 4.1 식별자

```c
typedef uint32_t ns_iid_t;
typedef uint32_t ns_mid_t;
```

- `iid`는 interface 계약을 식별합니다.
- `mid`는 해당 interface 안의 method를 식별합니다.
- 모든 IID에서 `mid == 0`은 interface 지원 여부를 묻는 query로 예약합니다.
- 실제 interface method 번호는 1부터 시작합니다.
- 같은 `mid` 값은 서로 다른 interface에서 다른 의미를 가질 수 있습니다.
- method의 buffer 방향, binary layout와 반환값은 해당 IID의 계약입니다.
- 공개한 kernel common IID와 MID 값은 이후 재배치하지 않습니다.

### 4.2 Interface query

별도의 `IID_OBJECT`나 interface 열거 method를 두지 않습니다. 호출자는 자신이
알고 있는 IID를 직접 query합니다.

```text
call(object, iid, 0, NULL, 0)
  -> OPAL_OK: iid 지원
  -> OPAL_ENOTSUPP: iid 미지원
```

- query는 side effect가 없고 user buffer를 읽거나 쓰지 않습니다.
- query 호출은 `buffer == NULL && size == 0`이어야 합니다.
- interface 지원 집합은 object 수명 동안 바뀌지 않습니다.
- interface 지원과 개별 method의 현재 실행 가능 여부는 구분합니다.
  - read-only STREAM도 `IID_STREAM` query에는 성공할 수 있습니다.
  - 같은 STREAM의 WRITE는 mode에 따라 접근 오류를 반환할 수 있습니다.
- object 정보나 metadata가 필요하면 실제 목적을 나타내는 별도 IID로 정의합니다.
- custom IID도 등록 없이 동일한 `mid == 0` query 계약을 따릅니다.

`dup`과 `close` 같은 handle lifecycle은 interface가 아니며 object-table 전용
generic syscall이 처리합니다.

### 4.3 `IID_STREAMABLE`: stream source semantic

`IID_STREAMABLE`은 VFS가 합성하는 interface가 아니라 실제 backing object가
구현하는 stream source 계약입니다.

첫 필수 method는 `OPEN`입니다.

```text
IID_STREAMABLE.OPEN(mode)
  -> retained ns_object supporting IID_STREAM
```

- regular filesystem node는 STREAMABLE을 구현하고 매 open마다 새 STREAM
  객체를 만들 수 있습니다.
- reader별 cursor가 필요한 HID 같은 non-filesystem object도 STREAMABLE을
  구현할 수 있습니다.
- filesystem directory backing object도 자신의 semantic으로 STREAMABLE을
  구현할 수 있습니다.
- STREAMABLE이 반환한 객체가 STREAM을 지원하지 않으면 `open_file()`은 구현
  오류로 처리하고 반환 객체를 release합니다.
- STREAMABLE은 모든 namespace object의 필수 base type이 아닙니다.
- parent, name, child tree, metadata 또는 mount 상태를 STREAMABLE에 넣지
  않습니다.

metadata, resize, truncate와 filesystem-specific node 동작은 실제 필요가 생길
때 별도 IID로 정의합니다. STREAMABLE은 `OPEN -> STREAM` 관계만 표현합니다.

### 4.4 `IID_STREAM`: 열린 I/O 객체

`IID_STREAM`은 이미 열린 I/O 세션을 뜻합니다.

- sequential position과 open mode가 필요하면 STREAM 객체가 직접 보유합니다.
- `READ`는 user buffer의 기존 내용을 읽지 않고 결과를 buffer에 씁니다.
- `WRITE`는 user buffer를 읽고 buffer에 결과를 쓰지 않습니다.
- `SEEK`은 해당 STREAM 객체가 position을 가질 때만 지원합니다.
- object가 stateless I/O를 제공한다면 namespace에 직접 바인딩된 객체가
  STREAM을
  구현할 수 있습니다.
- STREAM을 직접 구현한다는 것은 새 open 시에도 같은 객체 상태를 공유해도
  된다는 뜻입니다.

일반 filesystem file의 개념적인 분리는 다음과 같습니다.

```text
ns_entry
  -> backing object supporting IID_STREAMABLE

open_file()
  -> new stateful object supporting IID_STREAM
     - backing node reference
     - position
     - mode
     - position lock
```

### 4.5 STREAM forwarding object

STREAMABLE의 `OPEN`은 공통 `create_stream_of(source, fops)` helper 또는 이를
embed한 파생 STREAM 객체를 사용합니다.

여기서 `source`는 `OPEN`을 처리한 STREAMABLE object의 `this`입니다.

```c
struct ns_stream {
    struct ns_object object;

    struct ns_object *source;
    const struct ns_stream_ops *fops;
    fs_size_t position;
    enum open_mode mode;
};
```

- 생성된 STREAM object는 `source`를 retain합니다.
- helper는 새 STREAM 자신을 source로 받는 재귀 구성을 거부합니다.
- `iid == IID_STREAM` call은 `fops`로 처리합니다.
- 그 외 모든 IID는 `source` object로 그대로 forwarding합니다.
- forwarding 과정에서 IID 목록을 합성하거나 payload를 변환하지 않습니다.
- `call(stream, IID_STREAM, 0, NULL, 0)`은 wrapper가 성공으로 처리합니다.
- 다른 IID의 `mid == 0` query는 일반 call과 마찬가지로 source에 forwarding합니다.
- 마지막 STREAM 참조가 사라지면 source 참조를 release합니다.
- 별도 open은 새로운 wrapper와 position을 만들고, dup은 같은 wrapper를
  공유합니다.
- STREAMABLE.OPEN이 독자적인 STREAM 구현을 반환하더라도 동일한 non-STREAM
  forwarding 계약을 지켜야 합니다.

개념적인 dispatch:

```text
call(stream, iid, mid, buffer, size)
  -> iid == IID_STREAM?
       yes:
         mid == 0이면 query 성공
         그 외에는 stream.fops call
       no:  call(stream.source, iid, mid, buffer, size)
```

이 forwarding으로 `open_file("/a")`가 반환한 STREAM handle에서도 backing
object가 제공하는 metadata, delete 또는 custom IID를 별도 raw handle 없이
호출할 수 있습니다. namespace binding 자체를 제거하는 동작은 backing object의
delete와 구분하며 별도의 namespace syscall로 수행합니다.

### 4.6 기타 common/custom interface

향후 kernel common interface 후보:

- `IID_EVENT`
- `IID_MEMORY`
- `IID_PROCESS`
- `IID_TASK`
- `IID_CHANNEL`

첫 구현 범위는 `STREAMABLE`, `STREAM`과 pipe 대체에 필요한 최소 interface로
제한합니다.

custom IID registry, 동적 할당과 전역 충돌 해결은 이 계획에서 제외합니다.

- 낮은 범위는 kernel common IID로 예약합니다.
- `NS_IID_CUSTOM_BASE` 이상의 값은 구현체가 직접 해석하는 unmanaged 확장
  공간입니다.
- custom interface의 실제 method도 1부터 시작하고 `mid == 0`은 query로
  예약합니다.
- 호출자와 객체 구현은 ioctl 번호처럼 IID/MID와 payload를 사전에 합의합니다.
- 커널은 custom IID의 이름, 소유자 또는 전역 유일성을 등록하지 않습니다.
- 서로 다른 객체의 동일 custom IID 값은 handle이 대상을 특정하므로 충돌하지
  않습니다.
- 같은 객체 안의 custom IID 충돌은 해당 객체 구현의 책임입니다.

## 5. Syscall ABI

### 5.1 `ns_call`

목표 object call ABI:

```c
intptr_t ns_call(ns_handle_id_t handle, ns_iid_t iid, ns_mid_t mid,
    void *buffer, size_t size);
```

현재 `int 0x80` 진입 ABI에 다음처럼 배치할 수 있습니다.

```text
rdi = handle
rsi = iid
rdx = mid
r10 = buffer
r8  = size
```

공통 경로의 책임:

- process object table에서 slot lookup
- 모든 IID를 `slot->object` 구현으로 dispatch
- method 반환값 전달

`ns_call()`은 namespace 위치나 `slot->entry`를 해석하지 않습니다. object
interface 호출과 경로/binding 조작을 분리하기 위해 VFS 전용 IID나 특수 dispatch를
두지 않습니다.

object dispatch는 IID 구현을 찾은 뒤 `mid == 0`이면 method 구현을 호출하지 않고
지원 여부만 반환합니다. IID를 찾지 못하면 `OPAL_ENOTSUPP`이며, query의
buffer/size 계약이 잘못되면 인자 오류입니다. custom IID를 monolithic callback이
해석하는 객체도 같은 규칙을 구현해야 합니다.

공통 경로는 buffer를 미리 copy-in하거나 call 이후 일괄 copy-out하지 않습니다.

- buffer는 method가 직접 해석하는 user virtual address입니다.
- 접근 방향, user address 검증과 payload 검사는 method 구현이 담당합니다.
- STREAM READ는 write-only 방향으로 user memory에 접근합니다.
- STREAM WRITE는 read-only 방향으로 user memory에 접근합니다.
- request/response가 함께 필요한 method는 자신의 ABI에서 정한 구간만 각각
  읽고 씁니다.
- 큰 I/O를 공통 bounce buffer에 복사하지 않습니다.

현재는 복구 가능한 page fault와 완성된 user-memory helper가 없으므로 초기
구현이 user pointer를 직접 사용할 수 있습니다. 최종 계약은 method가 접근
방향에 맞는 helper를 호출하고 partial fault 결과를 일관되게 반환하는
형태입니다.

개념적인 dispatch:

```text
SYS_NS_CALL
  -> object table slot lookup
  -> slot.object의 iid/mid dispatch
  -> return
```

### 5.2 `open_object`

```c
ns_handle_id_t open_object(
    ns_handle_id_t base, const char *path);
```

- `base`는 상대 경로의 시작 directory handle입니다.
- 절대 경로는 process의 root namespace entry에서 시작합니다.
- 경로를 공통 `ns_entry` tree에서 해석합니다.
- 최종 entry의 raw backing `ns_object`를 retain해 새 object-table slot에
  설치합니다.
- 새 slot은 최종 entry 문맥도 retain합니다.
- 대상이 directory인지, STREAM인지, STREAMABLE인지에 따라 객체를 변환하지
  않습니다.
- pure object access와 명시적인 backing interface call에 사용합니다.

### 5.3 `open_file`

```c
enum file_create_flags : uint32_t {
    FILE_CREATE_NORMAL = 0,
    FILE_CREATE_DIRECTORY = 1 << 0,

    FILE_CREATE_MASK_ALL = FILE_CREATE_DIRECTORY,
};
```

```c
ns_handle_id_t open_file(
    ns_handle_id_t base, const char *path, enum open_mode mode,
    enum file_create_flags create_flags);
```

`open_file()`도 경로 기반 syscall이며 `open_object()`를 먼저 호출하도록 사용자
ABI를 강제하지 않습니다. 두 syscall은 내부 path resolver만 공유합니다.

`open_file()`은 기존 VFS의 open처럼 lookup과 조건부 create를 한 syscall에서
수행합니다.

```text
resolve parent and final name
  -> entry가 있고 OPEN_NONEXIST이면 OPAL_EEXIST
  -> entry가 없고 OPEN_CREATE가 아니면 OPAL_ENOENT
  -> entry가 없고 OPEN_CREATE이면
       parent.provider.ops.create(
           parent.provider, parent.object, name, create_flags)
       반환된 ns_provider_object로 ns_entry 생성
  -> 생성 여부와 관계없이 같은 open_entry_as_file(entry, mode) 수행
```

- `OPEN_CREATE`와 `OPEN_NONEXIST`는 `open_file()`의 일반 open mode입니다.
- `create_flags`는 새 binding을 만들 때의 일반 파일/디렉터리 생성 속성이며
  알려진 비트만 검증한 뒤 parent provider에 그대로 전달합니다.
- VFS core는 `create_flags`를 보고 반환 객체의 open 방식을 미리 결정하지
  않습니다.
- provider는 backing object와 새 entry의 `is_directory`를 반환합니다.
- `is_directory`이면 VFS가 parent entry의 provider를 retain해 새 entry에
  상속합니다.
- provider별 custom create flag ABI는 두지 않습니다.

entry가 준비되면 다음 우선순위를 사용합니다.

1. `entry->provider != NULL`
   - backing object의 STREAM/STREAMABLE 지원 여부보다 먼저 적용
   - VFS가 directory read session 객체를 생성
   - 반환 객체는 STREAM을 구현
2. backing object가 STREAM 지원
   - 같은 object를 retain해 새 slot에 설치
   - 별도 상태가 필요 없는 HID/console 등에 사용
3. backing object가 STREAMABLE 지원
   - kernel-side `IID_STREAMABLE.OPEN(mode)` 호출
   - 반환 객체가 STREAM을 지원하는지 검증 후 설치
4. 모두 미지원
   - `OPAL_ENOTSUPP`

directory를 STREAMABLE보다 먼저 처리해야 mount, bind와 synthetic child가
반영된 VFS directory stream을 얻을 수 있습니다. backing directory inode의
raw semantic이
필요하면 `open_object()`로 handle을 얻은 뒤
`ns_call(..., IID_STREAMABLE, ...)` 또는 다른 backing IID를 명시적으로
사용합니다.

모든 성공 경로의 새 slot은 최종 `ns_entry` 참조를 보존합니다. 따라서 그 handle을
이후 상대 경로 syscall의 base로 사용할 수 있고, STREAM의 non-STREAM IID는
wrapper가 backing source로 forwarding할 수 있습니다.

### 5.4 Namespace mutation syscall

namespace lookup과 binding mutation은 object method가 아닌 경로 기반 syscall로
공개합니다.

```c
enum rename_flags : uint32_t {
    RENAME_NONE = 0,
    RENAME_REPLACE = 1 << 0,

    RENAME_MASK_ALL = RENAME_REPLACE,
};
```

- `open_object(base, path)`
  - lookup 결과의 raw object와 entry 문맥을 반환
- `open_file(base, path, mode, create_flags)`
  - lookup, 조건부 create와 STREAM open을 한 흐름으로 수행
- `link_object(base, path, object_handle)`
  - provider를 호출하지 않고 기존 object를 explicit VFS binding으로 설치
  - 같은 이름의 provider cache나 explicit binding을 항상 shadow
- `unlink_object(base, path)`
  - 이름 binding을 제거하되 열린 object와 다른 binding은 유지
  - explicit binding이면 VFS tree에서만 제거하고 provider binding이면
    `provider.remove` 호출
- `rename_object(old_base, old_path, new_base, new_path, flags)`
  - explicit source는 VFS tree에서만 이동
  - provider source는 같은 provider 인스턴스 안에서 `provider.rename` 호출
  - provider가 다른 directory 사이의 rename은 `OPAL_ENOTSUPP`
  - destination이 있을 때 `RENAME_REPLACE`가 없으면 `OPAL_EEXIST`

각 syscall은 VFS가 경로, 이름 충돌과 entry 문맥을 검증한 뒤 private
`ns_provider` 동작이 필요한 경우에만 호출합니다. namespace syscall은
`ns_call()`로 forwarding되지 않으며 backing object의 interface 목록에도
나타나지 않습니다.

### 5.5 Object-table syscall

다음 동작은 object method가 아닌 generic syscall입니다.

- `dup(old_handle, new_handle)`
  - 같은 object와 optional entry 참조를 새 slot에 복제
- `close(handle)`
  - slot 제거와 보유 참조 release

기존 `read`, `write`, `seek`, `stat`, `ioctl` 같은 subtype syscall은 최종적으로
제거하고 STREAM/STREAMABLE/기타 IID의 `ns_call()`로 전환합니다.

초기 root object handle과 표준 입출력 object handle은 process 생성 시 설치하거나
부모 process에서 상속합니다.

## 6. VFS directory와 backing directory

### 6.1 충돌 분리

한 경로의 directory에는 서로 다른 두 semantic이 공존할 수 있습니다.

```text
VFS directory semantic
  - namespace에서 보이는 이름
  - parent와 ..
  - bind
  - mount/overlay
  - synthetic child

backing directory semantic
  - FAT cluster와 dentry
  - CPIO archive node
  - filesystem metadata
  - on-disk create/remove/rename
```

이를 backing object interface 하나로 합치지 않습니다.

```text
ns_entry "/mnt/data"
  ├── object -> FAT directory backing object, IID_STREAMABLE
  ├── provider -> FAT provider
  └── children -> common ns_entry cache/tree
```

- namespace syscall과 directory STREAM은 `entry->provider`와
  `entry->children`을 사용하는 VFS 공통 구현입니다.
- `IID_STREAMABLE`과 다른 backing IID는 `entry->object`를 사용하는 FAT/CPIO
  object 구현입니다.
- mount나 bind는 backing inode를 변경하지 않고 해당 root의 `ns_entry` tree를
  변경합니다.
- 같은 backing directory가 여러 위치나 여러 root tree에 bind되어도 각각의
  entry parent 문맥에서 `..`와 overlay를 해석할 수 있습니다.

### 6.2 Directory STREAM

`open_file()`이 namespace directory를 만나면 다음과 같은 VFS-owned 객체를
만듭니다.

```c
enum ns_dir_stream_phase {
    NS_DIR_STREAM_EXPLICIT,
    NS_DIR_STREAM_PROVIDER,
    NS_DIR_STREAM_DONE,
};

struct ns_dir_stream {
    struct ns_stream stream;

    struct ns_entry *entry;
    enum ns_dir_stream_phase phase;
    fs_size_t explicit_position;
    uint64_t provider_cookie;
};
```

- `ns_dir_stream`은 STREAM을 구현합니다.
- STREAM READ는 packed dirent를 user buffer에 기록합니다.
- 첫 단계에서 explicit VFS binding/mount 이름을 내보내고, 다음 단계에서 raw
  provider 이름을 내보냅니다.
- provider 이름이 explicit 이름과 충돌하면 provider 항목을 건너뜁니다.
- materialized provider cache entry는 explicit 단계에서 다시 내보내지 않습니다.
- provider dirent는 이름만 제공하며 object나 directory 여부를 materialize하지
  않습니다.
- `explicit_position`은 hash table entry의 주소를 보존하지 않는 숫자형 weak
  cursor입니다.
- explicit 이름의 순서는 hash bucket 순서이며 정렬이나 삽입 순서를 보장하지
  않습니다.
- `provider_cookie`는 provider 내부에서만 해석하는 opaque cursor입니다.
- 한 번의 STREAM READ 동안은 전역 namespace mutex가 mutation을 막습니다.
- READ 호출 사이에 mutation이나 table growth가 일어나면 weak iteration으로
  중복이나 누락이 생길 수 있지만, 해제될 수 있는 entry pointer를 다음 호출까지
  보존하지 않습니다.
- STREAM SEEK는 초기 directory stream에서 지원하지 않습니다.
- STREAM 외 IID는 `stream.source`인 backing object로 forwarding합니다.
- 마지막 STREAM 참조가 사라지면 entry와 backing source 참조를 해제합니다.

readdir 결과는 raw filesystem iteration 결과가 아닙니다.

```text
explicit VFS binding/mount 이름
+ provider가 직접 enumerate한 raw backend 이름
- explicit 이름에 shadow된 provider 이름
= STREAM READ가 반환하는 visible dirent
```

이 계약으로 현재 `path_entry`에만 존재하는 mount point가 readdir에서 빠지는
문제를 제거합니다.

## 7. `ns_provider`

`ns_provider`는 사용자에게 노출되는 object interface가 아니라 VFS가 backing
store와 공통 `ns_entry` tree를 연결하는 비공개 API입니다.

### 7.1 정체성과 수명

provider 인스턴스 하나는 mount된 filesystem volume 또는 memory backend 하나를
나타냅니다. directory별 provider 객체를 만들지 않으며, 각 op에 대상 directory의
backing `ns_object`를 함께 전달합니다.

```c
struct ns_provider_ops;

struct ns_provider {
    const struct ns_provider_ops *ops;
    unsigned refcount;
};

void ns_provider_init(
    struct ns_provider *provider, const struct ns_provider_ops *ops);
void ns_provider_retain(struct ns_provider *provider);
void ns_provider_release(struct ns_provider *provider);
```

- filesystem provider 구현은 이 구조를 volume별 구조체에 embed합니다.
- `ns_provider_init()`, `ns_provider_retain()`, `ns_provider_release()`를
  제공합니다.
- 마지막 release는 필수 `ops->destroy()`를 호출합니다.
- directory `ns_entry`는 자신의 `provider` 참조 하나를 소유합니다.
- provider op은 다른 provider를 반환하지 않습니다. backend child가
  directory이면 VFS가 현재 provider를 retain해 child entry에 상속합니다.
- backing object가 volume 상태를 필요로 하면 object 구현도 필요한 volume
  참조를 독립적으로 소유합니다.

### 7.2 Provider object와 ops

```c
struct ns_provider_object {
    struct ns_object *object;
    bool is_directory;
};

typedef bool (*ns_provider_iterate_cb)(
    const struct nsstr *name,
    uint64_t next_cookie,
    void *ctx);

struct ns_provider_ops {
    void (*destroy)(struct ns_provider *provider);

    kerrno_t (*lookup)(
        struct ns_provider *provider,
        struct ns_object *directory,
        const struct nsstr *name,
        struct ns_provider_object *result);

    kerrno_t (*iterate)(
        struct ns_provider *provider,
        struct ns_object *directory,
        uint64_t start_cookie,
        ns_provider_iterate_cb callback,
        void *ctx,
        bool *exhausted);

    kerrno_t (*create)(
        struct ns_provider *provider,
        struct ns_object *directory,
        const struct nsstr *name,
        enum file_create_flags flags,
        struct ns_provider_object *result);

    kerrno_t (*remove)(
        struct ns_provider *provider,
        struct ns_object *directory,
        const struct nsstr *name);

    kerrno_t (*rename)(
        struct ns_provider *provider,
        struct ns_object *source_directory,
        const struct nsstr *source_name,
        struct ns_object *destination_directory,
        const struct nsstr *destination_name,
        enum rename_flags flags);
};
```

모든 callback은 필수입니다. read-only provider도 `create`, `remove`, `rename`
stub을 제공하고 `OPAL_ENOTSUPP`를 반환합니다.

`lookup`과 `create`의 result 계약:

- 성공하면 `object`는 non-null retained reference입니다.
- `is_directory`는 해당 object를 directory로 삼아 같은 provider에서 하위 이름을
  계속 탐색할 수 있는지를 나타냅니다.
- `is_directory`이면 VFS는 호출에 사용한 provider를 retain해 새 provider-origin
  `ns_entry`에 저장합니다.
- `is_directory`가 아니면 새 entry의 provider는 `NULL`입니다.
- 실패하면 `object == NULL`, `is_directory == false`이며 부분 참조를 남기지
  않습니다.
- provider가 계약을 위반하면 kernel 구현 오류로 처리하고 반환된 object 참조를
  해제합니다.
- provider 전환은 explicit VFS mount가 담당하며 provider op은 다른 provider를
  선택하거나 반환하지 않습니다.

`directory`와 `name`은 호출 동안만 유효한 borrowed 입력입니다. VFS가
`entry->provider`와 `entry->object`의 유효한 조합만 전달하며 provider는 VFS
mount, explicit binding 또는 shadow를 해석하지 않습니다.

### 7.3 Lookup과 create

`lookup`은 해당 backend directory에서 이름 하나를 직접 찾습니다.

- miss는 `OPAL_ENOENT`입니다.
- negative result는 VFS tree에 cache하지 않습니다.
- 같은 backend node의 object가 아직 살아 있으면 provider object cache는 같은
  `ns_object`를 retain해 반환합니다.
- 외부 backend 변경은 초기 계약에서 지원하지 않습니다.

`create`는 `open_file(..., OPEN_CREATE, create_flags)`의 miss 경로에서만
호출합니다.

- `FILE_CREATE_MASK_ALL` 밖의 비트는 syscall 경계에서 `OPAL_EINVAL`입니다.
- provider는 `FILE_CREATE_NORMAL`과 `FILE_CREATE_DIRECTORY`를 해석합니다.
- provider별 create flag는 두지 않습니다.
- 이미 존재하는 backend 이름은 `OPAL_EEXIST`입니다.
- 성공 결과는 기존 entry와 동일한 directory, STREAM, STREAMABLE open
  우선순위를 거칩니다.
- 생성 후 open이 실패하더라도 생성된 backend binding과 `ns_entry`는
  유지합니다.

### 7.4 Directory iteration

`iterate`는 raw backend 이름만 제공하고 object, type 또는 `is_directory`를
반환하지 않습니다.

- `start_cookie == 0`은 iteration 시작입니다.
- cookie는 같은 provider, directory와 STREAM 안에서만 의미가 있는 opaque
  값입니다.
- callback의 `name`은 callback이 반환할 때까지만 유효합니다.
- provider는 빈 이름, `/`, `.`, `..`를 반환하지 않습니다.
- `next_cookie`는 현재 이름 다음 위치에서 재개할 cookie입니다.
- VFS callback은 현재 이름을 user buffer에 기록한 뒤에만 stream의 cookie를
  `next_cookie`로 갱신합니다.
- callback이 `false`를 반환하면 즉시 중단합니다.
- provider가 자연스럽게 끝까지 순회한 경우만 `*exhausted = true`로 설정합니다.
- 한 호출 안에서 이름을 중복 반환하지 않습니다.
- READ 호출 사이의 backend create/remove/rename으로 인한 중복이나 누락은
  허용하는 weak iteration입니다.

FAT dentry index, CPIO offset 같은 cursor는 provider 내부에만 남고 user ABI로
노출하지 않습니다.

### 7.5 Remove와 rename

`remove`는 file과 directory를 함께 처리합니다.

- 이름이 없으면 `OPAL_ENOENT`입니다.
- non-empty directory처럼 제거할 수 없는 대상은 `OPAL_EBUSY`입니다.
- 성공하면 backend 이름 binding을 제거하지만 이미 열린 object 참조는 계속
  유효합니다.

`rename`은 같은 provider 인스턴스에 속한 두 directory 사이에서만 호출합니다.

- provider가 다른 directory 사이의 rename은 VFS가 `OPAL_ENOTSUPP`로
  거부합니다.
- destination이 있고 `RENAME_REPLACE`가 없으면 `OPAL_EEXIST`입니다.
- `RENAME_REPLACE`가 있으면 destination을 원자적으로 교체합니다.
- non-empty directory나 file/directory 충돌처럼 교체할 수 없는 경우 provider가
  오류를 반환합니다.

`create`, `remove`, `rename`은 실패 시 backend를 변경하지 않고 성공 시 전체
변경을 반영해야 합니다.

### 7.6 VFS cache와 shadow

resolver 순서:

```text
parent의 explicit VFS binding/mount
  -> 있으면 사용
  -> 없으면 materialized provider cache
  -> 없으면 provider.lookup
```

- `link_object()`와 mount는 provider를 호출하지 않는 explicit VFS
  binding입니다.
- explicit binding은 같은 이름의 provider cache를 detach하고 항상 shadow합니다.
- explicit binding을 제거하면 다음 lookup이 provider에서 backend 항목을 다시
  materialize합니다.
- provider-origin remove/rename만 provider callback을 호출합니다.
- positive provider cache는 VFS를 통한 `create`, `remove`, `rename`으로만
  갱신됩니다.
- 초기 API에는 negative cache, revalidation, generation 또는 외부 invalidation을
  넣지 않습니다.

VFS는 provider mutation 전에 이름과 새 entry 등 필요한 자원을 모두 준비합니다.
child table insertion이나 destination 이동으로 growth가 필요하면 새 bucket
배열과 rehash도 provider mutation 전에 완료합니다. provider가 성공한 뒤에는
allocation 없는 tree commit만 수행합니다. provider가 실패하면 준비한 자원을
해제하고 기존 tree를 유지합니다.

namespace child table은 power-of-two bucket 수를 사용합니다.

- 첫 insertion 전에 최소 8개 bucket을 준비합니다.
- insertion 결과 load factor가 75%를 넘기 전에 bucket 수를 두 배로 늘립니다.
- 초기 구현은 table을 축소하지 않습니다.
- `rename`은 destination parent의 필요 용량을 먼저 확보합니다.
- shadow overwrite처럼 최종 entry 수가 늘지 않는 연산은 불필요하게 grow하지
  않습니다.

### 7.7 전역 namespace mutex

초기 구현은 per-directory lock 대신 전역 namespace mutex 하나를 사용합니다.

```c
static struct mutex g_ns_mutex;
```

다음 전체 흐름을 mutex 하나로 직렬화합니다.

- explicit binding 우선 lookup과 provider fallback
- provider result materialization
- `open_file()`의 조건부 create
- explicit/provider remove와 rename
- link, mount, shadow와 tree commit
- directory STREAM의 READ 한 번

provider callback은 이 mutex를 잡은 상태에서 block하거나 completion을 기다릴 수
있습니다. provider는 namespace API로 재진입하지 않으며, provider 내부 lock은
전역 namespace mutex 뒤에 획득합니다.

이 저장소의 `irqlock`은 IF 상태를 저장하고 interrupt를 비활성화하며,
`task_wait()`를 통한 대기를 지원합니다. 다만 task가 명시적으로 schedule된 동안
다른 task의 진입을 막는 owner 상태는 제공하지 않습니다. 따라서 전체 namespace
연산의 상호 배제는 `irqlock`만으로 구현하지 않고 owner와 wait list를 가진
`mutex`로 유지합니다. `irqlock`은 provider refcount나 wait-list 전환처럼 짧은
내부 상태 변경에 사용할 수 있습니다.

초기 provider:

- memory provider
  - directory backing object가 보유한 `nsstr` keyed hash table을 in-memory
    binding map으로 사용
  - 공통 `ns_entry` tree에는 다른 provider와 같은 방식으로 결과를 materialize
- CPIO provider
  - archive를 read-only child source로 사용
  - 새 provider로 전환할 때 directory별 child index를 `nsstr` keyed hash
    table로 구성
- FAT provider
  - FAT directory와 object cache를 backing store로 사용
  - on-disk directory scan을 namespace hash table로 강제 변환하지 않음
- 동적 kernel object 집합
  - 초기에는 provider 외부 invalidation 대신 explicit VFS binding으로 갱신
  - backend 자체가 비동기로 바뀌는 dynamic provider는 invalidation 계약과 함께
    후속 설계

## 8. Process root와 mount

mount는 backing object의 정체성을 바꾸는 동작이 아니라 특정 namespace에서
관찰하는 tree 구성을 바꾸는 동작입니다.

namespace의 정체성은 process가 보유한 root `ns_entry`입니다.

- 같은 root entry를 공유하는 process들은 같은 namespace mutation을 관찰합니다.
- 다른 namespace가 필요한 process는 다른 entry tree의 root를 가집니다.
- 두 tree는 서로 다른 entry topology를 가지면서 같은 backing object를 공유할
  수 있습니다.
- mount와 bind 결과는 별도 side table이 아니라 해당 root 아래의 `ns_entry`
  topology에 반영합니다.
- mount와 bind는 explicit VFS binding이며 같은 이름의 provider cache를
  overwrite합니다.
- overwrite된 provider entry는 covered stack으로 보존하지 않고, explicit
  binding 제거 후 provider lookup으로 다시 materialize합니다.
- absolute lookup과 `..`의 상한은 현재 process의 root entry입니다.

첫 구현은 모든 process가 같은 전역 root entry를 공유하도록 제한할 수 있습니다.
나중에 독립적인 mount namespace가 필요하면 전체 entry tree 복제 또는 필요한
경로만 복제하는 copy-on-write topology를 별도 단계로 설계합니다.

다음 금지사항을 유지합니다.

- backing `ns_object`에 parent 저장
- backing filesystem inode에 mount 대상 저장
- object interface 지원 여부만으로 namespace directory 판별
- raw provider iteration을 readdir 결과로 직접 노출
- root와 mount 상태를 다시 별도의 wrapper 객체로 분리

## 9. 구현 단계

### 9.1 계약과 테스트 기반

- 문서 마지막의 별도 `libcoll` hashtable 선행 이슈 완료
- `ns/nsstr.h`와 `ns/nsstr.c` 추가 및 cached hash 계약 테스트
- `ns_object`, `ns_entry`, `ns_provider`, object-table slot의 소유권 표 작성
- `ns_provider_object` 성공/실패 시 object 참조와 `is_directory` 계약 테스트
- common IID 범위와 `IID_STREAMABLE`, `IID_STREAM` 번호 확정
- 모든 common/custom IID의 `mid == 0` query 계약 테스트
- object call과 namespace syscall의 분리 계약 테스트
- `ns_call` buffer 방향, 오류와 partial result 규칙 확정
- open/dup/close에 따른 STREAM object 수명과 position 공유 테스트 설계

### 9.2 Object core와 process table

- `ns_object` retain/release와 object interface dispatch 구현
- process object table을 object + optional entry slot로 구현
- fork, exec와 exit의 table 수명 연결
- generic `dup`과 `close` 전환
- IID lookup과 side-effect 없는 query dispatch 구현

### 9.3 Namespace tree와 경로 syscall

- provider, binding source와 child hash table을 직접 보유하는 공통 `ns_entry`
  구현
- insertion 전 bucket reserve와 allocation-free commit 구현
- 전역 namespace mutex와 provider callback 재진입 금지 계약 구현
- hash 우선 이름 비교를 사용하는 memory provider와 explicit-first component
  lookup 구현
- 절대/상대 경로 및 `.`, `..` 처리
- `open_object`, `link_object`, `unlink_object`, `rename_object` 구현
- `open_file`의 lookup과 조건부 create 흐름 구현
- shadow overwrite, provider 재조회, 복수 binding과 unlink 후 object 생존 테스트

### 9.4 STREAMABLE, STREAM과 open syscall

- `create_stream_of(source, fops)`와 stateful STREAM 기본 계약 구현
- STREAM 외 IID의 source forwarding 구현
- `open_object(base, path)` 구현
- directory, STREAM, STREAMABLE 순서의
  `open_file(base, path, mode, create_flags)` 구현
- explicit 이름 우선, provider cookie 기반 directory STREAM 구현
- `ns_call` 5-argument wrapper와 user pointer direct dispatch 구현
- method별 방향성 있는 user-memory access helper 연결

### 9.5 Kernel object tree

- 전역 root entry와 bootstrap root handle 생성
- `kobjfs` 없이 HID, framebuffer console과 block device를 직접 bind
- stateless 장치는 직접 STREAM 구현
- per-reader 상태가 필요한 장치는 STREAMABLE.OPEN에서 별도 STREAM 객체 생성
- pipe를 namespace와 독립된 endpoint object로 전환

### 9.6 Filesystem 연결

- CPIO node를 backing STREAMABLE object로 전환
- CPIO directory별 child hash index, provider와 VFS entry materialization 연결
- CPIO root를 전역 root entry tree에 연결
- FAT volume provider와 FAT backing inode object 구현
- FAT lookup/iterate/create/remove/rename provider 구현
- provider mutation 실패 시 backend 무변경과 VFS no-fail commit 보장
- filesystem STREAM object의 position과 mode를 session object로 이동

### 9.7 기존 VFS와 subtype syscall 제거

- kernel과 userland 소비자를 object handle과 `ns_call()`로 전환
- `filetable`을 process object table로 교체
- `path_entry`, 기존 공통 `inode`, `file`, `superblock` 계층 제거
- 남은 기존 `hstr` 소비자와 `fs/hstr.h`, `fs/hstr.c` 제거
- `kobjfs`와 VFS 디렉터리 read 특례 제거
- read/write/stat/ioctl 및 기존 path open wrapper 제거
- 정식 filesystem, process와 syscall 문서를 새 계약으로 갱신

## 10. 검증 기준

### 10.1 Object와 open

- stateless HID STREAM은 중간 session 객체 없이 같은 object를 retain해
  열립니다.
- STREAMABLE을 두 번 open하면 서로 다른 STREAM object와 position을 얻습니다.
- 같은 STREAM handle을 dup하면 동일 STREAM object와 position을 공유합니다.
- close와 process exit에서 object가 정확히 한 번 최종 해제됩니다.
- STREAMABLE.OPEN이 STREAM 아닌 객체를 반환하면 이를 설치하지 않고 오류
  처리합니다.
- STREAM call은 wrapper fops가 처리하고 다른 IID는 backing source가 처리합니다.
- STREAM wrapper가 살아 있는 동안 backing source도 살아 있습니다.

### 10.2 Directory semantic

- child가 없더라도 provider가 있는 `ns_entry`는 namespace directory입니다.
- 같은 bucket에 배치되는 서로 다른 `nsstr` 이름을 정확히 구분합니다.
- child table growth와 rehash 뒤에도 모든 explicit/provider entry를 같은
  object와 binding source로 찾습니다.
- non-directory entry나 entry 없는 handle을 directory base로 사용하면 경로
  syscall이 `OPAL_ENOTDIR`을 반환합니다.
- `ns_call()`은 entry의 directory 여부와 관계없이 backing object만 호출합니다.
- filesystem directory가 STREAMABLE과 STREAM을 지원해도 `open_file()`은 VFS
  directory STREAM을 우선합니다.
- `open_object()`로 얻은 directory handle에는 backing STREAMABLE과 다른
  object method를 직접 호출할 수 있습니다.
- 같은 backing directory의 복수 binding과 서로 다른 root tree에서 `..`가
  entry parent와 process root에 따라 해석됩니다.
- explicit binding은 같은 이름의 provider entry를 제자리 변경하지 않고
  detach한 뒤 새 entry로 overwrite합니다.
- detach된 entry를 참조하는 기존 handle은 기존 object와 entry 문맥을 계속
  유지합니다.
- explicit binding을 제거한 다음 lookup은 provider의 raw backend 항목을 다시
  materialize합니다.

### 10.3 Readdir와 provider

- directory STREAM READ는 handle 내부가 아니라 STREAM 객체 내부 position을
  갱신합니다.
- explicit VFS 이름이 먼저 나타나고 같은 이름의 provider 항목은 한 번만
  나타납니다.
- provider iterate는 이름과 next cookie만 반환하며 object와 directory type을
  materialize하지 않습니다.
- callback 중단 후 next cookie에서 재개하고 자연 종료만 exhausted로 판정합니다.
- READ 호출 사이 mutation으로 인한 중복/누락은 허용하되 invalid memory 접근이나
  무한 반복은 발생하지 않습니다.
- explicit 이름 순서는 hash table 순서이며 정렬이나 삽입 순서를 검사하지
  않습니다.
- table growth 뒤에도 directory STREAM cursor는 해제된 entry pointer를
  역참조하지 않습니다.
- raw provider cursor와 backend dentry ID는 user ABI에 노출되지 않습니다.
- unlink 후 이미 열린 directory STREAM과 backing STREAM의 수명 계약이
  유지됩니다.
- provider mutation 실패 시 backend와 공통 tree가 모두 기존 상태를 유지합니다.
- provider 성공 뒤의 tree commit은 추가 allocation 없이 완료됩니다.
- `OPEN_CREATE`로 생성된 `ns_provider_object`에도 기존 entry와 동일한 open
  우선순위가 적용됩니다.
- read-only provider의 필수 create/remove/rename stub은 `OPAL_ENOTSUPP`를
  반환합니다.
- `RENAME_REPLACE` 유무, non-empty directory와 cross-provider rename 오류를
  검증합니다.
- remove, rename과 shadow 뒤에는 제거된 entry가 이름 hash table lookup에 남지
  않습니다.

### 10.4 Namespace 동시성

- 동시에 같은 이름을 create하면 provider create는 한 번만 성공합니다.
- lookup, provider call과 materialization 전체가 전역 namespace mutex 아래에서
  관찰됩니다.
- provider가 disk completion을 기다리는 동안에도 다른 task가 같은 namespace
  연산에 진입하지 않습니다.
- provider completion과 내부 lock 경로는 namespace API로 재진입하지 않습니다.
- directory STREAM READ 사이에는 mutation할 수 있지만 한 READ 안에서는 tree와
  provider iteration이 직렬화됩니다.

### 10.5 Interface와 user buffer

- 지원 IID의 `mid == 0`, null buffer query는 `OPAL_OK`를 반환합니다.
- 미지원 IID query는 `OPAL_ENOTSUPP`를 반환합니다.
- query는 method 구현을 호출하거나 object 상태를 변경하지 않습니다.
- STREAM은 `IID_STREAM` query를 직접 처리하고 다른 IID query를 source로
  forwarding합니다.
- `ns_call`은 namespace 전용 IID나 entry 기반 특수 dispatch를 갖지 않습니다.
- STREAM READ는 write-only 방향으로 user buffer에 접근합니다.
- STREAM WRITE는 read-only 방향으로 user buffer에 접근합니다.
- `ns_call` 공통 경로는 buffer 전체를 bounce copy하지 않습니다.
- malformed size와 method별 invalid payload가 일관된 오류를 반환합니다.

### 10.6 빌드/테스트

각 구현 단계에서 관련 범위에 따라 아래 경로를 순차 실행합니다.

```bash
make CONFIG=debug PLATFORM=pc-x64 NO_ANALYZER=1 >/dev/null
ASAN_OPTIONS=detect_leaks=0 make test CONFIG=debug PLATFORM=pc-x64 >/dev/null
make build UNIT_TEST=1 CONFIG=debug PLATFORM=pc-x64 NO_ANALYZER=1 >/dev/null
make unit-test QEMU_DISPNONE=1 CONFIG=debug PLATFORM=pc-x64 NO_ANALYZER=1
make CONFIG=release PLATFORM=pc-x64 >/dev/null
```

QEMU 유닛테스트는 `==== unit test end ====` 결과를 확인한 뒤 prompt에서 수동
종료합니다. 여러 `make` invocation은 동시에 실행하지 않습니다.

## 11. 주요 위험과 후속 결정

- object slot의 entry 참조와 namespace tree 사이의 cycle
- unlink된 entry를 참조하는 object handle 및 열린 directory STREAM의 `..` 의미
- mount/overlay mutation 중 기존 object slot이 관찰할 entry topology 일관성
- 전역 namespace mutex를 잡은 provider callback의 namespace 재진입
- 느린 provider I/O가 모든 namespace lookup과 directory READ를 막는 coarse
  locking 비용
- weak directory iteration에서 mutation으로 발생하는 중복과 누락
- direct STREAM object가 공유 상태를 갖는다는 계약을 구현자가 놓칠 가능성
- STREAM forwarding 중 source와 wrapper 사이의 재귀 dispatch 또는 ref cycle
- object method 안에서 user pointer access fault가 발생했을 때 partial result
- FAT backing object cache와 provider-origin `ns_entry` cache의 정체성 불일치
- 기존 shell과 userland를 단계적으로 전환할 호환 계층의 수명

구현 전에 세부 계약으로 고정할 항목:

1. namespace syscall의 정확한 인자 ABI와 path 문자열 접근 규칙
2. directory STREAM의 packed dirent layout
3. backing metadata, resize와 delete method의 IID 배치
4. provider object cache의 backend node identity와 eviction 규칙

## 12. 문서화

구현 중 이 문서를 진행 상태와 확정된 계약에 맞게 갱신합니다. 안정화된 내용은
다음 정식 문서에 반영합니다.

- `docs/opal/fs/vfs.md`를 대체할 object namespace 문서
- `docs/opal/fs/cpio.md`
- `docs/opal/fs/fat.md`
- `docs/opal/fs/pipefs.md`
- `docs/opal/task/process.md`
- `docs/opal/syscall.md`
- `docs/testing.md`

새로 확인된 구조적 결함과 의도적으로 남긴 제한은 재현, 원인과 영향을 포함해
`docs/todo.md`에 기록합니다.

## 13. 별도 선행 이슈: `libcoll` hashtable

이 절은 object namespace 자체의 구현 단계가 아니라 별도로 구현하고 검증할
`libcoll` 선행 이슈입니다. namespace 구현은 이 이슈의 API가 안정된 뒤 이를
소비합니다.

### 13.1 범위와 구조

`libcoll`에 allocator-free intrusive separate-chaining hash table을 추가합니다.

```c
struct hashtable_link {
    struct hashtable_link *prev;
    struct hashtable_link *next;
    uint32_t hash;
};

struct hashtable_bucket {
    struct hashtable_link *head;
};

struct hashtable {
    struct hashtable_bucket *buckets;
    size_t bucket_count;
    size_t count;
};

struct hashtable_iter {
    size_t bucket_index;
    struct hashtable_link *next;
};
```

- intrusive link가 cached 32비트 hash를 저장합니다.
- table은 key의 타입이나 수명을 소유하지 않습니다.
- hash가 같은 실제 key의 구분은 호출자가 제공하는 match callback이 담당합니다.
- 같은 key의 중복 방지는 namespace 같은 상위 typed wrapper의 책임입니다.
- bucket 배열의 할당과 해제, 성장 정책은 호출자가 담당합니다.
- `libcoll`은 allocator나 kernel memory API에 의존하지 않습니다.

### 13.2 API

다음 allocator-free API를 제공합니다.

```c
typedef bool (*hashtable_match_fn)(
    const struct hashtable_link *link, const void *key);

void hashtable_init(
    struct hashtable *table,
    struct hashtable_bucket *buckets,
    size_t bucket_count);

struct hashtable_link *hashtable_find(
    struct hashtable *table,
    uint32_t hash,
    hashtable_match_fn match,
    const void *key);

void hashtable_insert(
    struct hashtable *table,
    struct hashtable_link *link,
    uint32_t hash);

void hashtable_remove(
    struct hashtable *table,
    struct hashtable_link *link);

void hashtable_rehash(
    struct hashtable *table,
    struct hashtable_bucket *new_buckets,
    size_t new_bucket_count);

void hashtable_iter_init(
    struct hashtable *table,
    struct hashtable_iter *iter);

struct hashtable_link *hashtable_iter_next(
    struct hashtable *table,
    struct hashtable_iter *iter);
```

- `hashtable_init()`은 호출자가 제공한 bucket 배열을 빈 상태로 초기화합니다.
- `bucket_count`는 0이거나 power of two여야 합니다.
- 빈 table은 find와 iteration만 허용하며 insertion 전에 호출자가 non-empty
  bucket 배열로 rehash해야 합니다.
- bucket index는 `hash & (bucket_count - 1)`로 계산합니다.
- insert와 remove는 allocation하지 않으며 count를 갱신합니다.
- rehash는 새 bucket 배열을 초기화하고 모든 기존 link를 cached hash에 따라
  옮긴 뒤 table이 새 배열을 가리키게 합니다.
- 호출자는 rehash가 끝난 뒤 이전 bucket 배열을 해제할 수 있습니다.
- iteration API는 모든 bucket과 collision chain을 각각 한 번 방문하되 순서를
  보장하지 않습니다.
- iteration에서 얻은 link pointer와 iterator 상태는 table mutation 전까지만
  유효합니다. namespace의 호출 간 weak cursor는 이 pointer를 저장하지 않습니다.

### 13.3 별도 검증

`libcoll` hosted test에서 다음을 독립적으로 검증합니다.

- bucket이 없는 빈 table의 find와 iteration
- 단일 및 복수 insertion과 count
- 같은 bucket의 서로 다른 hash와 같은 hash의 서로 다른 key lookup
- collision chain의 head, middle, tail 제거
- rehash 전후 모든 link, cached hash와 count 보존
- 각 link를 정확히 한 번 방문하는 unordered iteration
- remove한 link의 재삽입과 table 전체 비우기
