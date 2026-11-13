#!/usr/bin/env python3
"""gen.py orig|new: extract class text and function bodies verbatim into <side>.inc."""
import os
import re
import subprocess
import sys

here = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, here)
from cppedit import span

side = sys.argv[1]
OUT = sys.argv[2]
W = os.path.abspath(os.path.join(here, '../../../../..'))
BASE = os.environ.get('A4_BASE', 'rust-port-batch3')
G = 'amnesia/src/game/'


def src(f):
    if side == 'orig':
        return subprocess.run(['git', '-C', W, 'show', BASE + ':' + G + f], capture_output=True).stdout.decode('utf-8', 'surrogateescape')
    return open(W + '/' + G + f, encoding='utf-8', errors='surrogateescape').read()


def cut(t, a, b, incl_b=False):
    i = t.index(a)
    j = t.index(b, i)
    return t[i:j + (len(b) if incl_b else 0)]


def class_text(t, head):
    i = t.index(head)
    j = re.compile(r'\n\{[ \t]*\n').search(t, i).start()
    d, k = 0, j + 1
    while True:
        c = t[k]
        d += c == '{'
        d -= c == '}'
        k += 1
        if d == 0:
            break
    return t[i:k] + ';\n'


def fn(t, name, nth=0):
    try:
        i, k = span(t, name, nth)
    except (IndexError, ValueError):
        return ''
    return t[i:k] + '\n'


out = []
h = src('LuxEnemy.h')
out.append(class_text(h, 'class cLuxEnemyPatrolNode'))
out.append(cut(h, 'enum eLuxEnemyState\n', 'class iLuxEnemyLoader : public'))
mh = src('LuxEnemyMover.h')
out.append(cut(mh, 'enum eLuxEnemyMoveState', 'class cLuxEnemyMover\n'))
out.append(class_text(mh, 'class cLuxEnemyMover\n'))
ph = src('LuxEnemyPathfinder.h')
out.append(class_text(ph, 'class cLuxEnemyPathfinder\n'))
gh = src('LuxEnemy_Grunt.h')
out.append(class_text(gh, 'class cLuxEnemy_Grunt : public iLuxEnemy'))
mph = src('LuxEnemy_ManPig.h')
out.append(class_text(mph, 'class cLuxEnemy_ManPig : public iLuxEnemy'))
wh = src('LuxEnemy_WaterLurker.h')
out.append(cut(wh, 'enum eWaterLurkerSplash', 'class cLuxEnemy_WaterLurker'))
out.append(class_text(wh, 'class cLuxEnemy_WaterLurker : public iLuxEnemy'))

e = src('LuxEnemy.cpp')
if side == 'new':
    a4 = src('LuxEnemyA4.cpp')
else:
    a4 = ''
m = src('LuxEnemyMover.cpp')
p = src('LuxEnemyPathfinder.cpp')
g = src('LuxEnemy_Grunt.cpp')
mp = src('LuxEnemy_ManPig.cpp')
wl = src('LuxEnemy_WaterLurker.cpp')

enemy_fns = [
    'iLuxEnemy::GiveDamage', 'iLuxEnemy::AlertOfPlayerPresence', 'iLuxEnemy::ResetProperties', 'iLuxEnemy::ChangeSoundState',
    'iLuxEnemy::UpdateSoundState', 'iLuxEnemy::ConvertAnimToAbsoluteTime', 'iLuxEnemy::UpdateDarknessGlow', 'iLuxEnemy::UpdateHallucination',
    'iLuxEnemy::UpdateAlignEntityWithGroundRay', 'iLuxEnemy::Attack', 'iLuxEnemy::GetDamageShapeMatrix', 'iLuxEnemy::OutsideStartRadius',
    'iLuxEnemy::A4Host', 'iLuxEnemy::A4Call', 'iLuxEnemy::SetPositionAtStartPos',
    'iLuxEnemy::iLuxEnemy', 'iLuxEnemy::SetupAfterLoad', 'iLuxEnemy::OnUpdate', 'iLuxEnemy::ChangeState', 'iLuxEnemy::SendMessage',
    'iLuxEnemy::PlayAnim', 'iLuxEnemy::PlayAnimImpl', 'iLuxEnemy::FadeOutCurrentAnim', 'iLuxEnemy::FadeOutCurrentAnimImpl',
    'iLuxEnemy::ShowPlayerPosition', 'iLuxEnemy::GetCurrentPatrolNode', 'iLuxEnemy::IsAtLastPatrolNode', 'iLuxEnemy::IncCurrentPatrolNode',
    'iLuxEnemy::StateEvent', 'iLuxEnemy::UpdateStateMachine', 'iLuxEnemy::CheckStateChange', 'iLuxEnemy::UpdateAnimation',
    'iLuxEnemy::UpdateCharBody', 'iLuxEnemy::UpdateCanSeePlayer', 'iLuxEnemy::UpdatePlayerDetected', 'iLuxEnemy::UpdatePlayerInRange',
    'iLuxEnemy::UpdateCheckStuckAtDoor', 'iLuxEnemy::UpdateCheckLastPlayerPos', 'iLuxEnemy::UpdateRegenHealth', 'iLuxEnemy::TriggersDisabled',
    'iLuxEnemy::SetMoveSpeed', 'iLuxEnemy::GetSearchForPlayerNode', 'iLuxEnemy::GetPatrolAroundPlayerNode', 'iLuxEnemy::GetPlayerFeetPos',
    'iLuxEnemy::DistToChar', 'iLuxEnemy::DistToChar2D', 'iLuxEnemy::DistToPlayer', 'iLuxEnemy::GetPlayerMovementTowardEnemyAmount', 'iLuxEnemy::InFOV',
    'iLuxEnemy::PlayerInFOV', 'iLuxEnemy::Ga3Host', 'iLuxEnemy::Ga3MoverParams', 'iLuxEnemy::Ga3StateCb', 'iLuxEnemy::Ga3DetectedCb',
    'iLuxEnemy::Ga3Call', 'iLuxEnemy::OnSetActive', 'iLuxEnemy::AddPatrolNode', 'iLuxEnemy::ClearPatrolNodes',
    'iLuxEnemy::GetClosestFood', 'iLuxEnemy::Dist2D', 'iLuxEnemy::GetDirection2D', 'iLuxEnemy::AbsHeightDistToChar',
    'iLuxEnemy::AbsHeightDistToPlayer', 'iLuxEnemy::GetDirection2DToPlayer', 'iLuxEnemy::IsSeenByPlayer',
    'iLuxEnemy::IsInPlayerFovAtFeetPos', 'iLuxEnemy::IsVisibleToPlayerAtFeetPos', 'iLuxEnemy::GetCurrentPoseSuffix',
]
body = []
if a4:
    for sname in ['static cVector3f A4Get', 'static void A4Put']:
        i = a4.index(sname)
        body.append(a4[i:a4.index('\n}\n', i) + 3])
for n in enemy_fns:
    body.append(fn(a4 if n.startswith('iLuxEnemy::A4') else e, n))
body.append(fn(e, 'iLuxEnemy::DistToPlayer2D', 0))
body.append(fn(e, 'iLuxEnemy::DistToPlayer2D', 1))
body.append(fn(e, 'iLuxEnemy::LineOfSight', 0))
body.append(fn(e, 'iLuxEnemy::LineOfSight', 1))
# statics used by Ga3Call
for sname in ['static void Ga3Put', 'static cVector3f Ga3Get']:
    if sname in e:
        i = e.index(sname)
        j = e.index('\n}\n', i) + 3
        body.insert(0, e[i:j])
# gvPosAdds
if 'static const cVector2f gvPosAdds[]' in e:
    i = e.index('static const cVector2f gvPosAdds[]')
    j = e.index('};', i) + 3
    body.insert(0, e[i:j])
for n in ['cLuxEnemyMover::cLuxEnemyMover', 'cLuxEnemyMover::OnUpdate', 'cLuxEnemyMover::MoveToPos', 'cLuxEnemyMover::TurnToPos',
          'cLuxEnemyMover::TurnToAngle', 'cLuxEnemyMover::UseMoveStateAnimations', 'cLuxEnemyMover::CalculateSpeedMul',
          'cLuxEnemyMover::GetMoveSpeed', 'cLuxEnemyMover::GetWantedSpeedAmount', 'cLuxEnemyMover::UpdateStuckCounter',
          'cLuxEnemyMover::UpdateTurning', 'cLuxEnemyMover::UpdateMoveAnimation']:
    body.append(fn(m, n))
for n in ['cLuxEnemyPathfinder::cLuxEnemyPathfinder', 'cLuxEnemyPathfinder::MoveTo', 'cLuxEnemyPathfinder::Stop',
          'cLuxEnemyPathfinder::GetNodeAtPos', 'cLuxEnemyPathfinder::OnUpdate', 'cLuxEnemyPathfinder::GetNextGoalPos',
          'cLuxEnemyPathfinder::UpdateMoving', 'cLuxEnemyPathfinder::GetFinalGoalPos']:
    body.append(fn(p, n))
for n in ['cLuxEnemy_Grunt::cLuxEnemy_Grunt', 'cLuxEnemy_Grunt::StateEventImplement', 'cLuxEnemy_Grunt::Ga3SoundName',
          'cLuxEnemy_Grunt::PlayerIsDetected', 'cLuxEnemy_Grunt::OnDisableTriggers', 'cLuxEnemy_Grunt::GetDamageMul',
          'cLuxEnemy_Grunt::PatrolUpdateGoal', 'cLuxEnemy_Grunt::PatrolEndOfPath']:
    body.append(fn(g, n))

pig = ['cLuxEnemy_ManPig::cLuxEnemy_ManPig', 'cLuxEnemy_ManPig::StateEventImplement', 'cLuxEnemy_ManPig::PlayerIsDetected',
       'cLuxEnemy_ManPig::OnDisableTriggers', 'cLuxEnemy_ManPig::GetDamageMul', 'cLuxEnemy_ManPig::PlayHuntSound',
       'cLuxEnemy_ManPig::InsidePlayerView', 'cLuxEnemy_ManPig::UpdateCheckInLantern', 'cLuxEnemy_ManPig::ForceTeslaSighting',
       'cLuxEnemy_ManPig::UpdateEnemySpecific', 'cLuxEnemy_ManPig::CheckEnemyAutoRemoval', 'cLuxEnemy_ManPig::PatrolUpdateGoal',
       'cLuxEnemy_ManPig::FleeTryToFindSafeNode', 'cLuxEnemy_ManPig::StalkFindNode', 'cLuxEnemy_ManPig::TrackFindNode',
       'cLuxEnemy_ManPig::TrackTeleportBehindPlayer', 'cLuxEnemy_ManPig::Ga3PigHost', 'cLuxEnemy_ManPig::Ga3PigCall',
       'cLuxEnemy_ManPig::Ga3SoundName']
for n in pig:
    f = fn(mp, n)
    if n.endswith('FleeTryToFindSafeNode') and f:
        assert 'if(pNode == NULL && DistToPlayer2D(pNode->GetPosition())<4.0f)' in f
        f = f.replace('if(pNode == NULL && DistToPlayer2D(pNode->GetPosition())<4.0f)', 'if(pNode == NULL)')
    body.append(f)
wlf = ['cLuxEnemy_WaterLurker::cLuxEnemy_WaterLurker', 'cLuxEnemy_WaterLurker::StateEventImplement', 'cLuxEnemy_WaterLurker::InRangeOfFood',
       'cLuxEnemy_WaterLurker::SplashWater', 'cLuxEnemy_WaterLurker::PlayerIsDetected', 'cLuxEnemy_WaterLurker::GetDamageMul',
       'cLuxEnemy_WaterLurker::Ga3WlHost', 'cLuxEnemy_WaterLurker::Ga3WlCall', 'cLuxEnemy_WaterLurker::UpdateEnemySpecific']
for n in wlf:
    body.append(fn(wl, n))

decl = '\n'.join(out).replace('private:', 'public:').replace('protected:', 'public:')
open(f'{OUT}/{side}_decl.inc', 'w').write(decl)
open(f'{OUT}/{side}_body.inc', 'w').write('\n'.join(body))
print(side, 'ok', sum(len(x) for x in out), sum(len(x) for x in body))
