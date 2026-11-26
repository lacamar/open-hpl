SCR=$HOME/.cache/open-hpl/soma-scratch-album
export OPENHPL_SOMA_SCRATCH=$SCR XDG_CONFIG_HOME=$SCR/.xdg/config XDG_CACHE_HOME=$SCR/.xdg/cache XDG_DATA_HOME=$SCR/.xdg/data XDG_STATE_HOME=$SCR/.xdg/state
S=$XDG_RUNTIME_DIR/ohpl-album.sock
H="python3 /home/lm/.local/src/open-hpl/scripts/hpl_control.py --socket $S"
shot(){ # x y z yaw pitch name
 $H set_camera pos_x=$1 pos_y=$2 pos_z=$3 yaw=$4 pitch=$5 >/dev/null; $H wait_frames n=30 >/dev/null
 $H screenshot path=$SCR/album-tmp/$6.bmp | tr -d '\n '; $H frame_stats | python3 -c "import json,sys;f=json.load(sys.stdin)['frame'];print(' lum',f['lum_mean'],'black',f['black_frac'],'white',f['white_frac'])"; }
sweep(){ # tag x y z pitch yaws...
 $H wait_frames n=120 max_ms=60000 >/dev/null 2>&1; t=$1; x=$2; y=$3; z=$4; p=$5; shift 5; fs=""
 for yw in "$@"; do $H set_camera pos_x=$x pos_y=$y pos_z=$z yaw=$yw pitch=$p >/dev/null 2>&1; $H wait_frames n=80 max_ms=60000 >/dev/null 2>&1
  $H screenshot path=$SCR/album-tmp/$t-$yw.bmp >/dev/null 2>&1; magick $SCR/album-tmp/$t-$yw.bmp -alpha off -resize 640x -gravity south -fill yellow -pointsize 24 -annotate 0 "$yw" $SCR/album-tmp/$t-$yw.png; fs="$fs $SCR/album-tmp/$t-$yw.png"; done
 magick montage $fs -tile 2x -geometry +2+2 $SCR/album-tmp/$t-sheet.png; }
go(){ $H quit >/dev/null 2>&1; timeout 300 bash -c 'while pgrep -x Soma.bin.aarch6 >/dev/null && ps -eo pid,args | grep -q "[.]/Soma.bin.aarch64"; do sleep 1; done'
 (cd /home/lm/.local/src/open-hpl; OPENHPL_SOMA_FREECAM=1 scripts/soma-run.sh $1.hpm $S >/dev/null 2>&1)
 timeout 900 bash -c ". /tmp/alb.sh >/dev/null 2>&1; until \$H wait_frames n=1 >/dev/null 2>&1; do sleep 1; done"; sleep 3; $H wait_frames n=60 max_ms=90000 >/dev/null 2>&1; $H camera_state | tr -d '\n '; echo; }
ss(){ t=$1; shift; read x y z <<<$($H camera_state | python3 -c "import json,sys;d=json.load(sys.stdin);print(d['pos_x'],d['pos_y'],d['pos_z'])"); sweep $t $x $y $z 0 0 1.57 3.14 4.71 >/dev/null 2>&1; }
sv(){ /home/lm/.local/src/open-hpl/scripts/ohpl-album.sh $SCR/album-tmp/$1.bmp $2; }
export OPENHPL_DEV_HUD=1
gl(){ # scratchdir bin   (launch to menu, headless)
 SC=$HOME/.cache/open-hpl/$1; $H quit >/dev/null 2>&1; timeout 300 bash -c 'while pgrep -f "^\./(Amnesia|Amfp|Rebirth|Bunker|Soma)\.bin" >/dev/null; do sleep 1; done'
 pactl list short sinks | grep -q $'\tohpl-ours\t' || pactl load-module module-null-sink sink_name=ohpl-ours >/dev/null
 rm -f $S; cd $SC; env XDG_CONFIG_HOME=$SC/.xdg/config XDG_CACHE_HOME=$SC/.xdg/cache XDG_DATA_HOME=$SC/.xdg/data XDG_STATE_HOME=$SC/.xdg/state ALSOFT_DRIVERS=pulse PULSE_SINK=ohpl-ours OPENHPL_SOUND_DEVICE=ohpl-ours OPENHPL_HEADLESS_SOCKET=$S setsid nohup ./$2 >run.out 2>&1 </dev/null &
 cd /home/lm/.local/src/open-hpl; timeout 600 bash -c ". /tmp/alb.sh >/dev/null 2>&1; until \$H wait_frames n=1 >/dev/null 2>&1; do sleep 1; done"; }
am(){ # map  -> start & settle
 $H start_map map=$1 >/dev/null 2>&1; $H wait_frames n=300 max_ms=240000 >/dev/null 2>&1; $H state | tr -d '\n '; echo; }
turn(){ $H input type=mouse_move xrel=$1 yrel=${2:-0} frames=${3:-1} >/dev/null 2>&1; $H wait_frames n=20 >/dev/null 2>&1; }
cap(){ $H wait_frames n=${3:-40} max_ms=60000 >/dev/null 2>&1; $H screenshot path=$SCR/album-tmp/$1.bmp >/dev/null 2>&1; magick $SCR/album-tmp/$1.bmp -alpha off -resize ${2:-640}x $SCR/album-tmp/$1.png; }
amv(){ t=${1%.map}; am $1 | grep -o '"pos[^v]*' | tr -d '\n '; echo " $t"; fs=""; for i in 0 1 2 3 4 5; do cap $t-$i 640 60; fs="$fs $SCR/album-tmp/$t-$i.png"; turn ${2:-1500}; done; magick montage $fs -tile 3x -geometry +2+2 $SCR/album-tmp/$t-sheet.png; }
