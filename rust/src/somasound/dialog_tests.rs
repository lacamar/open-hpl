use super::*;

fn branch(name: &[u8], next: &[u8]) {
    building().branches.push(Branch {
        name: name.to_vec(),
        next: next.to_vec(),
        ..Branch::default()
    });
}

fn pause(t: f32) {
    add_item(Item {
        subject: Vec::new(),
        pause: t,
        callback: Vec::new(),
        events: Vec::new(),
    });
}

fn event(kind: i32, val: i32, var: &[u8], new_branch: &[u8]) {
    add_event(Event {
        kind,
        val,
        var: var.to_vec(),
        new_branch: new_branch.to_vec(),
        only_end: true,
    });
}

fn run_out() {
    for _ in 0..1000 {
        if d().active.is_empty() {
            return;
        }
        update(1.0 / 60.0);
    }
}

/// The dialogs of the scenario files and the `DI`/`DF`/`DO` lines the C++ handler wrote for them in
/// the real run (`OHPL_S7_DUMP`), here without a voice handler or map script.
#[test]
fn scenario_matches_cpp_dump() {
    begin(b"t1");
    branch(b"start", b"second");
    pause(0.2);
    event(EVENT_ADD, 2, b"y", b"");
    event(EVENT_LESS, 7, b"y", b"second");
    pause(0.1);
    branch(b"second", b"");
    pause(0.2);
    end(b"");
    run_out();
    assert_eq!(d().vars.get(b"y".as_slice()), Some(&2));
    for name in [b"t2", b"t3"] {
        begin(name);
        branch(b"a", b"");
        pause(5.0);
        end(b"");
        if name == b"t2" {
            for x in d().active.iter_mut().filter(|x| x.name == name) {
                x.done = true;
            }
        } else {
            stop_all();
        }
        update(1.0 / 60.0);
        assert!(d().active.is_empty());
    }
    begin(b"t4");
    branch(b"a", b"");
    pause(0.05);
    add_option(Option_ {
        entry: b"Yes".to_vec(),
        branch: Vec::new(),
        id: 7,
        callback: Vec::new(),
    });
    end(b"");
    run_out();
    assert_eq!(
        dump::captured(),
        include_str!("../../tests/golden/S7/dialog-scenario.txt")
    );
}
