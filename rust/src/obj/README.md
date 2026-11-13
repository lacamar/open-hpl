# Object ownership (Rust endgame)

Plan: endgame-plan.md section 3. Everything here is for `OHPL_IMPL=rust`; the C++ default path is unchanged.

## Pieces

- `Arena<T>` / `Handle<T>`: generational slots; a stale handle never resolves. `to_bits`/`from_bits` carry a handle through C (0 = null).
- `List<T>`: `std::list` over an arena (push_back, erase at a position, stable cursors). A cursor on an erased element resolves to nothing.
- `Foreign`: per-kind vtable of C++ thunks (`name`, `id`, `drop`) for virtuals of classes that still have live C++ subclasses.

## Shell + handle (class still has C++ subclasses)

C++ keeps the class (vtable, constructors, fields it still reads). Rust owns the container, order and lifecycle:

1. C++ constructs the object (`hplNew`), then pushes its address into the Rust list (`ohpl_h13_reg_add`). The list value is the address the C++ container would hold (the static type's pointer, e.g. `iLight*`).
2. At setup C++ registers the kind's thunks once (`ohpl_h13_reg_set_foreign`; `OhplH13Foreign<T>` in `RustSceneH13.h` derives them).
3. Destroy and destroy-all run in Rust (`ohpl_h13_reg_destroy[_all]`): unlink, then call the `drop` thunk. No `&mut` to the registry is held across the thunk, because the C++ destructor may call back into the world.
4. Iteration: C++ iterator types gain a cursor mode (`cSTLIterator(reg, kind)`, `OhplH13Walk`); the cursor is a handle in a `u64`.
5. Delete the `Foreign` entry when the last C++ subclass of the family is ported.

## Opaque (class final, or hierarchy fully flipped)

C++ sees an incomplete type with inline forwarders `ohpl_<class>_<method>`; Rust owns a box-pinned arena slot and the C++ pointer is the slot's address or handle bits. No thunks.

## Worked example: `cWorld` entity lists

- Rust: `scene/h13_world.rs` (`Reg`: one `List<usize>` per kind, `EntityRef` enum of per-kind handles), `scene/ffi_h13.rs` (C ABI).
- C++: `World.cpp` calls `OhplH13Push/Destroy/Erase/DeleteAll/Name/Id/Index/Walk` from `RustSceneH13.h`; each falls back to the `std::list` member when the registry is null (cpp path).
- Public iterators (`GetLightIterator`, `GetDynamicMeshEntityIterator`, ...) return `cSTLIterator` over a Rust cursor.
- Test: `scene/tests_h13.rs` (golden registry replay, cursor walk with erase/append, destroy order via `Foreign`).
