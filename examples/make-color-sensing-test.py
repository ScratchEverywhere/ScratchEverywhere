"""Build a runtime regression for cached/backdrop color sensing and culling.

Expected: PASSES=14, ERRORS=0. This fixture is not a qualification game.
Uses only the Python standard library. Includes 90 distant visible sprites to
exercise query culling, opaque PNG cache restoration, an uploaded SVG backdrop,
masked colors, layered colors and transparent candidates.
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
command('done','control_stop',fields={'STOP_OPTION':['all',None]})
for i,key in enumerate(chain):
    blocks[key]['next']=chain[i+1] if i+1<len(chain) else None
    if i:blocks[key]['parent']=chain[i-1]

common=dict(variables={},lists={},broadcasts={},blocks={},comments={},currentCostume=0,sounds=[],volume=100)
stage=dict(common,isStage=True,name='Stage',costumes=[background,vector_background],layerOrder=0,tempo=60,videoTransparency=50,videoState='off',textToSpeechLanguage=None)
stage['variables']={n:[n,0] for n in ['PASSES','ERRORS','FAIL CASE']}
stage['broadcasts']={n:n for n in ['show-overlay','transparent-overlay','far-overlay']}
subject=dict(common,isStage=False,name='Subject',blocks=blocks,costumes=[red],layerOrder=100,visible=True,x=-100,y=0,size=100,direction=90,draggable=False,rotationStyle='all around')
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
distant=[]
for i in range(90):
    d=copy.deepcopy(overlay);d.update(name='Distant '+str(i),blocks={},x=150+(i%5)*10,y=-150+(i//5),size=10,visible=True,layerOrder=2+i)
    distant.append(d)
monitors=[dict(id=n,mode='default',opcode='data_variable',params={'VARIABLE':n},spriteName=None,value=0,width=0,height=0,x=20,y=20+40*i,visible=True,sliderMin=0,sliderMax=100,isDiscrete=True) for i,n in enumerate(['PASSES','ERRORS','FAIL CASE'])]
project=dict(targets=[stage,overlay]+distant+[subject],monitors=monitors,extensions=[],meta={'semver':'3.0.0','vm':'1.1.2','agent':'Color sensing regression'})
with zipfile.ZipFile(output,'w',zipfile.ZIP_DEFLATED) as z:
    z.writestr('project.json',json.dumps(project))
    for name,data in assets.items():z.writestr(name,data)
print(output)
