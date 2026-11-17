//! What the add-on logic needs from the engine. The real implementation is the shim; tests use a mock.

/// An engine pointer (type info, function, context, object) as an integer; 0 is null.
pub type P = usize;

pub const TYPEID_VOID: i32 = 0;
pub const TYPEID_BOOL: i32 = 1;
pub const TYPEID_INT8: i32 = 2;
pub const TYPEID_INT16: i32 = 3;
pub const TYPEID_INT32: i32 = 4;
pub const TYPEID_INT64: i32 = 5;
pub const TYPEID_UINT8: i32 = 6;
pub const TYPEID_UINT16: i32 = 7;
pub const TYPEID_UINT32: i32 = 8;
pub const TYPEID_UINT64: i32 = 9;
pub const TYPEID_FLOAT: i32 = 10;
pub const TYPEID_DOUBLE: i32 = 11;
pub const TYPEID_OBJHANDLE: i32 = 0x4000_0000;
pub const TYPEID_HANDLETOCONST: i32 = 0x2000_0000;
pub const TYPEID_MASK_OBJECT: i32 = 0x1C00_0000;
pub const TYPEID_MASK_SEQNBR: i32 = 0x03FF_FFFF;

pub const OBJ_REF: u64 = 1;
pub const OBJ_VALUE: u64 = 1 << 1;
pub const OBJ_GC: u64 = 1 << 2;
pub const OBJ_POD: u64 = 1 << 3;
pub const OBJ_ASHANDLE: u64 = 1 << 7;
pub const OBJ_SCRIPT_OBJECT: u64 = 1 << 21;
pub const OBJ_NOINHERIT: u64 = 1 << 23;

pub const TM_INREF: u32 = 1;
pub const TM_CONST: u32 = 4;

pub const EP_DISALLOW_VALUE_ASSIGN_FOR_REF_TYPE: i32 = 20;
pub const MULTIPLE_FUNCTIONS: i32 = -14;
pub const NO_FUNCTION: i32 = -6;

/// A context borrowed for comparison callbacks: the active one pushed, or a fresh one.
#[derive(Clone, Copy)]
pub struct CmpCtx {
    pub ctx: P,
    pub nested: bool,
}

/// A method of the element type with what `Precache` inspects.
pub struct FuncInfo {
    pub f: P,
    pub name: Vec<u8>,
    pub ret_id: i32,
    pub ret_flags: u32,
    pub params: u32,
    pub read_only: bool,
    pub p0_id: i32,
    pub p0_flags: u32,
}

pub trait Host {
    fn active_ctx(&self) -> P;
    /// `SetException` on the active context, if any.
    fn throw(&self, msg: &str);
    fn lock(&self, acquire: bool);

    fn type_engine(&self, ty: P) -> P;
    fn type_flags(&self, ty: P) -> u64;
    fn type_id(&self, ty: P) -> i32;
    fn type_name(&self, ty: P) -> String;
    fn type_size(&self, ty: P) -> usize;
    fn sub_type(&self, ty: P) -> P;
    fn sub_type_id(&self, ty: P) -> i32;
    fn type_by_id(&self, engine: P, id: i32) -> P;
    fn type_addref(&self, ty: P);
    fn type_release(&self, ty: P);
    fn type_user(&self, ty: P) -> P;
    fn set_type_user(&self, ty: P, p: P);
    fn methods(&self, ty: P) -> Vec<FuncInfo>;
    fn has_default_ctor(&self, ty: P) -> bool;
    fn has_default_factory(&self, ty: P) -> bool;

    fn engine_prop(&self, engine: P, prop: i32) -> i32;
    fn message(&self, engine: P, text: &str);
    fn type_decl(&self, engine: P, id: i32) -> String;

    fn create_object(&self, engine: P, ty: P) -> P;
    fn assign_object(&self, engine: P, dst: P, src: P, ty: P);
    fn addref_object(&self, engine: P, obj: P, ty: P);
    fn release_object(&self, engine: P, obj: P, ty: P);
    fn notify_gc(&self, engine: P, obj: P, ty: P);
    fn gc_enum(&self, engine: P, obj: P);
    fn gc_forward(&self, engine: P, obj: P, ty: P);
    /// Assigns through `opHndlAssign`; false when the type has none.
    fn hndl_assign(&self, engine: P, ty: P, dst: P, src: P) -> bool;

    fn begin_ctx(&self, engine: P, pooled: bool) -> CmpCtx;
    fn end_ctx(&self, engine: P, c: CmpCtx, pooled: bool);
    /// `opEquals`: finished and returned true.
    fn call_eq(&self, c: CmpCtx, f: P, obj: P, arg: P) -> bool;
    /// `opCmp`: the result when the call finished.
    fn call_cmp(&self, c: CmpCtx, f: P, obj: P, arg: P) -> Option<i32>;
    /// Sort callback `less(a, b)` with both arguments by address: the result when the call finished.
    fn call_less(&self, c: CmpCtx, f: P, a: P, b: P) -> Option<bool>;
}
