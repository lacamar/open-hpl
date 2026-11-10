use super::*;

#[test]
fn steam_root_cuts_after_common() {
    assert_eq!(
        steam_root(b"/home/u/.local/share/Steam/steamapps/common/Game/bin").unwrap(),
        b"/home/u/.local/share/Steam/steamapps/common"
    );
    assert_eq!(
        steam_root(b"/lib/steamapps/common").unwrap(),
        b"/lib/steamapps/common"
    );
    assert!(steam_root(b"/lib/steamapps/commonx").is_none());
    assert!(steam_root(b"").is_none());
}

#[test]
fn vdf_lines() {
    assert_eq!(
        vdf_path(b"\t\t\"path\"\t\t\"/mnt/games/SteamLibrary\"").unwrap(),
        b"/mnt/games/SteamLibrary"
    );
    assert_eq!(vdf_path(b"\"path\"\"\"").unwrap(), b"");
    assert!(vdf_path(b"\t\"label\"\t\"\"").is_none());
    assert!(vdf_path(b"\"path\" \"unterminated").is_none());
    assert!(vdf_path(b"\"path\" no quotes").is_none());
}

#[test]
fn steam_install_paths() {
    assert!(looks_like_steam_install(
        b"/a/steamapps/common/G/hpl.log",
        None
    ));
    assert!(looks_like_steam_install(b"C:\\steamapps\\common\\G", None));
    assert!(!looks_like_steam_install(b"hpl.log", Some(b"/home/u")));
    assert!(looks_like_steam_install(
        b"hpl.log",
        Some(b"/s/steamapps/common/G")
    ));
    assert!(!looks_like_steam_install(b"hpl.log", None));
    assert!(!looks_like_steam_install(
        b"",
        Some(b"/s/steamapps/common/G")
    ));
}

#[test]
fn remount_keeps_locked_flags() {
    assert_eq!(remount_flags(0), MS_BIND | MS_REMOUNT | MS_RDONLY);
    let all = ST_NOSUID | ST_NODEV | ST_NOEXEC | ST_NOATIME | ST_NODIRATIME | ST_RELATIME;
    assert_eq!(
        remount_flags(all | 1),
        MS_BIND
            | MS_REMOUNT
            | MS_RDONLY
            | MS_NOSUID
            | MS_NODEV
            | MS_NOEXEC
            | MS_NOATIME
            | MS_NODIRATIME
            | MS_RELATIME
    );
}

#[test]
fn args() {
    assert_eq!(classify_arg(b"-cwd", true), Arg::Cwd);
    assert_eq!(classify_arg(b"-psn_0_1", false), Arg::Skip);
    assert_eq!(classify_arg(b"game", false), Arg::GameDirCandidate);
    assert_eq!(classify_arg(b"", false), Arg::GameDirCandidate);
    assert_eq!(classify_arg(b"game", true), Arg::Command);
    assert_eq!(classify_arg(b"-x", false), Arg::Command);
}

#[test]
fn env_plan() {
    let e = |headless, alsoft_set, video, videodriver_set, display_set| {
        engine_env(&EnvInputs {
            headless,
            alsoft_set,
            video,
            videodriver_set,
            display_set,
        })
    };
    assert_eq!(e(false, false, true, false, true), ENV_DISABLE_SCREENSAVER);
    assert_eq!(
        e(true, false, true, false, true),
        ENV_ALSOFT_NULL | ENV_VIDEODRIVER_X11
    );
    assert_eq!(e(true, true, true, true, true), 0);
    assert_eq!(e(true, false, false, false, true), ENV_ALSOFT_NULL);
    assert_eq!(e(false, false, false, false, false), 0);
    assert!(
        renderer_is_vulkan(b"VulKan")
            && !renderer_is_vulkan(b"vulkan ")
            && !renderer_is_vulkan(b"")
    );
}

#[test]
fn script_text() {
    assert_eq!(
        script_message(b"s.hps", 3, 4, SCRIPT_MSG_WARNING, b"oops"),
        b"s.hps (3, 4) : WARN : oops\n"
    );
    assert_eq!(
        script_message(b"", -1, 0, SCRIPT_MSG_ERROR, b"").as_slice(),
        b" (-1, 0) : ERR  : \n"
    );
    assert_eq!(
        script_message(b"a", 1, 1, SCRIPT_MSG_INFO, b"m"),
        b"a (1, 1) : INFO : m\n"
    );
    let long = vec![b'x'; 5000];
    assert_eq!(script_message(&long, 1, 1, 0, &long).len(), 10_000 + 18);
    let lens = |n: usize| {
        script_chunks(&vec![b'a'; n])
            .iter()
            .map(|c| c.len())
            .collect::<Vec<_>>()
    };
    assert_eq!(lens(0), [0]);
    assert_eq!(lens(500), [500]);
    assert_eq!(lens(501), [500, 1]);
    assert_eq!(lens(1000), [500, 500]);
    assert_eq!(lens(1001), [500, 500, 1]);
}
