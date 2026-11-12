RunSomaScriptCheck reports (compile every game .hps against the recovered API), one per game.
Recorded with OHPL_IMPL=cpp; the OHPL_IMPL=rust report is byte-identical (cmp).
Recording: OPENHPL_SOMA_GAME_DIR=<scratch> OPENHPL_SOMA_SCRIPT_CHECK=<out.json> OPENHPL_SOMA_SCRIPT_API=<game>/data/script_api.txt Soma.bin.aarch64
Expected summary: soma 190/190, rebirth 232/232, bunker 209/209, api_errors 0.
