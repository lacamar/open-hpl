#!/usr/bin/env python3
import re
import subprocess
import sys

sys.path.insert(0, '/home/lm/.cache/open-hpl/ga3a-harness')
from cppedit import span

side = sys.argv[1]
W = '/home/lm/.local/src/open-hpl/.claude/worktrees/agent-aae97849e1b6dc9b1'
G = 'amnesia/src/game/'


def src(f):
    if side == 'orig':
        return subprocess.run(['git', '-C', W, 'show', 'rust-port-batch3:' + G + f], capture_output=True).stdout.decode('latin-1')
    return open(W + '/' + G + f, encoding='latin-1', newline='').read()


def fn(t, name):
    try:
        i, k = span(t, name, 0)
    except (IndexError, ValueError):
        return ''
    return t[i:k] + '\n\n'


P = [
    'void cLuxPlayer::GiveDamage', 'void cLuxPlayer::LowerSanity', 'void cLuxPlayer::SetHealth', 'void cLuxPlayer::SetSanity',
    'void cLuxPlayer::AddHealth', 'void cLuxPlayer::AddSanity', 'void cLuxPlayer::AddLampOil', 'void cLuxPlayer::FadePitchTo',
    'void cLuxPlayer::FadeFOVMulTo', 'void cLuxPlayer::FadeAspectMulTo', 'void cLuxPlayer::FadeRollTo', 'void cLuxPlayer::FadeLeanRollTo',
    'void cLuxPlayer::SetRoll',
    'void cLuxPlayer::UpdateHeadPosAdd', 'void cLuxPlayer::UpdateCamera', 'void cLuxPlayer::UpdateTerror', 'void cLuxPlayer::UpdateTerrorSound', 'void cLuxPlayer::A2HostOp', 'float cLuxPlayer::A2HostQuery', 'void cLuxPlayer::SpinHead',
    'void cLuxPlayer::UpdateHeadSpin', 'void cLuxPlayer::UpdateFocusText', 'void cLuxPlayer::UpdateAvgSpeed', 'void cLuxPlayer::RunVitals',
]
H = [
    'void cLuxPlayerInsanityCollapse::Update',
    'void cLuxPlayerCamDirEffects::Update', 'float cLuxPlayerCamDirEffects::AddAndGetYawAdd', 'float cLuxPlayerCamDirEffects::AddAndGetPitchAdd',
    'void cLuxPlayerCamDirEffects::SetSwayActive', 'void cLuxPlayerCamDirEffects::UpdateSway',
    'void cLuxPlayerHurt::Update',
    'void cLuxPlayerLookAt::Update',
    'void cLuxPlayerFlashback::Update',
    'void cLuxPlayerSanity::Update', 'void cLuxPlayerSanity::UpdateInsaneEffects', 'void cLuxPlayerSanity::UpdateCheckEnemySeen',
    'void cLuxPlayerSanity::UpdateHit', 'void cLuxPlayerSanity::UpdateInsanityVisuals', 'void cLuxPlayerSanity::UpdateEnemySeenEffect',
    'void cLuxPlayerSanity::UpdateLosingSanity', 'void cLuxPlayerSanity::UpdateLowSanity', 'void cLuxPlayerSanity::ToRust',
    'void cLuxPlayerSanity::FromRust', 'void cLuxPlayerSanity::Replay',
    'void cLuxPlayerLantern::Update', 'void cLuxPlayerLantern::SetActive',
    'void cLuxPlayerLightLevel::Update',
    'void cLuxPlayerInDarkness::Update',
    'void cLuxPlayerDeath::Update', 'void cLuxPlayerDeath::ToRust', 'void cLuxPlayerDeath::FromRust', 'void cLuxPlayerDeath::Replay',
]
D = [
    'void cLuxPlayerHands::Update', 'void cLuxPlayerHands::DoAction', 'void cLuxPlayerHands::SetCurrentHandObject',
    'void cLuxPlayerHands::UpdatePlayerHandsPos', 'void cLuxPlayerHands::ResetHandObjectVars',
]
out = []
for f, names in (('LuxPlayer.cpp', P), ('LuxPlayerHelpers.cpp', H), ('LuxPlayerHands.cpp', D)):
    t = src(f)
    for n in names:
        out.append(fn(t, n))
open('/home/lm/.cache/open-hpl/a2-harness/%s.inc' % side, 'w', encoding='latin-1', newline='').write(''.join(out))
print(side, sum(len(x) for x in out))
