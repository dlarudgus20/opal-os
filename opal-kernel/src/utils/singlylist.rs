use core::ptr::NonNull;

#[derive(Debug)]
pub struct Node {
    next: Option<NonNull<Node>>,
}

impl Node {
    pub const fn new() -> Self {
        Self { next: None }
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
}

impl List {
    pub const fn new() -> Self {
        Self { head: None }
    }

    pub fn is_empty(&self) -> bool {
        self.head.is_none()
    }

    pub fn head(&self) -> Option<NonNull<Node>> {
        self.head
    }

    pub fn push_front(&mut self, link: &mut Node) {
        link.next = self.head;
        self.head = Some(NonNull::from(link));
    }

    pub fn pop_front(&mut self) -> Option<NonNull<Node>> {
        let mut removed = self.head?;
        let next = unsafe { removed.as_ref().next };
        self.head = next;
        unsafe {
            removed.as_mut().next = None;
        }
        Some(removed)
    }

    /// # Safety
    /// `after` must point to a live link in this list, and `link` must not be linked.
    pub unsafe fn insert_after(&mut self, mut after: NonNull<Node>, link: &mut Node) {
        let after_ref = unsafe { after.as_mut() };
        link.next = after_ref.next;
        after_ref.next = Some(NonNull::from(link));
    }

    /// Removes the link after `before`, or the head when `before` is `None`.
    ///
    /// # Safety
    /// When present, `before` must point to a live link in this list.
    pub unsafe fn remove_after(&mut self, before: Option<NonNull<Node>>) -> Option<NonNull<Node>> {
        match before {
            Some(mut before) => {
                let before_ref = unsafe { before.as_mut() };
                let mut removed = before_ref.next?;
                before_ref.next = unsafe { removed.as_ref().next };
                unsafe {
                    removed.as_mut().next = None;
                }
                Some(removed)
            }
            None => self.pop_front(),
        }
    }

    pub fn iter(&self) -> Iter {
        Iter { next: self.head }
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

#[cfg(opal_ktest)]
mod tests {
    use super::{Node, List};
    use opal_ktest::ktest;

    #[ktest]
    fn singlylist_push_pop_front() {
        let mut list = List::new();
        let mut link1 = Node::new();
        let mut link2 = Node::new();

        assert!(list.is_empty());
        list.push_front(&mut link1);
        list.push_front(&mut link2);

        assert_eq!(list.pop_front(), Some((&raw mut link2).into()));
        assert_eq!(list.pop_front(), Some((&raw mut link1).into()));
        assert_eq!(list.pop_front(), None);
        assert!(list.is_empty());
    }

    #[ktest]
    fn singlylist_insert_and_remove_after() {
        let mut list = List::new();
        let mut link1 = Node::new();
        let mut link2 = Node::new();
        let mut link3 = Node::new();

        list.push_front(&mut link1);
        unsafe {
            list.insert_after((&raw mut link1).into(), &mut link3);
            list.insert_after((&raw mut link1).into(), &mut link2);
        }

        let mut iter = list.iter();
        assert_eq!(iter.next(), Some((&raw mut link1).into()));
        assert_eq!(iter.next(), Some((&raw mut link2).into()));
        assert_eq!(iter.next(), Some((&raw mut link3).into()));
        assert_eq!(iter.next(), None);

        assert_eq!(
            unsafe { list.remove_after(Some((&raw mut link1).into())) },
            Some((&raw mut link2).into())
        );
        assert_eq!(unsafe { list.remove_after(None) }, Some((&raw mut link1).into()));
        assert_eq!(list.pop_front(), Some((&raw mut link3).into()));
    }
}
