// the routines and variables of the IMP72 sources, for Rust (written by exports)

#[allow(non_snake_case, non_upper_case_globals)]
extern "C" {
    pub static mut FOO: i64;
    pub static mut TAB: [i64; 8];
    pub fn ADD2(A: *mut i64, B: *mut i64) -> i64;
    pub fn NOARGS() -> i64;
    pub fn GETTAB(I: *mut i64) -> i64;
    pub fn FOOPLUS(N: *mut i64, type_: *mut i64) -> i64;
    pub fn Mixed(X: *mut i64) -> i64;
}
