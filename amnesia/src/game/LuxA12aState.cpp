#define private public
#define protected public
#include "LuxEffectHandler.h"
#include "LuxPostEffects.h"
#undef private
#undef protected

#include "LuxA12aDump.h"
#include <string.h>

void OhplA12FillState(OhplA12aState *apState)
{
	cLuxEffectHandler *pE = gpBase->mpEffectHandler;
	memset(apState, 0, sizeof(*apState));
	cLuxEffect_Fade *pFade = pE->mpFade;
	apState->fade.goal = pFade->mfGoalAlpha; apState->fade.alpha = pFade->mfAlpha; apState->fade.speed = pFade->mfFadeSpeed; apState->fade.active = pFade->mbActive;
	cLuxEffect_Flash *pFlash = pE->mpFlash;
	apState->flash.alpha = pFlash->mfAlpha; apState->flash.step = pFlash->mlStep; apState->flash.count = pFlash->mfCount;
	apState->flash.inSpeed = pFlash->mfFadeInSpeed; apState->flash.whiteSpeed = pFlash->mfWhiteSpeed; apState->flash.outSpeed = pFlash->mfFadeOutSpeed; apState->flash.active = pFlash->mbActive;
	cLuxEffect_SanityGainFlash *pSanity = pE->mpSanityGainFlash;
	apState->sanity.alpha = pSanity->mfAlpha; apState->sanity.step = pSanity->mlStep; apState->sanity.count = pSanity->mfCount;
	apState->sanity.inSpeed = pSanity->mfFadeInSpeed; apState->sanity.whiteSpeed = pSanity->mfWhiteSpeed; apState->sanity.outSpeed = pSanity->mfFadeOutSpeed; apState->sanity.active = pSanity->mbActive;
	apState->trail.amount = pE->mpImageTrail->mfAmount; apState->trail.goal = pE->mpImageTrail->mfAmountGoal; apState->trail.speed = pE->mpImageTrail->mfFadeSpeed; apState->trail.active = pE->mpImageTrail->mbActive;
	apState->sepia.amount = pE->mpSepiaColor->mfAmount; apState->sepia.goal = pE->mpSepiaColor->mfAmountGoal; apState->sepia.speed = pE->mpSepiaColor->mfFadeSpeed; apState->sepia.active = pE->mpSepiaColor->mbActive;
	apState->radial.amount = pE->mpRadialBlur->mfSize; apState->radial.goal = pE->mpRadialBlur->mfSizeGoal; apState->radial.speed = pE->mpRadialBlur->mfFadeSpeed; apState->radial.active = pE->mpRadialBlur->mbActive;
	apState->radialStart = pE->mpRadialBlur->mfBlurStartDist;
	cLuxEffect_EmotionFlash *pEmo = pE->mpEmotionFlash;
	apState->emotion.alpha = pEmo->mfAlpha; apState->emotion.step = pEmo->mlStep; apState->emotion.count = pEmo->mfCount;
	apState->emotion.textAlpha = pEmo->mfTextAlpha; apState->emotion.textTime = pEmo->mfTextTime; apState->emotion.active = pEmo->mbActive;
	apState->shakeActive = pE->mpScreenShake->mbActive;
	apState->shakeCount = (uint32_t)pE->mpScreenShake->mlstShakes.size();
	cLuxEffect_PlayVoice *pVoice = pE->mpPlayVoice;
	apState->voiceActive = pVoice->mbActive; apState->voicePaused = pVoice->mbPaused; apState->voiceCount = (uint32_t)pVoice->mlstVoices.size();
	apState->voiceRows = (uint32_t)pVoice->mvCurrentTextRows.size(); apState->voiceQueue = (uint32_t)pVoice->mvTextEntryQueue.size();
	apState->voiceEntries = (pVoice->mpVoiceEntry != NULL ? 1 : 0) | (pVoice->mpEffectEntry != NULL ? 2 : 0);
	apState->voiceVol = pVoice->mfVolumeMul;
	apState->commentaryActive = pE->mpPlayCommentary->mbActive; apState->commentaryIcon = pE->mpPlayCommentary->mlIconID;
	apState->playerPaused = pE->mbPlayerIsPaused;
	cLuxPostEffect_Insanity *pIns = gpBase->mpPostEffectHandler->mpInsanity;
	apState->insT = pIns->mfT; apState->insAnim = pIns->mfAnimCount; apState->insWave = pIns->mfWaveAlpha; apState->insZoom = pIns->mfZoomAlpha; apState->insSpeed = pIns->mfWaveSpeed;
	apState->insActive = pIns->IsActive();
}
