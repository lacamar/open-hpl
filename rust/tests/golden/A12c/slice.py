import sys
src, dst, game_steps, other_steps = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4])
out=[]; cur=None; steps=0; skip=False
for l in open(src):
    l=l.rstrip('\n')
    if l.startswith('s begin'):
        cur=int(l.split()[2]); steps=0; skip=False
    elif l.startswith('s step'):
        steps+=1
        lim = game_steps if cur==1 else other_steps
        skip = steps>lim
    elif l.startswith('s end'):
        break
    if l.startswith('map ') : continue
    if l.startswith('s smooth'): skip=False
    if not skip: out.append(l)
while out and out[-1].startswith('c '): out.pop()
open(dst,'w').write('\n'.join(out)+'\n')
print(len(out))
