//! Constructs AngelScript 2.28 accepted but 2.38 rejects, as (file suffix, from, to).
//! A `soma:` / `!soma:` prefix limits a patch to SOMA or to the other HPL3 games.

pub(crate) const PATCHES: &[[&str; 3]] = &[
    [
        "custom_depth/agent_deepsea_suit.hps",
        "CheckShapeWorldCollision(cVector3f_Zero,",
        "CheckShapeWorldCollision(void,",
    ],
    [
        "modules/emotionhandler.hps",
        "lBeat<0 ? null : mvHeartbeats[lBeat]",
        "lBeat<0 ? null : @mvHeartbeats[lBeat]",
    ],
    [
        "modules/rathandler.hps",
        "? mvFoodEntities[lClosestIx] : null",
        "? @mvFoodEntities[lClosestIx] : null",
    ],
    [
        "helper_custom_depth_imgui.hps",
        "const cGuiDialogBoxSettings &in aSettings",
        "cGuiDialogBoxSettings &in aSettings",
    ],
    ["03_02_omicron_inside.hps", ".length-", ".length()-"],
    ["03_02_omicron_inside.hps", ".length -", ".length() -"],
    [
        "custom/player/player_custom.hps",
        "30.f, fRayDist, 0.f)",
        "30.f, fRayDist, void)",
    ],
    [
        "modules/playerbodyhandler.hps",
        "[ePlayerBodyAnimationState_Custom]",
        "[ePlayerBodyAnimationState::ePlayerBodyAnimationState_Custom]",
    ],
    [
        "modules/playerbodyhandler.hps",
        "abX ? ePlayerBodyAnimationState_Custom :",
        "abX ? ePlayerBodyAnimationState::ePlayerBodyAnimationState_Custom :",
    ],
    [
        "03_07_dwf_d_factory.hps",
        "\"PLAYER MUL \" = ",
        "\"PLAYER MUL \" + ",
    ],
    [
        "soma:modules/menuhandler.hps",
        "kOptionsGameplayBgSize = cVector2f(680, 255)",
        "kOptionsGameplayBgSize = cVector2f(680, 293)",
    ],
    [
        "soma:modules/menuhandler.hps",
        "OptionMenu_UpdateExtraWidth(\"CrosshairSimple\", true);",
        "OptionMenu_UpdateExtraWidth(\"CrosshairSimple\", true); OptionMenu_UpdateExtraWidth(\"OpenHplHud\", true);",
    ],
    [
        "soma:modules/menuhandler.hps",
        "OptionMenu_UpdateFocus(\"CrosshairSimple\", msSelectedGameplayButton);",
        "OptionMenu_UpdateFocus(\"CrosshairSimple\", msSelectedGameplayButton); } { bool bValue = mpConfig.GetBool(\"Gameplay\", \"OpenHplHud\", false); bool bNewValue = OptionMenu_ButtonOptionsToggle(\"OpenHplHud\", kMainMenuButtonPos, lY++, msSelectedGameplayButton, bValue, mlActionHorizontal); if(bValue != bNewValue) { mpConfig.SetBool(\"Gameplay\", \"OpenHplHud\", bNewValue); ApplySettings(); } msSelectedGameplayButton = OptionMenu_UpdateFocus(\"OpenHplHud\", msSelectedGameplayButton);",
    ],
    [
        "soma:modules/menuhandler.hps",
        "kOptionsVideoDisplayBgSize = cVector2f(680, 335)",
        "kOptionsVideoDisplayBgSize = cVector2f(680, 487)",
    ],
    [
        "soma:modules/menuhandler.hps",
        "OptionMenu_UpdateExtraWidth(\"FOV\", false);",
        "const cVector2f kOptionsBgPos = cVector2f(100, 190); const cVector2f kMainMenuButtonPos = cVector2f(136, 205); OptionMenu_UpdateExtraWidth(\"FOV\", false); OptionMenu_UpdateExtraWidth(\"Renderer\", false); OptionMenu_UpdateExtraWidth(\"ColorOutput\", false); OptionMenu_UpdateExtraWidth(\"HdrPaperWhite\", false); OptionMenu_UpdateExtraWidth(\"HdrPeak\", false);",
    ],
    [
        "soma:modules/menuhandler.hps",
        "msSelectedVideoDisplayButton = OptionMenu_UpdateFocus(\"AA\", msSelectedVideoDisplayButton);",
        "msSelectedVideoDisplayButton = OptionMenu_UpdateFocus(\"AA\", msSelectedVideoDisplayButton); } { array<tString> vRenderers = { \"OpenGL\", \"Vulkan\" }; lY++; int lValue = mpConfig.GetString(\"Graphics\", \"Renderer\", \"OpenGL\") == \"Vulkan\" ? 1 : 0; int lNewValue = OptionMenu_ButtonOptionsMultiSelect(\"Renderer\", kMainMenuButtonPos, lY, msSelectedVideoDisplayButton, vRenderers, lValue, false, mlActionHorizontal); if(lValue != lNewValue) { mlOptionChanges++; mpConfig.SetString(\"Graphics\", \"Renderer\", vRenderers[lNewValue]); } msSelectedVideoDisplayButton = OptionMenu_UpdateFocus(\"Renderer\", msSelectedVideoDisplayButton); } { array<tString> vOutputs = { \"SDR\", \"HDR10\", \"scRGB\", \"P3\" }; lY++; tString sOutput = mpConfig.GetString(\"Graphics\", \"ColorOutput\", \"SDR\"); int lValue = 0; for(uint i=0; i<vOutputs.length(); ++i) if(vOutputs[i] == sOutput) lValue = i; int lNewValue = OptionMenu_ButtonOptionsMultiSelect(\"ColorOutput\", kMainMenuButtonPos, lY, msSelectedVideoDisplayButton, vOutputs, lValue, false, mlActionHorizontal); if(lValue != lNewValue) { mlOptionChanges++; mpConfig.SetString(\"Graphics\", \"ColorOutput\", vOutputs[lNewValue]); } msSelectedVideoDisplayButton = OptionMenu_UpdateFocus(\"ColorOutput\", msSelectedVideoDisplayButton); } { lY++; float fNits = mpConfig.GetFloat(\"Graphics\", \"HdrPaperWhite\", 203); float fValue = (fNits - 80) / 320.0f; float fNewValue = OptionMenu_ButtonOptionsSlider(\"HdrPaperWhite\", kMainMenuButtonPos, lY, msSelectedVideoDisplayButton, fValue, 0.025f, mlActionHorizontal, \"\" + cMath_RoundToInt(fNits)); if(fValue != fNewValue) { mlOptionChanges++; mpConfig.SetFloat(\"Graphics\", \"HdrPaperWhite\", fNewValue * 320.0f + 80); } msSelectedVideoDisplayButton = OptionMenu_UpdateFocus(\"HdrPaperWhite\", msSelectedVideoDisplayButton); } { lY++; float fNits = mpConfig.GetFloat(\"Graphics\", \"HdrPeak\", 1000); float fValue = (fNits - 400) / 1600.0f; float fNewValue = OptionMenu_ButtonOptionsSlider(\"HdrPeak\", kMainMenuButtonPos, lY, msSelectedVideoDisplayButton, fValue, 0.025f, mlActionHorizontal, \"\" + cMath_RoundToInt(fNits)); if(fValue != fNewValue) { mlOptionChanges++; mpConfig.SetFloat(\"Graphics\", \"HdrPeak\", fNewValue * 1600.0f + 400); } msSelectedVideoDisplayButton = OptionMenu_UpdateFocus(\"HdrPeak\", msSelectedVideoDisplayButton);",
    ],
    [
        "!soma:modules/menuhandler.hps",
        "\"AllowSlowdowns\", \"FOV\"",
        "\"AllowSlowdowns\", \"Renderer\", \"ColorOutput\", \"HdrPaperWhite\", \"HdrPeak\", \"FOV\"",
    ],
    [
        "!soma:modules/menuhandler.hps",
        "OptionMenu_UpdateMultiSelectionExtraWidth(vVSyncValues);",
        "OptionMenu_UpdateMultiSelectionExtraWidth(vVSyncValues); array<tString> vRenderers = { \"OpenGL\", \"Vulkan\" }; array<tString> vOutputs = { \"SDR\", \"HDR10\", \"scRGB\", \"P3\" }; OptionMenu_UpdateMultiSelectionExtraWidth(vRenderers); OptionMenu_UpdateMultiSelectionExtraWidth(vOutputs);",
    ],
    [
        "!soma:modules/menuhandler.hps",
        "float fFOVMin = 50;",
        "int lValue = mpConfig.GetString(\"Graphics\", \"Renderer\", \"OpenGL\") == \"Vulkan\" ? 1 : 0; int lNewValue = OptionMenu_ButtonOptionsMultiSelect(vMenuItems[mlCurrentItemId-mlFirstItemId], kOptionsDisplayButtonPos, mlCurrentItemId, msSelectedDisplayButton, vRenderers, lValue, false, mlActionHorizontal, \"Menu\", 0, vRightAnchor, vInfoBoxPos, mfTransitionAlpha$MS); if(lValue != lNewValue) { mlOptionChanges++; mpConfig.SetString(\"Graphics\", \"Renderer\", vRenderers[lNewValue]); } msSelectedDisplayButton = OptionMenu_UpdateFocusCustom(vMenuItems, vMenuItems[mlCurrentItemId], msSelectedDisplayButton); } mlCurrentItemId++; if (OptionMenu_CheckOutOfLimits(mlCurrentItemId) == false) { tString sOutput = mpConfig.GetString(\"Graphics\", \"ColorOutput\", \"SDR\"); int lValue = 0; for(uint i=0; i<vOutputs.length(); ++i) if(vOutputs[i] == sOutput) lValue = i; int lNewValue = OptionMenu_ButtonOptionsMultiSelect(vMenuItems[mlCurrentItemId-mlFirstItemId], kOptionsDisplayButtonPos, mlCurrentItemId, msSelectedDisplayButton, vOutputs, lValue, false, mlActionHorizontal, \"Menu\", 0, vRightAnchor, vInfoBoxPos, mfTransitionAlpha$MS); if(lValue != lNewValue) { mlOptionChanges++; mpConfig.SetString(\"Graphics\", \"ColorOutput\", vOutputs[lNewValue]); } msSelectedDisplayButton = OptionMenu_UpdateFocusCustom(vMenuItems, vMenuItems[mlCurrentItemId], msSelectedDisplayButton); } mlCurrentItemId++; if (OptionMenu_CheckOutOfLimits(mlCurrentItemId) == false) { float fNits = mpConfig.GetFloat(\"Graphics\", \"HdrPaperWhite\", 203); float fValue = (fNits - 80) / 320.0f; float fNewValue = OptionMenu_ButtonOptionsSlider(vMenuItems[mlCurrentItemId-mlFirstItemId], kOptionsDisplayButtonPos, mlCurrentItemId, msSelectedDisplayButton, fValue, 0.025f, mlActionHorizontal, \"\" + cMath_RoundToInt(fNits), \"\", vRightAnchor, vInfoBoxPos, mfTransitionAlpha$SL); if(fValue != fNewValue) { mlOptionChanges++; mpConfig.SetFloat(\"Graphics\", \"HdrPaperWhite\", fNewValue * 320.0f + 80); } msSelectedDisplayButton = OptionMenu_UpdateFocusCustom(vMenuItems, vMenuItems[mlCurrentItemId], msSelectedDisplayButton); } mlCurrentItemId++; if (OptionMenu_CheckOutOfLimits(mlCurrentItemId) == false) { float fNits = mpConfig.GetFloat(\"Graphics\", \"HdrPeak\", 1000); float fValue = (fNits - 400) / 1600.0f; float fNewValue = OptionMenu_ButtonOptionsSlider(vMenuItems[mlCurrentItemId-mlFirstItemId], kOptionsDisplayButtonPos, mlCurrentItemId, msSelectedDisplayButton, fValue, 0.025f, mlActionHorizontal, \"\" + cMath_RoundToInt(fNits), \"\", vRightAnchor, vInfoBoxPos, mfTransitionAlpha$SL); if(fValue != fNewValue) { mlOptionChanges++; mpConfig.SetFloat(\"Graphics\", \"HdrPeak\", fNewValue * 1600.0f + 400); } msSelectedDisplayButton = OptionMenu_UpdateFocusCustom(vMenuItems, vMenuItems[mlCurrentItemId], msSelectedDisplayButton); } mlCurrentItemId++; if (OptionMenu_CheckOutOfLimits(mlCurrentItemId) == false) { float fFOVMin = 50;",
    ],
];
