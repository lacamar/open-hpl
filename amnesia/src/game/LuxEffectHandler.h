/*
 * Copyright © 2009-2020 Frictional Games
 *
 * This file is part of Amnesia: The Dark Descent.
 *
 * Amnesia: The Dark Descent is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * Amnesia: The Dark Descent is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Amnesia: The Dark Descent.  If not, see <https://www.gnu.org/licenses/>.
 */

#ifndef LUX_EFFECT_HANDLER_H
#define LUX_EFFECT_HANDLER_H

#include "LuxBase.h"
#include "RustLuxFxA12a.h"

// The effects live in Rust (rust/src/luxfx/fxobj.rs); these shells hold the handler handle.

class cLuxEffectShell
{
public:
	cLuxEffectShell(uint64_t alH, int alWhich) : mlH(alH), mlWhich(alWhich) {}
	bool IsActive(){ return ohpl_a12a_fx_active(mlH, mlWhich) != 0; }
protected:
	uint64_t mlH;
	int mlWhich;
};

class cLuxEffect_PlayCommentary : public cLuxEffectShell
{
public:
	cLuxEffect_PlayCommentary(uint64_t alH) : cLuxEffectShell(alH, eOhplA12aFx_Commentary) {}
	void Start(const tString &asTalker,const tString &asTopic, const tString &asFile, int alIconId)
	{
		ohpl_a12a_fx_commentary(mlH, 0, asFile.data(), asFile.size(), alIconId);
	}
	void Stop(){ ohpl_a12a_fx_commentary(mlH, 1, NULL, 0, 0); }
};

class cLuxEffect_EmotionFlash : public cLuxEffectShell
{
public:
	cLuxEffect_EmotionFlash(uint64_t alH) : cLuxEffectShell(alH, eOhplA12aFx_Emotion) {}
	void Start(const tString &asTextCat, const tString &asTextEntry, const tString &asSound)
	{
		ohpl_a12a_fx_emotion_start(mlH, asTextCat.data(), asTextCat.size(), asTextEntry.data(), asTextEntry.size(), asSound.data(), asSound.size());
	}
	void DoAction(eLuxPlayerAction aAction, bool abPressed){ ohpl_a12a_fx_action(mlH, abPressed); }
};

class cLuxEffect_AmountFade : public cLuxEffectShell
{
public:
	cLuxEffect_AmountFade(uint64_t alH, int alWhich, int alKind) : cLuxEffectShell(alH, alWhich), mlKind(alKind) {}
	void FadeTo(float afAmount, float afSpeed){ ohpl_a12a_fx_fade_to(mlH, mlKind, afAmount, afSpeed); }
private:
	int mlKind;
};

class cLuxEffect_RadialBlur : public cLuxEffect_AmountFade
{
public:
	cLuxEffect_RadialBlur(uint64_t alH) : cLuxEffect_AmountFade(alH, eOhplA12aFx_RadialBlur, eOhplA12aPost_RadialBlur) {}
	void SetBlurStartDist(float afDist){ ohpl_a12a_fx_radial_dist(mlH, afDist); }
};

class cLuxEffect_SepiaColor : public cLuxEffect_AmountFade
{
public:
	cLuxEffect_SepiaColor(uint64_t alH) : cLuxEffect_AmountFade(alH, eOhplA12aFx_Sepia, eOhplA12aPost_Sepia) {}
};

class cLuxEffect_ImageTrail : public cLuxEffect_AmountFade
{
public:
	cLuxEffect_ImageTrail(uint64_t alH) : cLuxEffect_AmountFade(alH, eOhplA12aFx_ImageTrail, eOhplA12aPost_ImageTrail) {}
};

class cLuxEffect_ShakeScreen : public cLuxEffectShell
{
public:
	cLuxEffect_ShakeScreen(uint64_t alH) : cLuxEffectShell(alH, eOhplA12aFx_Shake) {}
	void Start(float afAmount, float afTime, float afFadeInTime,float afFadeOutTime){ ohpl_a12a_fx_shake(mlH, afAmount, afTime, afFadeInTime, afFadeOutTime); }
};

class cLuxEffect_Fade : public cLuxEffectShell
{
public:
	cLuxEffect_Fade(uint64_t alH) : cLuxEffectShell(alH, eOhplA12aFx_Fade) {}
	void FadeIn(float afTime){ ohpl_a12a_fx_fade(mlH, 0, afTime); }
	void FadeOut(float afTime){ ohpl_a12a_fx_fade(mlH, 1, afTime); }
	void SetDirectAlpha(float afX){ ohpl_a12a_fx_fade(mlH, 2, afX); }
	bool IsFading(){ return ohpl_a12a_fx_fade(mlH, 3, 0) != 0; }
};

class cLuxEffect_SanityGainFlash : public cLuxEffectShell
{
public:
	cLuxEffect_SanityGainFlash(uint64_t alH) : cLuxEffectShell(alH, eOhplA12aFx_SanityFlash) {}
	void Start(){ ohpl_a12a_fx_sanity(mlH, 0, 0, 0); }
	void Update(float afTimeStep){ ohpl_a12a_fx_sanity(mlH, 1, afTimeStep, 0); }
	void DrawFlash(cGuiSet *apSet ,float afTimeStep){ ohpl_a12a_fx_sanity(mlH, 2, 0, (size_t)apSet); }
};

class cLuxEffect_Flash : public cLuxEffectShell
{
public:
	cLuxEffect_Flash(uint64_t alH) : cLuxEffectShell(alH, eOhplA12aFx_Flash) {}
	void Start(float afFadeIn, float afWhite, float afFadeOut){ ohpl_a12a_fx_flash_start(mlH, afFadeIn, afWhite, afFadeOut); }
};

class cLuxEffect_PlayVoice : public cLuxEffectShell
{
public:
	cLuxEffect_PlayVoice(uint64_t alH) : cLuxEffectShell(alH, eOhplA12aFx_Voice) {}

	void StopVoices(float afFadeOutSpeed){ ohpl_a12a_fx_voice(mlH, 0, afFadeOutSpeed); }
	void AddVoice(	const tString& asVoiceFile, const tString& asEffectFile,
					const tString& asTextCat, const tString& asTextEntry, bool abUsePostion,
					const cVector3f& avPosition, float afMinDistance, float afMaxDistance, int alPriority = -1, bool abRemoveInterrupted = true)
	{
		AddMultiSubbedVoice(asVoiceFile, asEffectFile, asTextCat, asTextEntry, 0.0f, "", 0.0f, "", 0.0f, "", 0.0f, "", 0.0f, "", 0.0f, "", 0.0f,
							abUsePostion, avPosition, afMinDistance, afMaxDistance, alPriority, abRemoveInterrupted);
	}

	void AddMultiSubbedVoice(
		const tString& asVoiceFile, const tString& asEffectFile, const tString& asTextCat,
		const tString& asTextEntry, float afTextDelay,
		const tString& asText2Entry, float afText2Delay,
		const tString& asText3Entry, float afText3Delay,
		const tString& asText4Entry, float afText4Delay,
		const tString& asText5Entry, float afText5Delay,
		const tString& asText6Entry, float afText6Delay,
		const tString& asText7Entry, float afText7Delay,
		bool abUsePostion,
		const cVector3f& avPosition, float afMinDistance, float afMaxDistance, int alPriority = -1, bool abRemoveInterrupted = true)
	{
		const tString *vEntries[7] = { &asTextEntry, &asText2Entry, &asText3Entry, &asText4Entry, &asText5Entry, &asText6Entry, &asText7Entry };
		const float vDelays[7] = { afTextDelay, afText2Delay, afText3Delay, afText4Delay, afText5Delay, afText6Delay, afText7Delay };
		OhplA12aVoiceIn v;
		v.voice.p = (const unsigned char*)asVoiceFile.data(); v.voice.n = asVoiceFile.size();
		v.effect.p = (const unsigned char*)asEffectFile.data(); v.effect.n = asEffectFile.size();
		v.cat.p = (const unsigned char*)asTextCat.data(); v.cat.n = asTextCat.size();
		for(int i=0; i<7; ++i)
		{
			v.entries[i].p = (const unsigned char*)vEntries[i]->data(); v.entries[i].n = vEntries[i]->size();
			v.delays[i] = vDelays[i];
		}
		v.usePos = abUsePostion; v.pos[0] = avPosition.x; v.pos[1] = avPosition.y; v.pos[2] = avPosition.z;
		v.minDist = afMinDistance; v.maxDist = afMaxDistance;
		v.priority = alPriority; v.removeInterrupted = abRemoveInterrupted;
		ohpl_a12a_fx_voice_add(mlH, &v);
	}

	void PauseCurrentVoices(){ ohpl_a12a_fx_voice(mlH, 1, 0); }
	void UnpauseCurrentVoices(){ ohpl_a12a_fx_voice(mlH, 2, 0); }
	void SetOverCallback(const tString& asFunc){ ohpl_a12a_fx_voice_cb_set(mlH, asFunc.data(), asFunc.size()); }
	void SetVolumeMul(float afMul){ ohpl_a12a_fx_voice(mlH, 3, afMul); }

	/** True if all voices (not effect files) are done playing */
	bool VoiceDonePlaying(){ return ohpl_a12a_fx_voice(mlH, 4, 0) != 0; }
};

class cLuxEffectHandler : public iLuxUpdateable
{
public:
	cLuxEffectHandler() : iLuxUpdateable("LuxEffectHandler"), mlH((OhplA12aInitHost(), ohpl_a12a_fx_new())),
		mFade(mlH), mFlash(mlH), mSanityGainFlash(mlH), mPlayVoice(mlH), mImageTrail(mlH), mScreenShake(mlH),
		mSepiaColor(mlH), mRadialBlur(mlH), mEmotionFlash(mlH), mPlayCommentary(mlH) {}
	~cLuxEffectHandler(){ ohpl_a12a_fx_free(mlH); }

	void OnClearFonts(){ ohpl_a12a_fx_fonts(mlH, 0); }
	void LoadFonts(){ ohpl_a12a_fx_fonts(mlH, 1); }
	void OnStart(){}
	void Update(float afTimeStep)
	{
		if(ohpl_a12a_dump_gated()) return;
		ohpl_a12a_fx_update(mlH, afTimeStep);
	}
	void Reset(){ ohpl_a12a_fx_reset(mlH); }

	void OnMapEnter(cLuxMap *apMap){}
	void OnMapLeave(cLuxMap *apMap){ ohpl_a12a_fx_map_leave(mlH); }

	void OnDraw(float afFrameTime)
	{
		ohpl_a12a_fx_draw(mlH);
	}

	void DoAction(eLuxPlayerAction aAction, bool abPressed){ ohpl_a12a_fx_action(mlH, abPressed); }

	bool GetPlayerIsPaused(){ return ohpl_a12a_fx_paused_get(mlH) != 0; }
	void SetPlayerIsPaused(bool abX){ ohpl_a12a_fx_paused_set(mlH, abX); }

	uint64_t GetHandle(){ return mlH; }

	cLuxEffect_Fade *GetFade(){ return &mFade;}
	cLuxEffect_Flash *GetFlash(){ return &mFlash;}
	cLuxEffect_SanityGainFlash *GetSanityGainFlash(){ return &mSanityGainFlash;}
	cLuxEffect_PlayVoice *GetPlayVoice(){ return &mPlayVoice;}
	cLuxEffect_ImageTrail *GetImageTrail(){ return &mImageTrail;}
	cLuxEffect_ShakeScreen *GetScreenShake(){ return &mScreenShake;}
	cLuxEffect_SepiaColor *GetSepiaColor(){ return &mSepiaColor;}
	cLuxEffect_RadialBlur *GetRadialBlur(){ return &mRadialBlur;}
	cLuxEffect_EmotionFlash *GetEmotionFlash(){ return &mEmotionFlash;}
	cLuxEffect_PlayCommentary *GetPlayCommentary(){ return &mPlayCommentary;}

private:
	uint64_t mlH;
	cLuxEffect_Fade mFade;
	cLuxEffect_Flash mFlash;
	cLuxEffect_SanityGainFlash mSanityGainFlash;
	cLuxEffect_PlayVoice mPlayVoice;
	cLuxEffect_ImageTrail mImageTrail;
	cLuxEffect_ShakeScreen mScreenShake;
	cLuxEffect_SepiaColor mSepiaColor;
	cLuxEffect_RadialBlur mRadialBlur;
	cLuxEffect_EmotionFlash mEmotionFlash;
	cLuxEffect_PlayCommentary mPlayCommentary;
};

#endif // LUX_EFFECT_HANDLER_H
