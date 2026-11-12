#!/usr/bin/env python3
"""Save/load round trip: change state by script, autosave, reload, diff state dumps (entities, lights, PS, vars, player).
  scripts/soma-save-roundtrip.py MAP [TIMER_FUNC] [--nostart] [--gaps]   # --gaps: sound/music/effects/hands/flashlight/spawned entity probes
Needs OHPL_GAME=soma and soma-init.sh env; dumps go to $XDG_RUNTIME_DIR/ohpl-roundtrip/."""
import importlib.machinery,importlib.util,os,sys,re,time,difflib,glob
os.environ.setdefault("OHPL_PLAY_NAME","ohpl-roundtrip")
l=importlib.machinery.SourceFileLoader('sp',os.path.join(os.path.dirname(os.path.abspath(__file__)),'soma-play.py'))
sp=importlib.util.module_from_spec(importlib.util.spec_from_loader('sp',l)); l.exec_module(sp)
TF=sys.argv[2] if len(sys.argv)>2 and not sys.argv[2].startswith('--') else 'LadderFadeUp'
OUT=os.path.join(os.environ.get('XDG_RUNTIME_DIR','/tmp'),'ohpl-roundtrip'); os.makedirs(OUT,exist_ok=True)
MOD=sys.argv[1]
DUMP='''array<iLuxEntity@> v; cLux_GetCurrentMap().GetEntityArray("*", eLuxEntityType_LastEnum, "", v);
for(uint i=0;i<v.length();++i){ iLuxEntity@ e=v[i]; cVector3f q=e.GetPosition(); tString c=e.GetClassName(); tString s=e.GetName()+"|"+c+"|a"+e.IsActive()+"|i"+e.GetInteractionDisabled()+"|t"+e.IsInteractedWith();
 s+="|p"+int(q.x*50)+","+int(q.y*50)+","+int(q.z*50);
 if(c=="cScrPropLamp") s+="|lit"+Lamp_GetLit(e.GetName());
 if(c=="cScrPropSwingDoor") s+="|d"+SwingDoor_GetState(e.GetName())+"_"+int(SwingDoor_GetOpenAmount(e.GetName())*20)+"_"+SwingDoor_GetLocked(e.GetName());
 __print(s);}
cLuxPlayer@ p=cLux_GetPlayer(); cVector3f f=p.GetCharacterBody().GetFeetPosition(); __print("PLAYER|"+int(f.x*50)+","+int(f.y*50)+","+int(f.z*50)+"|"+p.GetCurrentStateName());
for(int k=0;k<12;++k){ array<iLight@> L; Map_GetLightArray(cScript_GetGlobalVarString("qnp_L"+k), L); if(L.length()==0) continue; cColor c=L[0].GetDiffuseColor(); __print("LIGHT_"+L[0].GetName()+"|v"+L[0].IsVisible()+"|"+int(c.r*100)+","+int(c.g*100)+","+int(c.b*100)+"|r"+int(L[0].GetRadius()*10)); }
for(int k=0;k<6;++k){ array<cParticleSystem@> P; Map_GetParticleSystemArray(cScript_GetGlobalVarString("qnp_P"+k), P); if(P.length()==0){ __print("PS_gone"+k); continue;} __print("PS_"+P[0].GetName()+"|a"+P[0].IsActive()+"|v"+P[0].IsVisible()); }
__print("PL2|"+int(p.GetHealth())+"|tm"+int(cLux_GetCurrentMap().GetTimerTime("qnp_t")*10));
__print("VARS|"+cScript_GetGlobalVarInt("qnp_i")+"|"+cScript_GetGlobalVarString("qnp_s")+"|"+cScript_GetGlobalVarBool("qnp_b")+"|"+cScript_GetGlobalVarFloat("qnp_f"));'''
def dump(tag):
    o=sp.ex(DUMP, MOD)
    lines=sorted(x for x in o.splitlines() if "|" in x and not x.startswith(("Hitch","WARN","ERROR","SOMA")) and "Critter" not in x)
    open(f"{OUT}/{tag}.txt","w").write("\n".join(lines)+"\n"); return lines
def run(code): return sp.ex(code.replace('TIMERFUNC',TF), MOD)

CHANGE='''
array<iLuxEntity@> v; cLux_GetCurrentMap().GetEntityArray("*", eLuxEntityType_LastEnum, "", v);
int nl=0,nd=0,ng=0,nr=0,ns=0,nb=0;
for(uint i=0;i<v.length();++i){ iLuxEntity@ e=v[i]; tString c=e.GetClassName(); tString n=e.GetName();
 if(c=="cScrPropLamp" && nl<5){ Lamp_SetLit(n,false,false); nl++; }
 else if(c=="cScrPropSwingDoor" && nd<3){ SwingDoor_SetClosed(n,false,false); nd++; }
 else if(c=="cScrPropGrab" && ng<3){ Entity_AddImpulse(n,cVector3f(0,3,1),false,false); ng++; }
 else if(c=="cScrPropRigid" && nr<2){ Prop_SetHealth(n,0); nr++; }
 else if(c=="StaticProp" && ns<3){ Entity_SetActive(n,false); ns++; }
 else if(c=="cScrPropButton" && nb<2){ Entity_SetInteractionDisabled(n,true); nb++; }
}
 array<iLight@> L2; Map_GetLightArray("*", L2); int nk=0; for(uint i=0;i<L2.length() && nk<12;++i){ if(L2[i].GetEntityParent() !is null) continue; cScript_SetGlobalVarString("qnp_L"+nk,L2[i].GetName()); if(nk%3==0) L2[i].SetVisible(false); else if(nk%3==1) L2[i].SetDiffuseColor(cColor(0.3f,0.2f,0.1f,1)); else L2[i].SetRadius(L2[i].GetRadius()*0.5f); nk++; }
array<cParticleSystem@> P2; Map_GetParticleSystemArray("*", P2); for(uint i=0;i<P2.length() && i<6;++i){ cScript_SetGlobalVarString("qnp_P"+i,P2[i].GetName()); if(i%2==0) P2[i].SetVisible(false); else P2[i].Kill(); }
cScript_SetGlobalVarInt("qnp_i",42); cLux_GetPlayer().SetHealth(37); cLux_GetCurrentMap().AddTimer("qnp_t",900.0f,"TIMERFUNC"); cScript_SetGlobalVarString("qnp_s","hello"); cScript_SetGlobalVarBool("qnp_b",true); cScript_SetGlobalVarFloat("qnp_f",1.5f);
'''
def main():
    m=sys.argv[1]
    for _t in range(4):
        if "--nostart" in sys.argv: break
        try: sp.cmd_start(type("A",(),{"map":m,"pos":None,"save":None})()); break
        except Exception as ex: print("start retry",ex); time.sleep(3)
    sp.frames(2)
    d0=dump("d0")
    print("entities",len(d0))
    print(run(CHANGE).strip()[-200:])
    sp.frames(10)
    d1=dump("d1")
    run('Game_AutoSave();'); sp.frames(1)
    f=sorted(glob.glob(os.environ["XDG_DATA_HOME"]+"/open-hpl/soma/saves/AutoSave_*.sav"),key=os.path.getmtime)[-1]
    nm=os.path.basename(f); print("save",nm)
    sp.ex('cLux_GetSaveHandler().LoadGameFromFile("%s");'%nm)
    sp.frames(20)
    d2=dump("d2")
    diff=[x for x in difflib.unified_diff(d1,d2,lineterm="",n=0) if not x.startswith(("---","+++","@@"))]
    chg=[x for x in difflib.unified_diff(d0,d1,lineterm="",n=0) if not x.startswith(("---","+++","@@"))]
    print("changes applied:",len(chg)//2,"| round-trip diffs:",len(diff)//2)
    open(f"{OUT}/diff_{m}.txt","w").write("\n".join(diff)+"\n")
    print("\n".join(diff[:60]))
GAP_SETUP='''
Sound_CreateAtEntity("qnp_snd","Entities_OceanBottom/airlock/uw/pump/pump_loop","Player",0.0f,true,0.7f);
Music_PlayExt("01_02_catwalk_collapse", true, 0.8f, 1, 0, 0, eMusicPrio_BigEvent, true);
Effect_ToneMapping_FadeExposure(0.5f,0.0f);
Effect_Fade_Out(0.0f);
PlayerHands_SetVisible(false);
Player_SetFlashlightDisabled(true);
Player_SetJumpDisabled(true);
Entity_CreateAtEntity("qnp_new","entities/urban/toilet/bandages/bandage_01.ent","Player",true);
Entity_SetActive("qnp_new", true);
'''
GAP_PROBE='''
iLuxEntity@ ne=cLux_GetCurrentMap().GetEntityByName("qnp_new");
__print("snd="+Sound_Exists("qnp_snd"));
__print("tone="+int(Effect_ToneMapping_GetExposure()*100));
__print("hands="+PlayerHands_IsActive());
Player_SetFlashlightActive(true,true); __print("flash="+Player_GetFlashlightActive());
__print("spawn="+(ne !is null)+(ne !is null ? " "+ne.IsActive()+" "+ne.GetClassName() : ""));
'''
def gap_state():
    out=run(GAP_PROBE).strip().splitlines()
    st=sp.send({"cmd":"sound_stats"})
    rows=sorted(("|".join(r.split("|")[:2])+"|loop"+r.split("|")[5]) for r in st.get("detail","").splitlines() if r)
    out+=["music="+str(st.get("music"))+" vol="+str(round(float(st.get("music_volume") or 0),2))]+rows
    y0=float(run('__print(""+cLux_GetPlayer().GetCharacterBody().GetFeetPosition().y);').strip().splitlines()[-1])
    run('cScript_SetGlobalArgBool(0,false); cScript_RunGlobalFunc("MoveState_Normal","","_Global_Jump");'); sp.frames(0.3)
    y1=float(run('__print(""+cLux_GetPlayer().GetCharacterBody().GetFeetPosition().y);').strip().splitlines()[-1])
    out.append("jumped=%s"%(y1-y0>0.05))
    return [x for x in out if not x.startswith(("Hitch","WARN","ERROR","SOMA"))]
def gaps():
    run(GAP_SETUP); sp.frames(2)
    a=gap_state()
    run('Game_AutoSave();'); sp.frames(1)
    f=sorted(glob.glob(os.environ["XDG_DATA_HOME"]+"/open-hpl/soma/saves/AutoSave_*.sav"),key=os.path.getmtime)[-1]
    sp.ex('cLux_GetSaveHandler().LoadGameFromFile("%s");'%os.path.basename(f)); sp.frames(14)
    b=gap_state()
    print("gaps before:",*a,sep="\n  "); print("gaps after:",*b,sep="\n  ")
    d=[x for x in difflib.unified_diff(a,b,lineterm="",n=0) if not x.startswith(("---","+++","@@"))]
    print("gap diffs:",len(d)//2); print("\n".join(d))
try:
    if "--gaps" in sys.argv:
        for _t in range(4):
            if "--nostart" in sys.argv: break
            try: sp.cmd_start(type("A",(),{"map":sys.argv[1],"pos":None,"save":None})()); break
            except Exception as ex: print("start retry",ex); time.sleep(3)
        sp.frames(2); gaps()
    else:
        main()
finally:
    if "--nostart" not in sys.argv: sp.cmd_stop(None)
