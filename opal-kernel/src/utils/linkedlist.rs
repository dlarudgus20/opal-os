use core::ptr::NonNull;

#[derive(Debug)]
pub struct Node {
    prev: Option<NonNull<Node>>,
    next: Option<NonNull<Node>>,
}

impl Node {
    pub const fn new() -> Self {
        Self {
            prev: None,
            next: None,
        }
    }

    pub fn prev(&self) -> Option<NonNull<Node>> {
        self.prev
    }

    pub fn next(&self) -> Option<NonNull<Node>> {
        self.next
    }
}

impl Default for Node {
    fn default() -> Self {
        Self::new()
    }
}

#[derive(Debug)]
pub struct List {
    head: Option<NonNull<Node>>,
    tail: Option<NonNull<Node>>,
}

impl List {
    pub const fn new() -> Self {
        Self {
            head: None,
            tail: None,
        }
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    pub fn head(&self) -> Option<NonNull<Node>> {
        self.head
    }

    pub fn tail(&self) -> Option<NonNull<Node>> {
        self.tail
    }

    pub fn push_front(&mut self, link: &mut Node) {
        link.prev = None;
        link.next = self.head;

        let ptr = NonNull::from(link);
        match self.head {
            Some(mut head) => unsafe {
                head.as_mut().prev = Some(ptr);
            },
            None => {
                self.tail = Some(ptr);
            }
        }
        self.head = Some(ptr);
    }

    pub fn push_back(&mut self, link: &mut Node) {
        link.prev = self.tail;
        link.next = None;

        let ptr = NonNull::from(link);
        match self.tail {
            Some(mut tail) => unsafe {
                tail.as_mut().next = Some(ptr);
            },
            None => {
                self.head = Some(ptr);
            }
        }
        self.tail = Some(ptr);
    }

    pub fn pop_front(&mut self) -> Option<NonNull<Node>> {
        let head = self.head?;
        unsafe {
            self.remove(head);
        }
        Some(head)
    }

    pub fn pop_back(&mut self) -> Option<NonNull<Node>> {
        let tail = self.tail?;
        unsafe {
            self.remove(tail);
        }
        Some(tail)
    }

    /// # Safety
    /// `at` must point to a live link in this list, and `link` must not be linked.
    pub unsafe fn insert_before(&mut self, at: NonNull<Node>, link: &mut Node) {
        if self.head == Some(at) {
            self.push_front(link);
            return;
        }

        let mut at = at;
        let at_ref = unsafe { at.as_mut() };
        let mut prev = at_ref.prev.expect("linkedlist: non-head link without prev");

        link.prev = Some(prev);
        link.next = Some(at);
        let link_ptr = NonNull::from(link);

        unsafe {
            prev.as_mut().next = Some(link_ptr);
        }
        at_ref.prev = Some(link_ptr);
    }

    /// # Safety
    /// `at` must point to a live link in this list, and `link` must not be linked.
    pub unsafe fn insert_after(&mut self, at: NonNull<Node>, link: &mut Node) {
        if self.tail == Some(at) {
            self.push_back(link);
            return;
        }

        let mut at = at;
        let at_ref = unsafe { at.as_mut() };
        let mut next = at_ref.next.expect("linkedlist: non-tail link without next");

        link.prev = Some(at);
        link.next = Some(next);
        let link_ptr = NonNull::from(link);

        unsafe {
            next.as_mut().prev = Some(link_ptr);
        }
        at_ref.next = Some(link_ptr);
    }

    /// # Safety
    /// `link` must point to a live link in this list.
    pub unsafe fn remove(&mut self, mut link: NonNull<Node>) {
        let link_ref = unsafe { link.as_mut() };

        match link_ref.prev {
            Some(mut prev) => unsafe {
                prev.as_mut().next = link_ref.next;
            },
            None => {
                self.head = link_ref.next;
            }
        }

        match link_ref.next {
            Some(mut next) => unsafe {
                next.as_mut().prev = link_ref.prev;
            },
            None => {
                self.tail = link_ref.prev;
            }
        }

        link_ref.prev = None;
        link_ref.next = None;
    }

    pub fn iter(&self) -> Iter {
        Iter { next: self.head }
    }

    pub fn iter_rev(&self) -> IterRev {
        IterRev { prev: self.tail }
    }
}

impl Default for List {
    fn default() -> Self {
        Self::new()
    }
}

pub struct Iter {
    next: Option<NonNull<Node>>,
}

impl Iterator for Iter {
    type Item = NonNull<Node>;

    fn next(&mut self) -> Option<Self::Item> {
        let current = self.next?;
        self.next = unsafe { current.as_ref().next };
        Some(current)
    }
}

pub struct IterRev {
    prev: Option<NonNull<Node>>,
}

impl Iterator for IterRev {
    type Item = NonNull<Node>;

    fn next(&mut self) -> Option<Self::Item> {
        let current = self.prev?;
        self.prev = unsafe { current.as_ref().prev };
        Some(current)
    }
}

#[cfg(opal_ktest)]
mod tests {
    use super::{Node, List};
    use opal_ktest::ktest;

    #[ktest]
    fn linkedlist_push_pop_front_back() {
        let mut list = List::new();
        let mut link1 = Node::new();
        let mut link2 = Node::new();
        let mut link3 = Node::new();

        assert!(list.is_empty());
        list.push_back(&mut link1);
        list.push_back(&mut link2);
        list.push_front(&mut link3);

        assert_eq!(list.head(), Some((&raw mut link3).into()));
        assert_eq!(list.tail(), Some((&raw mut link2).into()));
        assert_eq!(list.pop_front(), Some((&raw mut link3).into()));
        assert_eq!(list.pop_back(), Some((&raw mut link2).into()));
        assert_eq!(list.pop_back(), Some((&raw mut link1).into()));
        assert_eq!(list.pop_front(), None);
        assert!(list.is_empty());
    }

    #[ktest]
    fn linkedlist_insert_remove_and_iterate() {
        let mut list = List::new();
        let mut link1 = Node::new();
        let mut link2 = Node::new();
        let mut link3 = Node::new();

        list.push_back(&mut link1);
        unsafe {
            list.insert_after((&raw mut link1).into(), &mut link3);
            list.insert_before((&raw mut link3).into(), &mut link2);
        }

        let mut iter = list.iter();
        assert_eq!(iter.next(), Some((&raw mut link1).into()));
        assert_eq!(iter.next(), Some((&raw mut link2).into()));
        assert_eq!(iter.next(), Some((&raw mut link3).into()));
        assert_eq!(iter.next(), None);

        let mut rev = list.iter_rev();
        assert_eq!(rev.next(), Some((&raw mut link3).into()));
        assert_eq!(rev.next(), Some((&raw mut link2).into()));
        assert_eq!(rev.next(), Some((&raw mut link1).into()));
        assert_eq!(rev.next(), None);

        unsafe {
            list.remove((&raw mut link2).into());
        }

        assert_eq!(list.pop_front(), Some((&raw mut link1).into()));
        assert_eq!(list.pop_front(), Some((&raw mut link3).into()));
        assert!(list.is_empty());
    }
}
