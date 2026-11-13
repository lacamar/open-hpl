#!/usr/bin/env python3
"""gen.py orig|new <outdir> <worktree>: extract the state function texts verbatim into <outdir>/<side>.inc.
orig = the C++ at the base commit (T0 kernels, OHPL_IMPL unset), new = the working tree (Rust-forwarding shells)."""
import os
import subprocess
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from cppedit import span

side, out, W = sys.argv[1], sys.argv[2], sys.argv[3]
BASE = os.environ.get('A3_BASE', 'rust-port-batch3')
G = 'amnesia/src/game/'


def src(f):
    if side == 'orig':
        return subprocess.run(['git', '-C', W, 'show', BASE + ':' + G + f], capture_output=True, check=True).stdout.decode('latin-1')
    return open(W + '/' + G + f, encoding='latin-1', newline='').read()


NEW_ONLY = ('A3Host', 'A3Gfx')


def fn(t, name):
    try:
        i, k = span(t, name, 0)
    except (IndexError, ValueError):
        if side == 'orig' and any(n in name for n in NEW_ONLY):
            return ''
        print('MISSING', name)
        return ''
    text = t[i:k] + '\n\n'
    return text.replace('iLuxPlayerState::A3Host(', 'iLuxPlayerState::A3HostReal(')


LISTS = [
    ('LuxPlayerState.cpp', ['void cLuxPlayerStateVars::SetupInteraction', 'iLuxPlayerState::iLuxPlayerState', 'int32_t iLuxPlayerState::A3Host']),
    ('LuxPlayerState_Interact.cpp', ['iLuxPlayerState_Interact::iLuxPlayerState_Interact', 'void iLuxPlayerState_Interact::SetupInteractVars',
                                     'void iLuxPlayerState_Interact::ResetInteractVars', 'void iLuxPlayerState_Interact::OnDestroyEntity',
                                     'void iLuxPlayerState_Interact::OnAttachBodyToStickyArea']),
    ('LuxPlayerState_DefaultBase.cpp', [
        'iLuxPlayerState_DefaultBase::iLuxPlayerState_DefaultBase', 'void iLuxPlayerState_DefaultBase::OnEnterState',
        'void iLuxPlayerState_DefaultBase::OnLeaveState', 'void iLuxPlayerState_DefaultBase::OnMapLeave',
        'void iLuxPlayerState_DefaultBase::Update', 'void iLuxPlayerState_DefaultBase::OnDraw', 'bool iLuxPlayerState_DefaultBase::OnDoAction',
        'cGuiGfxElement* iLuxPlayerState_DefaultBase::A3Gfx', 'cGuiGfxElement* iLuxPlayerState_DefaultBase::GetCrosshair',
        'bool iLuxPlayerState_DefaultBase::ShowOutlineOnEntity', 'bool iLuxPlayerState_DefaultBase::CanInteractWithEntity', 'static bool BodyIsAdded',
        'void iLuxPlayerState_DefaultBase::AddOutlineObjects', 'void iLuxPlayerState_DefaultBase::GetAttachedBodies']),
    ('LuxPlayerState_Normal.cpp', ['cLuxPlayerState_Normal::cLuxPlayerState_Normal', 'bool cLuxPlayerState_Normal::ImplementedDoAction']),
    ('LuxPlayerState_HandObject.cpp', ['cLuxPlayerState_HandObject::cLuxPlayerState_HandObject', 'bool cLuxPlayerState_HandObject::ImplementedDoAction',
                                       'void cLuxPlayerState_HandObject::ImplementedOnEnterState', 'void cLuxPlayerState_HandObject::ImplementedOnLeaveState']),
    ('LuxPlayerState_UseItem.cpp', [
        'cLuxPlayerState_UseItem::cLuxPlayerState_UseItem', 'void cLuxPlayerState_UseItem::ImplementedOnEnterState',
        'bool cLuxPlayerState_UseItem::ImplementedDoAction', 'void cLuxPlayerState_UseItem::ImplementedUpdate',
        'cGuiGfxElement* cLuxPlayerState_UseItem::GetCrosshair', 'bool cLuxPlayerState_UseItem::OnDrawCrossHair',
        'bool cLuxPlayerState_UseItem::ShowOutlineOnEntity', 'void cLuxPlayerState_UseItem::UseItem']),
    ('LuxPlayerState_InteractGrab.cpp', [
        'cLuxPlayerState_InteractGrab::cLuxPlayerState_InteractGrab', 'cLuxPlayerState_InteractGrab::~cLuxPlayerState_InteractGrab',
        'void cLuxPlayerState_InteractGrab::OnEnterState', 'void cLuxPlayerState_InteractGrab::OnLeaveState',
        'void cLuxPlayerState_InteractGrab::PostUpdate', 'bool cLuxPlayerState_InteractGrab::OnDoAction',
        'void cLuxPlayerState_InteractGrab::OnScroll', 'bool cLuxPlayerState_InteractGrab::OnAddYaw', 'bool cLuxPlayerState_InteractGrab::OnAddPitch',
        'void cLuxPlayerState_InteractGrab::OnSaveBody', 'bool cLuxPlayerState_InteractGrab::AllowBuoyancy',
        'void cLuxPlayerState_InteractGrab::SaveBodyProperties', 'bool cLuxPlayerState_InteractGrab::BodyIsAdded']),
    ('LuxPlayerState_InteractPush.cpp', [
        'cLuxPlayerState_InteractPush::cLuxPlayerState_InteractPush', 'void cLuxPlayerState_InteractPush::OnEnterState',
        'void cLuxPlayerState_InteractPush::OnLeaveState', 'void cLuxPlayerState_InteractPush::Update',
        'void cLuxPlayerState_InteractPush::PostUpdate', 'bool cLuxPlayerState_InteractPush::OnDoAction',
        'bool cLuxPlayerState_InteractPush::OnMove', 'void cLuxPlayerState_InteractPush::ClearMoveVars']),
    ('LuxPlayerState_InteractSlide.cpp', [
        'cLuxPlayerState_InteractSlide::cLuxPlayerState_InteractSlide', 'void cLuxPlayerState_InteractSlide::OnEnterState',
        'void cLuxPlayerState_InteractSlide::OnLeaveState', 'void cLuxPlayerState_InteractSlide::Update',
        'bool cLuxPlayerState_InteractSlide::OnDoAction', 'bool cLuxPlayerState_InteractSlide::OnAddYaw',
        'bool cLuxPlayerState_InteractSlide::OnAddPitch', 'bool cLuxPlayerState_InteractSlide::OnMove']),
    ('LuxPlayerState_InteractRotateBase.cpp', [
        'iLuxPlayerState_InteractRotateBase::iLuxPlayerState_InteractRotateBase', 'iLuxPlayerState_InteractRotateBase::~iLuxPlayerState_InteractRotateBase',
        'void iLuxPlayerState_InteractRotateBase::OnEnterState', 'void iLuxPlayerState_InteractRotateBase::OnLeaveState',
        'void iLuxPlayerState_InteractRotateBase::Update', 'bool iLuxPlayerState_InteractRotateBase::OnDoAction',
        'bool iLuxPlayerState_InteractRotateBase::OnMove', 'bool iLuxPlayerState_InteractRotateBase::OnAddYaw',
        'bool iLuxPlayerState_InteractRotateBase::OnAddPitch']),
    ('LuxPlayerState_InteractLever.cpp', ['float cLuxPlayerState_InteractLever::GetSpeedAdd', 'void cLuxPlayerState_InteractLever::OnThrow']),
    ('LuxPlayerState_InteractSwingDoor.cpp', ['float cLuxPlayerState_InteractSwingDoor::GetSpeedAdd', 'void cLuxPlayerState_InteractSwingDoor::OnThrow']),
    ('LuxPlayerState_InteractWheel.cpp', ['cLuxPlayerState_InteractWheel::cLuxPlayerState_InteractWheel',
                                          'void cLuxPlayerState_InteractWheel::EnterRotateBase', 'float cLuxPlayerState_InteractWheel::GetSpeedAdd',
                                          'void cLuxPlayerState_InteractWheel::OnThrow']),
    ('LuxPlayerState_Ladder.cpp', [
        'cLuxPlayerState_Ladder::cLuxPlayerState_Ladder', 'void cLuxPlayerState_Ladder::OnEnterState', 'void cLuxPlayerState_Ladder::OnLeaveState',
        'void cLuxPlayerState_Ladder::Update', 'bool cLuxPlayerState_Ladder::OnMove', 'bool cLuxPlayerState_Ladder::OnAddYaw',
        'bool cLuxPlayerState_Ladder::OnAddPitch', 'bool cLuxPlayerState_Ladder::OnRun', 'bool cLuxPlayerState_Ladder::OnJump',
        'bool cLuxPlayerState_Ladder::OnCrouch', 'bool cLuxPlayerState_Ladder::OnDoAction', 'void cLuxPlayerState_Ladder::PlaySound',
        'void cLuxPlayerState_Ladder::SetupHeadTurnLimits']),
]
chunks = []
for f, names in LISTS:
    t = src(f)
    for n in names:
        if n.endswith('::A3Host'):
            a = t.find('static cLuxMap *gpA3CbMap;')
            if a >= 0:
                chunks.append(t[a:t.index('int32_t iLuxPlayerState::A3Host')])
        chunks.append(fn(t, n))
os.makedirs(out, exist_ok=True)
open('%s/%s.inc' % (out, side), 'w', encoding='latin-1', newline='').write(''.join(chunks))
print(side, sum(len(x) for x in chunks))
