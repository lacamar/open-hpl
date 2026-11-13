#!/usr/bin/env python3
"""Derives the init/exit step order of cEngine (GameInit, ~cEngine) from the C++ source text.
usage: gen_steps.py <Engine.cpp> > steps.txt   (steps.txt is what h22_tests::init_exit_steps_match_source_order replays)"""
import re
import sys

src = open(sys.argv[1]).read()


def body(sig):
    i = src.index(sig)
    j = src.index("{", i)
    depth, k = 0, j
    while True:
        if src[k] == "{":
            depth += 1
        elif src[k] == "}":
            depth -= 1
            if depth == 0:
                return src[j + 1 : k]
        k += 1


MODS = {"Input": "Input", "Physics": "Physics", "Scene": "Scene", "Graphics": "Graphics", "Sound": "Sound", "AI": "Ai", "Gui": "Gui",
        "Resources": "Resources", "System": "System"}
RULE = '"--------------------------------------------------------\\n"'
RULE2 = '"--------------------------------------------------------\\n\\n"'

PATTERNS = [
    (r'Log\("Creating Engine Modules\\n"\)', "LogCreatingModules"),
    (r"Log\(%s\)" % re.escape(RULE2), "LogRuleBlank"),
    (r"Log\(%s\)" % re.escape(RULE), "LogRule"),
    (r"mpGraphics = mpGameSetup->CreateGraphics", "CreateGraphics"),
    (r"mpSystem = mpGameSetup->CreateSystem", "CreateSystem"),
    (r"mpResources = mpGameSetup->CreateResources", "CreateResources"),
    (r"mpInput = mpGameSetup->CreateInput", "CreateInput"),
    (r"mpSound = mpGameSetup->CreateSound", "CreateSound"),
    (r"mpPhysics = mpGameSetup->CreatePhysics", "CreatePhysics"),
    (r"mpAI = mpGameSetup->CreateAI", "CreateAi"),
    (r"mpGui = hplNew\(cGui", "CreateGui"),
    (r"mpScene = mpGameSetup->CreateScene", "CreateScene"),
    (r"mpResources->Init\(", "InitResources"),
    (r"mpGraphics->Init\(", "InitGraphics"),
    (r"mpSound->Init\(", "InitSound"),
    (r"mpPhysics->Init\(", "InitPhysics"),
    (r"mpAI->Init\(", "InitAi"),
    (r"mpGui->Init\(", "InitGui"),
    (r'Log\("Initializing Game Module\\n"\)', "LogInitGame"),
    (r'Log\(" Adding engine updates\\n"\)', "LogAddingUpdates"),
    (r"mpUpdater = hplNew\( cUpdater", "CreateUpdater"),
    (r"mpUpdater->AddGlobalUpdate\(mp(\w+)\)", lambda m: "AddGlobalUpdate(%s)" % MODS[m.group(1)]),
    (r'mpUpdater->AddContainer\("Default"\)', "DefaultContainer"),
    (r"mpLogicTimer = mpSystem->CreateLogicTimer", "CreateLogicTimer"),
    (r'Log\(" Initializing script functions\\n"\)', "LogScriptFuncs"),
    (r"cScriptFuncs::Init", "InitScriptFuncs"),
    (r"mpMutex = cPlatform::CreateMutEx", "CreateMutex"),
    (r"mbGameIsDone=false", "InitFlags"),
    (r"mpFPSCounter = hplNew", "CreateFpsCounter"),
    (r"mpFrameTimer = cPlatform::CreateTimer", "CreateFrameTimer"),
    (r'Log\("User Initialization\\n"\)', "LogUserInit"),
    (r"hplDelete\(mpHeadlessControl\)", "DeleteHeadless"),
    (r"hplDelete\(mpDevHud\)", "DeleteDevHud"),
    (r"hplDelete\(mpLogicTimer\)", "DeleteLogicTimer"),
    (r"hplDelete\(mpFPSCounter\)", "DeleteFpsCounter"),
    (r"hplDelete\(mpFrameTimer\)", "DeleteFrameTimer"),
    (r"hplDelete\(mpMutex\)", "DeleteMutex"),
    (r"hplDelete\(mpUpdater\)", "DeleteUpdater"),
    (r"hplDelete\(mp(Gui|Scene|Input|Sound|Graphics|Resources|Physics|AI|System)\)", lambda m: "DeleteModule(%s)" % MODS[m.group(1)]),
    (r'Log\(" Deleting game setup provided by user\\n"\)', "LogDeleteSetup"),
    (r"hplDelete\(mpGameSetup\)", "DeleteSetup"),
    (r'Log\("HPL Exit was successful!\\n"\)', "LogExitOk"),
]


def steps(text):
    found = []
    for pat, name in PATTERNS:
        for m in re.finditer(pat, text):
            found.append((m.start(), name(m) if callable(name) else name))
    found.sort()
    return [n for _, n in found]


init = body("void cEngine::GameInit(")
init = init[: init.index("if(OhplBackendRustRequested())")] + init[init.index("return;\n\t\t}", init.index("if(OhplBackendRustRequested())")) + 12 :]
exit_ = body("cEngine::~cEngine()")
exit_ = exit_[exit_.index("Log(") :]
for s in steps(init):
    print("init " + s)
for s in steps(exit_):
    print("exit " + s)
