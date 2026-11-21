#!/usr/bin/env bash
# S8c real-engine dump (OHPL_S8C_DUMP): loads one map of $OHPL_GAME (soma|rebirth|bunker), then drives every post effect
# through the script natives with fixed frame counts. Usage: scripts/s8c-scenario.sh <dump-file>
set -uo pipefail
R=$(cd "$(dirname "$0")/.." && pwd); cd "$R"
OUT=${1:?usage: s8c-scenario.sh <dump-file>}
GAME=${OHPL_GAME:-soma}
eval "$(scripts/soma-init.sh --env-only "$GAME" 2> /dev/null)"
case $GAME in
soma) MAP=00_01_apartment ;;
rebirth) MAP=02_02_courtyard ;;
*) MAP=arena ;;
esac
export OHPL_PLAY_NAME=${OHPL_PLAY_NAME:-ohpl-s8c-$$} OHPL_S8C_DUMP=$OUT
rm -f "$OUT" "$OUT.exec"
P=scripts/soma-play.py
COMP='cPostEffectComposite@ c = cLux_GetViewport().GetPostEffectComposite();'
HAS_VD=1 HAS_BLUR=1
[ "$GAME" = soma ] && HAS_BLUR=
[ "$GAME" != soma ] && HAS_VD=
LOG="$OUT.exec"
rm -f "$LOG"
cleanup() { $P stop > /dev/null 2>&1; }
trap cleanup EXIT
$P start --map "$MAP" > "$OUT.start" 2>&1 || { echo "start failed:" >&2; tail -5 "$OUT.start" >&2; exit 1; }
# game helpers (map module) own the effects; raw natives only for FXAA, which the game never touches
X() { $P exec --module "$MAP" "$1" >> "$LOG" 2>&1; }
W() { $P wait "$1" > /dev/null 2>&1; }

X "Effect_ChromaticAberration_SetDirect(0.4f, 1.2f, 80.0f, cVector2f(0.01f, -0.02f));
Effect_RadialBlur_SetDirect(0.2f, 0.1f, 0.5f);
Effect_ImageTrail_SetDirectAmount(0.7f);
$COMP cPostEffect_FXAA@ fx = cGraphics_CreatePostEffect_FXAA(); c.AddPostEffect(fx, 70); fx.SetActive(true);
__print(\"n=\" + c.GetPostEffectNum());"
[ -n "$HAS_VD" ] && X "Effect_VideoDistortion_SetDirectAmount(0.8f);"
W 2
X "Effect_ChromaticAberration_SetDirect(0.9f, 2.5f, 200.0f, cVector2f(-0.03f, 0.01f));
Effect_RadialBlur_SetDirect(0.5f, 0.3f, 0.8f);
Effect_ImageTrail_SetDirectAmount(0.2f);"
[ -n "$HAS_VD" ] && X "Effect_VideoDistortion_SetDirectAmount(0.3f);"
W 2
if [ -n "$HAS_BLUR" ]; then
	for AMT in 0.5f 2.0f 6.0f 0.05f 0.0f; do
		X "Effect_Blur_Reset(); Effect_Blur_Start($AMT, 0.0f);"
		W 1
	done
fi
X "$COMP cPostEffect_FXAA@ fx = cast<cPostEffect_FXAA@>(c.GetPostEffectFromType(\"FXAA\"));
fx.SetDisabled(true); __print(\"dis=\" + (fx.IsDisabled() ? 1 : 0)); fx.SetDisabled(false);
c.RemovePostEffect(fx); cGraphics_DestroyPostEffect(fx); __print(\"n=\" + c.GetPostEffectNum());"
W 1
X "Effect_ChromaticAberration_SetDirect(0, 0);
Effect_RadialBlur_SetDirect(0, 0, 0);"
W 1
if [ "$GAME" = rebirth ]; then
	X "Effect_FearTentacles_Start(); Effect_FearTentacles_SetIntensity(0.8f); Effect_FearTentacles_SetCoverage(0.6f);"
	W 2
	X "Effect_FearTentacles_SetMaterial(eFearTentacleMaterial_Focus); Effect_FearTentacles_SetAlpha(0.7f);"
	W 1
	X "Effect_FearTentacles_FadeMaterial(eFearTentacleMaterial_Default, 0.5f);"
	W 1
	X "Effect_FearTentacles_Stop();"
	W 1
fi
# time-accumulating effect last: its frame count may jitter, the gate compares up to the shorter run
X "Effect_ImageFadeFX_SetAmount(0.6f);"
W 1
