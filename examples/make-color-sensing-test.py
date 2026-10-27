"""Build a runtime regression for cached/backdrop color sensing and culling.

Expected on Xbox: PASSES=38, ERRORS=0. This fixture is not a qualification game.
Uses only the Python standard library. Includes 90 distant visible sprites to
exercise query culling, opaque PNG cache restoration, an uploaded SVG backdrop,
masked colors, layered colors, transparent candidates and pen composition.
"""
import copy
import hashlib
import json
from pathlib import Path
import struct
import sys
import zipfile
import zlib

output = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).with_name('Color-Sensing-Test.sb3')
assets = {}

def asset(name, data, extension, cx, cy):
    digest = hashlib.md5(data).hexdigest()
    assets[digest + '.' + extension] = data
    return dict(name=name, assetId=digest, md5ext=digest+'.'+extension, dataFormat=extension,
                bitmapResolution=1, rotationCenterX=cx, rotationCenterY=cy)

def chunk(kind, data):
    return struct.pack('>I',len(data))+kind+data+struct.pack('>I',zlib.crc32(kind+data)&0xffffffff)

row = b'\x00'+bytes([21,5,255,255])*240+bytes([255,255,255,255])*240
png = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',480,360,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(row*360))+chunk(b'IEND',b'')
background = asset('PNG',png,'png',240,180)
svg = b'<svg xmlns="http://www.w3.org/2000/svg" width="480" height="360"><rect width="480" height="360" fill="white"/><rect width="240" height="360" fill="#1505ff"/></svg>'
vector_background = asset('SVG',svg,'svg',240,180)
red = asset('red',b'<svg xmlns="http://www.w3.org/2000/svg" width="20" height="20"><rect width="20" height="20" fill="#ff0000"/></svg>','svg',10,10)
green = asset('green',b'<svg xmlns="http://www.w3.org/2000/svg" width="40" height="40"><rect width="40" height="40" fill="#00ff00"/></svg>','svg',20,20)
# A single opaque pixel at the costume origin makes a one-point masked
# reporter. It exposes reduced-grid snapping at the backdrop's exact edges.
probe_rows = b'\x00'+bytes([255,0,0,255])+bytes(19*4)+(b'\x00'+bytes(20*4))*19
probe_png = b'\x89PNG\r\n\x1a\n'+chunk(b'IHDR',struct.pack('>IIBBBBB',20,20,8,6,0,0,0))+chunk(b'IDAT',zlib.compress(probe_rows))+chunk(b'IEND',b'')
pixel_probe = asset('Pixel probe',probe_png,'png',0,0)
num = lambda n: [1,[4,str(n)]]
text = lambda t: [1,[10,t]]
color = lambda c: [1,[9,c]]

def block(op, nxt=None, parent=None, inputs=None, fields=None, top=False):
    b=dict(opcode=op,next=nxt,parent=parent,inputs=inputs or {},fields=fields or {},shadow=False,topLevel=top)
    if top:b.update(x=20,y=20)
    return b

blocks={}; chain=[]
def command(key,op,inputs=None,fields=None):
    blocks[key]=block(op,inputs=inputs,fields=fields);chain.append(key)

def check(key,expected,target,mask=None):
    query=key+'-query';test=key+'-test';yes=key+'-yes';no=key+'-no'
    inputs={'COLOR':color(mask),'COLOR2':color(target)} if mask else {'COLOR':color(target)}
    blocks[query]=block('sensing_coloristouchingcolor' if mask else 'sensing_touchingcolor',parent=test,inputs=inputs)
    command(test,'control_if_else',{'CONDITION':[2,query],'SUBSTACK':[2,yes],'SUBSTACK2':[2,no]})
    for name,success in [(yes,expected),(no,not expected)]:
        variable='PASSES' if success else 'ERRORS'
        blocks[name]=block('data_changevariableby',parent=test,inputs={'VALUE':num(1)},fields={'VARIABLE':[variable,variable]})
        if not success:
            blocks[name]['next']=name+'-case'
            blocks[name+'-case']=block('data_setvariableto',parent=name,inputs={'VALUE':text(key)},fields={'VARIABLE':['FAIL CASE','FAIL CASE']})

command('flag','event_whenflagclicked');blocks['flag'].update(topLevel=True,x=20,y=20)
for name in ['PASSES','ERRORS','FAIL CASE']:
    command('reset-'+name,'data_setvariableto',{'VALUE':num(0)},{'VARIABLE':[name,name]})
command('switch-png','looks_switchbackdropto',{'BACKDROP':text('PNG')})
command('home','motion_gotoxy',{'X':num(-100),'Y':num(0)})
command('settle','control_wait',{'DURATION':num(.2)})
check('png-blue',True,'#1505ff');check('png-not-white',False,'#ffffff')
check('masked-red-blue',True,'#1505ff','#ff0000');check('masked-not-green',False,'#1505ff','#00ff00')
command('right','motion_setx',{'X':num(100)})
check('png-white',True,'#ffffff');check('png-not-blue',False,'#1505ff')
command('show-overlay','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'show-overlay','show-overlay']]})
check('layer-green',True,'#00ff00');check('layer-not-white',False,'#ffffff')
command('transparent-overlay','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'transparent-overlay','transparent-overlay']]})
check('transparent-white',True,'#ffffff')
command('far-overlay','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'far-overlay','far-overlay']]})
check('disjoint-white',True,'#ffffff');check('disjoint-not-green',False,'#00ff00')
command('switch-svg','looks_switchbackdropto',{'BACKDROP':text('SVG')})
command('svg-settle','control_wait',{'DURATION':num(.2)})
command('left','motion_setx',{'X':num(-100)})
check('svg-blue',True,'#1505ff');check('svg-not-white',False,'#ffffff')
check('svg-masked-red',True,'#1505ff','#ff0000')
# The pen canvas sits above the backdrop and below sprites. Breakout erases
# bricks with opaque black stamps; these checks also exercise alpha and lines.
command('pen-reset','pen_clear')
command('opaque-stamp','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'stamp-opaque','stamp-opaque']]})
check('stamp-green',True,'#00ff00');check('stamp-not-blue',False,'#1505ff')
command('clear-opaque','pen_clear')
check('clear-blue',True,'#1505ff');check('clear-not-green',False,'#00ff00')
command('half-stamp','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'stamp-half','stamp-half']]})
# Xbox texture opacity truncates 0.5 * 255 to 127. Stored green/alpha are
# both 127, yielding RGB (11,130,128) over the blue backdrop.
check('stamp-half-blend',True,'#0b8280');check('stamp-half-mask',True,'#0b8280','#ff0000')
command('move-hidden-stamper','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'move-stamper','move-stamper']]})
check('stamp-persists-after-move',True,'#0b8280')
command('clear-half','pen_clear')
check('clear-not-blend',False,'#0b8280');check('clear-blue-again',True,'#1505ff')
command('ink-red','pen_setPenColorToColor',{'COLOR':color('#ff0000')})
command('ink-size','pen_setPenSizeTo',{'SIZE':num(40)})
command('dot-down','pen_penDown');command('dot-up','pen_penUp')
check('dot-red',True,'#ff0000');check('dot-not-blue',False,'#1505ff')
command('clear-dot','pen_clear');check('clear-not-red',False,'#ff0000')
command('line-start','motion_setx',{'X':num(-140)})
command('line-down','pen_penDown');command('line-end','motion_setx',{'X':num(-60)})
command('line-up','pen_penUp');command('line-probe','motion_setx',{'X':num(-100)})
check('line-red',True,'#ff0000');check('line-not-blue',False,'#1505ff');check('line-mask',True,'#ff0000','#ff0000')
command('front-overlay','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'front-overlay','front-overlay']]})
check('sprite-over-pen',True,'#00ff00');check('sprite-hides-pen',False,'#ff0000')
command('hide-front','event_broadcastandwait',{'BROADCAST_INPUT':[1,[11,'transparent-overlay','transparent-overlay']]})
check('transparent-sprite-reveals-pen',True,'#ff0000')
command('clear-last','pen_clear');check('final-blue',True,'#1505ff');check('final-not-red',False,'#ff0000')
command('edge-png','looks_switchbackdropto',{'BACKDROP':text('PNG')})
command('edge-costume','looks_switchcostumeto',{'COSTUME':text('Pixel probe')})
command('edge-home','motion_gotoxy',{'X':num(-1),'Y':num(0)})
command('edge-settle','control_wait',{'DURATION':num(.2)})
check('exact-left-blue',True,'#1505ff','#ff0000')
check('exact-left-not-white',False,'#ffffff','#ff0000')
command('outside-left','motion_setx',{'X':num(-241)})
check('outside-not-blue',False,'#1505ff','#ff0000')
check('outside-not-white',False,'#ffffff','#ff0000')
command('done','control_stop',fields={'STOP_OPTION':['all',None]})
for i,key in enumerate(chain):
    blocks[key]['next']=chain[i+1] if i+1<len(chain) else None
    if i:blocks[key]['parent']=chain[i-1]

common=dict(variables={},lists={},broadcasts={},blocks={},comments={},currentCostume=0,sounds=[],volume=100)
stage=dict(common,isStage=True,name='Stage',costumes=[background,vector_background],layerOrder=0,tempo=60,videoTransparency=50,videoState='off',textToSpeechLanguage=None)
stage['variables']={n:[n,0] for n in ['PASSES','ERRORS','FAIL CASE']}
stage['broadcasts']={n:n for n in ['show-overlay','transparent-overlay','far-overlay']}
subject=dict(common,isStage=False,name='Subject',blocks=blocks,costumes=[red,pixel_probe],layerOrder=100,visible=True,x=-100,y=0,size=100,direction=90,draggable=False,rotationStyle='all around')
overlay=dict(common,isStage=False,name='Overlay',costumes=[green],layerOrder=1,visible=False,x=100,y=0,size=100,direction=135,draggable=False,rotationStyle='all around')
overlay['blocks']={
    'flag':block('event_whenflagclicked','reset-hide',top=True),
    'reset-hide':block('looks_hide','reset-home','flag'),
    'reset-home':block('motion_gotoxy','reset-ghost','reset-hide',{'X':num(100),'Y':num(0)}),
    'reset-ghost':block('looks_seteffectto',parent='reset-home',inputs={'VALUE':num(0)},fields={'EFFECT':['GHOST',None]}),
}
for event,op,inputs,fields in [('show-overlay','looks_show',{},{}),('transparent-overlay','looks_seteffectto',{'VALUE':num(100)},{'EFFECT':['GHOST',None]}),('far-overlay','looks_seteffectto',{'VALUE':num(0)},{'EFFECT':['GHOST',None]})]:
    overlay['blocks'][event]=block('event_whenbroadcastreceived',event+'-do',fields={'BROADCAST_OPTION':[event,event]},top=True)
    overlay['blocks'][event+'-do']=block(op,parent=event,inputs=inputs,fields=fields)
    if event=='far-overlay':
        overlay['blocks'][event+'-do']['next']='far-x'
        overlay['blocks']['far-x']=block('motion_setx',parent=event+'-do',inputs={'X':num(200)})
for event,ghost,x,stamp,show in [('stamp-opaque',0,-100,True,False),('stamp-half',50,-100,True,False),('move-stamper',0,200,False,False),('front-overlay',0,-100,False,True)]:
    stage['broadcasts'][event]=event
    cmds=[('looks_show' if show else 'looks_hide',{},{}),
          ('motion_gotoxy',{'X':num(x),'Y':num(0)},{}),
          ('motion_pointindirection',{'DIRECTION':num(90)},{}),
          ('looks_seteffectto',{'VALUE':num(ghost)},{'EFFECT':['GHOST',None]})]
    if stamp:cmds.append(('pen_stamp',{},{}))
    overlay['blocks'][event]=block('event_whenbroadcastreceived',event+'-0',fields={'BROADCAST_OPTION':[event,event]},top=True)
    for i,(op,inputs,fields) in enumerate(cmds):
        key=event+'-'+str(i);prev=event if i==0 else event+'-'+str(i-1)
        overlay['blocks'][key]=block(op,event+'-'+str(i+1) if i+1<len(cmds) else None,prev,inputs,fields)
distant=[]
for i in range(90):
    d=copy.deepcopy(overlay);d.update(name='Distant '+str(i),blocks={},x=150+(i%5)*10,y=-150+(i//5),size=10,visible=True,layerOrder=2+i)
    distant.append(d)
monitors=[dict(id=n,mode='default',opcode='data_variable',params={'VARIABLE':n},spriteName=None,value=0,width=0,height=0,x=20,y=20+40*i,visible=True,sliderMin=0,sliderMax=100,isDiscrete=True) for i,n in enumerate(['PASSES','ERRORS','FAIL CASE'])]
project=dict(targets=[stage,overlay]+distant+[subject],monitors=monitors,extensions=["pen"],meta={'semver':'3.0.0','vm':'1.1.2','agent':'Color sensing regression'})
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('project.json',json.dumps(project))
    for name,data in assets.items():z.writestr(name,data)
print(output)
