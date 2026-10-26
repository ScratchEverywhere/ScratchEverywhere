"""Generate Xbox color-effect pixel/collision/cache integration evidence.

The companion JSON describes independent expected HDMI samples. Expected runtime
monitors: PASSES=5, ERRORS=0, CYCLES=40. This is not a qualification game.
"""
import colorsys
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile
import zlib

out=Path(sys.argv[1]) if len(sys.argv)>1 else Path(__file__).with_name('Color-Effects-Test.sb3')
assets={};samples=[]
def costume(name,data,extension,w,h):
    digest=hashlib.md5(data).hexdigest();assets[digest+'.'+extension]=data
    return dict(name=name,assetId=digest,md5ext=digest+'.'+extension,dataFormat=extension,bitmapResolution=1,rotationCenterX=w/2,rotationCenterY=h/2)
def png(w,h,pixel):
    def chunk(k,d):return struct.pack('>I',len(d))+k+d+struct.pack('>I',zlib.crc32(k+d)&0xffffffff)
    rows=b''.join(b'\0'+b''.join(bytes(pixel(x,y)) for x in range(w)) for y in range(h))
    return b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',w,h,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(rows))+chunk(b'IEND',b'')
colors=[(255,0,0,255),(0,0,0,255),(255,255,255,255),(255,0,0,128)]
bands=costume('bands',png(192,96,lambda x,y:colors[x//48]),'png',192,96)
large=costume('large',png(520,80,lambda x,y:(255,0,0,255)),'png',520,80)
cycler=costume('cycler',png(512,512,lambda x,y:(255,0,0,255)),'png',512,512)
red=costume('probe',png(4,4,lambda x,y:(255,0,0,255)),'png',4,4)
green=costume('overlay',png(8,8,lambda x,y:(0,255,0,255)),'png',8,8)
background=costume('background',b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="#102030"/></svg>','svg',480,360)
num=lambda x:[1,[4,str(x)]]
color=lambda x:[1,[9,x]]
def block(op,next=None,parent=None,inputs=None,fields=None,top=False):
    b=dict(opcode=op,next=next,parent=parent,inputs=inputs or {},fields=fields or {},shadow=False,topLevel=top)
    if top:b.update(x=20,y=20)
    return b
common=dict(variables={},lists={},broadcasts={},comments={},currentCostume=0,sounds=[],volume=100)
def sprite(name,c,x,y,size,layer,hue=0,brightness=0,ghost=0):
    bs={'flag':block('event_whenflagclicked','color',top=True),'color':block('looks_seteffectto','brightness','flag',{'VALUE':num(hue)},{'EFFECT':['COLOR',None]}),'brightness':block('looks_seteffectto','ghost','color',{'VALUE':num(brightness)},{'EFFECT':['BRIGHTNESS',None]}),'ghost':block('looks_seteffectto',parent='brightness',inputs={'VALUE':num(ghost)},fields={'EFFECT':['GHOST',None]})}
    return dict(common,isStage=False,name=name,blocks=bs,costumes=[c],layerOrder=layer,visible=True,x=x,y=y,size=size,direction=90,draggable=False,rotationStyle='all around')
def transformed(rgb,hue,brightness):
    r,g,b=(v/255 for v in rgb)
    if hue:
        h,s,v=colorsys.rgb_to_hsv(r,g,b)
        if v<.055:h,s,v=0,1,.055
        elif s<.09:h,s=0,.09
        r,g,b=colorsys.hsv_to_rgb((h+hue/200)%1,s,v)
    return [max(0,min(255,c*255+max(-100,min(100,brightness))*2.55)) for c in (r,g,b)]
def sample(name,x,y,rgba,hue,brightness,ghost):
    rgb=transformed(rgba[:3],hue,brightness);alpha=rgba[3]/255*(1-ghost/100);expected=[round(c*alpha+b*(1-alpha)) for c,b in zip(rgb,[16,32,48])]
    samples.append(dict(name=name,x=round(360+x*1.5),y=round(240-y*4/3),expected=expected,tolerance=9))
cases=[(0,0,0),(17,0,0),(35,0,0),(60,0,0),(105,0,0),(120,0,0),(165,0,0),(-95,0,0),(200,0,0),(400,0,0),(0,50,0),(0,-50,0),(0,100,0),(0,-100,0),(35,25,0),(105,-25,0),(60,0,50),(60,0,90),(120,50,50),(0,0,50),(17,0,0),(165,0,0),(-200,0,0),(.01,0,0)]
stage=dict(common,isStage=True,name='Stage',variables={n:[n,0] for n in ['PASSES','ERRORS','CYCLES']},blocks={},costumes=[background],layerOrder=0,tempo=60,videoTransparency=50,videoState='off',textToSpeechLanguage=None)
targets=[stage]
for i,(hue,brightness,ghost) in enumerate(cases):
    x=-200+80*(i%6);y=125-70*(i//6);targets.append(sprite(f'Case {i}',bands,x,y,20,10+i,hue,brightness,ghost))
    for band,rgba in enumerate(colors):sample(f'case-{i}-band-{band}',x+(band*48+24-96)*.2,y,rgba,hue,brightness,ghost)
targets.append(sprite('Large tiled',large,0,-140,70,2,105))
for px in [200,252,255,256,259,508,511,512,515]:sample('tile-seam-'+str(px),(px-260)*.7,-140,(255,0,0,255),105,0,0)
spin=sprite('Cache churn',cycler,215,-145,8,3)
spin['blocks']={'flag':block('event_whenflagclicked','reset',top=True),'reset':block('data_setvariableto','loop','flag',{'VALUE':num(0)},{'VARIABLE':['CYCLES','CYCLES']}),'loop':block('control_repeat',parent='reset',inputs={'TIMES':num(40),'SUBSTACK':[2,'hue']}),'hue':block('looks_changeeffectby','count','loop',{'CHANGE':num(5)},{'EFFECT':['COLOR',None]}),'count':block('data_changevariableby','wait','hue',{'VALUE':num(1)},{'VARIABLE':['CYCLES','CYCLES']}),'wait':block('control_wait',parent='count',inputs={'DURATION':num(.05)})}
targets.append(spin)
stamp_costume=costume('stamp',png(8,8,lambda x,y:(255,0,0,255)),'png',8,8)
for i,ghost in enumerate([0,50]):
    stamp=sprite('Pen stamp '+str(i),stamp_costume,-200+50*i,-104,100,5+i,120,10,ghost)
    stamp['blocks']['ghost']['next']='wait'
    stamp['blocks']['wait']=block('control_wait','stamp','ghost',{'DURATION':num(.3)})
    stamp['blocks']['stamp']=block('pen_stamp','hide','wait')
    stamp['blocks']['hide']=block('looks_hide',parent='stamp')
    targets.append(stamp)
    sample('pen-stamp-'+str(i),-200+50*i,-104,(255,0,0,255),120,10,ghost)
targets.append(sprite('Sensor overlay',green,-225,-145,100,4,100,-40))
probe=sprite('Sensor probe',red,-225,-145,100,100,100)
bs={};chain=[]
def cmd(key,op,inputs=None,fields=None):bs[key]=block(op,inputs=inputs,fields=fields);chain.append(key)
def check(key,expected,masked=False,source='#00ffff',destination='#990099'):
    q=key+'-q';yes=key+'-yes';no=key+'-no'
    bs[q]=block('sensing_coloristouchingcolor' if masked else 'sensing_touchingcolor',parent=key,inputs={'COLOR':color(source),'COLOR2':color(destination)} if masked else {'COLOR':color(destination)})
    cmd(key,'control_if_else',{'CONDITION':[2,q],'SUBSTACK':[2,yes],'SUBSTACK2':[2,no]})
    for name,ok in [(yes,expected),(no,not expected)]:
        var='PASSES' if ok else 'ERRORS';bs[name]=block('data_changevariableby',parent=key,inputs={'VALUE':num(1)},fields={'VARIABLE':[var,var]})
cmd('flag','event_whenflagclicked');bs['flag'].update(topLevel=True,x=20,y=20)
for n in ['PASSES','ERRORS']:cmd('reset-'+n,'data_setvariableto',{'VALUE':num(0)},{'VARIABLE':[n,n]})
cmd('settle','control_wait',{'DURATION':num(1)})
cmd('color','looks_seteffectto',{'VALUE':num(100)},{'EFFECT':['COLOR',None]})
check('cyan-mask',True,True);check('raw-red-mask',False,True,source='#ff0000');check('effected-environment',True);check('occluded-background',False,destination='#102030')
cmd('bright','looks_seteffectto',{'VALUE':num(100)},{'EFFECT':['BRIGHTNESS',None]});check('white-mask',True,True,source='#ffffff')
for i,key in enumerate(chain):
    bs[key]['next']=chain[i+1] if i+1<len(chain) else None
    if i:bs[key]['parent']=chain[i-1]
probe['blocks']=bs;targets.append(probe)
monitors=[dict(id=n,mode='default',opcode='data_variable',params={'VARIABLE':n},spriteName=None,value=0,width=0,height=0,x=10+150*i,y=2,visible=True,sliderMin=0,sliderMax=100,isDiscrete=True) for i,n in enumerate(['PASSES','ERRORS','CYCLES'])]
project=dict(targets=targets,monitors=monitors,extensions=['pen'],meta={'semver':'3.0.0','vm':'1.1.2','agent':'Color/brightness/cache regression'})
with zipfile.ZipFile(out,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('project.json',json.dumps(project))
    for name,data in assets.items():z.writestr(name,data)
out.with_suffix('.expected.json').write_text(json.dumps(dict(passes=5,errors=0,cycles=40,samples=samples),indent=2)+'\n')
print(out,':',len(samples),'pixel samples, 5 collision checks, 40 cache-churn steps')
